/* =============================================================================
 * sequencial/mandelbrot_perfil.cpp — PERFILAMENTO do baseline
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 * Frente 1: Baseline e Perfilamento
 *
 * É o MESMO cálculo do baseline (03_mandelbrot.cpp), só que "instrumentado":
 * medimos o tempo de cada trecho do programa e o custo de cada linha da imagem.
 * Com isso respondemos, com números:
 *   1. ONDE o programa gasta tempo (hotspot)?
 *   2. Qual a fração serial f e o speedup máximo pela Lei de Amdahl?
 *   3. O custo é igual em todas as linhas? (não! -> desbalanceamento de carga)
 *
 * Compilar:  make            (gera bin/mandelbrot_perfil)
 * Executar:  ./bin/mandelbrot_perfil 2000 2000
 * =============================================================================*/
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "../comum/mandelbrot_comum.hpp"
using namespace std;

int main(int argc, char** argv) {
    int W = (argc > 1) ? atoi(argv[1]) : 1000;
    int H = (argc > 2) ? atoi(argv[2]) : 1000;

    // ---------------------------------------------------------------- trecho 1
    // Alocação: vector<long>(W*H) aloca E zera W*H posições. É serial em todas as versões.
    double t_ini = agora();
    vector<long> hist(W * H);
    double t_alocado = agora();

    // ---------------------------------------------------------------- trecho 2
    // Cálculo (o laço duplo do baseline). Medimos também cada linha separada.
    vector<double> tempo_linha(H);
    for (int y = 0; y < H; y++) {
        double tl = agora();
        for (int x = 0; x < W; x++)
            hist[y*W + x] = iteracoes_pixel(x, y, W, H);
        tempo_linha[y] = agora() - tl;
    }
    double t_calculado = agora();

    // ---------------------------------------------------------------- trecho 3
    // Checksum: a soma de todas as iterações (o "gabarito").
    long soma = 0;
    for (int i = 0; i < W*H; i++) soma += hist[i];
    double t_somado = agora();

    // ---------------------------------------------------------------- trecho 4
    // Saída (printf). Medimos só a linha principal, como no baseline.
    printf("W=%d H=%d  checksum=%ld  tempo=%.6f s\n", W, H, soma, t_calculado - t_alocado);
    double t_fim = agora();

    double T_aloc = t_alocado - t_ini, T_calc = t_calculado - t_alocado;
    double T_soma = t_somado - t_calculado, T_saida = t_fim - t_somado;
    double T_total = t_fim - t_ini;

    printf("\n=== 1. Tempo por trecho (perfil) ===\n");
    printf("%-34s %12s %9s\n", "trecho", "tempo (s)", "% total");
    printf("%-34s %12.6f %8.3f%%\n", "alocacao + zerar vetor (serial)", T_aloc, 100 * T_aloc / T_total);
    printf("%-34s %12.6f %8.3f%%\n", "calculo dos pixels (HOTSPOT)", T_calc, 100 * T_calc / T_total);
    printf("%-34s %12.6f %8.3f%%\n", "soma do checksum", T_soma, 100 * T_soma / T_total);
    printf("%-34s %12.6f %8.3f%%\n", "saida (printf)", T_saida, 100 * T_saida / T_total);
    printf("%-34s %12.6f %8.3f%%\n", "TOTAL", T_total, 100.0);

    // Estatísticas de carga
    long pixels_no_conjunto = 0;
    for (int i = 0; i < W*H; i++) if (hist[i] >= MAXIT) pixels_no_conjunto++;
    double media_pixel = (double)soma / ((double)W * H);
    double tl_min = *min_element(tempo_linha.begin(), tempo_linha.end());
    double tl_max = *max_element(tempo_linha.begin(), tempo_linha.end());
    double tl_med = T_calc / H;

    printf("\n=== 2. O custo NAO e uniforme (desbalanceamento natural) ===\n");
    printf("iteracoes por pixel: media = %.1f   maximo = %d\n", media_pixel, MAXIT);
    printf("pixels dentro do conjunto (chegaram a %d iteracoes): %.1f%%\n", MAXIT, 100.0 * pixels_no_conjunto / ((double)W * H));
    printf("tempo por linha: min = %.3f ms   media = %.3f ms   max = %.3f ms   (max/min = %.0fx)\n",
           1e3 * tl_min, 1e3 * tl_med, 1e3 * tl_max, tl_max / (tl_min > 0 ? tl_min : 1e-12));

    printf("\nCarga por faixa horizontal (10 faixas de linhas):\n");
    printf("%-14s %12s %10s\n", "linhas", "tempo (s)", "% calculo");
    for (int k = 0; k < 10; k++) {
        int a = (int)((long long)k * H / 10), b = (int)((long long)(k + 1) * H / 10);
        double tf = 0; for (int y = a; y < b; y++) tf += tempo_linha[y];
        printf("%5d-%-8d %12.4f %9.2f%%  ", a, b - 1, tf, 100 * tf / T_calc);
        int barras = (int)(100 * tf / T_calc + 0.5);
        for (int j = 0; j < barras; j++) putchar('#');
        putchar('\n');
    }

    // O que aconteceria com divisão em BLOCOS contíguos (como um MPI ingênuo faria):
    // o tempo paralelo seria o do bloco mais caro.
    printf("\nPrevisao com divisao em BLOCOS contiguos de linhas (sem custo de comunicacao):\n");
    for (int p : {2, 4, 8, 16}) {
        double pior = 0;
        for (int r = 0; r < p; r++) {
            int a = (int)((long long)r * H / p), b = (int)((long long)(r + 1) * H / p);
            double tb = 0; for (int y = a; y < b; y++) tb += tempo_linha[y];
            pior = max(pior, tb);
        }
        printf("  p=%-2d  bloco mais caro = %.4f s  -> speedup maximo em blocos = %.2f (ideal = %d)\n",
               p, pior, T_calc / pior, p);
    }

    // Amdahl: tudo o que não é o cálculo dos pixels conta como parte serial.
    // (No OpenMP a soma também é paralelizada, então f real é ainda menor; usamos
    //  a estimativa conservadora.)
    double f = (T_total - T_calc) / T_total;
    printf("\n=== 3. Lei de Amdahl ===\n");
    printf("fracao serial f = (total - calculo) / total = %.5f   ->  fracao paralelizavel = %.3f%%\n", f, 100 * (1 - f));
    printf("S(p) = 1 / (f + (1-f)/p)\n");
    for (int p : {2, 4, 8, 16, 1000000}) {
        double s = 1.0 / (f + (1 - f) / p);
        if (p == 1000000) printf("  p -> infinito : S_max = 1/f = %.1f\n", 1.0 / f);
        else              printf("  p = %-11d: S = %.2f\n", p, s);
    }
    printf("Complexidade: O(W*H*MAXIT) no pior caso; custo real = W*H*(media de iteracoes) = %.3e iteracoes\n",
           (double)soma);

    // Linha de máquina (lida pelo script de medição)
    printf("RESUMO;%d;%d;%.6f;%.6f;%.6f;%.6f\n", W, H, T_total, T_calc, T_total - T_calc, f);
    return 0;
}
