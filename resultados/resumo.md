# Resumo das medições (gerado por `make graficos`)

Escalabilidade forte, imagem 2000x2000. Baseline do professor: **2.3084 s**. Fração serial medida no perfil: f = 0.479% (S máx. de Amdahl = 209).

| versão | p | tempo (s) | speedup | eficiência |
|---|---:|---:|---:|---:|
| OpenMP static | 1 | 2.3167 | 1.00 | 99.6% |
| OpenMP static | 2 | 1.1764 | 1.96 | 98.1% |
| OpenMP static | 4 | 1.1056 | 2.09 | 52.2% |
| OpenMP static | 8 | 0.6918 | 3.34 | 41.7% |
| OpenMP static | 16 | 0.4343 | 5.32 | 33.2% |
| OpenMP dynamic | 1 | 2.3105 | 1.00 | 99.9% |
| OpenMP dynamic | 2 | 1.1708 | 1.97 | 98.6% |
| OpenMP dynamic | 4 | 0.5920 | 3.90 | 97.5% |
| OpenMP dynamic | 8 | 0.3661 | 6.30 | 78.8% |
| OpenMP dynamic | 16 | 0.3509 | 6.58 | 41.1% |
| OpenMP guided | 1 | 2.3197 | 1.00 | 99.5% |
| OpenMP guided | 2 | 1.1712 | 1.97 | 98.5% |
| OpenMP guided | 4 | 0.8522 | 2.71 | 67.7% |
| OpenMP guided | 8 | 0.4627 | 4.99 | 62.4% |
| OpenMP guided | 16 | 0.3850 | 6.00 | 37.5% |
| MPI bloco | 1 | 2.3366 | 0.99 | 98.8% |
| MPI bloco | 2 | 1.1853 | 1.95 | 97.4% |
| MPI bloco | 4 | 1.1998 | 1.92 | 48.1% |
| MPI bloco | 8 | 0.8227 | 2.81 | 35.1% |
| MPI ciclico | 1 | 2.3319 | 0.99 | 99.0% |
| MPI ciclico | 2 | 1.1857 | 1.95 | 97.3% |
| MPI ciclico | 4 | 0.6016 | 3.84 | 95.9% |
| MPI ciclico | 8 | 0.3700 | 6.24 | 78.0% |

Escalabilidade fraca (~1.00 milhao de pixels por unidade), E = T(1)/T(p):

| versão | p | imagem | tempo (s) | eficiência |
|---|---:|---:|---:|---:|
| OpenMP dynamic | 1 | 1000x1000 | 0.5777 | 100.0% |
| OpenMP dynamic | 2 | 1414x1414 | 0.5856 | 98.7% |
| OpenMP dynamic | 4 | 2000x2000 | 0.6045 | 95.6% |
| OpenMP dynamic | 8 | 2828x2828 | 0.7000 | 82.5% |
| OpenMP dynamic | 16 | 4000x4000 | 1.4219 | 40.6% |
| MPI ciclico | 1 | 1000x1000 | 0.5797 | 100.0% |
| MPI ciclico | 2 | 1414x1414 | 0.5869 | 98.8% |
| MPI ciclico | 4 | 2000x2000 | 0.5993 | 96.7% |
| MPI ciclico | 8 | 2828x2828 | 0.6936 | 83.6% |

Overhead de comunicação do MPI (comunicacao = tempo total - maior tempo de cálculo):

| decomposição | experimento | p | imagem | tempo total (s) | comunicação (ms) | % do tempo |
|---|---|---:|---:|---:|---:|---:|
| bloco | forte | 1 | 2000x2000 | 2.3366 | 15.7 | 0.7% |
| bloco | forte | 2 | 2000x2000 | 1.1853 | 10.8 | 0.9% |
| bloco | forte | 4 | 2000x2000 | 1.1998 | 9.5 | 0.8% |
| bloco | forte | 8 | 2000x2000 | 0.8227 | 8.9 | 1.1% |
| ciclico | forte | 1 | 2000x2000 | 2.3319 | 15.6 | 0.7% |
| ciclico | forte | 2 | 2000x2000 | 1.1857 | 12.1 | 1.0% |
| ciclico | forte | 4 | 2000x2000 | 0.6016 | 11.0 | 1.8% |
| ciclico | forte | 8 | 2000x2000 | 0.3700 | 12.8 | 3.5% |
| ciclico | fraca | 1 | 1000x1000 | 0.5797 | 3.8 | 0.6% |
| ciclico | fraca | 2 | 1414x1414 | 0.5869 | 5.5 | 0.9% |
| ciclico | fraca | 4 | 2000x2000 | 0.5993 | 10.6 | 1.8% |
| ciclico | fraca | 8 | 2828x2828 | 0.6936 | 18.0 | 2.6% |

Checksum: todas as execuções iguais ao baseline.
