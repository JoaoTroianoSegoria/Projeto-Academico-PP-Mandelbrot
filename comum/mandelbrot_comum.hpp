/* =============================================================================
 * comum/mandelbrot_comum.hpp
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 *
 * O cálculo de UM pixel, escrito EXATAMENTE como no baseline do professor
 * (sequencial/03_mandelbrot.cpp). Todas as versões (perfil, OpenMP, MPI) usam
 * esta mesma função, então a conta é a mesma em todas — por isso o checksum
 * tem que bater.
 *
 * Regra do projeto: "não é permitido alterar a lógica de cálculo nem a entrada".
 * Aqui só tiramos o corpo do laço de dentro do main() e demos um nome a ele.
 * As expressões são idênticas (mesma ordem das operações em ponto flutuante).
 * =============================================================================*/
#pragma once
#include <ctime>

// Número máximo de iterações por pixel (igual ao baseline).
static const int MAXIT = 1000;

// Relógio monotônico (o mesmo do baseline): não "volta no tempo" se o relógio
// do sistema for ajustado. Retorna segundos como double.
static inline double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* Quantas iterações o pixel (x, y) leva para "escapar".
 *
 *   c = cr + ci·i  é o ponto do plano complexo que o pixel representa:
 *       x em [0, W)  ->  cr em [-2.5, 1.0)     (eixo real)
 *       y em [0, H)  ->  ci em [-1.25, 1.25)   (eixo imaginário)
 *
 *   z começa em 0 e repete  z = z² + c.
 *   Com z = zr + zi·i:   z² = (zr² - zi²) + (2·zr·zi)·i
 *
 *   Para quando |z|² = zr² + zi² > 4 (ou seja |z| > 2: escapou, NÃO pertence
 *   ao conjunto) ou quando chega a MAXIT (consideramos que pertence).
 */
static inline int iteracoes_pixel(int x, int y, int W, int H) {
    double cr = -2.5 + 3.5 * x / W;
    double ci = -1.25 + 2.5 * y / H;
    double zr = 0, zi = 0; int it = 0;
    while (zr*zr + zi*zi <= 4.0 && it < MAXIT) {
        double t = zr*zr - zi*zi + cr;
        zi = 2*zr*zi + ci; zr = t; it++;
    }
    return it;
}

// Calcula uma linha inteira (y fixo) e grava em linha[0..W-1].
// Devolve a soma das iterações da linha (útil para o checksum e para medir carga).
static inline long calcula_linha(int y, int W, int H, long* linha) {
    long soma = 0;
    for (int x = 0; x < W; x++) {
        linha[x] = iteracoes_pixel(x, y, W, H);
        soma += linha[x];
    }
    return soma;
}
