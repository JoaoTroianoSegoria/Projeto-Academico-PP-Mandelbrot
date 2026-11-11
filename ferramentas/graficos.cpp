/* =============================================================================
 * ferramentas/graficos.cpp — gera os gráficos (SVG) a partir dos dados medidos
 * Projeto Acadêmico CCO085 (IESB 2026/2) — Conjunto de Mandelbrot
 *
 * Lê  resultados/medicoes.csv   (gerado por scripts/medir.sh)
 *     resultados/perfil.txt     (linha RESUMO -> fração serial f para a curva de Amdahl)
 * Gera graficos/tempo_forte.svg       tempo x threads/processos (2000x2000)
 *      graficos/speedup_openmp.svg    speedup do OpenMP: static, dynamic, guided, ideal e Amdahl
 *      graficos/speedup_mpi.svg       speedup do MPI: bloco, ciclico, ideal e Amdahl
 *      graficos/eficiencia.svg        eficiência na escalabilidade forte e fraca
 *
 * Escrito em C++ puro (sem Python/matplotlib) para rodar no mesmo container do
 * projeto, que só tem g++. SVG é texto: abre em qualquer navegador e no GitHub.
 *
 * Definições usadas:
 *   speedup     S(p) = T_baseline / T(p)       (baseline = código do professor, mesmo tamanho)
 *   eficiência  E(p) = S(p) / p                (forte)
 *   eficiência  E(p) = T(1) / T(p)             (fraca: o trabalho cresce junto com p,
 *                                               então o ideal é o tempo NÃO mudar -> E = 100%)
 *   Amdahl      S(p) = 1 / (f + (1 - f) / p)
 * =============================================================================*/
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
using namespace std;

struct Linha {
    string experimento, versao, variante;
    int p = 0, W = 0;
    double tempo = 0;
    bool ok = false;
};

struct Serie {
    string nome, cor;
    bool tracejada = false;
    vector<pair<double, double>> pontos;   // (p, valor)
};

// ------------------------------------------------------------------- leitura
vector<Linha> ler_csv(const string& caminho) {
    vector<Linha> linhas;
    ifstream f(caminho);
    string s;
    getline(f, s);   // cabeçalho
    while (getline(f, s)) {
        if (s.empty()) continue;
        vector<string> c;
        stringstream ss(s); string campo;
        while (getline(ss, campo, ',')) c.push_back(campo);
        if (c.size() < 12) continue;
        Linha l;
        l.experimento = c[0]; l.versao = c[1]; l.variante = c[2];
        l.p = stoi(c[3]); l.W = stoi(c[4]); l.tempo = stod(c[6]); l.ok = (c[11] == "1");
        linhas.push_back(l);
    }
    return linhas;
}

// fração serial f da linha "RESUMO;W;H;total;calculo;serial;f" do perfil
double ler_f(const string& caminho) {
    ifstream f(caminho);
    string s;
    while (getline(f, s))
        if (s.rfind("RESUMO;", 0) == 0) return stod(s.substr(s.rfind(';') + 1));
    return -1;
}

// ------------------------------------------------------------------- gráfico SVG
string fmt(double v, int casas = 2) { char b[32]; snprintf(b, sizeof b, "%.*f", casas, v); return b; }

