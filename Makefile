# ============================================================================
#  Makefile — Busca exaustiva paralela (Problema 9)
#  Programação Paralela (CCO085) — 2026/2
#
#  Alvos principais:
#    make            compila as três versões em bin/
#    make validar    compara OpenMP/MPI com o baseline (corretude)
#    make dados      coleta as medições em dados/bruto.csv
#    make graficos   agrega os dados e gera os SVGs em graficos/
#    make tudo       dados + graficos
#    make limpar     remove binários e dados gerados
#
#  O baseline é compilado DIRETAMENTE de docs/baseline_sequencial.cpp, sem
#  nenhuma alteração. Ele é o gabarito de resultado e de tempo.
# ============================================================================

CXX      ?= g++
MPICXX   ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
OMPFLAGS ?= -fopenmp
INCLUDES := -Isrc
BIN      := bin

.PHONY: all sequencial openmp mpi validar dados graficos tudo limpar limpar-dados ajuda

all: $(BIN)/sequencial $(BIN)/openmp $(BIN)/mpi

$(BIN):
	mkdir -p $(BIN)

# Baseline do professor: nenhuma flag extra além de -O2 (como no cabeçalho
# original). Não altere o arquivo em docs/.
$(BIN)/sequencial: docs/baseline_sequencial.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/openmp: src/openmp/openmp.cpp src/comum/busca.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(INCLUDES) -o $@ $<

$(BIN)/mpi: src/mpi/mpi.cpp src/comum/busca.hpp | $(BIN)
	$(MPICXX) $(CXXFLAGS) -DOMPI_SKIP_MPICXX $(INCLUDES) -o $@ $<

sequencial: $(BIN)/sequencial
openmp: $(BIN)/openmp
mpi: $(BIN)/mpi

validar: all
	bash scripts/validar.sh

dados: all
	bash scripts/coletar.sh

graficos:
	python3 scripts/gerar_graficos.py

tudo: dados graficos

limpar:
	rm -rf $(BIN)

# Apaga também os artefatos de medição e os gráficos gerados.
# Use com cuidado: os dados e os gráficos do README são versionados.
limpar-dados:
	rm -f dados/bruto.csv dados/resultados.csv
	rm -f graficos/*.svg

ajuda:
	@sed -n '2,20p' Makefile
