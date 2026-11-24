# Dados de medição

Três arquivos, todos gerados pelos scripts (**não editar à mão**). Os valores
são medidos: se precisarem mudar, rode os scripts de novo.

| Arquivo | Gerado por | Conteúdo |
|---|---|---|
| `perfilamento.csv` | `scripts/coletar.sh` | hotspot do baseline (3 tamanhos) |
| `bruto.csv` | `scripts/coletar.sh` | uma linha por execução (115 linhas) |
| `resultados.csv` | `scripts/gerar_graficos.py` | uma linha por configuração (mediana) |

Para regenerar tudo:

```bash
make dados && make graficos
```

## Ambiente de medição

Ubuntu 24.04 dentro de Docker Desktop/WSL2, com 12 CPUs lógicas, compilação com
`-O2`. Para registrar o hardware de uma nova coleta (por exemplo, na máquina do
laboratório), guarde a saída de `lscpu` junto dos dados. Isso ajuda a explicar
a saturação das curvas quando o número de unidades passa dos núcleos físicos
(SMT).

## `perfilamento.csv`

| Coluna | Significado |
|---|---|
| `espaco` | tamanho do espaço de busca |
| `tempo_total` | menor de 3 medições do programa inteiro (medido fora, sem alterar o baseline) |
| `tempo_laco` | menor de 3 valores do tempo do laço impresso pelo baseline |
| `percentual_laco` | `100 × tempo_laco / tempo_total` |

**Por que aparece 100,50 % no maior tamanho?** `tempo_total` e `tempo_laco` vêm
de execuções diferentes, e cada um é o mínimo de 3 medições. Quando o custo
fixo (início e término do processo) é de poucos milissegundos, a diferença entre
as duas escolhas de mínimo pode fazer o laço parecer maior que o total. É ruído
de medição, e a leitura correta é: o laço do hash representa praticamente 100 %
do tempo (p ≈ 1). No menor tamanho (60.000.000) o custo fixo pesa mais, por isso
o percentual é 82 %.

## `bruto.csv`

| Coluna | Significado |
|---|---|
| `versao` | `sequencial`, `openmp` ou `mpi` |
| `escala` | `forte` (espaço fixo) ou `fraca` (trabalho por unidade fixo) |
| `espaco` | tamanho do espaço naquela execução |
| `chave` | chave encontrada (sempre `espaco / 3`, igual ao baseline) |
| `threads` / `processos` | unidades de paralelismo (a que não se aplica vale 1) |
| `tempo_busca` | tempo da região de busca, em segundos; no baseline é o campo `tempo` |
| `mensagens` | mensagens ponto a ponto contadas pelo mestre (0 fora do MPI) |
| `execucao` | número da repetição (1..5) |

## `resultados.csv`

Mesma identificação das execuções, mais `repeticoes`, `tempo_mediano`,
`tempo_minimo`, `tempo_maximo`, `speedup` (`T1/Tp` da mesma versão),
`eficiencia` (`speedup / unidades`, em %) e `mensagens`. Uma linha por
configuração, calculada como mediana das 5 repetições de `bruto.csv`.

> O baseline aparece em três linhas (uma por tamanho de entrada). Nas curvas
> fortes, o tempo de referência usado nos gráficos é o da linha com
> `espaco = 4800000000`, o mesmo das versões paralelas.

Números usam **ponto** decimal (padrão CSV); o README usa **vírgula**.
