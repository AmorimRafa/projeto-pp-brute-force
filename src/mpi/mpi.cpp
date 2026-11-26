// ============================================================================
//  mpi.cpp — versão de memória distribuída (mestre-escravo)
//  Problema 9 — Busca exaustiva (força bruta)
//  Programação Paralela (CCO085) — 2026/2
//
//  DECOMPOSIÇÃO (pedido do enunciado)
//  ----------------------------------
//  O problema 9 é naturalmente mestre-escravo: existe um espaço de chaves
//  particionável e um critério de parada que pode acontecer antes de varrer
//  tudo. O rank 0 é o MESTRE: não busca, só distribui blocos e coordena o
//  término. Os ranks 1..P-1 são ESCRAVOS: pedem um bloco, varrem, pedem outro.
//
//  Por que só esse padrão? É o destacado no enunciado, mantém o código curto
//  e permite discutir o principal afastamento da Lei de Amdahl: com P processos
//  apenas P-1 buscam, então o speedup máximo é P-1 e a eficiência (P-1)/P.
//  A decisão está registrada em docs/ANALISE_EXPERIMENTAL.txt (seção 7).
//
//  OPERAÇÕES COLETIVAS (pedido do enunciado: pelo menos uma)
//  ---------------------------------------------------------
//  1. MPI_Bcast: o rank 0 espalha espaço, alvo e tamanho do bloco. Todos os
//     processos passam a usar exatamente os mesmos parâmetros.
//  2. MPI_Allreduce(MPI_MIN): consolida a menor chave achada. Usar MIN garante
//     a semântica de "primeira ocorrência" do baseline, independente da ordem
//     em que as mensagens chegaram ao mestre.
//
//  PROTOCOLO PONTO A PONTO
//  -----------------------
//    BLOCO      mestre -> escravo : 2 x uint64 [inicio, fim)
//    PEDIDO     escravo -> mestre : 0 bytes ("terminei, me dê outro")
//    RESULTADO  escravo -> mestre : 1 x uint64 (chave encontrada)
//    PARAR      mestre -> escravo : 0 bytes (sair: achou ou acabou o trabalho)
//
//  As mensagens têm no máximo 16 bytes, então usam o protocolo eager do MPI:
//  não há espera circular de buffer e, portanto, não há deadlock.
//
//  LEITURA GUIADA:
//    1. `MPI_Init` abre o ambiente; o mesmo `main` roda em P cópias;
//    2. cada cópia descobre seu `rank` (crachá) e qual é seu papel;
//    3. `MPI_Bcast` espalha os parâmetros para todo mundo;
//    4. rank 0 roda `mestre`; ranks 1..P-1 rodam `escravo`;
//    5. `MPI_Allreduce` junta a menor chave;
//    6. só o rank 0 imprime a linha de resultado.
// ============================================================================
#include <mpi.h>

#include <cstdio>

#include "comum/busca.hpp"

using namespace busca;

