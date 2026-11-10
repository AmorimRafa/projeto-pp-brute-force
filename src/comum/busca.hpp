// ============================================================================
//  busca.hpp — Núcleo comum às versões paralelas
//  Projeto: busca exaustiva de pré-imagem de hash (Problema 9)
//  Disciplina: Programação Paralela (CCO085) — 2026/2
//
//  Este arquivo concentra o que precisa ser IDÊNTICO nas versões sequencial,
//  OpenMP e MPI:
//
//    1. a função de hash (mesma lógica do baseline do professor);
//    2. o cálculo do alvo (alvo = hash(ESPACO / 3));
//    3. a leitura dos argumentos de linha de comando;
//    4. a impressão do resultado em um formato único, fácil de medir.
//
//  A lógica de cálculo do baseline NÃO foi alterada: `hash_chave` reproduz
//  exatamente as operações de `h()` em docs/baseline_sequencial.cpp. O próprio
//  baseline é compilado separadamente (sem alterações) para servir de gabarito.
// ============================================================================
#ifndef BUSCA_HPP
#define BUSCA_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace busca {

// Sentinela usado quando nenhuma chave satisfaz o alvo. É o maior uint64_t
// possível, então `min` funciona como "melhor resultado até agora" sem casos
// especiais. Na impressão ele vira -1, igual ao baseline.
constexpr uint64_t NAO_ENCONTRADO = ~uint64_t{0};

// ----------------------------------------------------------------------------
//  Tempo
// ----------------------------------------------------------------------------
//  steady_clock é monotônico: não sofre ajustes do relógio do sistema durante a
//  medição (diferente de system_clock). O baseline usa clock_gettime com
//  CLOCK_MONOTONIC; a semântica é a mesma.
inline double agora() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ----------------------------------------------------------------------------
//  Hash — CÓPIA FIEL da função h() do baseline
// ----------------------------------------------------------------------------
//  É o finalizador do MurmurHash3 (fmix64). Cada etapa é invertível:
//    - x ^= x >> 33 é uma bijeção (pode ser desfeita bit a bit);
//    - multiplicar por uma constante ímpar módulo 2^64 é uma bijeção.
//  Logo `hash_chave` é bijetora: para cada alvo existe UMA única chave.
//  Isso é usado na validação (basta comparar a chave encontrada) e justifica
//  por que o término antecipado pode ser aplicado sem risco de "perder" uma
//  ocorrência anterior.
inline uint64_t hash_chave(uint64_t k) {
    k ^= k >> 33; k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33; return k;
}

// ----------------------------------------------------------------------------
//  Política de escalonamento (OpenMP) e modo de decomposição (MPI)
// ----------------------------------------------------------------------------
enum class Politica { Ciclica, Estatica, Dinamica, Guiada };

inline const char* nome_politica(Politica p) {
    switch (p) {
        case Politica::Ciclica:  return "ciclica";
        case Politica::Estatica: return "estatica";
        case Politica::Dinamica: return "dinamica";
        case Politica::Guiada:   return "guiada";
    }
    return "desconhecida";
}

// ----------------------------------------------------------------------------
//  Configuração compartilhada
// ----------------------------------------------------------------------------
struct Configuracao {
    // Entrada (mesma convenção do baseline: 1º argumento posicional = ESPACO).
    uint64_t espaco = 60000000ULL;

    // Granularidade de trabalho.
    //  - OpenMP: tamanho do bloco de chaves.
    //  - MPI: tamanho MÍNIMO de bloco (o mestre aumenta conforme P).
    uint64_t bloco = 16384;
    bool bloco_definido = false;

    // OpenMP.
    int threads = 0;  // 0 = usar o padrão do ambiente (omp_get_max_threads)

    // Modo de varredura:
    //  - termino_antecipado = true  -> para na primeira ocorrência (baseline).
    //  - termino_antecipado = false -> varre todo o espaço ("--completo").
    bool termino_antecipado = true;

    Politica politica = Politica::Ciclica;

    // MPI: "mestre-escravo" (padrão) ou "estatico".
    std::string modo_mpi = "mestre-escravo";

    // MPI: a cada quantas chaves o modo estático verifica a mensagem de parada.
    uint64_t intervalo = 65536;

