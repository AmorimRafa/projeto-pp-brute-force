# Plano de Execução — Marco 2 (Busca Exaustiva, Problema 9)

> Documento de planejamento do trio. Não substitui o README: o README é o
> briefing técnico de leitura; este arquivo é o **plano de trabalho** com as
> decisões, a ordem de execução e os critérios de aceite.
>
> Disciplina: Programação Paralela (CCO085) — 2026/2 — Prof. Rodrigo Gonçalves
> Entrega do Marco 2: 12/11/2026

---

## 1. Objetivo

Transformar o baseline sequencial fornecido pelo professor
(`docs/baseline_sequencial.cpp`) em duas versões paralelas que produzam
**exatamente a mesma chave** e executem em menos tempo:

1. **OpenMP** — memória compartilhada, com tratamento explícito de condições de
   corrida e curva de speedup por número de threads.
2. **MPI** — memória distribuída, com decomposição justificada, operação
   coletiva e curva de speedup por número de processos.

Além do código, o Marco 2 exige: validação de corretude contra o baseline,
CSV de medições, gráficos (tempo, speedup, eficiência, escalabilidade forte e
fraca) e discussão do afastamento em relação à Lei de Amdahl.

O baseline **não será alterado**: o Makefile compila diretamente o arquivo
original em `docs/`, sem uma linha modificada, garantindo que a lógica de
cálculo e a entrada de referência sejam preservadas.

---

## 2. Análise do problema (o que decide o projeto)

### 2.1 O baseline

```cpp
alvo = h(ESPACO / 3);
for (uint64_t k = 0; k < ESPACO; k++)
    if (h(k) == alvo) { achou = k; break; }
```

- Complexidade: **O(ESPACO)** no pior caso; com término antecipado, o trabalho
  efetivo é `ESPACO/3` iterações.
- O laço do hash concentra praticamente **100% do tempo** (não há I/O nem
  alocação no caminho quente). A fração paralelizável é ~1, portanto a Lei de
  Amdahl prevê speedup próximo de `P`; o afastamento observado vem de
  overheads reais (criação/entrada de threads, granularidade do término
  antecipado, processo mestre ocioso no MPI, comunicação e SMT/turbo).
- `h` é o finalizador do MurmurHash3, formado por operações **invertíveis**
  (`x ^= x >> 33` e multiplicação por constantes ímpares módulo 2^64). Logo `h`
  é **bijetora**: existe **uma única** pré-imagem de `alvo`, que é exatamente
  `ESPACO/3`. Isso permite validar qualquer versão apenas comparando a chave
  encontrada com a do baseline.

### 2.2 O ponto crítico: término antecipado × distribuição do trabalho

O trabalho não é fixo: o algoritmo para na primeira ocorrência. Duas
consequências:

1. **Distribuição importa mais que o número de threads.** Se cada thread
   receber um bloco contíguo grande, a thread que contém `ESPACO/3` começa
   perto do alvo e as demais ficam ociosas; o tempo (e o speedup) passa a
   depender da posição da chave, não da paralelização. A versão principal usa
   **distribuição cíclica em blocos pequenos**: todas as threads percorrem o
   espaço "lado a lado" e param juntas, com desbalanceamento de no máximo um
   bloco.
2. **O trabalho total é `ESPACO/3`, não `ESPACO`.** As curvas de speedup devem
   ser comparadas com o baseline que também para cedo. Para medir
   escalabilidade com trabalho constante, existe o modo `--completo`, que
   varre todo o espaço e usa `reduction(min:)`/`MPI_Allreduce(MIN)`.

---

## 3. Decisões de implementação

### 3.1 OpenMP

