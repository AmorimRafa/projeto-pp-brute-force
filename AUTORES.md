# Autores — Busca Exaustiva Paralela (Problema 9)

> **Disciplina:** Programação Paralela (CCO085) — 2026/2
> **Docente:** Rodrigo Gonçalves Pinto 
> **Turma:** CCODM3B-CCO085-20262
> **Apresentação:** 26/12/2026

---

## 1. Integrantes

| # | Nome | Matrícula | Frente (responsabilidade principal) |
|---|---|---|---|
| 1 | Caio Freitas Leandro, Rafael Costa Amorim | 2422130050, 2422130043 | Baseline, perfilamento, metodologia e validação |
| 2 | Caio Freitas Leandro | 2422130050 | Versão OpenMP e condição de corrida |
| 3 | Rafael Costa Amorim | 2422130043 | Versão MPI (mestre-escravo) e coletivas |

---

## 2. O que cada frente entregou

### Frente 1 — Baseline, perfilamento e método

* Baseline original do professor, compilado sem alteração.
* `dados/perfilamento.csv`: tempo total externo × tempo do laço, mostrando que
  o hotspot é o hash.
* Estimativa de Amdahl: `p ≈ 1`, logo `S(P) ≈ P` como teto teórico.
* `scripts/validar.sh`: 70 combinações comparadas com o baseline.
* Protocolo: 1 aquecimento, 5 repetições, mediana.

### Frente 2 — OpenMP

* `src/openmp/openmp.cpp`: blocos dinâmicos de 16.384 chaves,
  `std::atomic<bool>` para a flag de parada e `critical` uma vez por thread.
* Sem comparação de políticas: o custo por chave é constante (explicado no
  cabeçalho de `src/comum/busca.hpp`).
* Curva de speedup em `graficos/speedup.svg`.

### Frente 3 — MPI

* `src/mpi/mpi.cpp`: mestre-escravo com tags `BLOCO`, `PEDIDO`, `RESULTADO` e
  `PARAR`.
* Coletivas: `MPI_Bcast` e `MPI_Allreduce(MPI_MIN)`.
* `docs/DECISAO_MPI.md`: justificativa da decomposição e do teto `(P−1)/P`.
* Contagem de mensagens no próprio mestre (sem `MPI_Reduce` extra).

---

## 3. Declaração

Os integrantes declaram que:

1. `docs/baseline_sequencial.cpp` é o código original do professor e **não foi
   modificado**;
2. o núcleo de cálculo (hash, alvo e laço de busca) é idêntico nas três versões;
3. todas as medições foram obtidas com os scripts versionados e podem ser
   reproduzidas com `make validar && make dados && make graficos`;
4. o material escrito cita o enunciado da disciplina e a documentação de OpenMP/MPI.