    // Derivado em ler_argumentos(): alvo = hash(espaco / 3).
    uint64_t alvo = 0;

    bool ajuda = false;
};

// ----------------------------------------------------------------------------
//  Aritmética segura de blocos
// ----------------------------------------------------------------------------
//  Evita overflow quando o fim do bloco passaria de 2^64 (entradas gigantes).
inline uint64_t fim_do_bloco(uint64_t inicio, uint64_t bloco, uint64_t espaco) {
    return (espaco - inicio < bloco) ? espaco : inicio + bloco;
}

inline uint64_t contar_blocos(uint64_t espaco, uint64_t bloco) {
    return espaco / bloco + (espaco % bloco != 0 ? 1 : 0);
}

// ----------------------------------------------------------------------------
//  Impressão de ajuda
// ----------------------------------------------------------------------------
inline void imprimir_ajuda(const char* programa) {
    std::printf(
        "Uso: %s [ESPACO] [opcoes]\n"
        "\n"
        "  ESPACO                 tamanho do espaco de busca (padrao: 60000000)\n"
        "                         alvo = hash(ESPACO / 3), como no baseline\n"
        "\n"
        "  --threads=N            numero de threads (OpenMP; 0 = padrao)\n"
        "  --politica=NOME        ciclica | estatica | dinamica | guiada\n"
        "                         (estatica/guiada so no modo --completo)\n"
        "  --modo=NOME            mestre-escravo | estatico (MPI)\n"
        "  --bloco=N              chaves por bloco (OpenMP) / bloco minimo (MPI)\n"
        "  --intervalo=N          chaves entre checagens de parada (MPI estatico)\n"
        "  --completo             varre todo o espaco em vez de parar no 1o acerto\n"
        "  --ajuda                mostra esta mensagem\n"
        "\n"
        "Saida (uma linha, formato chave=valor):\n"
        "  versao politica modo espaco alvo chave threads processos\n"
        "  tempo_busca tempo_total mensagens\n"
        "\n"
        "Exemplos:\n"
        "  %s 60000000\n"
        "  %s 60000000 --threads=8 --politica=ciclica\n"
        "  mpirun -np 4 %s 60000000 --modo=estatico\n",
        programa, programa, programa, programa);
}

