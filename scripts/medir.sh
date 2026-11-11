#!/usr/bin/env bash
# =============================================================================
# scripts/medir.sh — gera os dados de medição em CSV (seção 5.3 do PDF)
# Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
#
# Uso:   make medir          (ou: bash scripts/medir.sh)
#
# PROTOCOLO DE MEDIÇÃO (o mesmo para todas as versões):
#   - 1 execução de AQUECIMENTO descartada (carrega o programa e as bibliotecas
#     na memória, "acorda" a CPU) + 3 execuções medidas;
#   - guardamos a MEDIANA das 3 (não a média: uma execução atrapalhada pelo
#     sistema operacional não puxa o resultado);
#   - o tempo é o que o próprio programa imprime (só o cálculo, como no baseline);
#   - TODA execução tem o checksum conferido com o do baseline do professor.
#
# EXPERIMENTOS:
#   baseline -> o sequencial do professor em todos os tamanhos usados (gabarito)
#   forte    -> ESCALABILIDADE FORTE: imagem FIXA (2000x2000), aumenta threads/processos.
#               Pergunta: "com mais núcleos, o MESMO problema fica quanto mais rápido?"
#   fraca    -> ESCALABILIDADE FRACA: ~1 milhão de pixels POR thread/processo.
#               Lado da imagem = 1000 * raiz(p): p=4 -> 2000x2000 (4 milhões de pixels).
#               Pergunta: "se o problema cresce junto com a máquina, o tempo fica constante?"
#
# Variáveis opcionais (ex.: THREADS="1 2 4" bash scripts/medir.sh):
#   THREADS      threads do OpenMP          (padrão: 1 2 4 8 16)
#   PROCESSOS    processos do MPI           (padrão: 1 2 4 8)
#   N_FORTE      lado da imagem no teste forte   (padrão: 2000)
#   LADO_FRACA   lado da imagem com p=1 no teste fraco (padrão: 1000)
#
# Saídas:  resultados/medicoes.csv, resultados/perfil.txt, resultados/maquina.txt
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

THREADS=${THREADS:-"1 2 4 8 16"}
PROCESSOS=${PROCESSOS:-"1 2 4 8"}
N_FORTE=${N_FORTE:-2000}
LADO_FRACA=${LADO_FRACA:-1000}
REPETICOES=3

BIN=bin
SAIDA=resultados
CSV=$SAIDA/medicoes.csv
mkdir -p "$SAIDA"

# Dentro do Docker o OpenMPI não consegue usar a cópia direta entre processos
# (CMA/ptrace é bloqueado pelo container) e imprime erros; isto desliga só esse
# atalho — a comunicação passa pela memória compartilhada normal (/dev/shm).
export OMPI_MCA_btl_vader_single_copy_mechanism=none
# --oversubscribe: deixa rodar mais processos que núcleos (para ver a curva saturar)
MPIRUN="mpirun --oversubscribe"

TEM_MPI=1
if ! command -v mpirun >/dev/null || [ ! -x "$BIN/mandelbrot_mpi" ]; then
    TEM_MPI=0
    echo "AVISO: mpirun ou bin/mandelbrot_mpi nao encontrado -> pulando as medicoes de MPI"
    echo "       (rode dentro do container Docker, onde o OpenMPI esta instalado)"
fi

# ---------------------------------------------------------------- máquina
{
    echo "data: $(date '+%Y-%m-%d %H:%M')"
    echo "cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')"
    echo "nucleos logicos (nproc): $(nproc)"
    command -v lscpu >/dev/null && lscpu | grep -E '^(Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|L3 cache)' || true
    echo "memoria: $(grep MemTotal /proc/meminfo | awk '{printf "%.1f GB", $2/1048576}')"
    echo "compilador: $(g++ --version | head -1)"
    if [ $TEM_MPI = 1 ]; then echo "mpi: $(mpirun --version 2>/dev/null | head -1)"; fi
    echo "repeticoes: 1 aquecimento + $REPETICOES medidas (mediana)"
} > "$SAIDA/maquina.txt"
cat "$SAIDA/maquina.txt"
echo

# ---------------------------------------------------------------- auxiliares
# extrai "checksum=NNN" e "tempo=X.XXX" da primeira linha da saída
checksum_de() { grep -m1 -o 'checksum=[0-9-]*' <<<"$1" | cut -d= -f2; }
tempo_de()    { grep -m1 -o 'tempo=[0-9.]*'     <<<"$1" | cut -d= -f2; }
mediana()     { printf '%s\n' "$@" | sort -g | awk '{v[NR]=$1} END {print v[int((NR+1)/2)]}'; }

declare -A GABARITO       # GABARITO[lado] = checksum do baseline
declare -A T_SEQ          # T_SEQ[lado]    = tempo do baseline
FALHAS=0

echo "experimento,versao,variante,p,W,H,tempo_s,tempo_min_s,tempo_max_s,checksum,checksum_baseline,ok" > "$CSV"

