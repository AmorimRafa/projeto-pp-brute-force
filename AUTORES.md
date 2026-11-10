# Autores — Busca Exaustiva Paralela (Problema 9)

> **Disciplina:** Programação Paralela (CCO085) — 2026/2
> **Docente:** [NOME DO PROFESSOR] — [E-MAIL DO PROFESSOR]
> **Turma:** [TURMA / CÓDIGO DA TURMA]
> **Período:** 2026/2
> **Data da entrega do Marco 2:** 12/11/2026

---

## 1. Integrantes

| # | Nome | matrícula | Frente (responsabilidade principal) | Arquivos sob responsabilidade |
|---|---|---|---|---|
| 1 | [NOME COMPLETO DO INTEGRANTE 1] | [MATRÍCULA 1] | Baseline, análise do problema, validação de corretude e método de medição | `docs/baseline_sequencial.cpp` (referência, intocado), `scripts/validar.sh`, `src/comum/busca.hpp` |
| 2 | [NOME COMPLETO DO INTEGRANTE 2] | [MATRÍCULA 2] | Versão OpenMP, condição de corrida, políticas de escalonamento e escalabilidade forte | `src/openmp/openmp.cpp`, `graficos/speedup_openmp.svg`, `graficos/eficiencia_openmp.svg`, `graficos/speedup_politicas.svg` |
| 3 | [NOME COMPLETO DO INTEGRANTE 3] | [MATRÍCULA 3] | Versão MPI (mestre-escravo e estático), protocolo de mensagens, escalabilidade fraca | `src/mpi/mpi.cpp`, `docs/DECISAO_MPI_MODOS.md`, `graficos/speedup_mpi.svg`, `graficos/escalabilidade_fraca.svg` |

*(Se o grupo tiver mais de três integrantes, duplicar a linha e redistribuir as
frentes; a divisão acima é a que consta em `docs/PLANO_MARCO2.md`.)*

---

## 2. O que cada frente entregou

### Frente 1 — Baseline, corretude e método

* Análise do problema: hash bijetor, término antecipado e por que a
  distribuição do trabalho importa mais que a contagem de threads.
* `src/comum/busca.hpp`: hash idêntico ao do professor, alvo `h(ESPACO/3)`,
  cronômetro monotônico e saída padronizada.
* `scripts/validar.sh`: 160 combinações (8 tamanhos × políticas × threads ×
  processos) comparadas com o baseline original.
* Método de medição: aquecimento, descarte de warm-up, 5 repetições, mediana
  e **referência pareada** (T1 medido ao lado de cada configuração) para
  neutralizar a variação de frequência da CPU.

### Frente 2 — OpenMP

* `src/openmp/openmp.cpp`: distribuição cíclica em blocos pequenos,
  `std::atomic<bool>` para a flag de parada (lida a cada 1024 chaves),
  `#pragma omp critical` uma vez por thread para a fusão do resultado e
  `reduction(min:)` no modo `--completo`.
* Comparação das políticas `estatica`, `dinamica` e `guiada` em modo de
  varredura completa (trabalho constante), onde o efeito da política aparece
  sem a confusão do término antecipado.

### Frente 3 — MPI

* `src/mpi/mpi.cpp`: modo **mestre-escravo** (blocos sob demanda, tags
  `BLOCO`/`PEDIDO`/`RESULTADO`/`PARAR`/`FIM`) e modo **estático** (domínio em
  blocos cíclicos, término por `MPI_Iprobe`).
* Coletivas: `MPI_Bcast` (configuração), `MPI_Allreduce(MIN)` (resultado) e
  `MPI_Reduce(SUM)` (contagem de mensagens).
* `docs/DECISAO_MPI_MODOS.md`: por que as duas decomposições existem e o que
  cada uma responde.
* Escalabilidade fraca (trabalho por unidade constante) para as três séries.

---

## 3. Onde estão os placeholders

Todos os textos que ainda precisam ser completados pelo grupo estão marcados
com colchetes (`[ ... ]`). A lista completa, arquivo por arquivo, está em
[`docs/PENDENCIAS.md`](docs/PENDENCIAS.md).

| Marcador | Onde aparece | O que colocar |
|---|---|---|
| `[NOME COMPLETO DO INTEGRANTE 1..3]` | `AUTORES.md` (tabela), `README.md` (seção 12), `docs/PLANO_MARCO2.md` | nome de cada integrante |
| `[MATRÍCULA 1..3]` | `AUTORES.md` | número de matrícula |
| `[TURMA / CÓDIGO DA TURMA]` | `AUTORES.md`, `README.md` | turma da disciplina |
| `[NOME DO PROFESSOR]`, `[E-MAIL DO PROFESSOR]` | `AUTORES.md` | identificação do docente |
| `[DATA DA APRESENTAÇÃO]` | `AUTORES.md`, `README.md` | data da defesa |
| `[MÁQUINA DO LABORATÓRIO]`, `[NÚCLEOS DO LABORATÓRIO]` | `README.md` (seção 5.1 e 8.4) | equipamento onde a apresentação será feita |
| `[LINK DO SLIDE/DECK]` | `AUTORES.md` | endereço da apresentação, se houver |
| `[TURMA/GRUPO N. ...]` | `docs/PENDENCIAS.md` | identificação do grupo no moodle |

Para localizar todos de uma vez:

```bash
grep -rn "\[NOME\|\[MATRÍCULA\|\[TURMA\|\[DATA\|\[MÁQUINA\|\[LINK\|\[E-MAIL" .
```

---

## 4. Declaração

Os três integrantes declaram que:

1. O arquivo `docs/baseline_sequencial.cpp` é o código original do professor e
   **não foi modificado** — a versão sequencial é compilada diretamente a
   partir dele.
2. O núcleo de cálculo (função de hash, definição do alvo e laço de busca) é
   idêntico nas três versões; a paralelização altera apenas a distribuição do
   trabalho e a forma de parar.
3. Todas as medições apresentadas no README foram obtidas com os scripts
   versionados neste repositório, em máquina identificada, e podem ser
   reproduzidas com `make validar && make dados && make graficos`.
4. O material escrito entregue à disciplina cita as fontes e referências
   utilizadas (documentação do OpenMP, do MPI e do enunciado da disciplina).

*[LOCAL E DATA]* — [NOME COMPLETO DO INTEGRANTE 1], [NOME COMPLETO DO
INTEGRANTE 2] e [NOME COMPLETO DO INTEGRANTE 3].
