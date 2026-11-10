// ============================================================================
//  mpi.cpp — Busca exaustiva paralela com MPI
//  Problema 9 — Programação Paralela (CCO085) — 2026/2
//
//  DOIS MODOS DE DECOMPOSIÇÃO (veja docs/DECISAO_MPI_MODOS.md)
//  -----------------------------------------------------------
//  1. --modo=mestre-escravo (padrão)
//     O rank 0 distribui blocos sob demanda. Os demais varrem e pedem mais.
//     É o padrão destacado no enunciado do problema 9. Custo estrutural:
//     um processo não busca, então o speedup ideal é (P-1) e a eficiência
//     máxima é (P-1)/P.
//
//  2. --modo=estatico
//     Decomposição de domínio CÍCLICA EM BLOCOS: o espaço é dividido em
//     blocos de `--intervalo` chaves (padrão 65.536) e o rank r processa os
//     blocos r, r+P, r+2P, ... em ordem crescente. Todos os processos buscam.
//     O término é avisado por mensagem ponto a ponto (MPI_Iprobe ao fim de
//     cada bloco). É o modo que escala mais próximo de P e serve de
//     referência para a escalabilidade fraca.
//
//  OPERAÇÕES COLETIVAS
//  -------------------
//  - MPI_Bcast: espalha a configuração (espaço, alvo, bloco, intervalo, modo)
//    a partir do rank 0. Garante que todos os processos usem exatamente os
//    mesmos parâmetros, independentemente de como o launcher montou o argv.
//  - MPI_Allreduce(MPI_MIN): consolida a menor chave encontrada. Usar MIN (e
//    não apenas a mensagem de resultado) mantém a semântica do baseline e
//    torna o resultado independente da ordem de chegada das mensagens.
//  - MPI_Reduce(MPI_SUM): soma a contagem de mensagens ponto a ponto para a
//    análise de overhead (cada mensagem é contada pelo remetente).
//
//  PROTOCOLO MESTRE-ESCRAVO (tags)
//  -------------------------------
//    BLOCO      mestre -> escravo   2 x uint64: [inicio, fim)
//    PEDIDO     escravo -> mestre   0 bytes: "terminei, me dê outro"
//    RESULTADO  escravo -> mestre   1 x uint64: chave encontrada
//    PARAR      mestre -> escravo   0 bytes: encerrar agora
//    FIM        mestre -> escravo   0 bytes: não há mais blocos
//
//  Todas as mensagens têm no máximo 16 bytes e usam o protocolo eager do MPI;
//  não há dependência circular de buffers, portanto não há deadlock.
// ============================================================================
#include <mpi.h>

#include <algorithm>
#include <cstdio>

#include "comum/busca.hpp"

using namespace busca;

