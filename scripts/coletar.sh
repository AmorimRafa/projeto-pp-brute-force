#!/usr/bin/env bash
# ============================================================================
#  coletar.sh — Gera dados/bruto.csv com todas as medições do Marco 2
#
#  PROTOCOLO (o motivo está explicado no README)
#  ---------------------------------------------
#  Em um notebook, a frequência da CPU muda com temperatura e carga. Medir o
#  baseline no início e as versões paralelas minutos depois produz speedups
#  falsos (o relógio mudou no meio). Para eliminar esse viés:
#
#    - há um aquecimento global (~8 execuções) antes de qualquer medição;
#    - CADA configuração tem sua própria referência medida ao lado dela
#      (o mesmo código com 1 thread ou 1 processo). O speedup é calculado
#      como ref/tempo DENTRO da mesma janela de tempo;
#    - 1 execução de aquecimento por configuração é descartada;
#    - R repetições (padrão 5) para as curvas principais e R_AUX (3) para as
#      auxiliares; a mediana é calculada em scripts/gerar_graficos.py.
#
#  Variáveis de ambiente (todas opcionais):
#    REPETICOES, REPETICOES_AUX, AQUECIMENTO
#    TAMANHO_FORTE       espaço fixo da escalabilidade forte
#    TAMANHO_COMPLETO    espaço do estudo de políticas (varredura completa)
#    TAMANHOS_BASELINE   lista de tamanhos para a tabela do baseline
#    BASE_FRACA          chaves por thread/processo na escalabilidade fraca
#    THREADS, PROCESSOS  listas explícitas (padrão: 1 2 4 8 <nproc>)
#    MPIRUN              comando do MPI (ex.: "mpirun --allow-run-as-root")
#
#  Uso: make dados   (ou: bash scripts/coletar.sh)
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."

MPIRUN="${MPIRUN:-mpirun --use-hwthread-cpus}"
read -r -a MPIRUN_CMD <<< "${MPIRUN}"

NUCLEOS="$(nproc)"
REPETICOES="${REPETICOES:-5}"
REPETICOES_AUX="${REPETICOES_AUX:-3}"
AQUECIMENTO="${AQUECIMENTO:-1}"

# Tamanhos calibrados para o sequencial ficar na casa de 1-2 s no trecho de
# busca (evita ruído de medição). Podem ser sobrescritos por ambiente.
TAMANHO_FORTE="${TAMANHO_FORTE:-4800000000}"
TAMANHO_COMPLETO="${TAMANHO_COMPLETO:-600000000}"
TAMANHOS_BASELINE="${TAMANHOS_BASELINE:-60000000 480000000 4800000000}"
BASE_FRACA="${BASE_FRACA:-480000000}"

ARQUIVO="dados/bruto.csv"
mkdir -p dados graficos
: > "$ARQUIVO"
echo "versao,politica,modo,escala,espaco,alvo,chave,threads,processos,tempo_busca,tempo_total,tempo_coletivas,mensagens,execucao,tempo_referencia" >> "$ARQUIVO"

# ----------------------------------------------------------------------------
#  Utilidades
# ----------------------------------------------------------------------------
campo() {
    awk -v k="$1" '{
        for (i = 1; i <= NF; i++) {
            n = split($i, a, "=");
            if (n == 2 && a[1] == k) { print a[2]; exit }
        }
    }' <<< "$2"
}

lista_padrao() {
    local extra="${1:-}" t
    for t in 1 2 4 8 "$NUCLEOS" $extra; do
        if [[ "$t" =~ ^[0-9]+$ ]] && [[ "$t" -ge 1 ]] && [[ "$t" -le "$NUCLEOS" ]]; then
            echo "$t"
        fi
    done | sort -n -u
}

THREADS_LISTA="${THREADS:-$(lista_padrao)}"
PROCESSOS_LISTA="${PROCESSOS:-$(lista_padrao)}"

INICIO_TOTAL=$(date +%s)
PROGRESSO=0

progresso() {
    PROGRESSO=$((PROGRESSO + 1))
    local agora decorrido
    agora=$(date +%s)
    decorrido=$((agora - INICIO_TOTAL))
    printf "  [%02d] %-58s %3ds\n" "$PROGRESSO" "$1" "$decorrido"
}