namespace {

// Cada mensagem é como um envelope com um RÓTULO dizendo o que há dentro.
// Sem rótulo, quem recebe não saberia se chegou um bloco, um pedido ou um
// "pare". `enum` só dá nomes amigáveis para os números 1, 2, 3 e 4.

// ---------------------CONFIG DE MENSAGENS E ESPAÇO-------------------------
enum Tag { TAG_BLOCO = 1, TAG_PEDIDO = 2, TAG_RESULTADO = 3, TAG_PARAR = 4 };

// Varre um intervalo contíguo e para no primeiro acerto, como o baseline.
uint64_t varrer(uint64_t inicio, uint64_t fim, uint64_t alvo) {
    for (uint64_t k = inicio; k < fim; ++k) {
        if (hash_chave(k) == alvo) return k;
    }
    return NAO_ENCONTRADO;
}

uint64_t contar_blocos(uint64_t espaco, uint64_t bloco) {
    return espaco / bloco + (espaco % bloco != 0 ? 1 : 0); //arredonda para uma unidade a mais caso a divisao nao seja exata
}

// A ordem dos argumentos do MPI_Send é:
//   MPI_Send(o_que_mandar, quantos, tipo_de_cada_um, para_quem, rotulo, grupo)
// Aqui mandamos 2 números de 64 bits: o início e o fim do intervalo.
// O `[inicio, fim)` com parêntese no fim quer dizer "inclui início, não inclui fim".
void enviar_bloco(int destino, uint64_t inicio, uint64_t fim) {
    uint64_t dados[2] = {inicio, fim};
    MPI_Send(dados, 2, MPI_UINT64_T, destino, TAG_BLOCO, MPI_COMM_WORLD);
}

// Mensagem de 0 bytes (quantos = 0): o conteúdo não importa, só o rótulo.
// É um "bilhete vazio" que significa "pode sair, não há mais trabalho".
void enviar_sinal(int destino) {
    uint64_t nada = 0;
    MPI_Send(&nada, 0, MPI_UINT64_T, destino, TAG_PARAR, MPI_COMM_WORLD);
}

// ------------------------------- MESTRE -------------------------------------
// Devolve a melhor chave. `mensagens` é contado AQUI: como toda mensagem passa
// pelo mestre (ele envia os blocos e recebe pedidos/resultados), essa contagem
// já é completa. Por isso não precisamos de um MPI_Reduce extra só para isso.
// O `&` em `long long& mensagens` é uma referência: a contagem feita aqui
// aparece no `main` que chamou esta função.
uint64_t mestre(uint64_t espaco, uint64_t bloco, int processos,long long& mensagens) {

    const uint64_t blocos = contar_blocos(espaco, bloco);
    uint64_t proximo = 0;   // índice do próximo bloco ainda não entregue
    uint64_t melhor = NAO_ENCONTRADO;
    int ativos = 0;         // quantos slaves estao em trabalho

    // Despacho inicial: um bloco para cada escravo que ainda tenha trabalho.
    // O rank 0 é o mestre, então os escravos são 1, 2, ..., processos-1.
    for (int destino = 1; destino < processos; ++destino) {
        if (proximo < blocos) {
            const uint64_t inicio = proximo * bloco;
            const uint64_t fim = (espaco - inicio < bloco) ? espaco : inicio + bloco;
            enviar_bloco(destino, inicio, fim);
            ++mensagens;
            ++proximo;
            ++ativos;
        } else {
            enviar_sinal(destino);
            ++mensagens;
        }
    }

    bool encontrou = false;

    // Recebe pedidos e resultados de qualquer escravo, em qualquer ordem.
    // `MPI_ANY_SOURCE` = "aceito de quem chegar"; `MPI_ANY_TAG` = "com qualquer
    // rótulo". O `&status` é importante: a função preenche essa variável com
    // quem enviou (`status.MPI_SOURCE`) e qual rótulo veio (`status.MPI_TAG`).
    while (ativos > 0) {
        uint64_t dado = 0;
        MPI_Status status;
        MPI_Recv(&dado, 1, MPI_UINT64_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        ++mensagens;

        if (status.MPI_TAG == TAG_RESULTADO) {
            if (dado < melhor) melhor = dado;
            encontrou = true;
            break;  // primeiro acerto: não há motivo para continuar
        }

        // TAG_PEDIDO: entrega o próximo bloco ou encerra esse escravo.
        if (proximo < blocos) {
            const uint64_t inicio = proximo * bloco;
            const uint64_t fim = (espaco - inicio < bloco) ? espaco : inicio + bloco;
            enviar_bloco(status.MPI_SOURCE, inicio, fim);
            ++mensagens;
            ++proximo;
        } else {
            enviar_sinal(status.MPI_SOURCE);
            ++mensagens;
            --ativos;
        }
    }

    // Se alguém achou, os outros podem estar no meio de um bloco ou parados
    // no MPI_Recv esperando um bloco que não virá mais. Avisamos a TODOS,
    // inclusive quem já tiver terminado o último bloco.
    if (encontrou) {
        for (int destino = 1; destino < processos; ++destino) {
            enviar_sinal(destino);
            ++mensagens;
        }
    }
    return melhor;
}

// ------------------------------- ESCRAVO ------------------------------------
void escravo(uint64_t alvo) {
    // `for (;;)` é um laço sem condição que ele só finaliza no break.
    // O escravo vive nesse ciclo: recebe bloco -> varre -> pede outro.
    for (;;) {
        uint64_t dados[2] = {0, 0};
        MPI_Status status;
        // Recebe do rank 0 (o mestre), com qualquer rótulo.
        MPI_Recv(dados, 2, MPI_UINT64_T, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_PARAR) break;

        const uint64_t chave = varrer(dados[0], dados[1], alvo);
        uint64_t valor = 0;
        if (chave != NAO_ENCONTRADO) {
            valor = chave;
            MPI_Send(&valor, 1, MPI_UINT64_T, 0, TAG_RESULTADO, MPI_COMM_WORLD);
        } else {
            // Sem conteúdo: só avisa "estou livre, me dê outro bloco".
            MPI_Send(&valor, 0, MPI_UINT64_T, 0, TAG_PEDIDO, MPI_COMM_WORLD);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    // Abre o ambiente MPI. Depois desta linha existem P CÓPIAS do programa,
    // cada uma com seu próprio `rank` (um crachá de 0 a P-1).
    MPI_Init(&argc, &argv);

    int rank = 0;
    int processos = 1;
    // `MPI_COMM_WORLD` é o grupo com todos os processos.
    // O `&` faz a função preencher `rank` e `processos` (são parâmetros de saída).
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // "quem sou eu?"
    MPI_Comm_size(MPI_COMM_WORLD, &processos);  // "quantos somos?"

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

    // Bloco grande o suficiente para amortizar o par pedido/resposta, pequeno
    // o suficiente para não desperdiçar muita busca depois do término antecipado.
    // 128 blocos por escravo é uma escolha simples e estável.
    // Quem busca são P-1 processos (o rank 0 só coordena), por isso dividimos
    // por `trabalhadores`. `static_cast` só converte `int` para `uint64_t`.
    const uint64_t trabalhadores = (processos > 1) ? static_cast<uint64_t> (processos - 1) : 1;
    
    uint64_t bloco = cfg.espaco / (trabalhadores * 128);
    if (bloco < 16384) bloco = 16384;

    // Coletiva 1 — MPI_Bcast ("broadcast"): é como um alto-falante. O rank 0
    // fala, todos os outros ouvem e copiam. Sem isso, cada processo poderia
    // estar usando um espaço ou um alvo diferente.
    {
        uint64_t pacote[3] = {cfg.espaco, cfg.alvo, bloco};
        MPI_Bcast(pacote, 3, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        cfg.espaco = pacote[0];
        cfg.alvo = pacote[1];
        bloco = pacote[2];
    }

    long long mensagens = 0;
    uint64_t local = NAO_ENCONTRADO;

    const double inicio = agora();

    if (processos == 1) {
        // Sem escravos, o próprio rank 0 faz a busca: curva começa em 1,0x.
        local = varrer(0, cfg.espaco, cfg.alvo);
    } else if (rank == 0) {
        local = mestre(cfg.espaco, bloco, processos, mensagens);
    } else {
        escravo(cfg.alvo);
    }

    const double fim = agora();

    // Coletiva 2 — MPI_Allreduce: todo mundo entra com um valor (`local`) e
    // todo mundo sai com o resultado da operação (`global`). Aqui a operação
    // é MPI_MIN: fica a MENOR chave. Isso reproduz a "primeira ocorrência" do
    // baseline mesmo que as mensagens tenham chegado em outra ordem.
    uint64_t global = NAO_ENCONTRADO;
    MPI_Allreduce(&local, &global, 1, MPI_UINT64_T, MPI_MIN, MPI_COMM_WORLD);

    // Só o rank 0 imprime: se todos imprimissem, apareceriam P linhas iguais
    // e os scripts de coleta contariam mensagens repetidas.
    if (rank == 0) {
        Resultado r;
        r.versao = "mpi";
        r.espaco = cfg.espaco;
        r.alvo = cfg.alvo;
        r.chave = global;
        r.threads = 1;
        r.processos = processos;
        r.tempo_busca = fim - inicio;
        r.mensagens = mensagens;
        imprimir_resultado(r);
    }

    MPI_Finalize();
    return 0;
}