// Desenha um gráfico de linhas. Eixo x em escala log2 (1, 2, 4, 8, 16...).
void grafico(const string& arquivo, const string& titulo, const string& subtitulo,
             const string& rotulo_x, const string& rotulo_y, const vector<Serie>& series,
             bool y_percentual = false) {
    const double L = 820, A = 520;                    // tamanho da figura
    const double ml = 75, mr = 200, mt = 70, mb = 60; // margens (direita = legenda)
    const double pw = L - ml - mr, ph = A - mt - mb;

    double xmin = 1e9, xmax = 0, ymax = 0;
    for (auto& s : series) for (auto& pt : s.pontos) {
        xmin = min(xmin, pt.first); xmax = max(xmax, pt.first); ymax = max(ymax, pt.second);
    }
    if (xmax <= xmin) xmax = xmin * 2;
    // arredonda o topo do eixo y para um valor "redondo"
    double passo = y_percentual ? 20 : (ymax > 10 ? 2 : (ymax > 4 ? 1 : 0.5));
    ymax = ceil(ymax * 1.05 / passo) * passo;
    auto X = [&](double p) { return ml + pw * (log2(p) - log2(xmin)) / (log2(xmax) - log2(xmin)); };
    auto Y = [&](double v) { return mt + ph * (1 - v / ymax); };

    FILE* f = fopen(arquivo.c_str(), "w");
    if (!f) { fprintf(stderr, "nao consegui criar %s (a pasta graficos/ existe?)\n", arquivo.c_str()); return; }
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\" "
               "font-family=\"Helvetica, Arial, sans-serif\">\n", L, A, L, A);
    fprintf(f, "<rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n");
    fprintf(f, "<text x=\"%.0f\" y=\"30\" font-size=\"19\" font-weight=\"bold\" fill=\"#1f2933\">%s</text>\n", ml, titulo.c_str());
    fprintf(f, "<text x=\"%.0f\" y=\"52\" font-size=\"13\" fill=\"#52606d\">%s</text>\n", ml, subtitulo.c_str());

    // grade horizontal + rótulos do eixo y
    for (double v = 0; v <= ymax + 1e-9; v += passo) {
        fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#e4e7eb\"/>\n", ml, Y(v), ml + pw, Y(v));
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"12\" text-anchor=\"end\" fill=\"#52606d\">%s%s</text>\n",
                ml - 8, Y(v) + 4, fmt(v, (passo < 1) ? 1 : 0).c_str(), y_percentual ? "%" : "");
    }
    // marcas do eixo x (potências de 2)
    for (double p = xmin; p <= xmax + 1e-9; p *= 2) {
        fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#e4e7eb\"/>\n", X(p), mt, X(p), mt + ph);
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"12\" text-anchor=\"middle\" fill=\"#52606d\">%.0f</text>\n",
                X(p), mt + ph + 20, p);
    }
    fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#7b8794\"/>\n", ml, mt + ph, ml + pw, mt + ph);
    fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#7b8794\"/>\n", ml, mt, ml, mt + ph);
    fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"13\" text-anchor=\"middle\" fill=\"#1f2933\">%s</text>\n",
            ml + pw / 2, A - 15, rotulo_x.c_str());
    fprintf(f, "<text transform=\"translate(20 %.1f) rotate(-90)\" font-size=\"13\" text-anchor=\"middle\" fill=\"#1f2933\">%s</text>\n",
            mt + ph / 2, rotulo_y.c_str());

    // séries
    int i = 0;
    for (auto& s : series) {
        if (s.pontos.empty()) continue;
        string d;
        for (size_t k = 0; k < s.pontos.size(); k++)
            d += (k ? " L" : "M") + fmt(X(s.pontos[k].first), 1) + " " + fmt(Y(s.pontos[k].second), 1);
        fprintf(f, "<path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%s\"%s/>\n", d.c_str(), s.cor.c_str(),
                s.tracejada ? "1.5" : "2.5", s.tracejada ? " stroke-dasharray=\"6 4\"" : "");
        if (!s.tracejada)
            for (auto& pt : s.pontos)
                fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"4\" fill=\"%s\"><title>%s: p=%.0f -> %s</title></circle>\n",
                        X(pt.first), Y(pt.second), s.cor.c_str(), s.nome.c_str(), pt.first,
                        fmt(pt.second, 2).c_str());
        // legenda
        double ly = mt + 10 + 24 * i++;
        fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2.5\"%s/>\n",
                ml + pw + 20, ly, ml + pw + 46, ly, s.cor.c_str(), s.tracejada ? " stroke-dasharray=\"6 4\"" : "");
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"13\" fill=\"#1f2933\">%s</text>\n", ml + pw + 54, ly + 4, s.nome.c_str());
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    printf("  gerado: %s\n", arquivo.c_str());
}

// ------------------------------------------------------------------- dados
// tempo do baseline para cada tamanho (lado da imagem)
map<int, double> tempos_baseline(const vector<Linha>& d) {
    map<int, double> t;
    for (auto& l : d) if (l.experimento == "baseline") t[l.W] = l.tempo;
    return t;
}

vector<pair<double, double>> pontos(const vector<Linha>& d, const string& exp, const string& versao,
                                    const string& variante) {
    vector<pair<double, double>> v;
    for (auto& l : d)
        if (l.experimento == exp && l.versao == versao && l.variante == variante) v.push_back({ (double)l.p, l.tempo });
    sort(v.begin(), v.end());
    return v;
}

// transforma tempos em speedup (T_baseline / T)
vector<pair<double, double>> speedup(vector<pair<double, double>> v, double t_base) {
    for (auto& pt : v) pt.second = t_base / pt.second;
    return v;
}

const char* COR_STATIC = "#2563eb", *COR_DYNAMIC = "#dc2626", *COR_GUIDED = "#16a34a";
const char* COR_BLOCO = "#9333ea", *COR_CICLICO = "#ea580c";
const char* COR_IDEAL = "#9aa5b1", *COR_AMDAHL = "#1f2933";

Serie ideal(double pmax) {
    Serie s{ "ideal (S = p)", COR_IDEAL, true, {} };
    for (double p = 1; p <= pmax; p *= 2) s.pontos.push_back({ p, p });
    return s;
}

