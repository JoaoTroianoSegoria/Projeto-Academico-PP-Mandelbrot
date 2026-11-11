# Resumo das medições (gerado por `make graficos`)

Escalabilidade forte, imagem 2000x2000. Baseline do professor: **1.7889 s**. Fração serial medida no perfil: f = 0.766% (S máx. de Amdahl = 130).

| versão | p | tempo (s) | speedup | eficiência |
|---|---:|---:|---:|---:|
| OpenMP static | 1 | 1.8125 | 0.99 | 98.7% |
| OpenMP static | 2 | 0.9161 | 1.95 | 97.6% |
| OpenMP static | 4 | 0.8758 | 2.04 | 51.1% |
| OpenMP static | 8 | 0.5454 | 3.28 | 41.0% |
| OpenMP static | 16 | 0.3081 | 5.81 | 36.3% |
| OpenMP dynamic | 1 | 1.7987 | 0.99 | 99.5% |
| OpenMP dynamic | 2 | 0.9140 | 1.96 | 97.9% |
| OpenMP dynamic | 4 | 0.4631 | 3.86 | 96.6% |
| OpenMP dynamic | 8 | 0.2350 | 7.61 | 95.2% |
| OpenMP dynamic | 16 | 0.1601 | 11.18 | 69.9% |
| OpenMP guided | 1 | 1.8137 | 0.99 | 98.6% |
| OpenMP guided | 2 | 0.9120 | 1.96 | 98.1% |
| OpenMP guided | 4 | 0.6715 | 2.66 | 66.6% |
| OpenMP guided | 8 | 0.3411 | 5.25 | 65.6% |
| OpenMP guided | 16 | 0.1831 | 9.77 | 61.1% |
| MPI bloco | 1 | 1.8172 | 0.98 | 98.4% |
| MPI bloco | 2 | 0.9298 | 1.92 | 96.2% |
| MPI bloco | 4 | 0.8763 | 2.04 | 51.0% |
| MPI bloco | 8 | 0.5613 | 3.19 | 39.8% |
| MPI ciclico | 1 | 1.8239 | 0.98 | 98.1% |
| MPI ciclico | 2 | 0.9267 | 1.93 | 96.5% |
| MPI ciclico | 4 | 0.4684 | 3.82 | 95.5% |
| MPI ciclico | 8 | 0.2456 | 7.28 | 91.0% |

Escalabilidade fraca (~1.00 milhao de pixels por unidade), E = T(1)/T(p):

| versão | p | imagem | tempo (s) | eficiência |
|---|---:|---:|---:|---:|
| OpenMP dynamic | 1 | 1000x1000 | 0.4671 | 100.0% |
| OpenMP dynamic | 2 | 1414x1414 | 0.4708 | 99.2% |
| OpenMP dynamic | 4 | 2000x2000 | 0.4699 | 99.4% |
| OpenMP dynamic | 8 | 2828x2828 | 0.4684 | 99.7% |
| OpenMP dynamic | 16 | 4000x4000 | 0.5203 | 89.8% |
| MPI ciclico | 1 | 1000x1000 | 0.4532 | 100.0% |
| MPI ciclico | 2 | 1414x1414 | 0.4649 | 97.5% |
| MPI ciclico | 4 | 2000x2000 | 0.4723 | 96.0% |
| MPI ciclico | 8 | 2828x2828 | 0.4989 | 90.8% |

Checksum: todas as execuções iguais ao baseline.
