#!/usr/bin/env bash
# =============================================================================
# scripts/demo.sh — roteiro da DEMONSTRAÇÃO AO VIVO (~9 min, seção 7 do PDF)
# Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
#
# Uso:   make demo                (pausa entre as frentes: ENTER para seguir)
#        PAUSA=0 make demo        (roda tudo direto, sem pausas)
#        NP=8 make demo           (número de processos do MPI; padrão 4)
#
# O PDF exige, no mínimo: o baseline rodando e produzindo o resultado de
# referência; o OpenMP acelerando com o mesmo resultado; o MPI rodando com
# vários processos com o mesmo resultado; e um gráfico de speedup feito com os
# dados medidos. Cada bloco abaixo é a vez de um integrante falar.
# =============================================================================
set -uo pipefail
cd "$(dirname "$0")/.."

N=${N:-2000}
NP=${NP:-4}
PAUSA=${PAUSA:-1}
export OMPI_MCA_btl_vader_single_copy_mechanism=none   # ver scripts/medir.sh

titulo() { printf '\n\033[1;31m==== %s ====\033[0m\n%s\n\n' "$1" "$2"; }
pausa()  { [ "$PAUSA" = 1 ] && read -rp $'\n[ENTER para a proxima frente] ' _ || true; }
rodar()  { printf '\033[1;34m$ %s\033[0m\n' "$*"; "$@"; }

titulo "MAQUINA" "O speedup tem que aparecer numa maquina com varios nucleos."
echo "nucleos logicos: $(nproc)   cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')"
pausa

titulo "FRENTE 1 - BASELINE E PERFILAMENTO" \
       "O codigo do professor, sem nenhuma alteracao. O checksum impresso e o GABARITO."
rodar ./bin/mandelbrot_seq "$N" "$N"
echo
printf '\033[1;34m$ ./bin/mandelbrot_perfil %s %s\033[0m\n' "$N" "$N"
./bin/mandelbrot_perfil "$N" "$N" | sed -n '/=== 1\./,/^RESUMO/p' | grep -v '^RESUMO'
pausa

titulo "FRENTE 2 - OPENMP (1/2): CONDICAO DE CORRIDA" \
       "A soma do checksum feita por varias threads na mesma variavel. Sem protecao da errado."
rodar ./bin/corrida_demo 1000 1000 "$(nproc)"
pausa

titulo "FRENTE 2 - OPENMP (2/2): SPEEDUP E ESCALONAMENTO" \
       "static x dynamic x guided com 1, 2, 4, 8 e 16 threads. Repare na coluna desbalanceamento."
rodar ./bin/mandelbrot_omp "$N" "$N"
pausa

titulo "FRENTE 3 - MPI" "Decomposicao em bloco x ciclica com $NP processos, coletivas Bcast/Gatherv/Reduce."
if command -v mpirun >/dev/null && [ -x bin/mandelbrot_mpi ]; then
    rodar mpirun --oversubscribe -np "$NP" ./bin/mandelbrot_mpi "$N" "$N"
else
    echo "mpirun nao encontrado: rode a demonstracao dentro do container Docker."
fi
pausa

titulo "ANALISE EXPERIMENTAL" "Graficos construidos a partir de resultados/medicoes.csv (make medir)."
if [ -f resultados/medicoes.csv ]; then
    rodar make --no-print-directory graficos
    echo
    echo "Abra no navegador: graficos/speedup_openmp.svg  graficos/speedup_mpi.svg  graficos/eficiencia.svg"
else
    echo "Ainda nao ha medicoes. Rode antes (leva alguns minutos):  make medir"
fi