Serie amdahl(double f, double pmax) {
    Serie s{ "Amdahl (f = " + fmt(100 * f, 2) + "%)", COR_AMDAHL, true, {} };
    for (double p = 1; p <= pmax; p *= 2) s.pontos.push_back({ p, 1.0 / (f + (1 - f) / p) });
    return s;
}

double maior_p(const vector<Serie>& ss) {
    double m = 1;
    for (auto& s : ss) for (auto& pt : s.pontos) m = max(m, pt.first);
    return m;
}

void imprime_tabela(const string& nome, const vector<pair<double, double>>& tempos, double t_base) {
    if (tempos.empty()) return;
    printf("  %-16s", nome.c_str());
    for (auto& pt : tempos) printf("  p=%-2.0f S=%5.2f E=%5.1f%%", pt.first, t_base / pt.second, 100 * t_base / pt.second / pt.first);
    printf("\n");
}

// Tabela em Markdown (resultados/resumo.md), citada no README: assim os números
// do README vêm SEMPRE da última medição, sem ninguém copiar à mão.
void tabela_md(FILE* md, const string& nome, const vector<pair<double, double>>& tempos, double t_base) {
    for (auto& pt : tempos)
        fprintf(md, "| %s | %.0f | %.4f | %.2f | %.1f%% |\n", nome.c_str(), pt.first, pt.second,
                t_base / pt.second, 100 * t_base / pt.second / pt.first);
}

