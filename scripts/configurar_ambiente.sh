#!/usr/bin/env bash
# ============================================================================
#  configurar_ambiente.sh — instala o toolchain necessário no Ubuntu/WSL
#
#  O script é idempotente: pode ser executado quantas vezes for preciso.
#  Ele instala:
#    - g++ (compilador C++), make
#    - Open MPI (mpicxx / mpirun) e as headers de desenvolvimento
#    - python3 (usado apenas para gerar os gráficos)
#
#  Uso:
#    bash scripts/configurar_ambiente.sh
#
#  Alternativa sem Docker: ver a seção 4.1 do README.
#  Se você não tem senha de sudo, use o caminho do Docker (seção 4.1).
# ============================================================================
set -euo pipefail

echo "=============================================================================="
echo " Configuração do ambiente — Programação Paralela (CCO085)"
echo "=============================================================================="

if ! command -v apt-get > /dev/null 2>&1; then
    echo "ERRO: este script usa 'apt-get' (Ubuntu/Debian)."
    echo "      Em outro sistema, instale manualmente: g++, make, Open MPI e python3."
    exit 1
fi

SUDO=""
if [[ "$(id -u)" != "0" ]]; then
    if command -v sudo > /dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "ERRO: execute como root ou instale o 'sudo'."
        exit 1
    fi
fi

echo
echo "-> Atualizando a lista de pacotes..."
$SUDO apt-get update -qq

echo "-> Instalando build-essential, Open MPI e python3..."
$SUDO apt-get install -y --no-install-recommends \
    build-essential \
    openmpi-bin \
    libopenmpi-dev \
    python3 \
    make

echo
echo "=============================================================================="
echo " Versões instaladas:"
echo "=============================================================================="
g++ --version | head -1
mpicxx --version | head -1
mpirun --version | head -1
python3 --version
make --version | head -1
echo "Núcleos lógicos disponíveis: $(nproc)"

echo
echo "=============================================================================="
echo " Próximos passos (na raiz do repositório):"
echo "   make           compila as três versões em bin/"
echo "   make validar   prova de corretude contra o baseline"
echo "   make dados     coleta as medições em dados/bruto.csv"
echo "   make graficos  gera dados/resultados.csv e graficos/*.svg"
echo "=============================================================================="