# ----------------------------------------------------------------------------
#  Baseline (docs/baseline_sequencial.cpp, sem alterações)
# ----------------------------------------------------------------------------
coletar_sequencial() {
    local escala="$1" espaco="$2" reps="$3"
    local i saida chave tempo

    ./bin/sequencial "$espaco" > /dev/null

    for ((i = 1; i <= reps; i++)); do
        saida=$(./bin/sequencial "$espaco")
        chave=$(campo chave "$saida")
        tempo=$(campo tempo "$saida")
        # A referência de uma execução sequencial é ela mesma (speedup = 1).
        printf 'sequencial,-,antecipado,%s,%s,-,%s,1,1,%s,%s,0,0,%d,%s\n' \
               "$escala" "$espaco" "$chave" "$tempo" "$tempo" "$i" "$tempo" \
               >> "$ARQUIVO"
    done
}

# ----------------------------------------------------------------------------
#  Versões paralelas (OpenMP e MPI)
# ----------------------------------------------------------------------------
#  Argumentos:
#    versao politica modo escala espaco threads processos reps ref_cmd cmd...
#
#  `ref_cmd` é a MESMA versão com 1 thread/processo, medida imediatamente
#  antes das repetições. É ela que define o T1 usado no speedup, neutralizando
#  a deriva de frequência da máquina.
coletar_paralelo() {
    local versao="$1" politica="$2" modo="$3" escala="$4" espaco="$5"
    local threads="$6" processos="$7" reps="$8" ref_str="$9"
    shift 9
    local cmd=("$@")

    local ref_cmd=("${cmd[@]}")
    if [[ -n "$ref_str" ]]; then
        read -r -a ref_cmd <<< "$ref_str"
    fi

    "${cmd[@]}" > /dev/null   # aquecimento (descartado)

    local i saida saida_ref referencia
    for ((i = 1; i <= reps; i++)); do
        # Referência imediatamente ao lado da execução: se a frequência da CPU
        # mudar, o par ref/config muda junto e a razão continua válida.
        saida_ref=$("${ref_cmd[@]}")
        referencia=$(campo tempo_busca "$saida_ref")

        saida=$("${cmd[@]}")
        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%s\n' \
               "$versao" "$politica" "$modo" "$escala" "$espaco" \
               "$(campo alvo "$saida")" "$(campo chave "$saida")" \
               "$(campo threads "$saida")" "$(campo processos "$saida")" \
               "$(campo tempo_busca "$saida")" "$(campo tempo_total "$saida")" \
               "$(campo tempo_coletivas "$saida")" "$(campo mensagens "$saida")" \
               "$i" "$referencia" >> "$ARQUIVO"
    done
}

echo "=============================================================================="
echo " Coleta de dados — $(date '+%d/%m/%Y %H:%M')"
echo " Núcleos: ${NUCLEOS} | repetições: ${REPETICOES} (aux: ${REPETICOES_AUX})"
echo " Forte: ${TAMANHO_FORTE} | Completo: ${TAMANHO_COMPLETO} | Fraca: ${BASE_FRACA}/unidade"
echo " Threads: $(echo $THREADS_LISTA) | Processos: $(echo $PROCESSOS_LISTA)"
echo "=============================================================================="

# ----------------------------------------------------------------------------
#  Aquecimento global: leva a CPU a um estado térmico estável antes de medir.
# ----------------------------------------------------------------------------
echo " Aquecendo a CPU..."
for _ in 1 2 3 4 5 6; do ./bin/sequencial "$TAMANHO_FORTE" > /dev/null; done

# ----------------------------------------------------------------------------
#  1. Baseline em três tamanhos (tabela do Marco 1/2)
# ----------------------------------------------------------------------------
for espaco in $TAMANHOS_BASELINE; do
    coletar_sequencial forte "$espaco" "$REPETICOES"
    progresso "baseline sequencial  ESPACO=${espaco}"
done

# ----------------------------------------------------------------------------
#  2. OpenMP — curva forte com término antecipado
# ----------------------------------------------------------------------------
for t in $THREADS_LISTA; do
    coletar_paralelo openmp ciclica antecipado forte "$TAMANHO_FORTE" "$t" 1 \
        "$REPETICOES" "./bin/openmp $TAMANHO_FORTE --threads=1" \
        ./bin/openmp "$TAMANHO_FORTE" --threads="$t"
    progresso "openmp ciclica    antecipado  threads=${t}"
done

for t in $THREADS_LISTA; do
    coletar_paralelo openmp dinamica antecipado forte "$TAMANHO_FORTE" "$t" 1 \
        "$REPETICOES" "./bin/openmp $TAMANHO_FORTE --threads=1" \
        ./bin/openmp "$TAMANHO_FORTE" --threads="$t" --politica=dinamica
    progresso "openmp dinamica   antecipado  threads=${t}"
