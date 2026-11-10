#!/usr/bin/env bash
# ============================================================================
#  validar.sh — Prova de corretude contra o baseline
#
#  Roda o baseline (docs/baseline_sequencial.cpp, compilado sem alterações) e
#  compara a chave encontrada por TODAS as versões, em vários tamanhos de
#  espaço, políticas, números de threads e números de processos.
#
#  Uso:  make validar
#  Saída: uma linha OK/FALHA por combinação; código de saída != 0 se falhar.
# ============================================================================
set -uo pipefail

cd "$(dirname "$0")/.."

MPIRUN="${MPIRUN:-mpirun --use-hwthread-cpus}"
read -r -a MPIRUN_CMD <<< "${MPIRUN}"

ESPACOS="${ESPACOS:-0 1 2 3 10 1000 100003 60000000}"
NUCLEOS="$(nproc)"
VERDE=$'\033[32m'
VERMELHO=$'\033[31m'
ZERO=$'\033[0m'
falhas=0
total=0

# Extrai o valor de um campo "chave=valor" da linha de saída.
campo() {
    awk -v k="$1" '{
        for (i = 1; i <= NF; i++) {
            n = split($i, a, "=");
            if (n == 2 && a[1] == k) { print a[2]; exit }
        }
    }' <<< "$2"
}

verificar() {
    local descricao="$1" esperado="$2" obtido="$3"
    total=$((total + 1))
    if [[ "$obtido" == "$esperado" ]]; then
        printf "  %-62s %sOK%s   (chave=%s)\n" "$descricao" "$VERDE" "$ZERO" "$obtido"
    else
        printf "  %-62s %sFALHA%s (esperado=%s, obtido=%s)\n" \
               "$descricao" "$VERMELHO" "$ZERO" "$esperado" "$obtido"
        falhas=$((falhas + 1))
    fi
}

if [[ ! -x bin/sequencial || ! -x bin/openmp || ! -x bin/mpi ]]; then
    echo "ERRO: compile antes com 'make'."
    exit 1
fi

# Lista de threads/processos limitada aos núcleos disponíveis.
lista() {
    local valores="1 2 4 8 ${NUCLEOS}" t
    for t in $valores; do
        if [[ "$t" -le "$NUCLEOS" ]]; then echo "$t"; fi
    done | sort -n -u
}
THREADS_LISTA=$(lista)
MAIOR=$(echo "$THREADS_LISTA" | tail -1)

echo "=============================================================================="
echo " Validação de corretude — baseline x OpenMP x MPI"
echo " Núcleos disponíveis: ${NUCLEOS}"
echo "=============================================================================="

for espaco in $ESPACOS; do
    saida_base=$(./bin/sequencial "$espaco")
    referencia=$(campo chave "$saida_base")
    echo
    echo "ESPACO=${espaco}  (baseline: chave=${referencia})"

    # ---------------- OpenMP ----------------
    for nt in $THREADS_LISTA; do
        r=$(./bin/openmp "$espaco" --threads="$nt")
        verificar "openmp  ciclica  antecipado  threads=${nt}" "$referencia" "$(campo chave "$r")"
    done

    r=$(./bin/openmp "$espaco" --threads="$MAIOR" --politica=dinamica)
    verificar "openmp  dinamica antecipado  threads=${MAIOR}" "$referencia" "$(campo chave "$r")"

    r=$(./bin/openmp "$espaco" --threads=1 --completo --politica=estatica)
    verificar "openmp  estatica completo    threads=1" "$referencia" "$(campo chave "$r")"

    r=$(./bin/openmp "$espaco" --threads="$MAIOR" --completo --politica=guiada)
    verificar "openmp  guiada   completo    threads=${MAIOR}" "$referencia" "$(campo chave "$r")"

    # ---------------- MPI ----------------
    for np in $THREADS_LISTA; do
        r=$("${MPIRUN_CMD[@]}" -np "$np" ./bin/mpi "$espaco" --modo=mestre-escravo)
        verificar "mpi     mestre-escravo antecipado  -np ${np}" "$referencia" "$(campo chave "$r")"
    done

    for np in $THREADS_LISTA; do
        r=$("${MPIRUN_CMD[@]}" -np "$np" ./bin/mpi "$espaco" --modo=estatico)
        verificar "mpi     estatico       antecipado  -np ${np}" "$referencia" "$(campo chave "$r")"
    done

    r=$("${MPIRUN_CMD[@]}" -np "$MAIOR" ./bin/mpi "$espaco" --modo=estatico --completo)
    verificar "mpi     estatico       completo    -np ${MAIOR}" "$referencia" "$(campo chave "$r")"

    r=$("${MPIRUN_CMD[@]}" -np 2 ./bin/mpi "$espaco" --modo=mestre-escravo --completo)
    verificar "mpi     mestre-escravo completo    -np 2" "$referencia" "$(campo chave "$r")"
done

echo
echo "=============================================================================="
if [[ "$falhas" -eq 0 ]]; then
    printf " %sRESULTADO: %d/%d combinações OK — todas as versões reproduzem o baseline.%s\n" \
           "$VERDE" "$total" "$total" "$ZERO"
    exit 0
else
    printf " %sRESULTADO: %d falha(s) em %d combinações.%s\n" \
           "$VERMELHO" "$falhas" "$total" "$ZERO"
    exit 1
fi
