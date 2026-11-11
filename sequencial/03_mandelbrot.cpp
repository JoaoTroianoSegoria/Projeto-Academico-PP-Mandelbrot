/* Baseline 03 — Conjunto de Mandelbrot (contagem de iteracoes)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: custo MUITO desigual por linha/pixel; motiva
 * schedule(dynamic) em OpenMP e balanceamento de carga em MPI.
 *
 * Compilar: g++ -O2 -o 03_mandelbrot 03_mandelbrot.cpp
 * Executar: ./03_mandelbrot 1000 1000    (largura altura)
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>
using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    int W = (argc > 1) ? atoi(argv[1]) : 1000;
    int H = (argc > 2) ? atoi(argv[2]) : 1000;
    const int MAXIT = 1000;
    vector<long> hist(W*H);

    double t0 = agora();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double cr = -2.5 + 3.5 * x / W;
            double ci = -1.25 + 2.5 * y / H;
            double zr = 0, zi = 0; int it = 0;
            while (zr*zr + zi*zi <= 4.0 && it < MAXIT) {
                double t = zr*zr - zi*zi + cr;
                zi = 2*zr*zi + ci; zr = t; it++;
            }
            hist[y*W + x] = it;
        }
    double t1 = agora();

    long soma = 0;
    for (int i = 0; i < W*H; i++) soma += hist[i];
    printf("W=%d H=%d  checksum=%ld  tempo=%.6f s\n", W, H, soma, t1 - t0);
    return 0;
}