| Decisão | Justificativa |
|---|---|
| Distribuição cíclica em blocos (`schedule(static,1)` sobre blocos de 16 KiB) no modo com término antecipado | Balanceia a carga e evita que uma única thread seja dona da região do alvo; mantém a parada próxima entre threads |
| `std::atomic<bool>` para o sinal de parada, lido a cada 1024 chaves | `atomic` é a primitiva mais barata para uma flag booleana; a leitura espaçada evita tráfego de coerência de cache a cada iteração |
| `#pragma omp critical` uma única vez por thread para fundir o melhor resultado | A fusão ocorre O(T) vezes; `critical` por iteração seria um gargalo e `reduction` não pode ser combinado com parada antecipada |
| `reduction(min:)` no modo `--completo` | É a construção correta quando não há parada: sem seção crítica e com redução reconhecida pelo compilador |
| `omp_set_schedule` + `schedule(runtime)` | Permite comparar `static`, `dynamic` e `guided` sem duplicar o corpo do laço |
| Variáveis locais por thread | Evita *false sharing*; não há escrita compartilhada no caminho quente |

Políticas disponíveis:

- `--politica=ciclica` (padrão no modo antecipado): blocos round-robin.
- `--politica=dinamica`: blocos entregues sob demanda.
- `--politica=estatica`: um bloco contíguo por thread — **somente no modo
  `--completo`**, porque com término antecipado o resultado/tempo depende da
  posição da chave (ver README).
- `--politica=guiada`: blocos decrescentes — **somente no modo `--completo`**,
  pelo mesmo motivo (os primeiros blocos são grandes).

### 3.2 MPI — dois modos (decisão registrada em `docs/DECISAO_MPI_MODOS.md`)

| Modo | Decomposição | Coletivas | Uso |
|---|---|---|---|
| `--modo=mestre-escravo` (padrão) | Tarefas: mestre distribui blocos sob demanda | `MPI_Bcast` (configuração) + `MPI_Allreduce(MIN)` (resultado) | Demonstra o padrão destacado pelo problema; teto de eficiência `(P−1)/P` porque o mestre não busca |
| `--modo=estatico` | Domínio: cada rank varre blocos cíclicos do espaço | `MPI_Bcast` + `MPI_Allreduce(MIN)` + mensagem de parada | Melhor escalabilidade; referência para escalabilidade fraca e para discutir o custo de manter um processo coordenador |

Ambos os modos:

- Entregam a mesma chave do baseline para qualquer `-np`.
- Medem `tempo_busca`, `tempo_total` e o número de mensagens do mestre.
- Têm modo `--completo` para medir trabalho constante.

### 3.3 Protocolo do mestre-escravo

```
mestre                                   escravo
  |-- Bcast(config) ----------------------->|
  |-- BLOCO(inicio,fim) ------------------->|
  |                                         | varre o bloco
  |<-------------------------- PEDIDO ------|
  |-- BLOCO(inicio,fim) ------------------->|
  |                                         | encontra a chave
  |<------------------------ RESULTADO -----|
  |-- PARAR (para todos) ------------------>|  (escravos param)
  |-- Allreduce(MIN) -----------------------|
  |<-------------------------- chave -------|
```

- Blocos são distribuídos **em ordem crescente**; a granularidade é
  `max(16 KiB, ESPACO/(128·(P−1)))`.
- O mestre só encerra após receber um `RESULTADO` ou depois de todos os
  escravos receberem `FIM`.
- `-np 1` executa a busca no próprio rank 0, para a curva começar em 1,0×.

### 3.4 Medição

- **Aquecimento**: 1 execução global (6 rodadas do baseline) + 1 execução por
  configuração, ambas descartadas.
- **Repetições**: 5 (medianas) para as curvas principais; 3 para as auxiliares.
- **Estatística**: mediana, mínimo, média e máximo no CSV agregado.
- **Tempo comparável**: `tempo_busca` mede apenas a região de busca nas três
  versões (o baseline já imprime exatamente o tempo do laço).
- **Referência pareada** (decisão tomada durante a execução): cada execução é
  precedida da medição do **T1 da própria implementação** (1 thread/1 processo,
  mesmo espaço). Sem isso, a variação de frequência da CPU do notebook
  (o mesmo código mediu 1,28 s e 2,29 s em janelas distintas) contamina o
  speedup. O speedup passa a ser a razão `ref/tempo` dentro da mesma janela.
- **Tamanhos**: 3 tamanhos para a tabela de baseline; um tamanho fixo para
  escalabilidade forte; `ESPACO = base × P` para a fraca.
