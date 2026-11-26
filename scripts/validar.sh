#!/usr/bin/env bash
# ============================================================================
#  validar.sh — prova de corretude contra o baseline
#
#  Compara a CHAVE encontrada pelo baseline (arquivo original do professor)
#  com a chave das versões OpenMP e MPI, em vários tamanhos de espaço e em
#  diferentes números de threads/processos.
#
#  Uso: make validar
# ============================================================================
set -uo pipefail

cd "$(dirname "$0")/.."

MPIRUN="${MPIRUN:-mpirun --use-hwthread-cpus}"
read -r -a MPIRUN_CMD <<< "$MPIRUN"

ESPACOS="${ESPACOS:-0 1 2 3 10 1000 60000000}"
NUCLEOS="$(nproc)"

# 70 combinações = 7 tamanhos de espaço × (5 contagens de threads + 5 de processos).
# Os tamanhos pequenos (0, 1, 2, 3) são os mais importantes: a resposta fica no
# começo, então qualquer erro de término antecipado aparece na hora.

# Extrai o valor de um campo "chave=valor" da linha de saída.
# O awk quebra a linha em pedaços (`NF` é o total), separa cada pedaço em
# "nome=valor" (`split`) e devolve o valor quando o nome bate com o pedido.
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
        printf "  %-52s OK   (chave=%s)\n" "$descricao" "$obtido"
    else
        printf "  %-52s FALHA (esperado=%s, obtido=%s)\n" \
               "$descricao" "$esperado" "$obtido"
        falhas=$((falhas + 1))
    fi
}

if [[ ! -x bin/sequencial || ! -x bin/openmp || ! -x bin/mpi ]]; then
    echo "ERRO: compile antes com 'make'."
    exit 1
fi

# Lista 1, 2, 4, 8 e N (sem repetir e sem passar dos núcleos da máquina).
lista() {
    local t
    for t in 1 2 4 8 "$NUCLEOS"; do
        if [[ "$t" -le "$NUCLEOS" ]]; then echo "$t"; fi
    done | sort -n -u
}
LISTA="$(lista)"

total=0
falhas=0

echo "=============================================================================="
echo " Validação de corretude — baseline x OpenMP x MPI"
echo " Núcleos disponíveis: ${NUCLEOS}"
echo "=============================================================================="

for espaco in $ESPACOS; do
    saida_base="$(./bin/sequencial "$espaco")"
    referencia="$(campo chave "$saida_base")"
    echo
    echo "ESPACO=${espaco}  (baseline: chave=${referencia})"

    for t in $LISTA; do
        r="$(./bin/openmp "$espaco" --threads="$t")"
        verificar "openmp  ESPACO=${espaco}  threads=${t}" \
                  "$referencia" "$(campo chave "$r")"
    done

    for p in $LISTA; do
        r="$("${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$espaco")"
        verificar "mpi     ESPACO=${espaco}  -np=${p}" \
                  "$referencia" "$(campo chave "$r")"
    done
done

echo
echo "=============================================================================="
if [[ "$falhas" -eq 0 ]]; then
    echo " RESULTADO: ${total}/${total} combinações OK — todas reproduzem o baseline."
    exit 0
else
    echo " RESULTADO: ${falhas} falha(s) em ${total} combinações."
    exit 1
fi