done

# ----------------------------------------------------------------------------
#  3. OpenMP — estudo de políticas no modo completo (trabalho constante)
# ----------------------------------------------------------------------------
for politica in estatica dinamica guiada; do
    for t in $THREADS_LISTA; do
        coletar_paralelo openmp "$politica" completo forte "$TAMANHO_COMPLETO" "$t" 1 \
            "$REPETICOES_AUX" \
            "./bin/openmp $TAMANHO_COMPLETO --threads=1 --completo --politica=estatica" \
            ./bin/openmp "$TAMANHO_COMPLETO" --threads="$t" \
            --completo --politica="$politica"
        progresso "openmp ${politica}  completo   threads=${t}"
    done
done

# ----------------------------------------------------------------------------
#  4. MPI — mestre-escravo e estático, com término antecipado
# ----------------------------------------------------------------------------
for p in $PROCESSOS_LISTA; do
    coletar_paralelo mpi mestre-escravo antecipado forte "$TAMANHO_FORTE" 1 "$p" \
        "$REPETICOES" "$MPIRUN -np 1 ./bin/mpi $TAMANHO_FORTE --modo=mestre-escravo" \
        "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$TAMANHO_FORTE" --modo=mestre-escravo
    progresso "mpi mestre-escravo antecipado  -np ${p}"
done

for p in $PROCESSOS_LISTA; do
    coletar_paralelo mpi estatico antecipado forte "$TAMANHO_FORTE" 1 "$p" \
        "$REPETICOES" "$MPIRUN -np 1 ./bin/mpi $TAMANHO_FORTE --modo=estatico" \
        "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$TAMANHO_FORTE" --modo=estatico
    progresso "mpi estatico       antecipado  -np ${p}"
done

# ----------------------------------------------------------------------------
#  5. MPI — modo completo (trabalho constante)
# ----------------------------------------------------------------------------
for modo in mestre-escravo estatico; do
    for p in $PROCESSOS_LISTA; do
        coletar_paralelo mpi "$modo" completo forte "$TAMANHO_COMPLETO" 1 "$p" \
            "$REPETICOES_AUX" \
            "$MPIRUN -np 1 ./bin/mpi $TAMANHO_COMPLETO --modo=$modo --completo" \
            "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$TAMANHO_COMPLETO" \
            --modo="$modo" --completo
        progresso "mpi ${modo} completo   -np ${p}"
    done
done

# ----------------------------------------------------------------------------
#  6. Escalabilidade fraca (trabalho por unidade constante)
# ----------------------------------------------------------------------------
#  Aqui o tempo é o dado relevante; a referência de cada linha é ela mesma
#  (o speedup não é usado neste gráfico, e sim o tempo absoluto).
for t in $THREADS_LISTA; do
    espaco=$((BASE_FRACA * t))
    coletar_paralelo openmp ciclica antecipado fraca "$espaco" "$t" 1 \
        "$REPETICOES_AUX" "" ./bin/openmp "$espaco" --threads="$t"
    progresso "openmp fraca  threads=${t}  ESPACO=${espaco}"
done

for p in $PROCESSOS_LISTA; do
    espaco=$((BASE_FRACA * p))
    coletar_paralelo mpi estatico antecipado fraca "$espaco" 1 "$p" \
        "$REPETICOES_AUX" "" "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$espaco" \
        --modo=estatico
    progresso "mpi estatico fraca  -np ${p}  ESPACO=${espaco}"

    # No mestre-escravo há P-1 trabalhadores: escala-se o espaço por P-1 para
    # manter o trabalho por trabalhador constante.
    if [[ "$p" -gt 1 ]]; then
        espaco=$((BASE_FRACA * (p - 1)))
    else
        espaco=$BASE_FRACA
    fi
    coletar_paralelo mpi mestre-escravo antecipado fraca "$espaco" 1 "$p" \
        "$REPETICOES_AUX" "" "${MPIRUN_CMD[@]}" -np "$p" ./bin/mpi "$espaco" \
        --modo=mestre-escravo
    progresso "mpi mestre-escravo fraca  -np ${p}  ESPACO=${espaco}"
done

FIM_TOTAL=$(date +%s)
LINHAS=$(( $(wc -l < "$ARQUIVO") - 1 ))

echo "=============================================================================="
echo " Concluído em $((FIM_TOTAL - INICIO_TOTAL)) s — ${LINHAS} execuções em ${ARQUIVO}"
echo " Próximo passo: make graficos"
echo "=============================================================================="
