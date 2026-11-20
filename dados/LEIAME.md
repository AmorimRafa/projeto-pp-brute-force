# Dados de medição

Três arquivos, todos gerados pelos scripts (não editar à mão):

| Arquivo | Gerado por | Conteúdo |
|---|---|---|
| `perfilamento.csv` | `scripts/coletar.sh` | hotspot do baseline (3 tamanhos) |
| `bruto.csv` | `scripts/coletar.sh` | uma linha por execução (115 linhas) |
| `resultados.csv` | `scripts/gerar_graficos.py` | uma linha por configuração (mediana) |

---

## `perfilamento.csv`

| Coluna | Significado |
|---|---|
| `espaco` | tamanho do espaço de busca |
| `tempo_total` | menor de 3 medições do programa inteiro (medido fora, sem alterar o baseline) |
| `tempo_laco` | menor de 3 valores do tempo do laço impresso pelo baseline |
| `percentual_laco` | `100 × tempo_laco / tempo_total` |

## `bruto.csv`

| Coluna | Significado |
|---|---|
| `versao` | `sequencial`, `openmp` ou `mpi` |
| `escala` | `forte` (espaço fixo) ou `fraca` (trabalho por unidade fixo) |
| `espaco` | tamanho do espaço naquela execução |
| `chave` | chave encontrada (igual em todas as linhas) |
| `threads` / `processos` | unidades de paralelismo (uma é 1) |
| `tempo_busca` | tempo da região de busca; no baseline é o campo `tempo` |
| `mensagens` | mensagens ponto a ponto (sempre 0 fora do MPI) |
| `execucao` | número da repetição (1..5) |

## `resultados.csv`

Mesma identificação das execuções, mais `repeticoes`, `tempo_mediano`,
`tempo_minimo`, `tempo_maximo`, `speedup` (`T1/Tp` da mesma versão),
`eficiencia` (`speedup / unidades`) e `mensagens`.

> O baseline aparece em três linhas (um por tamanho de entrada). Nas curvas
> fortes, o tempo de referência usado nos gráficos é o da linha com
> `espaco = 4800000000`, o mesmo das versões paralelas.

Números usam **ponto** decimal (padrão CSV); o README usa **vírgula**.
