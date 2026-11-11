/* =============================================================================
 * openmp/mandelbrot_omp.cpp — versão OpenMP (memória compartilhada)
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 * Frente 2: Versão OpenMP
 *
 * IDEIA: todas as threads enxergam a MESMA memória (o mesmo vetor da imagem).
 * Então paralelizar = dividir as LINHAS da imagem entre as threads. Cada thread
 * calcula linhas inteiras e escreve só nas posições dessas linhas.
 *
 * CONDIÇÕES DE CORRIDA — onde poderiam acontecer e como foram tratadas:
 *   1. A imagem (hist[y*W + x]): cada linha y é de UMA thread só, então duas
 *      threads nunca escrevem na mesma posição. Não há corrida -> não precisa
 *      de proteção nenhuma.
 *   2. A soma do checksum: TODAS as threads somariam na mesma variável. Isso é
 *      corrida (ler-somar-escrever não é atômico). Tratamos com
 *      reduction(+:soma): cada thread soma numa cópia PRIVADA e o OpenMP junta
 *      as cópias no final. Por que reduction e não critical/atomic? Ver
 *      openmp/corrida_demo.cpp, que mede as quatro opções.
 *
 * ESCALONAMENTO (quem pega qual linha):
 *   static  -> divide as linhas em blocos contíguos de ~H/p, decididos ANTES de
 *              começar. Custo quase zero, mas DESBALANCEADO: as linhas do meio
 *              da imagem atravessam o conjunto (1000 iterações por pixel) e as
 *              das bordas escapam rápido.
 *   dynamic -> uma "fila" de linhas: quem termina pega a próxima (pedaço = 1
 *              linha). Equilibra a carga; paga um pouco de sincronização por
 *              linha, que aqui é desprezível perto do custo de uma linha.
 *   guided  -> como o dynamic, mas os pedaços começam grandes e vão diminuindo
 *              (menos idas à fila no início, ajuste fino no fim).
 *
 * Executar:
 *   ./bin/mandelbrot_omp 2000 2000                -> DEMONSTRAÇÃO: tabela 1,2,4,8,16 threads x 3 escalonamentos
 *   ./bin/mandelbrot_omp 2000 2000 8 dynamic      -> UMA execução (usada pelo scripts/medir.sh)
 * =============================================================================*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <omp.h>
#include "../comum/mandelbrot_comum.hpp"
using namespace std;

const int STATIC = 0, DYNAMIC = 1, GUIDED = 2;
const char* NOME_ESCALONAMENTO[] = { "static", "dynamic", "guided" };

struct Medida {
    long checksum = 0;
    double tempo = 0;                 // tempo do cálculo (como no baseline)
    vector<double> tempo_por_thread;  // quanto cada thread ficou calculando
};

/* Calcula a imagem inteira com nt threads e o escalonamento pedido.
 * hist precisa ter W*H posições. */
Medida rodar(int escalonamento, int W, int H, int nt, long* hist) {
    Medida m;
    m.tempo_por_thread.assign(nt, 0.0);
    long soma = 0;

    double t0 = omp_get_wtime();
    // parallel: cria a equipe de nt threads.
    // reduction(+:soma): cada thread ganha a SUA "soma" (começa em 0); no fim
    // da região o OpenMP soma todas as cópias na "soma" original. Sem corrida.
    #pragma omp parallel num_threads(nt) reduction(+:soma)
    {
        double t_thread = omp_get_wtime();
        // Todas as threads entram no mesmo "case", então o "omp for" é
        // encontrado por todas, como o OpenMP exige.
        // nowait: a thread que acaba não espera as outras aqui (a barreira do
        // fim do "parallel" já espera); assim medimos o tempo REAL de cada uma.
        switch (escalonamento) {
        case STATIC:
            #pragma omp for schedule(static) nowait
            for (int y = 0; y < H; y++) soma += calcula_linha(y, W, H, &hist[(size_t)y * W]);
            break;
        case DYNAMIC:
            #pragma omp for schedule(dynamic, 1) nowait
            for (int y = 0; y < H; y++) soma += calcula_linha(y, W, H, &hist[(size_t)y * W]);
            break;
        default: // GUIDED
            #pragma omp for schedule(guided) nowait
            for (int y = 0; y < H; y++) soma += calcula_linha(y, W, H, &hist[(size_t)y * W]);
            break;
        }
        // Cada thread escreve só a SUA posição do vetor: sem corrida.
        m.tempo_por_thread[omp_get_thread_num()] = omp_get_wtime() - t_thread;
    }   // <- barreira implícita: aqui todas terminaram e "soma" já está reduzida
    m.tempo = omp_get_wtime() - t0;
    m.checksum = soma;
    return m;
}

