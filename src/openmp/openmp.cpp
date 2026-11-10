// ============================================================================
//  openmp.cpp — Busca exaustiva paralela com OpenMP
//  Problema 9 — Programação Paralela (CCO085) — 2026/2
//
//  ESTRATÉGIA
//  ----------
//  O baseline varre k = 0, 1, 2, ... e para na primeira chave cujo hash é o
//  alvo. Paralelizar "do jeito óbvio" (um bloco contíguo por thread) é ruim
//  neste problema por causa do término antecipado: a thread que contém
//  ESPACO/3 começa perto do alvo, as outras terminam antes e ficam ociosas.
//
//  Aqui a distribuição principal é CÍCLICA EM BLOCOS PEQUENOS:
//
//      thread 0: bloco 0, bloco T, bloco 2T, ...
//      thread 1: bloco 1, bloco T+1, bloco 2T+1, ...
//
//  Todas as threads caminham "lado a lado" pelo espaço; quando uma encontra a
//  chave, as demais estão a no máximo alguns milhares de chaves de distância.
//  Isso mantém o custo por thread equilibrado e o speedup próximo de T.
//
//  CONDIÇÃO DE CORRIDA E ESCOLHA DAS PRIMITIVAS
//  --------------------------------------------
//  - `parar` (std::atomic<bool>): é escrito por uma thread quando ela encontra
//    a chave e lido por todas as outras. `atomic` evita a corrida com o menor
//    custo possível para um booleano. A leitura é feita a cada 1024 chaves,
//    não a cada chave, para não gerar tráfego de coerência de cache no laço
//    quente (o hash é o que domina o tempo).
//  - `#pragma omp critical`: usado UMA vez por thread, no final, para fundir o
//    melhor resultado local no resultado global. Crítico a cada iteração seria
//    um gargalo; `atomic` também seria desnecessário para uma operação que
//    acontece O(T) vezes.
//  - `reduction(min:)`: usado no modo `--completo`, em que NÃO há parada
//    antecipada. É a forma correta e mais rápida: o compilador mantém uma
//    cópia privada por thread e combina no final, sem seção crítica.
//    `reduction` não pode ser combinada com `break`/parada antecipada, por
//    isso os dois modos têm laços separados.
//  - As variáveis do laço são locais de cada thread (pilha), então não há
//    escrita compartilhada no caminho quente e não há false sharing.
// ============================================================================
#include <omp.h>

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "comum/busca.hpp"

using namespace busca;

namespace {

// Traduz a política escolhida para a API de escalonamento em tempo de execução
// do OpenMP (`schedule(runtime)`). Assim o corpo do laço é escrito uma vez só.
struct PlanoEscalonamento {
    omp_sched_t tipo;
    int pedaco;
};

PlanoEscalonamento planejar(Politica politica, long long total_blocos, int threads) {
    switch (politica) {
        case Politica::Ciclica:
            // Round-robin de blocos: 1 bloco por vez para cada thread.
            return {omp_sched_static, 1};

        case Politica::Estatica: {
            // Um intervalo contíguo de blocos por thread (equivale ao
            // `schedule(static)` clássico). Só é permitido no modo completo.
            long long pedaco = (total_blocos + threads - 1) / threads;
            if (pedaco < 1) pedaco = 1;
            return {omp_sched_static, static_cast<int>(pedaco)};
        }

        case Politica::Dinamica:
            // Cada bloco é entregue sob demanda: bom quando o custo varia.
            return {omp_sched_dynamic, 1};

        case Politica::Guiada:
            // Blocos grandes no início, menores no fim.
            return {omp_sched_guided, 1};
    }
    return {omp_sched_static, 1};
}

}  // namespace

