# Pendências antes da apresentação

> Só falta informação do grupo. O código, os scripts, os dados e os gráficos
> já estão prontos e validados.

---

## 1. Identificação (preencher em `AUTORES.md`)

| # | Placeholder | Onde |
|---|---|---|
| 1 | `[NOME COMPLETO DO INTEGRANTE 1]` | tabela de integrantes |
| 2 | `[NOME COMPLETO DO INTEGRANTE 2]` | tabela de integrantes |
| 3 | `[NOME COMPLETO DO INTEGRANTE 3]` | tabela de integrantes |
| 4 | `[MATRÍCULA 1..3]` | tabela de integrantes |
| 5 | `[TURMA / CÓDIGO DA TURMA]` | cabeçalho |
| 6 | `[NOME DO PROFESSOR]`, `[E-MAIL DO PROFESSOR]` | cabeçalho |
| 7 | `[DATA DA APRESENTAÇÃO]` | cabeçalho |
| 8 | `[LOCAL E DATA]` | declaração final |

Conferir com:

```bash
grep -rn "\[NOME\|\[MATRÍCULA\|\[TURMA\|\[DATA\|\[LOCAL\|\[E-MAIL" .
```

---

## 2. Se a apresentação for em outro equipamento

As medições foram feitas em Ubuntu 24.04 dentro de Docker Desktop/WSL2, com
12 CPUs lógicas. Se o laboratório tiver outra máquina:

```bash
make dados && make graficos
```

Depois, atualizar as tabelas do README (seções 6 e 7) com os novos valores de
`dados/perfilamento.csv` e `dados/resultados.csv`. O texto da seção 8 continua
válido: as causas do afastamento de Amdahl são estruturais e de hardware.

---

## 3. Verificações finais

| # | Comando | Resultado esperado |
|---|---|---|
| 1 | `make limpar && make` | compila sem avisos |
| 2 | `make validar` | 70/70 combinações OK |
| 3 | `make dados && make graficos` | 115 execuções e 4 SVGs |
| 4 | `git diff -- docs/baseline_sequencial.cpp` | vazio |
| 5 | `git status --porcelain` | `bin/` não aparece |
| 6 | `grep -rn "TBD\|estatico\|completo" README.md` | nenhum resultado |

---

## 4. Git

Não foram feitos commits nesta etapa. O histórico ficou a cargo do grupo,
conforme solicitado (o enunciado avalia o histórico de commits).