// Desbalanceamento = maior tempo de thread / tempo médio (1,00 = perfeito)
double desbalanceamento(const vector<double>& t) {
    double maior = 0, soma = 0;
    for (double v : t) { maior = max(maior, v); soma += v; }
    return soma > 0 ? maior / (soma / t.size()) : 1.0;
}

int ler_escalonamento(const char* s) {
    for (int e = STATIC; e <= GUIDED; e++) if (strcmp(s, NOME_ESCALONAMENTO[e]) == 0) return e;
    return -1;
}

int main(int argc, char** argv) {
    int W = (argc > 1) ? atoi(argv[1]) : 2000;
    int H = (argc > 2) ? atoi(argv[2]) : 2000;
    vector<long> hist((size_t)W * H);

    // ------------------------------------------------------------------------
    // Modo UMA EXECUÇÃO: ./mandelbrot_omp W H threads escalonamento
    // Mesma primeira linha do baseline (o script compara o checksum).
    // ------------------------------------------------------------------------
    if (argc > 3) {
        int nt = atoi(argv[3]);
        int esc = (argc > 4) ? ler_escalonamento(argv[4]) : DYNAMIC;
        if (nt < 1 || esc < 0) {
            fprintf(stderr, "uso: %s W H threads [static|dynamic|guided]\n", argv[0]);
            return 1;
        }
        Medida m = rodar(esc, W, H, nt, hist.data());
        printf("W=%d H=%d  checksum=%ld  tempo=%.6f s\n", W, H, m.checksum, m.tempo);
        printf("threads=%d  escalonamento=%s  desbalanceamento=%.2f\n",
               nt, NOME_ESCALONAMENTO[esc], desbalanceamento(m.tempo_por_thread));
        return 0;
    }

    // ------------------------------------------------------------------------
    // Modo DEMONSTRAÇÃO
    // ------------------------------------------------------------------------
    printf("Mandelbrot %dx%d  |  nucleos logicos visiveis ao OpenMP: %d\n\n", W, H, omp_get_num_procs());

    // Gabarito: o mesmo laço do baseline, sem OpenMP. Guardamos a imagem
    // inteira para comparar PIXEL A PIXEL (mais forte que comparar só a soma).
    vector<long> gabarito((size_t)W * H);
    long soma_gab = 0;
    double t0 = omp_get_wtime();
    for (int y = 0; y < H; y++) soma_gab += calcula_linha(y, W, H, &gabarito[(size_t)y * W]);
    double t_seq = omp_get_wtime() - t0;
    printf("sequencial: checksum=%ld  tempo=%.4f s   (este e o gabarito)\n\n", soma_gab, t_seq);

    rodar(DYNAMIC, W, H, 2, hist.data());   // aquecimento (cria o pool de threads)

    const int THREADS[] = { 1, 2, 4, 8, 16 };
    printf("%-9s %7s %10s %9s %11s %17s   %s\n", "schedule", "threads", "tempo (s)", "speedup",
           "eficiencia", "desbalanceamento", "resultado");
    bool tudo_igual = true;
    double melhor[3] = { 0, 0, 0 };
    for (int esc = STATIC; esc <= GUIDED; esc++) {
        for (int nt : THREADS) {
            fill(hist.begin(), hist.end(), 0L);   // apaga a imagem: nada "sobra" da execução anterior
            Medida m = rodar(esc, W, H, nt, hist.data());
            bool igual = (m.checksum == soma_gab) && (hist == gabarito);
            tudo_igual = tudo_igual && igual;
            double speedup = t_seq / m.tempo;
            melhor[esc] = max(melhor[esc], speedup);
            printf("%-9s %7d %10.4f %9.2f %10.1f%% %17.2f   %s\n", NOME_ESCALONAMENTO[esc], nt, m.tempo,
                   speedup, 100.0 * speedup / nt, desbalanceamento(m.tempo_por_thread),
                   igual ? "igual" : "DIFERENTE!");
        }
        printf("\n");
    }

    printf("melhor speedup:  static = %.2f   dynamic = %.2f   guided = %.2f\n",
           melhor[STATIC], melhor[DYNAMIC], melhor[GUIDED]);
    printf("%s\n\n", tudo_igual ? "OK: todas as execucoes deram a MESMA imagem e o MESMO checksum do sequencial"
                                : "ERRO: alguma execucao deu resultado diferente do sequencial!");
    printf("speedup = tempo sequencial / tempo paralelo   eficiencia = speedup / threads\n"
           "desbalanceamento = maior tempo de thread / tempo medio (1,00 = perfeito)\n"
           "resultado = imagem comparada pixel a pixel e checksum com o sequencial\n");
    return tudo_igual ? 0 : 2;
}
