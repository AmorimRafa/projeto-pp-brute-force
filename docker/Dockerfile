# ============================================================================
#  Ambiente Linux reprodutível para o projeto
#  Programação Paralela (CCO085) — 2026/2
#
#  Uso (na raiz do repositório):
#    docker build -t pp-busca -f docker/Dockerfile .
#    docker run --rm -it -v "%cd%":/projeto -w /projeto pp-busca bash
#    # dentro do container:
#    make && make validar && make tudo
#
#  No Linux/macOS, troque "%cd%" por "$(pwd)".
#
#  O container roda como root (padrão do Docker). O OpenMPI exige uma
#  confirmação explícita para rodar como root; as variáveis abaixo fazem isso
#  de forma transparente, sem precisar de --allow-run-as-root em cada comando.
# ============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    OMPI_ALLOW_RUN_AS_ROOT=1 \
    OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        openmpi-bin \
        libopenmpi-dev \
        python3 \
        make \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /projeto

# Verificação rápida do toolchain durante o build.
RUN g++ --version | head -1 \
    && mpicxx --version | head -1 \
    && python3 --version

CMD ["bash"]