int main(int argc, char** argv) {
    const double inicio_programa = agora();

    Configuracao cfg;
    if (!ler_argumentos(argc, argv, cfg)) return 2;
    if (cfg.ajuda) {
        imprimir_ajuda(argv[0]);
        return 0;
    }

    // No modo com término antecipado, `estatica` (bloco contíguo) e `guiada`
    // (blocos iniciais grandes) fariam o tempo depender da posição da chave:
    // uma thread poderia abandonar um trecho ainda não verificado. Como o hash
    // é bijetor o resultado continuaria correto, mas a medição deixaria de ser
    // uma comparação justa. Elas ficam restritas ao modo `--completo`.
    if (cfg.termino_antecipado &&
        (cfg.politica == Politica::Estatica || cfg.politica == Politica::Guiada)) {
        std::fprintf(stderr,
                     "ERRO: no modo com termino antecipado use --politica=ciclica "
                     "ou --politica=dinamica.\n"
                     "      As politicas estatica/guiada so fazem sentido com "
                     "--completo (veja o README).\n");
        return 2;
    }

    omp_set_dynamic(0);  // número de threads fixo: medicao reprodutível
    int threads = (cfg.threads > 0) ? cfg.threads : omp_get_max_threads();
    if (threads < 1) threads = 1;

    const uint64_t total_blocos64 = contar_blocos(cfg.espaco, cfg.bloco);
    const long long total_blocos = static_cast<long long>(total_blocos64);

    const PlanoEscalonamento plano = planejar(cfg.politica, total_blocos, threads);
    omp_set_schedule(plano.tipo, plano.pedaco);

    uint64_t melhor = NAO_ENCONTRADO;

    const double inicio_busca = agora();

    if (cfg.termino_antecipado) {
        // --------------------------------------------------------------------
        //  Modo com término antecipado: flag atômica + fusão por critical.
        // --------------------------------------------------------------------
        std::atomic<bool> parar{false};

#pragma omp parallel num_threads(threads)
        {
            uint64_t melhor_local = NAO_ENCONTRADO;

#pragma omp for schedule(runtime)
            for (long long b = 0; b < total_blocos; ++b) {
                // Blocos posteriores ao que encontrou a chave são descartados.
                if (parar.load(std::memory_order_relaxed)) continue;

                const uint64_t inicio = static_cast<uint64_t>(b) * cfg.bloco;
                const uint64_t fim = fim_do_bloco(inicio, cfg.bloco, cfg.espaco);

                // Varre o bloco em janelas de 1024 chaves. A checagem da flag
                // só acontece entre janelas, mantendo o laço interno compacto.
                for (uint64_t k = inicio; k < fim;) {
                    const uint64_t limite = (fim - k < 1024) ? fim : k + 1024;
                    for (; k < limite; ++k) {
                        if (hash_chave(k) == cfg.alvo) {
                            melhor_local = k;
                            break;
                        }
                    }
                    if (melhor_local != NAO_ENCONTRADO) {
                        parar.store(true, std::memory_order_relaxed);
                        break;
                    }
                    if (parar.load(std::memory_order_relaxed)) break;
                }
            }

            // Fusão: uma vez por thread. `local < melhor` obtém o menor índice,
            // mantendo a semântica de "primeira ocorrência" do baseline.
#pragma omp critical(busca_fusao)
            {
                if (melhor_local < melhor) melhor = melhor_local;
            }
        }
    } else {
        // --------------------------------------------------------------------
        //  Modo completo: sem parada, redução nativa de mínimo.
        // --------------------------------------------------------------------
        const uint64_t alvo = cfg.alvo;

#pragma omp parallel for schedule(runtime) num_threads(threads) reduction(min : melhor)
        for (long long b = 0; b < total_blocos; ++b) {
            const uint64_t inicio = static_cast<uint64_t>(b) * cfg.bloco;
            const uint64_t fim = fim_do_bloco(inicio, cfg.bloco, cfg.espaco);
            for (uint64_t k = inicio; k < fim; ++k) {
                if (hash_chave(k) == alvo && k < melhor) melhor = k;
            }
        }
    }

    const double fim_busca = agora();

    Resultado r;
    r.versao = "openmp";
    r.politica = nome_politica(cfg.politica);
    r.modo = cfg.termino_antecipado ? "antecipado" : "completo";
    r.espaco = cfg.espaco;
    r.alvo = cfg.alvo;
    r.chave = melhor;
    r.threads = threads;
    r.processos = 1;
    r.tempo_busca = fim_busca - inicio_busca;
    r.tempo_total = agora() - inicio_programa;
    r.tempo_coletivas = 0.0;
    r.mensagens = 0;
    imprimir_resultado(r);

    return 0;
}
