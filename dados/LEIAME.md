# Dados de medição

Dois arquivos, gerados pelos scripts:

| Arquivo | Granularidade | Gerado por |
|---|---|---|
| `bruto.csv` | **uma linha por execução** (235 linhas) | `scripts/coletar.sh` (`make dados`) |
| `resultados.csv` | **uma linha por configuração** (63 linhas), com medianas | `scripts/gerar_graficos.py` (`make graficos`) |

---

## 1. `bruto.csv` — uma linha por execução

| Coluna | Tipo | Significado |
|---|---|---|
| `versao` | texto | `sequencial`, `openmp` ou `mpi` |
| `politica` | texto | `ciclica`, `estatica`, `dinamica`, `guiada` (OpenMP) ou `mestre-escravo`, `estatico` (MPI); `-` no sequencial |
| `modo` | texto | `antecipado` (para no 1º acerto, como o baseline) ou `completo` (varre todo o espaço) |
| `escala` | texto | `forte` (problema fixo, P varia) ou `fraca` (trabalho por unidade fixo, P e ESPACO variam) |
| `espaco` | inteiro | tamanho do espaço de busca (`ESPACO`) |
| `alvo` | inteiro | `hash(ESPACO/3)` — `-` no sequencial, que não imprime esse campo |
| `chave` | inteiro | chave encontrada (`-1` quando não encontrada) |
| `threads` | inteiro | threads efetivas |
| `processos` | inteiro | processos MPI efetivos |
| `tempo_busca` | segundos | **tempo da região de busca** (comparável ao `tempo` do baseline) |
| `tempo_total` | segundos | programa inteiro (inicialização + busca + impressão) |
| `tempo_coletivas` | segundos | tempo em `MPI_Bcast` + `MPI_Allreduce` + `MPI_Reduce` (0 nas versões sem MPI) |
| `mensagens` | inteiro | mensagens ponto a ponto trocadas (somadas entre os ranks) |
| `execucao` | inteiro | número da repetição (1..R) |
| `tempo_referencia` | segundos | **T1 medido imediatamente antes desta execução** (mesma versão com 1 thread/processo) |

> **Por que existe a coluna `tempo_referencia`?** Em notebooks a frequência da
> CPU muda com temperatura e com o plano de energia. Se o T1 for medido uma
> vez e as demais configurações minutos depois, o speedup fica distorcido.
> Medindo a referência ao lado de cada execução, o par
> (referência, configuração) é afetado da mesma forma e a razão continua
> válida. É por isso que o speedup em T=1 vale 0,98 e não exatamente 1,00.

## 2. `resultados.csv` — uma linha por configuração

Mesma identificação (`versao`, `politica`, `modo`, `escala`, `espaco`,
`threads`, `processos`) mais:

| Coluna | Significado |
|---|---|
| `chave` | chave encontrada (igual em todas as linhas — prova de corretude) |
| `repeticoes` | quantas execuções entraram na agregação |
| `tempo_mediana` | mediana de `tempo_busca` (**estatística usada nos gráficos**) |
| `tempo_minimo` / `tempo_medio` / `tempo_maximo` | dispersão da mesma série |
| `speedup` | mediana das razões `tempo_referencia / tempo_busca` |
| `referencia_mediana` | mediana dos T1 medidos ao lado |
| `mensagens_mediana` | mediana da contagem de mensagens |
| `tempo_coletivas_mediana` | mediana do tempo em coletivas |

## 3. Notas de leitura

* O `speedup` de uma linha é sempre `T1/Tp` **da própria implementação**: o
  `openmp` compara com o `openmp` de 1 thread, o `mpi` compara com o `mpi` de 1
  processo. Isso isola o ganho da paralelização.
* O tempo do baseline do professor está na tabela 7.1 do README
  (`versao = sequencial`).
* Nas linhas de `escala = fraca` o `tempo_referencia` é o próprio tempo
  (não há T1 comparável, pois o espaço muda com P): nesse gráfico o dado
  relevante é o **tempo absoluto**.
* Os números usam **ponto** como separador decimal (padrão CSV); o README
  usa **vírgula**, por convenção brasileira.
