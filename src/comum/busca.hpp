// ============================================================================
//  busca.hpp — código comum às versões OpenMP e MPI
//  Problema 9 — Busca exaustiva (força bruta)
//  Programação Paralela (CCO085) — 2026/2
//
//  Aqui está apenas o que precisa ser IDÊNTICO nas três versões:
//    1. a função de hash (cópia fiel da função h() do baseline do professor);
//    2. o cálculo do alvo: alvo = hash(ESPACO / 3);
//    3. a leitura dos argumentos e a impressão do resultado.
//
//  O arquivo docs/baseline_sequencial.cpp NÃO é alterado: ele é compilado à
//  parte e serve de gabarito. Esta versão só repete a mesma lógica de cálculo.
//
//  COMO LER ESTE ARQUIVO (para quem está começando):
//    1. veja `hash_chave` — a "máquina de misturar números" copiada do baseline;
//    2. veja `ler_argumentos` — transforma a linha de comando na struct cfg;
//    3. veja `imprimir_resultado` — a linha única que todos os scripts leem.
//  O guia completo, com exemplos, está em docs/GUIA_INICIANTE.md.
// ============================================================================

// Estas duas linhas formam o "include guard": na 1ª vez, BUSCA_HPP não existe,
// então o arquivo é lido e a macro é definida. Se outro arquivo incluir este
// mesmo cabeçalho de novo, a definição já existe e o conteúdo é pulado.
// Sem isso, as funções seriam declaradas duas vezes e o compilador reclamaria.
#ifndef BUSCA_HPP
#define BUSCA_HPP

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace busca {

// Valor "sentinela": o maior uint64_t que existe.
// `~` inverte todos os bits: ~0 vira 1111...1111, ou seja, 2^64 - 1.
// Como queremos a MENOR chave, esse valor nunca pode ser uma resposta real.
// Ele funciona como "ainda não achei" sem precisar de um caso especial, e na
// impressão vira -1 (igual ao baseline quando ESPACO = 0, que não tem solução).
// `constexpr` = "esse valor pode ser resolvido enquanto o programa compila".
constexpr uint64_t NAO_ENCONTRADO = ~uint64_t{0};

// Relógio monotônico: só anda para frente. Não sofre ajuste de horário nem
// de fuso durante a medição — mesma ideia do CLOCK_MONOTONIC do baseline.
// `steady_clock::now()` devolve o instante atual; `time_since_epoch()` diz
// quanto tempo passou desde o "zero" do relógio; `duration<double>` converte
// isso para segundos com casa decimal (por isso o retorno é `double`).
// `inline` permite que esta função exista no cabeçalho sem erro de link,
// mesmo sendo incluída por openmp.cpp e mpi.cpp ao mesmo tempo.
inline double agora() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ----------------------------------------------------------------------------
//  Hash — mesma função do baseline
// ----------------------------------------------------------------------------
//  Cada chamada executa SEMPRE as mesmas operações (3 XORs e 2 multiplicações),
//  qualquer que seja o valor de k. Não há leitura de vetor, memcpy nem ramo
//  dependente dos dados. Portanto:
//
//    * o custo por chave é CONSTANTE;
//    * a carga de trabalho é naturalmente equilibrada entre threads/processos;
//    * NÃO há carga desigual que justifique comparar static/dynamic/guided.
//      O enunciado só pede esse estudo "quando o problema tiver carga desigual".
//
//  As operações são invertíveis, logo a função é bijetora: para cada alvo
//  existe uma única chave. Isso permite validar comparando apenas a chave.
//
//  Para quem não conhece os operadores, as 3 linhas abaixo são assim:
//    k ^= k >> 33   -> desloca 33 bits para a direita e mistura com XOR (`^`).
//                      É como "dobrar o papel e esfregar as duas metades".
//    k *= constante -> multiplica por um número esquisito. O estouro do
//                      uint64_t é INTENCIONAL: ele ajuda a embaralhar os bits.
//  Repete-se o par duas vezes e faz-se um último XOR: é o "finalizador" do
//  MurmurHash3. As constantes foram copiadas do baseline — não são inventadas.
inline uint64_t hash_chave(uint64_t k) {
    k ^= k >> 33; 
    k *= 0xff51afd7ed558ccdULL;

    k ^= k >> 33; 
    k *= 0xc4ceb9fe1a85ec53ULL;
    
    k ^= k >> 33; return k;
}

// Configuração mínima: só o que as versões paralelas realmente usam.
// `struct` é um "pacote" de variáveis com nome. Em vez de passar 4 parâmetros
// separados para cada função, passamos um único `Configuracao`.
struct Configuracao {
    uint64_t espaco = 60000000ULL;  // 1º argumento, como no baseline
    uint64_t alvo = 0;              // calculado em ler_argumentos()
    int threads = 0;                // 0 = usar o padrão do ambiente (OpenMP)
    bool ajuda = false;
};

inline void imprimir_ajuda(const char* programa) {
    std::printf(
        "Uso: %s [ESPACO] [opcoes]\n"
        "\n"
        "  ESPACO              tamanho do espaco de busca (padrao: 60000000)\n"
        "                      alvo = hash(ESPACO / 3), como no baseline\n"
        "  --threads=N         numero de threads (so na versao OpenMP)\n"
        "  --ajuda             mostra esta mensagem\n"
        "\n"
        "Saida (uma linha):\n"
        "  versao espaco alvo chave threads processos tempo_busca mensagens\n",
        programa);
}

// Aceita "ESPACO" posicional e "--threads=N" (ou "--threads N").
//
// O sistema operacional entrega `argc` (quantos pedaços têm a linha de comando)
// e `argv` (os pedaços, como um vetor de textos). Exemplo:
//   ./bin/openmp 60000000 --threads=4
//   argc = 3 ; argv[0] = "./bin/openmp", argv[1] = "60000000", argv[2] = "--threads=4"
// Não é necessário decorar: basta saber que este laço `for (i = 1; ...)` percorre
// os argumentos e preenche a struct `cfg`.
//
// O `&` em `Configuracao& cfg` é uma REFERÊNCIA: a função altera a variável
// original que está no `main`, e não uma cópia.
inline bool ler_argumentos(int argc, char** argv, Configuracao& cfg) {
    bool espaco_definido = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--ajuda") == 0 || std::strcmp(arg, "-h") == 0) {
            cfg.ajuda = true;
            return true;
        }

        if (arg[0] != '-') {
            if (espaco_definido) {
                std::fprintf(stderr, "ERRO: ESPACO informado mais de uma vez.\n");
                return false;
            }
            char* fim = nullptr;
            errno = 0;
            unsigned long long valor = std::strtoull(arg, &fim, 10);
            if (errno != 0 || fim == arg || *fim != '\0') {
                std::fprintf(stderr, "ERRO: ESPACO invalido: '%s'.\n", arg);
                return false;
            }
            cfg.espaco = static_cast<uint64_t>(valor);
            espaco_definido = true;
            continue;
        }

        if (std::strncmp(arg, "--threads", 9) == 0) {
            // Aceita os dois formatos: "--threads=4" e "--threads 4".
            // A expressão `condicao ? sim : nao` é um `if` em uma linha só.
            // `nullptr` quer dizer "não aponta para nada" (não veio valor).
            const char* igual = std::strchr(arg, '=');
            const char* valor = igual ? igual + 1
                                      : (i + 1 < argc ? argv[++i] : nullptr);
            if (valor == nullptr) {
                std::fprintf(stderr, "ERRO: --threads exige um valor.\n");
                return false;
            }
            char* fim = nullptr;
            long n = std::strtol(valor, &fim, 10);
            if (fim == valor || *fim != '\0' || n < 0 || n > 1024) {
                std::fprintf(stderr, "ERRO: --threads invalido: '%s'.\n", valor);
                return false;
            }
            cfg.threads = static_cast<int>(n);
            continue;
        }

        std::fprintf(stderr, "ERRO: opcao desconhecida: '%s' (use --ajuda).\n", arg);
        return false;
    }

    // Mesma regra do baseline: sempre existe solução dentro do espaço.
    cfg.alvo = hash_chave(cfg.espaco / 3);
    return true;
}

