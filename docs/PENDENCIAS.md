# Pendências antes da entrega do Marco 2

> Lista de tudo que ainda precisa ser preenchido pelo grupo, com o local
> exato de cada item. Use o comando de busca ao final para conferir que
> nenhum marcador ficou para trás.

Data-limite: **12/11/2026**.

---

## 1. Identificação do grupo

| # | Placeholder | Arquivo | Seção / linha aproximada | Valor esperado |
|---|---|---|---|---|
| 1 | `[NOME COMPLETO DO INTEGRANTE 1]` | `AUTORES.md` | tabela da seção 1 | nome do responsável por baseline/corretude/método |
| 2 | `[NOME COMPLETO DO INTEGRANTE 2]` | `AUTORES.md` | tabela da seção 1 | nome do responsável por OpenMP |
| 3 | `[NOME COMPLETO DO INTEGRANTE 3]` | `AUTORES.md` | tabela da seção 1 | nome do responsável por MPI |
| 4 | `[MATRÍCULA 1..3]` | `AUTORES.md` | tabela da seção 1 | matrícula de cada integrante |
| 5 | `[TURMA / CÓDIGO DA TURMA]` | `AUTORES.md` (cabeçalho) e `README.md` (seção 12) | cabeçalho | ex.: `CCO085 - turma 03` |
| 6 | `[NOME DO PROFESSOR]`, `[E-MAIL DO PROFESSOR]` | `AUTORES.md` (cabeçalho) | cabeçalho | identificação do docente |
| 7 | `[LOCAL E DATA]` | `AUTORES.md` | final da declaração | cidade e data da assinatura |
| 8 | `[DATA DA APRESENTAÇÃO]` | `AUTORES.md` (cabeçalho) | cabeçalho | data da defesa oral |

## 2. Ajuste à máquina da apresentação

| # | Placeholder / ação | Arquivo | Seção | Observação |
|---|---|---|---|---|
| 9 | `[MÁQUINA DO LABORATÓRIO]`, `[NÚCLEOS DO LABORATÓRIO]` | `README.md` | 5.1 (Máquina e toolchain) e 8.4 | as medições deste repositório foram feitas em um notebook i5-1235U (12 CPUs lógicas / 6 núcleos expostos, 15 W). Se a apresentação for feita no laboratório, **regere os dados**: `make dados && make graficos` e atualize as tabelas das seções 7.1 a 7.6. |
| 10 | Números das tabelas da seção 7 | `README.md` | 7.1 a 7.6 | são os valores medidos nesta máquina; se regerar os dados, copiar as novas medianas de `dados/resultados.csv` |
| 11 | Conclusão da seção 8.4 | `README.md` | 8.4 | o texto explica a queda de eficiência pela CPU híbrida/TDP desta máquina; em hardware homogêneo, ajustar a interpretação |

## 3. Verificações finais (sem placeholders, mas obrigatórias)

| # | Ação | Como |
|---|---|---|
| 12 | Recompilar do zero | `make limpar && make` |
| 13 | Prova de corretude | `make validar` → precisa terminar com `160/160 combinações OK` |
| 14 | Regenerar dados e gráficos | `make dados && make graficos` |
| 15 | Conferir a saída da coleta | `dados/resultados.csv` deve ter 63 linhas de configuração |
| 16 | Conferir os gráficos | `graficos/` deve ter 8 arquivos `.svg` |
| 17 | Verificar se o baseline foi alterado | `git diff --stat docs/baseline_sequencial.cpp` deve ser vazio — **atenção:** a pasta `docs/` (PDF do enunciado + baseline do professor) **ainda não está versionada** no Git; ela precisa ser adicionada no primeiro commit do grupo, e `docs/baseline_sequencial.cpp` deve entrar **sem nenhuma modificação** |
| 18 | Verificar se `bin/` não foi versionado | `git status` não deve listar `bin/` (já está no `.gitignore`) |
| 19 | Ler o README de ponta a ponta | procurar por `[` seguido de maiúscula (marcador de placeholder) |
| 20 | Conferir o PDF do enunciado | `docs/Projeto_Academico_PP_20262.pdf` — prazo, itens obrigatórios e formato de entrega |

## 4. Comandos de conferência

```bash
# 1. Todos os placeholders restantes
grep -rn "\[NOME\|\[MATRÍCULA\|\[TURMA\|\[DATA\|\[MÁQUINA\|\[LINK\|\[E-MAIL\|\[LOCAL" .

# 2. O baseline do professor está intacto?
git diff --stat -- docs/baseline_sequencial.cpp     # esperado: vazio

# 3. Nada de binários ou artefatos temporários versionados?
git status --porcelain

# 4. O README aponta para gráficos que existem?
grep -o "graficos/[a-z_]*\.svg" README.md | sort -u | while read g; do
  [ -f "$g" ] && echo "OK  $g" || echo "FALTA $g"
done
```

## 5. O que **não** é pendência (já está pronto)

* Código das três versões, Makefile, scripts de coleta/validação/gráficos,
  Dockerfile, ambiente de execução.
* Documentos de apoio: `docs/PLANO_MARCO2.md` (plano) e
  `docs/DECISAO_MPI_MODOS.md` (decisão sobre os dois modos de MPI).
* Dados medidos (`dados/bruto.csv`, `dados/resultados.csv`) e os 8 gráficos.
* Seção de defence (FAQ) do README, seção 10.
