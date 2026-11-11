# =============================================================================
# Makefile — Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
#
#   make            compila tudo (sequencial, perfil, OpenMP, MPI, gráficos)
#   make medir      roda o script de medição (gera resultados/*.csv, ~3 min)
#   make graficos   gera os 3 gráficos SVG a partir dos CSV
#   make demo       roteiro rápido para a apresentação (cada frente roda a sua parte)
#   make limpar     apaga os executáveis
#
# Todos usam -O2 (como pede o README do professor). -ffp-contract=off garante
# que o compilador NÃO funda multiplicação+soma em FMA, o que mudaria o
# arredondamento e poderia mudar o checksum (ex.: se alguém usar -march=native).
# =============================================================================
CXX      ?= g++
MPICXX   ?= mpicxx
OTIM     = -O2 -ffp-contract=off
CXXFLAGS = $(OTIM) -std=c++17 -Wall -Wextra
BIN      = bin
COMUM    = comum/mandelbrot_comum.hpp

PROGRAMAS = $(BIN)/mandelbrot_seq $(BIN)/mandelbrot_perfil $(BIN)/mandelbrot_omp \
            $(BIN)/corrida_demo $(BIN)/mandelbrot_mpi $(BIN)/graficos

.PHONY: all medir graficos demo limpar clean

all: $(PROGRAMAS)

$(BIN):
	mkdir -p $(BIN)

# Baseline do professor: compilado exatamente como no cabeçalho dele (g++ -O2)
$(BIN)/mandelbrot_seq: sequencial/03_mandelbrot.cpp | $(BIN)
	$(CXX) -O2 -o $@ $<

$(BIN)/mandelbrot_perfil: sequencial/mandelbrot_perfil.cpp $(COMUM) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

# OpenMP: a flag -fopenmp liga as diretivas #pragma omp e linka a libgomp
$(BIN)/mandelbrot_omp: openmp/mandelbrot_omp.cpp $(COMUM) | $(BIN)
	$(CXX) $(CXXFLAGS) -fopenmp -o $@ $<

$(BIN)/corrida_demo: openmp/corrida_demo.cpp $(COMUM) | $(BIN)
	$(CXX) $(CXXFLAGS) -fopenmp -o $@ $<

# MPI: mpicxx é o g++ "embrulhado" com os includes e bibliotecas do OpenMPI.
# OMPI_SKIP_MPICXX: usamos a API em C do MPI; isso desliga os cabeçalhos antigos
# de C++ do OpenMPI (que só geram avisos de compilação).
$(BIN)/mandelbrot_mpi: mpi/mandelbrot_mpi.cpp $(COMUM) | $(BIN)
	$(MPICXX) $(CXXFLAGS) -DOMPI_SKIP_MPICXX -o $@ $<

$(BIN)/graficos: ferramentas/graficos.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

medir: all
	bash scripts/medir.sh

graficos: $(BIN)/graficos
	mkdir -p graficos
	./$(BIN)/graficos

demo: all
	bash scripts/demo.sh

limpar clean:
	rm -rf $(BIN)