// ----------------------------------------------------------------------------
//  Leitura de argumentos
// ----------------------------------------------------------------------------
//  Aceita tanto `--opcao=valor` quanto `--opcao valor`. Qualquer argumento que
//  não comece com '-' é tratado como ESPACO (apenas o primeiro).
inline bool ler_argumentos(int argc, char** argv, Configuracao& cfg) {
    bool espaco_definido = false;

    // Devolve o valor de uma opção com '=' ou consome o próximo argumento.
    auto valor_de = [&](int& i, const char* nome) -> const char* {
        const char* arg = argv[i];
        const char* igual = std::strchr(arg, '=');
        if (igual != nullptr) return igual + 1;
        if (i + 1 >= argc) {
            std::fprintf(stderr, "ERRO: a opcao %s exige um valor.\n", nome);
            return nullptr;
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--ajuda") == 0 || std::strcmp(arg, "-h") == 0) {
            cfg.ajuda = true;
            return true;
        }
        if (std::strcmp(arg, "--completo") == 0) {
            cfg.termino_antecipado = false;
            continue;
        }
        if (arg[0] != '-') {
            if (espaco_definido) {
                std::fprintf(stderr, "ERRO: ESPACO foi informado mais de uma vez ('%s').\n", arg);
                return false;
            }
            char* fim = nullptr;
            errno = 0;
            unsigned long long v = std::strtoull(arg, &fim, 10);
            if (errno != 0 || fim == arg || *fim != '\0') {
                std::fprintf(stderr, "ERRO: ESPACO invalido: '%s'.\n", arg);
                return false;
            }
            cfg.espaco = static_cast<uint64_t>(v);
            espaco_definido = true;
            continue;
        }

        // Opções com valor.
        auto comeca_com = [&](const char* prefixo) {
            return std::strncmp(arg, prefixo, std::strlen(prefixo)) == 0;
        };

        if (comeca_com("--threads")) {
            const char* v = valor_de(i, "--threads");
            if (v == nullptr) return false;
            char* fim = nullptr;
            long n = std::strtol(v, &fim, 10);
            if (fim == v || *fim != '\0' || n < 0 || n > 1024) {
                std::fprintf(stderr, "ERRO: --threads invalido: '%s'.\n", v);
                return false;
            }
            cfg.threads = static_cast<int>(n);
            continue;
        }
        if (comeca_com("--politica")) {
            const char* v = valor_de(i, "--politica");
            if (v == nullptr) return false;
            if (std::strcmp(v, "ciclica") == 0)       cfg.politica = Politica::Ciclica;
            else if (std::strcmp(v, "estatica") == 0) cfg.politica = Politica::Estatica;
            else if (std::strcmp(v, "dinamica") == 0) cfg.politica = Politica::Dinamica;
            else if (std::strcmp(v, "guiada") == 0)   cfg.politica = Politica::Guiada;
            else {
                std::fprintf(stderr, "ERRO: --politica desconhecida: '%s'.\n", v);
                return false;
            }
            continue;
        }
        if (comeca_com("--modo")) {
            const char* v = valor_de(i, "--modo");
            if (v == nullptr) return false;
            if (std::strcmp(v, "mestre-escravo") != 0 &&
                std::strcmp(v, "estatico") != 0) {
                std::fprintf(stderr, "ERRO: --modo desconhecido: '%s'.\n", v);
                return false;
            }
            cfg.modo_mpi = v;
            continue;
        }
        if (comeca_com("--bloco")) {
            const char* v = valor_de(i, "--bloco");
            if (v == nullptr) return false;
            char* fim = nullptr;
            unsigned long long b = std::strtoull(v, &fim, 10);
            if (fim == v || *fim != '\0' || b == 0) {
                std::fprintf(stderr, "ERRO: --bloco invalido: '%s'.\n", v);
                return false;
            }
            cfg.bloco = static_cast<uint64_t>(b);
            cfg.bloco_definido = true;
            continue;
        }
        if (comeca_com("--intervalo")) {
            const char* v = valor_de(i, "--intervalo");
            if (v == nullptr) return false;
            char* fim = nullptr;
            unsigned long long n = std::strtoull(v, &fim, 10);
            if (fim == v || *fim != '\0' || n < 1024) {
                std::fprintf(stderr, "ERRO: --intervalo invalido (minimo 1024): '%s'.\n", v);
                return false;
            }
            cfg.intervalo = static_cast<uint64_t>(n);
            continue;
        }

        std::fprintf(stderr, "ERRO: opcao desconhecida: '%s' (use --ajuda).\n", arg);
        return false;
    }

    // Mesma regra do baseline: alvo = h(ESPACO / 3). Não alterar.
    cfg.alvo = hash_chave(cfg.espaco / 3);
    return true;
}

// ----------------------------------------------------------------------------
//  Saída padronizada
// ----------------------------------------------------------------------------
//  Uma única linha, com rótulos. Os scripts de coleta/validação extraem os
//  campos por nome (por exemplo, `sed -n 's/.*chave=\([-0-9]*\).*/\1/p'`).
//  `tempo_busca` mede apenas a região de busca (comparável ao baseline);
//  `tempo_total` inclui inicialização, leitura de argumentos e impressão.
struct Resultado {
    const char* versao;
    const char* politica;
    const char* modo;
    uint64_t espaco;
    uint64_t alvo;
    uint64_t chave;
    int threads;
    int processos;
    double tempo_busca;
    double tempo_total;
    double tempo_coletivas;  // MPI: tempo em Bcast/Allreduce/Reduce
    long long mensagens;
};

inline void imprimir_resultado(const Resultado& r) {
    long long chave_impressa =
        (r.chave == NAO_ENCONTRADO) ? -1LL : static_cast<long long>(r.chave);

    std::printf(
        "versao=%s politica=%s modo=%s espaco=%" PRIu64 " alvo=%" PRIu64
        " chave=%lld threads=%d processos=%d tempo_busca=%.6f tempo_total=%.6f"
        " tempo_coletivas=%.6f mensagens=%lld\n",
        r.versao, r.politica, r.modo, r.espaco, r.alvo, chave_impressa,
        r.threads, r.processos, r.tempo_busca, r.tempo_total,
        r.tempo_coletivas, r.mensagens);
}

}  // namespace busca

#endif  // BUSCA_HPP
