# Plano de execução — Marco 2 (reformulado)

> Disciplina: Programação Paralela (CCO085) — 2026/2
> Problema 9 — Busca exaustiva (força bruta)
> Este documento registra o **escopo final simplificado** do projeto.

---

## 1. Objetivo

Transformar o baseline sequencial fornecido pelo professor em duas versões
paralelas que produzam **exatamente a mesma chave** e executem em menos tempo:

1. **OpenMP** — memória compartilhada, condição de corrida tratada com
   `atomic` + `critical`, curva de speedup por threads.
2. **MPI** — memória distribuída, padrão **mestre-escravo**, pelo menos uma
   operação coletiva, curva de speedup por processos.

Além do código: validação de corretude, CSVs de medição, 4 gráficos e discussão
do afastamento em relação à Lei de Amdahl.

---

## 2. O que foi removido e por quê

| Item removido | Motivo |
|---|---|
| Modo MPI `estatico` | O enunciado do problema 9 destaca mestre-escravo; manter uma segunda decomposição só alongaria a apresentação |
| `--completo` (varredura integral) | Muda a semântica do baseline (que para no primeiro acerto) e não é pedido |
| Opções `--politica` (static/dynamic/guided) | O custo por chave é constante; o enunciado só exige comparar políticas quando há carga desigual |
| `MPI_Reduce` só para contar mensagens | O mestre já centraliza todas as mensagens, então a contagem é feita nele |
| `scripts/configurar_ambiente.sh` | O `docker/Dockerfile` já instala todo o toolchain |
| 8 gráficos e referência pareada | Rigor excessivo para uma demonstração de ~9 minutos |

A justificativa da carga uniforme está no comentário da função `hash_chave`
em `src/comum/busca.hpp` e no FAQ do README.

---

## 3. Decisões de implementação

### OpenMP

* `schedule(dynamic, 1)` sobre blocos de 16.384 chaves: equilibra a parada
  antecipada sem depender da posição da chave.
* `std::atomic<bool> parar`: evita a corrida na flag de término.
* `#pragma omp critical` uma vez por thread: fusão do menor índice.
* `reduction(min:)` descartada porque o laço termina cedo.
* Sem vetores compartilhados escritos: não há false sharing.

### MPI

* Rank 0 = mestre (distribui blocos); ranks 1..P−1 = escravos (buscam).
* Tags: `BLOCO`, `PEDIDO`, `RESULTADO`, `PARAR`.
* Coletivas: `MPI_Bcast` (configuração) e `MPI_Allreduce(MPI_MIN)` (resultado).
* Bloco ≈ `ESPACO / (128 × (P−1))`, com mínimo de 16.384 chaves.
* Teto estrutural: speedup `P−1`, eficiência `(P−1)/P`.

---

## 4. Medição

* 3 tamanhos de baseline; 1 aquecimento por configuração; 5 repetições;
  mediana.
* Escalabilidade forte: `ESPACO = 4,8×10^9`.
* Escalabilidade fraca: `ESPACO = 4,8×10^8 × threads` (OpenMP) e
  `× (processos−1)` (MPI, pois há um processo ocioso).
* Perfilamento: menor de 3 medições do tempo total externo comparado com o
  tempo do laço impresso pelo próprio baseline (que permanece intocado).

---

## 5. Critérios de aceite

| Etapa | Critério | Situação |
|---|---|---|
| Baseline intacto | `git diff -- docs/baseline_sequencial.cpp` vazio | OK |
| Compilação | `make` compila as três versões sem avisos | OK |
| Corretude | `make validar` → 70/70 combinações OK | OK |
| Coleta | `dados/bruto.csv` com 115 execuções + `perfilamento.csv` | OK |
| Gráficos | 4 SVGs em `graficos/` | OK |
| Git | sem commit e sem push nesta etapa | conforme solicitação do grupo |

---

## 6. Comandos de reprodução

```bash
docker build -t pp-busca -f docker/Dockerfile .
docker run --rm -v "%cd%":/projeto -w /projeto pp-busca bash
make && make validar && make dados && make graficos
```
