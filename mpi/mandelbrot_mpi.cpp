/* =============================================================================
 * mpi/mandelbrot_mpi.cpp — versão MPI (memória distribuída)
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 * Frente 3: Versão MPI
 *
 * IDEIA: cada processo (rank) tem a SUA memória; o rank 2 não enxerga o vetor do
 * rank 0. Então dividir o trabalho = decidir QUAIS LINHAS cada rank calcula
 * (DECOMPOSIÇÃO DE DOMÍNIO) e depois JUNTAR os pedaços no rank 0 (comunicação).
 *
 * Duas decomposições:
 *   bloco    -> o rank r calcula um bloco CONTÍGUO de H/p linhas.
 *               Simples, mas DESBALANCEADO: quem pega o meio da imagem pega as
 *               linhas caras (as que atravessam o conjunto).
 *   ciclico  -> o rank r calcula as linhas r, r+p, r+2p, ... (intercaladas, como
 *               distribuir cartas). Cada rank recebe um pouco de cada região da
 *               imagem -> a carga fica equilibrada.
 *
 * OPERAÇÕES COLETIVAS (todos os ranks participam):
 *   MPI_Bcast   -> o rank 0 lê os parâmetros (W, H, modo) e transmite a todos (aula 05)
 *   MPI_Gatherv -> junta as linhas de cada rank no rank 0; "v" porque cada rank pode
 *                  mandar uma quantidade diferente (aula 06)
 *   MPI_Reduce  -> soma os checksums parciais (MPI_SUM) e pega o tempo do rank mais
 *                  lento (MPI_MAX) (aulas 07 e 12)
 *   MPI_Gather  -> traz o tempo de cálculo de cada rank, para ver a carga
 *
 * Executar:
 *   mpirun -np 4 ./bin/mandelbrot_mpi 2000 2000            -> DEMONSTRAÇÃO: sequencial x bloco x ciclico
 *   mpirun -np 4 ./bin/mandelbrot_mpi 2000 2000 ciclico    -> UMA execução (usada pelo scripts/medir.sh)
 * =============================================================================*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <mpi.h>
#include "../comum/mandelbrot_comum.hpp"
using namespace std;

const int BLOCO = 0, CICLICO = 1, DEMONSTRACAO = 2;
const char* NOME_MODO[] = { "bloco", "ciclico" };
const int RANK_ROOT = 0;

// Primeira linha do bloco do rank r (divisão em blocos contíguos de ~H/p linhas)
int inicio_bloco(int r, int p, int H) { return (int)((long long)r * H / p); }

// Quantas linhas o rank r calcula
int linhas_do_rank(int modo, int r, int p, int H) {
    if (modo == BLOCO) return inicio_bloco(r + 1, p, H) - inicio_bloco(r, p, H);
    return (r < H) ? (H - r + p - 1) / p : 0;          // ciclico: r, r+p, r+2p, ... < H
}

// Qual linha da imagem é a k-ésima linha do rank r
int linha_global(int modo, int r, int p, int H, int k) {
    return (modo == BLOCO) ? inicio_bloco(r, p, H) + k : r + k * p;
}

struct Medida {
    long checksum = 0;
    double tempo_total = 0;           // do rank mais lento (é o tempo do programa)
    double comunicacao = 0;           // tempo_total - maior tempo de cálculo
    vector<double> calculo_por_rank;  // para ver o desbalanceamento
};

// Executado por TODOS os ranks. O resultado completo só existe no rank 0.
Medida rodar(int modo, int W, int H, int meu_rank, int p) {
    Medida m;
    vector<long> imagem, recebido;                         // imagem completa: só no root
    if (meu_rank == RANK_ROOT) { imagem.resize((size_t)W * H); recebido.resize((size_t)W * H); }

    MPI_Barrier(MPI_COMM_WORLD);                           // todos largam juntos
    double t_inicio = MPI_Wtime();

    // 1) CÁLCULO: cada rank calcula só as SUAS linhas, num buffer local
    int minhas_linhas = linhas_do_rank(modo, meu_rank, p, H);
    vector<long> local((size_t)minhas_linhas * W);
    long soma_local = 0;
    double t_calc = MPI_Wtime();
    for (int k = 0; k < minhas_linhas; k++) {
        int y = linha_global(modo, meu_rank, p, H, k);
        soma_local += calcula_linha(y, W, H, &local[(size_t)k * W]);
    }
    double tempo_calculo = MPI_Wtime() - t_calc;

    // 2) JUNTAR A IMAGEM: MPI_Gatherv precisa saber quanto vem de cada rank e onde colocar
    vector<int> quantidades(p), deslocamentos(p);
    int acumulado = 0;
    for (int r = 0; r < p; r++) {
        quantidades[r] = linhas_do_rank(modo, r, p, H) * W;
        deslocamentos[r] = acumulado;
        acumulado += quantidades[r];
    }
    MPI_Gatherv(local.data(), minhas_linhas * W, MPI_LONG,
                recebido.data(), quantidades.data(), deslocamentos.data(), MPI_LONG,
                RANK_ROOT, MPI_COMM_WORLD);

    // O root recebe "rank por rank"; coloca cada linha no lugar certo da imagem.
    // (No bloco a ordem já é a da imagem; no cíclico as linhas vêm intercaladas.)
    if (meu_rank == RANK_ROOT)
        for (int r = 0; r < p; r++)
            for (int k = 0; k < linhas_do_rank(modo, r, p, H); k++) {
                int y = linha_global(modo, r, p, H, k);
                memcpy(&imagem[(size_t)y * W], &recebido[(size_t)deslocamentos[r] + (size_t)k * W], W * sizeof(long));
            }

    // 3) CHECKSUM: cada rank tem uma parte da soma; MPI_Reduce com MPI_SUM junta no root
    MPI_Reduce(&soma_local, &m.checksum, 1, MPI_LONG, MPI_SUM, RANK_ROOT, MPI_COMM_WORLD);
    double meu_tempo = MPI_Wtime() - t_inicio;

    // 4) MEDIÇÕES: o tempo do programa é o do rank MAIS LENTO (MPI_MAX);
    //    MPI_Gather traz o tempo de cálculo de cada rank (para ver a carga).
    MPI_Reduce(&meu_tempo, &m.tempo_total, 1, MPI_DOUBLE, MPI_MAX, RANK_ROOT, MPI_COMM_WORLD);
    m.calculo_por_rank.resize(p);
    MPI_Gather(&tempo_calculo, 1, MPI_DOUBLE, m.calculo_por_rank.data(), 1, MPI_DOUBLE, RANK_ROOT, MPI_COMM_WORLD);

    if (meu_rank == RANK_ROOT) {
        double maior_calculo = *max_element(m.calculo_por_rank.begin(), m.calculo_por_rank.end());
        m.comunicacao = m.tempo_total - maior_calculo;   // o que não foi conta: mensagens + espera
        // Conferência: a imagem montada no root soma o mesmo que o Reduce?
        long soma_imagem = 0;
        for (long v : imagem) soma_imagem += v;
        if (soma_imagem != m.checksum) printf("ATENCAO: imagem montada nao confere com o Reduce!\n");
    }
    return m;
}

// Desbalanceamento = maior tempo de cálculo / tempo médio (1,00 = perfeito)
double desbalanceamento(const vector<double>& t) {
    double maior = 0, soma = 0;
    for (double v : t) { maior = max(maior, v); soma += v; }
    return maior / (soma / t.size());
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int meu_rank, p;
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    // PARÂMETROS: só o root interpreta a linha de comando; MPI_Bcast espalha para todos
    int params[3] = { 2000, 2000, DEMONSTRACAO };          // W, H, modo
    if (meu_rank == RANK_ROOT) {
        if (argc > 1) params[0] = atoi(argv[1]);
        if (argc > 2) params[1] = atoi(argv[2]);
        if (argc > 3) {
            if      (strcmp(argv[3], "bloco") == 0)   params[2] = BLOCO;
            else if (strcmp(argv[3], "ciclico") == 0) params[2] = CICLICO;
            else { fprintf(stderr, "decomposicao invalida: %s (use bloco ou ciclico)\n", argv[3]); params[2] = -1; }
        }
    }
    MPI_Bcast(params, 3, MPI_INT, RANK_ROOT, MPI_COMM_WORLD);
    const int W = params[0], H = params[1], modo = params[2];
    if (modo < 0) { MPI_Finalize(); return 1; }

    // ------------------------------------------------------------------------
    // Modo UMA EXECUÇÃO: mesma linha do baseline + comunicação (para o script)
    // ------------------------------------------------------------------------
    if (modo != DEMONSTRACAO) {
        Medida m = rodar(modo, W, H, meu_rank, p);
        if (meu_rank == RANK_ROOT) {
            printf("W=%d H=%d  checksum=%ld  tempo=%.6f s\n", W, H, m.checksum, m.tempo_total);
            printf("processos=%d  decomposicao=%s  comunicacao=%.6f s  desbalanceamento=%.2f\n",
                   p, NOME_MODO[modo], m.comunicacao, desbalanceamento(m.calculo_por_rank));
        }
        MPI_Finalize();
        return 0;
    }

    // ------------------------------------------------------------------------
    // Modo DEMONSTRAÇÃO: sequencial (só no rank 0) x bloco x cíclico
    // ------------------------------------------------------------------------
    long gabarito = 0;
    double t_seq = 0;
    if (meu_rank == RANK_ROOT) {
        char maquina[MPI_MAX_PROCESSOR_NAME]; int tam;
        MPI_Get_processor_name(maquina, &tam);
        printf("Mandelbrot %dx%d  |  processos: %d  |  maquina: %s\n\n", W, H, p, maquina);
        vector<long> imagem((size_t)W * H);
        double t0 = MPI_Wtime();
        for (int y = 0; y < H; y++) gabarito += calcula_linha(y, W, H, &imagem[(size_t)y * W]);
        t_seq = MPI_Wtime() - t0;
        printf("sequencial (rank 0 sozinho): checksum=%ld  tempo=%.4f s   (este e o gabarito)\n\n", gabarito, t_seq);
    }

    Medida medidas[2];
    for (int d = BLOCO; d <= CICLICO; d++) medidas[d] = rodar(d, W, H, meu_rank, p);

    if (meu_rank == RANK_ROOT) {
        printf("%-12s %10s %9s %11s %14s %17s   %s\n", "decomposicao", "tempo (s)", "speedup", "eficiencia",
               "comunicacao", "desbalanceamento", "checksum");
        for (int d = BLOCO; d <= CICLICO; d++) {
            const Medida& m = medidas[d];
            double speedup = t_seq / m.tempo_total;
            printf("%-12s %10.4f %9.2f %10.1f%% %13.1f%% %17.2f   %s\n", NOME_MODO[d], m.tempo_total, speedup,
                   100.0 * speedup / p, 100.0 * m.comunicacao / m.tempo_total,
                   desbalanceamento(m.calculo_por_rank), m.checksum == gabarito ? "igual" : "DIFERENTE!");
        }
        printf("\nTempo calculando em cada rank (s):\n");
        for (int d = BLOCO; d <= CICLICO; d++) {
            printf("  %-8s", NOME_MODO[d]);
            for (int r = 0; r < p; r++) printf("  r%d=%.3f", r, medidas[d].calculo_por_rank[r]);
            printf("\n");
        }
        printf("\nspeedup = tempo sequencial / tempo paralelo   eficiencia = speedup / processos\n"
               "comunicacao = %% do tempo fora do calculo (mensagens + espera)\n"
               "desbalanceamento = maior tempo de calculo / tempo medio (1,00 = perfeito)\n");
    }

    MPI_Finalize();
    return 0;
}