- **Máquina**: Docker Desktop sobre WSL2, 12 CPUs lógicas (ver README para o
  comando de regeneração no laboratório).

---

## 4. Estrutura de arquivos

```
docs/baseline_sequencial.cpp      baseline original do professor (intocado)
docs/PLANO_MARCO2.md              este documento
docs/DECISAO_MPI_MODOS.md         por que existem dois modos de MPI
docs/PENDENCIAS.md                lista de placeholders a preencher
src/comum/busca.hpp               hash, cronômetro, parsing e impressão
src/openmp/openmp.cpp             versão OpenMP
src/mpi/mpi.cpp                   versão MPI
scripts/coletar.sh                gera dados/bruto.csv
scripts/validar.sh                compara resultados com o baseline
scripts/gerar_graficos.py         agrega CSV e gera SVGs (sem dependências)
scripts/configurar_ambiente.sh    instala o toolchain no WSL/laboratório
docker/Dockerfile                 ambiente Linux reprodutível
Makefile                          compila, valida, coleta e plota
README.md                         briefing completo do projeto
AUTORES.md                        frentes dos integrantes (placeholders)
dados/, graficos/                 artefatos gerados, versionados
```

---

## 5. Ordem de execução e critérios de aceite

| # | Etapa | Critério de aceite | Situação |
|---|---|---|---|
| 1 | Documentos de plano e decisão | `docs/PLANO_MARCO2.md` e `docs/DECISAO_MPI_MODOS.md` presentes | **concluído** |
| 2 | `src/comum/busca.hpp` | hash idêntico ao baseline; parsing compatível (`ESPACO` como 1º argumento) | **concluído** |
| 3 | OpenMP | compila com `-fopenmp`; mesma chave para 1..N threads; mede tempo | **concluído** |
| 4 | MPI | compila com `mpicxx`; mesma chave para `-np 1..N`; Bcast + Allreduce; mede tempo/mensagens | **concluído** |
| 5 | `scripts/validar.sh` | todas as combinações PASS, inclusive `ESPACO` 0, 1, 2 e 3 | **concluído — 160/160 OK** |
| 6 | `scripts/coletar.sh` | `dados/bruto.csv` com colunas documentadas e execuções repetidas | **concluído — 235 execuções** |
| 7 | `scripts/gerar_graficos.py` | `dados/resultados.csv` + SVGs de tempo, speedup, eficiência, forte e fraca | **concluído — 8 gráficos** |
| 8 | README | roda do zero em Linux/Docker; contém caso de análise, metodologia, resultados e leitura crítica | **concluído** (falta só preencher os placeholders do grupo) |
| 9 | `AUTORES.md` | três frentes declaradas, com placeholders claramente marcados | **concluído — com placeholders** |
| 10 | Git | **sem commit e sem push** nesta etapa (conforme solicitado) | **pendente de decisão do grupo** |

---

## 6. Riscos e mitigação

| Risco | Mitigação |
|---|---|
| Speedup do mestre-escravo limitado a `(P−1)/P` | Modo `estatico` como comparação; discussão explícita no README |
| Curva achatando em 12 threads | Medir 1..12 e discutir SMT/turbo; usar mediana e aquecimento |
| Tempos curtos demais (ruído) | Calibrar `ESPACO` para o sequencial ficar entre ~0,5 s e ~1 s |
| Blocos de MPI muito grandes (término grosseiro) | Fórmula de bloco com 128 blocos por escravo + opção `--bloco` |
| Docker sem OpenMPI ou com root | Dockerfile instala `openmpi-bin`/`libopenmpi-dev`; usar `MPIRUN="mpirun --allow-run-as-root"` quando necessário |
| Ambiente do laboratório diferente | Curvas regeneráveis com um comando; versões compiladas com `-O2` e sem `-march=native` |

---

## 7. Fora de escopo

- Alterar a lógica, a entrada ou o resultado do baseline.
- Implementar os outros nove problemas da lista.
- Fazer commit ou push (solicitação explícita do grupo nesta etapa).
