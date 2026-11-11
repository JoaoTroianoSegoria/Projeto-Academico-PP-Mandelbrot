# =============================================================================
# Dockerfile — ambiente igual ao do laboratório (Linux + g++ + make + OpenMPI)
# Projeto Acadêmico CCO085 — Conjunto de Mandelbrot (OpenMP e MPI)
#
# Pense neste arquivo como uma RECEITA:
#   1. comece com um Linux Ubuntu limpo;
#   2. instale o compilador C++ (g++), o make e o OpenMPI;
#   3. deixe tudo pronto para compilar e rodar o projeto.
#
# Montar a imagem (uma vez, ou quando este arquivo mudar):
#   docker build -t mandelbrot-pp .
#
# Entrar no container com a pasta do projeto espelhada em /projeto:
#   docker run --rm -it --shm-size=1g -v "${PWD}:/projeto" mandelbrot-pp
# =============================================================================

# Ponto de partida: Ubuntu 24.04 LTS (versão fixa, para todos terem o mesmo ambiente)
FROM ubuntu:24.04

# Evita que o apt fique perguntando coisas durante a instalação
ENV DEBIAN_FRONTEND=noninteractive

# g++ e make        -> compilar o código (o OpenMP já vem dentro do g++: flag -fopenmp)
# openmpi-bin       -> os comandos mpirun / mpiexec
# libopenmpi-dev    -> o compilador mpicxx e o cabeçalho <mpi.h>
# openssh-client    -> o OpenMPI usa para lançar processos (evita aviso)
# procps            -> comandos como "nproc" e "top" para ver núcleos e uso de CPU
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        g++ make openmpi-bin libopenmpi-dev openssh-client procps \
 && rm -rf /var/lib/apt/lists/*

# O OpenMPI se recusa a rodar como "root" (administrador).
# A imagem do Ubuntu já tem um usuário comum chamado "ubuntu"; usamos ele.
USER ubuntu

# Pasta de trabalho dentro do container. É aqui que a pasta do projeto
# do Windows aparece quando usamos  -v "${PWD}:/projeto"
WORKDIR /projeto

# Ao entrar no container, abre um terminal bash
CMD ["bash"]