int main() {
    const string CSV = "resultados/medicoes.csv";
    vector<Linha> d = ler_csv(CSV);
    if (d.empty()) { fprintf(stderr, "sem dados em %s -- rode antes: make medir\n", CSV.c_str()); return 1; }

    auto tb = tempos_baseline(d);
    int n_forte = 0;
    for (auto& l : d) if (l.experimento == "forte") { n_forte = l.W; break; }
    if (!tb.count(n_forte)) { fprintf(stderr, "faltou o baseline de %dx%d no CSV\n", n_forte, n_forte); return 1; }
    double t_base = tb[n_forte];
    double f = ler_f("resultados/perfil.txt");
    string tam = to_string(n_forte) + "x" + to_string(n_forte);
    long px_fraca = 0;   // pixels por thread/processo no teste fraco (imagem com p = 1)
    for (auto& l : d) if (l.experimento == "fraca" && l.p == 1) px_fraca = (long)l.W * l.W;
    string txt_fraca = "~" + fmt(px_fraca / 1e6, 2) + " milhao de pixels por unidade";

    // ---- séries do teste forte
    auto st = pontos(d, "forte", "omp", "static"),  dy = pontos(d, "forte", "omp", "dynamic");
    auto gu = pontos(d, "forte", "omp", "guided"),  bl = pontos(d, "forte", "mpi", "bloco");
    auto ci = pontos(d, "forte", "mpi", "ciclico");

    printf("Speedup (S) e eficiencia (E) na escalabilidade forte (%s, baseline = %.3f s):\n", tam.c_str(), t_base);
    imprime_tabela("OpenMP static", st, t_base);  imprime_tabela("OpenMP dynamic", dy, t_base);
    imprime_tabela("OpenMP guided", gu, t_base);  imprime_tabela("MPI bloco", bl, t_base);
    imprime_tabela("MPI ciclico", ci, t_base);
    printf("\n");

    // 1) tempo
    {
        vector<Serie> s = {
            { "OpenMP static", COR_STATIC, false, st }, { "OpenMP dynamic", COR_DYNAMIC, false, dy },
            { "OpenMP guided", COR_GUIDED, false, gu }, { "MPI bloco", COR_BLOCO, false, bl },
            { "MPI ciclico", COR_CICLICO, false, ci } };
        grafico("graficos/tempo_forte.svg", "Tempo de execucao - " + tam,
                "mediana de 3 execucoes; baseline do professor = " + fmt(t_base, 3) + " s",
                "threads (OpenMP) / processos (MPI)", "tempo (s)", s);
    }
    // 2) speedup OpenMP
    {
        vector<Serie> s = { { "static", COR_STATIC, false, speedup(st, t_base) },
                            { "dynamic, 1", COR_DYNAMIC, false, speedup(dy, t_base) },
                            { "guided", COR_GUIDED, false, speedup(gu, t_base) } };
        double pm = maior_p(s);
        s.push_back(ideal(pm));
        if (f > 0) s.push_back(amdahl(f, pm));
        grafico("graficos/speedup_openmp.svg", "Speedup OpenMP por numero de threads - " + tam,
                "S = T(baseline) / T(p); todas as execucoes com o mesmo checksum do baseline",
                "threads", "speedup", s);
    }
    // 3) speedup MPI
    if (!bl.empty() || !ci.empty()) {
        vector<Serie> s = { { "bloco", COR_BLOCO, false, speedup(bl, t_base) },
                            { "ciclico", COR_CICLICO, false, speedup(ci, t_base) } };
        double pm = maior_p(s);
        s.push_back(ideal(pm));
        if (f > 0) s.push_back(amdahl(f, pm));
        grafico("graficos/speedup_mpi.svg", "Speedup MPI por numero de processos - " + tam,
                "S = T(baseline) / T(p); inclui o tempo de comunicacao (Gatherv + Reduce)",
                "processos (mpirun -np)", "speedup", s);
    } else {
        printf("  (sem dados de MPI no CSV: speedup_mpi.svg nao foi gerado)\n");
    }
    // 4) eficiência forte e fraca
    {
        auto ef_forte = [&](vector<pair<double, double>> v) {
            for (auto& pt : v) pt.second = 100 * t_base / pt.second / pt.first;
            return v;
        };
        auto ef_fraca = [&](vector<pair<double, double>> v) {   // E = T(1) / T(p)
            if (v.empty()) return v;
            double t1 = v[0].second;
            for (auto& pt : v) pt.second = 100 * t1 / pt.second;
            return v;
        };
        vector<Serie> s = {
            { "forte - OpenMP dynamic", COR_DYNAMIC, false, ef_forte(dy) },
            { "forte - MPI ciclico", COR_CICLICO, false, ef_forte(ci) },
            { "fraca - OpenMP dynamic", COR_STATIC, false, ef_fraca(pontos(d, "fraca", "omp", "dynamic")) },
            { "fraca - MPI ciclico", COR_GUIDED, false, ef_fraca(pontos(d, "fraca", "mpi", "ciclico")) } };
        double pm = maior_p(s);
        Serie cem{ "ideal (100%)", COR_IDEAL, true, { { 1, 100 }, { pm, 100 } } };
        s.push_back(cem);
        grafico("graficos/eficiencia.svg", "Eficiencia: escalabilidade forte e fraca",
                "forte: E = S/p com " + tam + " fixo;  fraca: E = T(1)/T(p) com " + txt_fraca,
                "threads / processos", "eficiencia", s, true);
    }

    // ---- validação final
    int erros = 0;
    for (auto& l : d) if (!l.ok) erros++;

    // ---- resumo em Markdown
    if (FILE* md = fopen("resultados/resumo.md", "w")) {
        fprintf(md, "# Resumo das medições (gerado por `make graficos`)\n\n");
        fprintf(md, "Escalabilidade forte, imagem %s. Baseline do professor: **%.4f s**. ", tam.c_str(), t_base);
        if (f > 0) fprintf(md, "Fração serial medida no perfil: f = %.3f%% (S máx. de Amdahl = %.0f).", 100 * f, 1 / f);
        fprintf(md, "\n\n| versão | p | tempo (s) | speedup | eficiência |\n|---|---:|---:|---:|---:|\n");
        tabela_md(md, "OpenMP static", st, t_base);  tabela_md(md, "OpenMP dynamic", dy, t_base);
        tabela_md(md, "OpenMP guided", gu, t_base);  tabela_md(md, "MPI bloco", bl, t_base);
        tabela_md(md, "MPI ciclico", ci, t_base);
        fprintf(md, "\nEscalabilidade fraca (%s), E = T(1)/T(p):\n\n"
                    "| versão | p | imagem | tempo (s) | eficiência |\n|---|---:|---:|---:|---:|\n", txt_fraca.c_str());
        for (string v : { "omp", "mpi" }) {
            double t1 = 0;
            for (auto& l : d)
                if (l.experimento == "fraca" && l.versao == v) {
                    if (l.p == 1) t1 = l.tempo;
                    fprintf(md, "| %s | %d | %dx%d | %.4f | %s |\n", v == "omp" ? "OpenMP dynamic" : "MPI ciclico",
                            l.p, l.W, l.W, l.tempo, t1 > 0 ? (fmt(100 * t1 / l.tempo, 1) + "%").c_str() : "-");
                }
        }
        fprintf(md, "\nChecksum: %s\n", erros == 0 ? "todas as execuções iguais ao baseline." : "HÁ EXECUÇÕES DIFERENTES DO BASELINE!");
        fclose(md);
        printf("  gerado: resultados/resumo.md\n");
    }
    printf("\n%zu execucoes no CSV\n", d.size());
    if (erros == 0) { printf("OK: todas as execucoes produziram o MESMO checksum do baseline\n"); return 0; }
    printf("ERRO: %d execucao(oes) com checksum diferente do baseline\n", erros);
    return 1;
}
