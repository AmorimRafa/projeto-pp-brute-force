# ============================================================================
#  Makefile — Busca exaustiva paralela (Problema 9)
#  Programação Paralela (CCO085) — 2026/2
#
#  Alvos:
#    make            compila as três versões em bin/
#    make validar    compara OpenMP/MPI com o baseline
#    make dados      gera dados/bruto.csv e dados/perfilamento.csv
#    make graficos   gera dados/resultados.csv e graficos/*.svg
#    make tudo       dados + graficos
#    make limpar     remove os binários
#
#  O baseline é compilado DIRETAMENTE de docs/baseline_sequencial.cpp,
#  sem nenhuma alteração: ele é o gabarito de resultado e de tempo.
# ============================================================================

CXX      ?= g++
MPICXX   ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
OMPFLAGS ?= -fopenmp
INCLUDES := -Isrc
BIN      := bin

.PHONY: all sequencial openmp mpi validar dados graficos tudo limpar ajuda

all: $(BIN)/sequencial $(BIN)/openmp $(BIN)/mpi

$(BIN):
	mkdir -p $(BIN)

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

ajuda:
	@sed -n '2,16p' Makefile