// Tudo o que queremos contar sobre uma execução, num único pacote.
struct Resultado {
    const char* versao;
    uint64_t espaco;
    uint64_t alvo;
    uint64_t chave;
    int threads;
    int processos;
    double tempo_busca;   // apenas a região de busca (comparável ao baseline)
    long long mensagens;  // MPI: mensagens ponto a ponto; OpenMP: 0
};

// Uma única linha de saída, com campos nomeados (`chave=...`, `tempo=...`).
// Os scripts leem pelo NOME do campo, então a ordem não importa.
// `%` dentro da string do printf marca onde uma variável entra no texto.
// `PRIu64` é o código certo para imprimir um `uint64_t` (ele muda de nome
// entre Windows e Linux; por isso não usamos um `%llu` escrito na mão).
inline void imprimir_resultado(const Resultado& r) {
    // O sentinela interno (2^64-1) aparece para o usuário como -1.
    // `static_cast<long long>(...)` só avisa ao compilador que queremos
    // explicitamente converter o tipo para imprimir.
    long long chave_impressa = (r.chave == NAO_ENCONTRADO) ? -1LL : static_cast<long long>(r.chave);

    std::printf(
        "versao=%s espaco=%" PRIu64 " alvo=%" PRIu64 " chave=%lld"
        " threads=%d processos=%d tempo_busca=%.6f mensagens=%lld\n",
        r.versao, r.espaco, r.alvo, chave_impressa, r.threads, r.processos,
        r.tempo_busca, r.mensagens);
}

}  // namespace busca

#endif  // BUSCA_HPP
