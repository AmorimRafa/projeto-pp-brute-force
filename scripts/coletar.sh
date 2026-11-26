#!/usr/bin/env bash
# ============================================================================
#  coletar.sh — gera os CSVs de medição
#
#  Arquivos gerados:
#    dados/perfilamento.csv  uma linha por tamanho: tempo total (medido fora)
#                            x tempo do laço (medido pelo próprio baseline)
#    dados/bruto.csv         uma linha por execução das três versões
#
#  Protocolo simples e suficiente para a apresentação:
#    - 1 execução de aquecimento por configuração (descartada);
#    - 5 repetições; a mediana é calculada em gerar_graficos.py;
#    - o speedup usa o T1 da MESMA versão, medido na mesma curva.
#
#  Uso: make dados
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."

MPIRUN="${MPIRUN:-mpirun --use-hwthread-cpus}"
read -r -a MPIRUN_CMD <<< "$MPIRUN"

NUCLEOS="$(nproc)"
REPETICOES="${REPETICOES:-5}"

TAMANHO_FORTE="${TAMANHO_FORTE:-4800000000}"
BASE_FRACA="${BASE_FRACA:-480000000}"
TAMANHOS_BASELINE="${TAMANHOS_BASELINE:-60000000 480000000 4800000000}"

mkdir -p dados graficos
BRUTO="dados/bruto.csv"
PERFIL="dados/perfilamento.csv"

echo "versao,escala,espaco,chave,threads,processos,tempo_busca,mensagens,execucao" > "$BRUTO"
echo "espaco,tempo_total,tempo_laco,percentual_laco" > "$PERFIL"

# Lê a saída "chave=123 ..." e devolve só o número do campo pedido.
# Assim não precisamos depender da ordem em que os campos aparecem.
campo() {
    awk -v k="$1" '{
        for (i = 1; i <= NF; i++) {
            n = split($i, a, "=");
            if (n == 2 && a[1] == k) { print a[2]; exit }
        }
    }' <<< "$2"
}

lista() {
    local t
    for t in 1 2 4 8 "$NUCLEOS"; do
        if [[ "$t" -le "$NUCLEOS" ]]; then echo "$t"; fi
    done | sort -n -u
}
LISTA="$(lista)"

# Perfilamento sem modificar o baseline: medimos o tempo TOTAL com date(1) e
# comparamos com o tempo DO LAÇO que o próprio baseline imprime. A diferença
# é o custo de inicialização + leitura de argumentos + impressão.
perfil() {
    local espaco="$1" i saida tempo_laco t0 t1 total candidato
    # Mínimo de 3 medições reduz o ruído de escalonamento do sistema.
    tempo_laco=""
    total=""
    for ((i = 1; i <= 3; i++)); do
        saida="$(./bin/sequencial "$espaco")"
        candidato="$(campo tempo "$saida")"
        if [[ -z "$tempo_laco" ]] || awk -v a="$candidato" -v b="$tempo_laco" \
             'BEGIN { exit !(a < b) }'; then tempo_laco="$candidato"; fi

        t0="$(date +%s%N)"
        ./bin/sequencial "$espaco" > /dev/null
        t1="$(date +%s%N)"
        candidato="$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.6f", (b-a)/1e9 }')"
        if [[ -z "$total" ]] || awk -v a="$candidato" -v b="$total" \
             'BEGIN { exit !(a < b) }'; then total="$candidato"; fi
    done
    awk -v e="$espaco" -v t="$total" -v l="$tempo_laco" \
        'BEGIN { printf "%s,%s,%s,%.2f\n", e, t, l, 100*l/t }' >> "$PERFIL"
}

# Executa o baseline (saída diferente: campo "tempo" em vez de "tempo_busca").
coletar_sequencial() {
    local espaco="$1" i saida
    ./bin/sequencial "$espaco" > /dev/null   # aquecimento
    for ((i = 1; i <= REPETICOES; i++)); do
        saida="$(./bin/sequencial "$espaco")"
        printf 'sequencial,forte,%s,%s,1,1,%s,0,%s\n' \
               "$espaco" "$(campo chave "$saida")" \
               "$(campo tempo "$saida")" "$i" >> "$BRUTO"
    done
}

# Executa uma versão paralela. Os argumentos após os 5 primeiros formam o
# comando completo (já com mpirun, quando for o caso).
coletar() {
    local versao="$1" escala="$2" espaco="$3" threads="$4" processos="$5"
    # `shift 5` descarta esses 5 primeiros argumentos: daqui em diante "$@"
    # é só o comando a executar (./bin/openmp ... ou mpirun ... ./bin/mpi ...).
    shift 5
    local i saida
    "$@" > /dev/null   # aquecimento (descartado)
    for ((i = 1; i <= REPETICOES; i++)); do
        saida="$("$@")"
        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
               "$versao" "$escala" "$espaco" \
               "$(campo chave "$saida")" "$threads" "$processos" \
               "$(campo tempo_busca "$saida")" \
               "$(campo mensagens "$saida")" "$i" >> "$BRUTO"
    done
}

echo "=============================================================================="
echo " Coleta de dados — $(date '+%d/%m/%Y %H:%M')"
echo " Núcleos: ${NUCLEOS} | repetições: ${REPETICOES}"
echo " Forte: ESPACO=${TAMANHO_FORTE} | Fraca: ${BASE_FRACA} por unidade"
echo "=============================================================================="

echo " Aquecendo a CPU..."
for _ in 1 2 3; do ./bin/sequencial "$TAMANHO_FORTE" > /dev/null; done

echo " 1/5 Baseline (3 tamanhos) e perfilamento"
for espaco in $TAMANHOS_BASELINE; do
    perfil "$espaco"
    coletar_sequencial "$espaco"
done

echo " 2/5 OpenMP — escalabilidade forte"
for t in $LISTA; do
    coletar openmp forte "$TAMANHO_FORTE" "$t" 1 \
        ./bin/openmp "$TAMANHO_FORTE" --threads="$t"
done

echo " 3/5 MPI (mestre-escravo) — escalabilidade forte"
for p in $LISTA; do
    coletar mpi forte "$TAMANHO_FORTE" 1 "$p" \
        "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$TAMANHO_FORTE"
done

echo " 4/5 OpenMP — escalabilidade fraca"
for t in $LISTA; do
    espaco=$((BASE_FRACA * t))
    coletar openmp fraca "$espaco" "$t" 1 \
        ./bin/openmp "$espaco" --threads="$t"
done

echo " 5/5 MPI — escalabilidade fraca"
for p in $LISTA; do
    # No mestre-escravo há P-1 trabalhadores: mantemos o trabalho POR TRABALHADOR
    # constante, então o espaço cresce com P-1 (e não com P).
    if [[ "$p" -gt 1 ]]; then
        espaco=$((BASE_FRACA * (p - 1)))
    else
        espaco=$BASE_FRACA
    fi
    coletar mpi fraca "$espaco" 1 "$p" \
        "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$espaco"
done

echo "=============================================================================="
echo " Concluído: $(( $(wc -l < "$BRUTO") - 1 )) execuções em ${BRUTO}"
echo " Próximo passo: make graficos"
echo "=============================================================================="
