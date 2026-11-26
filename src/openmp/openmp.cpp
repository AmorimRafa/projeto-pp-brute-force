// ============================================================================
//  openmp.cpp — versão de memória compartilhada
//  Problema 9 — Busca exaustiva (força bruta)
//  Programação Paralela (CCO085) — 2026/2
//
//  ESTRATÉGIA
//  ---------
//  O baseline varre k = 0, 1, 2, ... e para no primeiro acerto. Aqui o espaço
//  é dividido em BLOCOS PEQUENOS e cada bloco é entregue a uma thread sob
//  demanda (schedule(dynamic, 1)). Assim todas as threads avançam "lado a
//  lado" e a parada antecipada não deixa ninguém preso num trecho gigante.
//
//  CONDIÇÃO DE CORRIDA
//  ---------------------------------------------------------------
//  * `parar` -> std::atomic<bool>: uma thread escreve e todas leem. Sem atomic
//    haveria corrida. Foi escolhido atomic (e não critical) porque é só um
//    booleano e a leitura acontece no início de cada bloco, não a cada chave.
//  * `melhor_local` é variável de pilha, privada de cada thread. No fim, cada
//    thread entra UMA vez em `#pragma omp critical` para fundir seu resultado
//    no global. Critical a cada chave seria um gargalo.
//  * `reduction(min:)` NÃO foi usada: ela exige que o laço termine sempre, e
//    aqui nós saímos cedo quando encontramos a chave.
//  * Não existe vetor compartilhado escrito pelas threads; portanto não há
//    FALSE SHARING (duas threads disputando a mesma linha de cache).
// ============================================================================
#include <omp.h>

#include <atomic>
#include <cstdio>

#include "comum/busca.hpp"

using namespace busca;

namespace {

// Bloco de 16.384 chaves: pequeno o bastante para balancear o término
// antecipado e grande o bastante para a checagem da flag não dominar o tempo.
constexpr uint64_t BLOCO = 16384;

// Número de blocos, arredondando para cima sem estourar uint64_t.
inline uint64_t total_de_blocos(uint64_t espaco) {
    return espaco / BLOCO + (espaco % BLOCO != 0 ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
    Configuracao cfg;
    if (!ler_argumentos(argc, argv, cfg)) return 2; //

    if (cfg.ajuda) {
        imprimir_ajuda(argv[0]);
        return 0;
    }

    omp_set_dynamic(0); 
    //bloqueia o ajuste dinamico do openMP 
    //Garante que todas as threads selecionadas serão usadas 

    // Se o usuário não passou --threads, usamos o padrão do ambiente.
    // É o operador curto `cond ? sim : nao` de novo.
    int threads = (cfg.threads > 0 ) ? cfg.threads : omp_get_max_threads();

    const uint64_t blocos = total_de_blocos(cfg.espaco);
    const uint64_t alvo = cfg.alvo;

    // `std::atomic<bool>` é um booleano com acesso SEGURO entre threads.
    // Sem `atomic`, uma thread poderia escrever `true` enquanto outra lia, e
    // o compilador poderia estar usando uma cópia antiga em cache: corrida.
    // Aqui `false` é o valor inicial (ninguém achou nada ainda).
    std::atomic<bool> parar{false};

    // Resultado global. Só é escrito depois, dentro do `critical`.
    uint64_t melhor = NAO_ENCONTRADO;

    const double inicio = agora();

// `#pragma` é uma instrução ESPECIAL para o compilador: em C++ puro isso não
// existe. Quem entende esta linha é o compilador chamado com -fopenmp.
// `parallel` cria `threads` threads. Tudo que está entre as chaves abaixo é
// executado por CADA uma delas; depois do `}` o programa volta a 1 thread.
#pragma omp parallel num_threads(threads)
    {
        // Declarada DENTRO da região parallel: cada thread ganha sua própria
        // cópia na pilha. Ninguém mais mexe nela, então não há corrida nem
        // false sharing aqui.
        uint64_t melhor_local = NAO_ENCONTRADO;

        // Outra diretiva: ela transforma este `for` numa FILA de blocos.
        // `dynamic, 1` = "quem terminar seu bloco, pega o próximo da fila,
        // um de cada vez". Com `static`, cada thread receberia sua parte de
        // uma vez só — aqui isso desperdiçaria tempo por causa da parada
        // antecipada (a resposta fica em 1/3 do espaço).
#pragma omp for schedule(dynamic, 1)
        for (uint64_t b = 0; b < blocos; ++b) {
            // Flag que registra globalmente para todas as threads se o valor já foi achado
            // "Continue" pula o laco que navega entre as chaves
            if (parar.load(std::memory_order_relaxed)) continue;

            // O último bloco pode ser menor que BLOCO; o `?:` aqui é um
            // "mínimo" escrito na mão: fim = min(inicio + BLOCO, espaco).
            const uint64_t inicio_bloco = b * BLOCO;
            const uint64_t fim_bloco = (cfg.espaco - inicio_bloco < BLOCO) ? cfg.espaco : inicio_bloco + BLOCO;

            // O laço de verdade: testa uma chave por vez, como o baseline.
            for (uint64_t k = inicio_bloco; k < fim_bloco; ++k) {
                if (hash_chave(k) == alvo) {
                    melhor_local = k;                              // guarda a chave
                    parar.store(true, std::memory_order_relaxed);  // avisa as outras
                    break;  // sai SÓ deste laço interno; a thread ainda
                            // continuará no laço de blocos, mas vai pular todos.
                }
            }
        }

        // `#pragma omp critical(nome)` = "uma thread de cada vez neste trecho".
        // Cada thread entra UMA única vez, no fim, para juntar seu resultado
        // no global. Colocar o critical dentro do laço de chaves travaria as
        // threads a cada iteração e destruiria o desempenho.
        // O `<` (menor índice) reproduz a ideia de "primeira ocorrência".
#pragma omp critical(fusao_resultado)
        {
            if (melhor_local < melhor) {
                melhor = melhor_local;
            }
            
        }
    }

    const double fim = agora();

    Resultado r;
    r.versao = "openmp";
    r.espaco = cfg.espaco;
    r.alvo = cfg.alvo;
    r.chave = melhor;
    r.threads = threads;
    r.processos = 1;
    r.tempo_busca = fim - inicio;
    r.mensagens = 0;
    imprimir_resultado(r);
    return 0;
}