namespace {

// Tags do protocolo. Valores explícitos para facilitar a leitura de traces.
constexpr int TAG_BLOCO     = 101;
constexpr int TAG_PEDIDO    = 102;
constexpr int TAG_RESULTADO = 103;
constexpr int TAG_PARAR     = 104;
constexpr int TAG_FIM       = 105;

// Envia um intervalo de trabalho. 2 x uint64_t cabem em uma única mensagem.
void enviar_bloco(int destino, uint64_t inicio, uint64_t fim, long long& mensagens) {
    uint64_t dados[2] = {inicio, fim};
    MPI_Send(dados, 2, MPI_UINT64_T, destino, TAG_BLOCO, MPI_COMM_WORLD);
    ++mensagens;
}

// Envia um sinal sem conteúdo (PARAR ou FIM). O buffer é válido mesmo com
// contagem zero; usar &nada evita implementações que rejeitam ponteiro nulo.
void enviar_sinal(int destino, int tag, long long& mensagens) {
    uint64_t nada = 0;
    MPI_Send(&nada, 0, MPI_UINT64_T, destino, tag, MPI_COMM_WORLD);
    ++mensagens;
}

// ----------------------------------------------------------------------------
//  Varredura de um intervalo contíguo em janelas de 1024 chaves
// ----------------------------------------------------------------------------
//  O laço interno compacto permite ao compilador desenrolar e sobrepor as
//  multiplicações do hash. É a mesma estrutura usada na versão OpenMP, para
//  que a comparação de tempo entre as versões seja justa.
template <bool PARAR_NO_PRIMEIRO>
uint64_t varrer_intervalo(uint64_t inicio, uint64_t fim, uint64_t alvo) {
    uint64_t achado = NAO_ENCONTRADO;
    for (uint64_t k = inicio; k < fim;) {
        const uint64_t limite = (fim - k < 1024) ? fim : k + 1024;
        for (; k < limite; ++k) {
            if (hash_chave(k) == alvo) {
                achado = k;
                if (PARAR_NO_PRIMEIRO) return achado;
            }
        }
    }
    return achado;
}

// ----------------------------------------------------------------------------
//  Modo estático: decomposição de domínio em blocos cíclicos
// ----------------------------------------------------------------------------
//  O espaço é dividido em blocos de `--intervalo` chaves (padrão 65.536).
//  O rank r processa os blocos r, r+P, r+2P, ... em ordem crescente. Essa
//  distribuição tem duas vantagens sobre "uma chave por rank":
//
//    1. o laço interno percorre um intervalo contíguo (k++), o que permite ao
//       compilador desenrolar/otimizar como na versão OpenMP;
//    2. o trabalho continua balanceado: todos os ranks avançam juntos e param
//       com diferença de no máximo um bloco.
//
//  A cada bloco, uma verificação NÃO BLOQUEANTE (MPI_Iprobe) vê se outro rank
//  encontrou a chave. Quem encontra envia PARAR a todos; a resposta final vem
//  do Allreduce. O resultado é o mesmo do baseline (o hash é bijetor).
uint64_t buscar_estatico(const Configuracao& cfg, int rank, int processos,
                         long long& mensagens) {
    uint64_t melhor = NAO_ENCONTRADO;

    const uint64_t bloco = std::max<uint64_t>(cfg.intervalo, 1024);
    const uint64_t num_blocos = contar_blocos(cfg.espaco, bloco);

    for (uint64_t b = static_cast<uint64_t>(rank); b < num_blocos;
         b += static_cast<uint64_t>(processos)) {
        const uint64_t inicio = b * bloco;
        const uint64_t fim = fim_do_bloco(inicio, bloco, cfg.espaco);

        // Bloco contíguo: mesmo laço otimizado da versão OpenMP.
        const uint64_t achado = cfg.termino_antecipado
                                    ? varrer_intervalo<true>(inicio, fim, cfg.alvo)
                                    : varrer_intervalo<false>(inicio, fim, cfg.alvo);

        if (achado != NAO_ENCONTRADO && achado < melhor) melhor = achado;

        if (!cfg.termino_antecipado) continue;

        if (melhor != NAO_ENCONTRADO) {
            // Avisa todos os outros ranks. Mensagens de 8 bytes: eager.
            for (int destino = 0; destino < processos; ++destino) {
                if (destino == rank) continue;
                MPI_Send(&melhor, 1, MPI_UINT64_T, destino, TAG_PARAR,
                         MPI_COMM_WORLD);
                ++mensagens;
            }
            break;
        }

        // Checagem de término: feita entre blocos, nunca no laço quente.
        int chegou = 0;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_PARAR, MPI_COMM_WORLD, &chegou, &status);

        if (chegou) {
            uint64_t chave_recebida = NAO_ENCONTRADO;
            MPI_Recv(&chave_recebida, 1, MPI_UINT64_T, status.MPI_SOURCE,
                     TAG_PARAR, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            ++mensagens;
            if (chave_recebida < melhor) melhor = chave_recebida;
            break;
        }
    }

    return melhor;
}

// ----------------------------------------------------------------------------
//  Modo mestre-escravo: blocos sob demanda
// ----------------------------------------------------------------------------
uint64_t buscar_mestre_escravo(const Configuracao& cfg, int rank, int processos,
                               uint64_t bloco, long long& mensagens) {
    uint64_t melhor = NAO_ENCONTRADO;

    // Caso especial: um único processo. Sem escravos, o próprio rank 0 faz a
    // busca sequencial, para que a curva de speedup comece em 1,0x.
    if (processos == 1) {
        if (cfg.termino_antecipado) {
            return varrer_intervalo<true>(0, cfg.espaco, cfg.alvo);
        }
        return varrer_intervalo<false>(0, cfg.espaco, cfg.alvo);
    }

    const uint64_t num_blocos = contar_blocos(cfg.espaco, bloco);

    if (rank == 0) {
        // ===================== MESTRE =====================
        uint64_t proximo = 0;
        int ativos = 0;

        // Despacho inicial: um bloco para cada escravo que houver trabalho.
        for (int destino = 1; destino < processos; ++destino) {
            if (proximo < num_blocos) {
                const uint64_t inicio = proximo * bloco;
                enviar_bloco(destino, inicio,
                             fim_do_bloco(inicio, bloco, cfg.espaco), mensagens);
                ++proximo;
                ++ativos;
            } else {
                enviar_sinal(destino, TAG_FIM, mensagens);
            }
        }

        bool encontrou = false;

        // Laço de coordenação: recebe pedidos/resultados de qualquer escravo.
        while (ativos > 0) {
            uint64_t chave = NAO_ENCONTRADO;
            MPI_Status status;
            MPI_Recv(&chave, 1, MPI_UINT64_T, MPI_ANY_SOURCE, MPI_ANY_TAG,
                     MPI_COMM_WORLD, &status);
            ++mensagens;

            if (status.MPI_TAG == TAG_RESULTADO) {
                // Término antecipado: guarda a chave e manda todos pararem.
                if (chave < melhor) melhor = chave;
                encontrou = true;
                break;
            }

            // TAG_PEDIDO: entrega o próximo bloco ou encerra esse escravo.
            if (proximo < num_blocos) {
                const uint64_t inicio = proximo * bloco;
                enviar_bloco(status.MPI_SOURCE, inicio,
                             fim_do_bloco(inicio, bloco, cfg.espaco), mensagens);
                ++proximo;
            } else {
                enviar_sinal(status.MPI_SOURCE, TAG_FIM, mensagens);
                --ativos;
            }
        }

        if (encontrou) {
            // Envia PARAR a TODOS, inclusive ao escravo que encontrou (ele está
            // bloqueado esperando a próxima ordem). Pedidos que ainda estiverem
            // na fila do mestre são simplesmente ignorados: o escravo recebe o
            // PARAR que já foi enviado.
            for (int destino = 1; destino < processos; ++destino) {
                enviar_sinal(destino, TAG_PARAR, mensagens);
            }
        }

        return melhor;
    }

    // ===================== ESCRAVO =====================
    for (;;) {
        uint64_t dados[2] = {0, 0};  // [inicio, fim)
        MPI_Status status;
        MPI_Recv(dados, 2, MPI_UINT64_T, 0, MPI_ANY_TAG, MPI_COMM_WORLD,
                 &status);
        ++mensagens;

        if (status.MPI_TAG == TAG_PARAR || status.MPI_TAG == TAG_FIM) break;

        const uint64_t inicio = dados[0];
        const uint64_t fim = dados[1];
        const uint64_t achado = cfg.termino_antecipado
                                    ? varrer_intervalo<true>(inicio, fim, cfg.alvo)
                                    : varrer_intervalo<false>(inicio, fim, cfg.alvo);
        const bool achou = (achado != NAO_ENCONTRADO);
        if (achou && achado < melhor) melhor = achado;

        if (achou && cfg.termino_antecipado) {
            // Avisa o mestre e aguarda o PARAR (volta ao topo do laço).
            MPI_Send(&melhor, 1, MPI_UINT64_T, 0, TAG_RESULTADO,
                     MPI_COMM_WORLD);
            ++mensagens;
        } else {
            // Terminou o bloco (ou não há parada antecipada): pede outro.
            uint64_t nada = 0;
            MPI_Send(&nada, 0, MPI_UINT64_T, 0, TAG_PEDIDO, MPI_COMM_WORLD);
            ++mensagens;
        }
    }

    return melhor;
}

}  // namespace