# mede "comando..." e grava uma linha no CSV
# uso: medir EXPERIMENTO VERSAO VARIANTE P LADO comando...
medir() {
    local exp=$1 versao=$2 variante=$3 p=$4 lado=$5; shift 5
    "$@" >/dev/null 2>&1 || true                       # aquecimento (descartado)
    local tempos=() soma="" saida
    for _ in $(seq $REPETICOES); do
        saida=$("$@" 2>&1) || { echo "ERRO ao executar: $*"; echo "$saida"; exit 1; }
        local s; s=$(checksum_de "$saida")
        # se UMA das repetições der checksum diferente, o resultado é marcado como errado
        if [ -n "$soma" ] && [ "$s" != "$soma" ]; then soma="INCONSISTENTE"; else soma=${soma:-$s}; fi
        tempos+=("$(tempo_de "$saida")")
    done
    local med min max
    med=$(mediana "${tempos[@]}")
    min=$(printf '%s\n' "${tempos[@]}" | sort -g | head -1)
    max=$(printf '%s\n' "${tempos[@]}" | sort -g | tail -1)
    local gab=${GABARITO[$lado]:-$soma} ok=1
    [ "$soma" = "$gab" ] || { ok=0; FALHAS=$((FALHAS + 1)); }
    echo "$exp,$versao,$variante,$p,$lado,$lado,$med,$min,$max,$soma,$gab,$ok" >> "$CSV"
    printf '  %-8s %-8s %-8s p=%-3s %5sx%-5s  %9.4f s   %s\n' "$exp" "$versao" "$variante" "$p" \
           "$lado" "$lado" "$med" "$([ $ok = 1 ] && echo igual || echo 'CHECKSUM DIFERENTE!')"
}

# lados usados no teste fraco: 1000*raiz(p)
lado_fraco() { awk -v b="$LADO_FRACA" -v p="$1" 'BEGIN { printf "%d", b * sqrt(p) + 0.5 }'; }
LADOS_FRACA=()
for p in $(echo "$THREADS $PROCESSOS" | tr ' ' '\n' | sort -nu); do LADOS_FRACA+=("$(lado_fraco "$p")"); done

# ---------------------------------------------------------------- 0. perfil
echo "== perfil do baseline ($N_FORTE x $N_FORTE) -> $SAIDA/perfil.txt"
"$BIN/mandelbrot_perfil" "$N_FORTE" "$N_FORTE" > "$SAIDA/perfil.txt"
grep -A6 '=== 1.' "$SAIDA/perfil.txt" | sed 's/^/  /'
echo

# ---------------------------------------------------------------- 1. baseline
echo "== 1. baseline do professor (gabarito de cada tamanho)"
for lado in $(printf '%s\n' "$N_FORTE" "${LADOS_FRACA[@]}" | sort -nu); do
    medir baseline seq - 1 "$lado" "$BIN/mandelbrot_seq" "$lado" "$lado"
    GABARITO[$lado]=$(tail -1 "$CSV" | cut -d, -f10)
done
echo

# ---------------------------------------------------------------- 2. forte
echo "== 2. escalabilidade FORTE (${N_FORTE}x${N_FORTE} fixo)"
for esc in static dynamic guided; do
    for t in $THREADS; do
        medir forte omp "$esc" "$t" "$N_FORTE" "$BIN/mandelbrot_omp" "$N_FORTE" "$N_FORTE" "$t" "$esc"
    done
done
if [ $TEM_MPI = 1 ]; then
    for dec in bloco ciclico; do
        for np in $PROCESSOS; do
            medir forte mpi "$dec" "$np" "$N_FORTE" $MPIRUN -np "$np" "$BIN/mandelbrot_mpi" "$N_FORTE" "$N_FORTE" "$dec"
        done
    done
fi
echo

# ---------------------------------------------------------------- 3. fraca
echo "== 3. escalabilidade FRACA (~$((LADO_FRACA * LADO_FRACA)) pixels por thread/processo)"
for t in $THREADS; do
    lado=$(lado_fraco "$t")
    medir fraca omp dynamic "$t" "$lado" "$BIN/mandelbrot_omp" "$lado" "$lado" "$t" dynamic
done
if [ $TEM_MPI = 1 ]; then
    for np in $PROCESSOS; do
        lado=$(lado_fraco "$np")
        medir fraca mpi ciclico "$np" "$lado" $MPIRUN -np "$np" "$BIN/mandelbrot_mpi" "$lado" "$lado" ciclico
    done
fi
echo

# ---------------------------------------------------------------- validação
echo "Dados gravados em $CSV ($(($(wc -l < "$CSV") - 1)) linhas)"
if [ $FALHAS -eq 0 ]; then
    echo "OK: todas as execucoes produziram o MESMO checksum do baseline"
else
    echo "ERRO: $FALHAS execucao(oes) com checksum DIFERENTE do baseline (coluna ok=0 no CSV)"
    exit 1
fi
