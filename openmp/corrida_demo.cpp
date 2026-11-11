/* =============================================================================
 * openmp/corrida_demo.cpp — CONDIÇÃO DE CORRIDA na soma do checksum
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 * Frente 2: Versão OpenMP (tratamento de condições de corrida — aulas 006 e 007)
 *
 * O checksum é a soma das iterações de TODOS os pixels. Se várias threads
 * somarem na MESMA variável ao mesmo tempo, a conta "soma += it" vira três
 * passos que podem se misturar:
 *
 *     thread A: lê soma (100)                    thread B: lê soma (100)
 *     thread A: calcula 100 + 7 = 107            thread B: calcula 100 + 5 = 105
 *     thread A: escreve 107                      thread B: escreve 105   <- o +7 SUMIU
 *
 * Isso é uma CONDIÇÃO DE CORRIDA: o resultado depende de quem chega primeiro,
 * então dá errado e muda a cada execução. Quatro versões do mesmo cálculo:
 *
 *   1. sem protecao  -> errado (e diferente a cada vez)
 *   2. critical      -> correto. Só UMA thread por vez entra no bloco (uma
 *                       "tranca"). Serve para qualquer trecho de código, mas é
 *                       o mais caro: as threads fazem fila a cada pixel.
 *   3. atomic        -> correto. A própria CPU faz o "ler-somar-escrever" como
 *                       uma instrução indivisível. Mais barato que critical, mas
 *                       só vale para UMA operação simples (x += v, x++, ...) e
 *                       todas as threads ainda disputam a mesma variável.
 *   4. reduction     -> correto e o mais rápido. Cada thread soma numa cópia
 *                       PRIVADA (sem disputa nenhuma); só no final o OpenMP
 *                       junta as cópias (uma soma por thread, não por pixel).
 *                       É a escolha usada em openmp/mandelbrot_omp.cpp.
 *
 * PEGADINHA — por que a versão sem proteção perde TANTO (às vezes mais da metade)?
 * Com -O2 o compilador pode guardar "soma" num registrador durante a linha
 * inteira (ele supõe que ninguém mais mexe nela) e só gravar na memória no fim
 * da linha. Aí a "janela" entre ler e escrever dura a linha toda, e o que se
 * perde são linhas inteiras, não pixels. O erro existe com ou sem otimização;
 * a otimização só deixa ele maior. (Pelo padrão C++, corrida de dados é
 * comportamento indefinido: o compilador não tem obrigação de "acertar".)
 *
 * Executar:  ./bin/corrida_demo 1000 1000 8      (largura altura threads)
 * =============================================================================*/
#include <cstdio>
#include <cstdlib>
#include <omp.h>
#include "../comum/mandelbrot_comum.hpp"

const int REPETICOES_SEM_PROTECAO = 3;

// 1) SEM PROTEÇÃO: todas as threads fazem "soma += it" na variável compartilhada
long soma_sem_protecao(int W, int H, int nt) {
    long soma = 0;
    #pragma omp parallel for num_threads(nt) schedule(dynamic, 1)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            soma += iteracoes_pixel(x, y, W, H);     // CORRIDA!
    return soma;
}

// 2) CRITICAL: uma thread por vez dentro do bloco
long soma_critical(int W, int H, int nt) {
    long soma = 0;
    #pragma omp parallel for num_threads(nt) schedule(dynamic, 1)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int it = iteracoes_pixel(x, y, W, H);    // a conta pesada fica FORA da tranca
            #pragma omp critical
            soma += it;
        }
    return soma;
}

// 3) ATOMIC: a soma vira uma operação indivisível do processador
long soma_atomic(int W, int H, int nt) {
    long soma = 0;
    #pragma omp parallel for num_threads(nt) schedule(dynamic, 1)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int it = iteracoes_pixel(x, y, W, H);
            #pragma omp atomic
            soma += it;
        }
    return soma;
}

// 4) REDUCTION: cada thread tem a sua "soma"; o OpenMP junta no final
long soma_reduction(int W, int H, int nt) {
    long soma = 0;
    #pragma omp parallel for num_threads(nt) schedule(dynamic, 1) reduction(+:soma)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            soma += iteracoes_pixel(x, y, W, H);
    return soma;
}

int main(int argc, char** argv) {
    int W  = (argc > 1) ? atoi(argv[1]) : 1000;
    int H  = (argc > 2) ? atoi(argv[2]) : 1000;
    int nt = (argc > 3) ? atoi(argv[3]) : 8;

    // Gabarito: soma sequencial (sem threads)
    double t0 = omp_get_wtime();
    long gabarito = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) gabarito += iteracoes_pixel(x, y, W, H);
    double t_seq = omp_get_wtime() - t0;

    printf("Mandelbrot %dx%d  |  %d threads  |  %d pixels = %d somas na mesma variavel\n",
           W, H, nt, W * H, W * H);
    printf("sequencial: checksum=%ld  tempo=%.4f s   (gabarito)\n\n", gabarito, t_seq);
    printf("%-14s %14s %12s %10s   %s\n", "versao", "checksum", "tempo (s)", "speedup", "resultado");

    for (int r = 1; r <= REPETICOES_SEM_PROTECAO; r++) {
        t0 = omp_get_wtime();
        long s = soma_sem_protecao(W, H, nt);
        double t = omp_get_wtime() - t0;
        printf("sem protecao %d %14ld %12.4f %10.2f   %s (faltam %ld)\n",
               r, s, t, t_seq / t, s == gabarito ? "OK (sorte!)" : "ERRADO", gabarito - s);
    }

    struct { const char* nome; long (*f)(int, int, int); } versoes[] = {
        { "critical", soma_critical }, { "atomic", soma_atomic }, { "reduction", soma_reduction } };
    for (auto& v : versoes) {
        t0 = omp_get_wtime();
        long s = v.f(W, H, nt);
        double t = omp_get_wtime() - t0;
        printf("%-14s %14ld %12.4f %10.2f   %s\n", v.nome, s, t, t_seq / t, s == gabarito ? "OK" : "ERRADO");
    }

    printf("\nPor que \"sem protecao\" muda a cada execucao? Porque a perda depende de quantas\n"
           "vezes duas threads leram a soma ao mesmo tempo -- e isso depende do sistema operacional.\n"
           "critical e atomic acertam, mas TODAS as threads ainda disputam a mesma variavel;\n"
           "reduction acerta sem disputa: cada thread soma na sua copia e o OpenMP junta no fim.\n");
    return 0;
}