int main(int argc, char** argv) {
    const double inicio_programa = agora();

    MPI_Init(&argc, &argv);

    int rank = 0;
    int processos = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processos);

    Configuracao cfg;
    if (!ler_argumentos(argc, argv, cfg)) {
        MPI_Finalize();
        return 2;
    }
    if (cfg.ajuda) {
        if (rank == 0) imprimir_ajuda(argv[0]);
        MPI_Finalize();
        return 0;
    }

    // ------------------------------------------------------------------
    //  Tamanho do bloco do mestre-escravo
    // ------------------------------------------------------------------
    //  O padrão é ~128 blocos por escravo: blocos grandes o suficiente para
    //  amortizar a latência das mensagens e pequenos o suficiente para que o
    //  término antecipado não desperdice muito trabalho. `--bloco` define um
    //  piso (granularidade mínima).
    uint64_t bloco = cfg.bloco;
    if (!cfg.bloco_definido) {
        const uint64_t trabalhadores = (processos > 1)
                                           ? static_cast<uint64_t>(processos - 1)
                                           : 1;
        const uint64_t automatico = cfg.espaco / (trabalhadores * 128);
        if (automatico > bloco) bloco = automatico;
    }

    // ------------------------------------------------------------------
    //  Coletiva 1: configuração (MPI_Bcast)
    // ------------------------------------------------------------------
    //  Mesmo que todos os ranks tenham lido o mesmo argv, o broadcast torna o
    //  programa explícito e garante que rank 0 é a única fonte da verdade.
    const double inicio_bcast = agora();
    {
        uint64_t pacote[4] = {cfg.espaco, cfg.alvo, bloco, cfg.intervalo};
        MPI_Bcast(pacote, 4, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        cfg.espaco = pacote[0];
        cfg.alvo = pacote[1];
        bloco = pacote[2];
        cfg.intervalo = pacote[3];

        int sinalizadores[2] = {cfg.termino_antecipado ? 1 : 0,
                                (cfg.modo_mpi == "estatico") ? 1 : 0};
        MPI_Bcast(sinalizadores, 2, MPI_INT, 0, MPI_COMM_WORLD);
        cfg.termino_antecipado = (sinalizadores[0] != 0);
        cfg.modo_mpi = (sinalizadores[1] != 0) ? "estatico" : "mestre-escravo";
    }
    const double tempo_bcast = agora() - inicio_bcast;

    // ------------------------------------------------------------------
    //  Busca
    // ------------------------------------------------------------------
    long long mensagens = 0;
    uint64_t melhor = NAO_ENCONTRADO;

    const double inicio_busca = agora();
    if (cfg.modo_mpi == "estatico") {
        melhor = buscar_estatico(cfg, rank, processos, mensagens);
    } else {
        melhor = buscar_mestre_escravo(cfg, rank, processos, bloco, mensagens);
    }
    const double fim_busca = agora();

    // ------------------------------------------------------------------
    //  Coletivas 2 e 3: resultado (Allreduce MIN) e mensagens (Reduce SUM)
    // ------------------------------------------------------------------
    const double inicio_coletivas = agora();

    uint64_t chave_global = NAO_ENCONTRADO;
    MPI_Allreduce(&melhor, &chave_global, 1, MPI_UINT64_T, MPI_MIN,
                  MPI_COMM_WORLD);

    long long mensagens_total = 0;
    MPI_Reduce(&mensagens, &mensagens_total, 1, MPI_LONG_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);

    const double tempo_coletivas =
        tempo_bcast + (agora() - inicio_coletivas);

    // ------------------------------------------------------------------
    //  Saída (somente rank 0, para não misturar linhas no CSV)
    // ------------------------------------------------------------------
    if (rank == 0) {
        Resultado r;
        r.versao = "mpi";
        r.politica = cfg.modo_mpi.c_str();
        r.modo = cfg.termino_antecipado ? "antecipado" : "completo";
        r.espaco = cfg.espaco;
        r.alvo = cfg.alvo;
        r.chave = chave_global;
        r.threads = 1;
        r.processos = processos;
        r.tempo_busca = fim_busca - inicio_busca;
        r.tempo_total = agora() - inicio_programa;
        r.tempo_coletivas = tempo_coletivas;
        r.mensagens = mensagens_total;
        imprimir_resultado(r);
    }

    MPI_Finalize();
    return 0;
}
