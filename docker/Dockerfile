# ============================================================================
#  Ambiente Linux reprodutível para o projeto
#  Programação Paralela (CCO085) — 2026/2
#
#  Uso (na raiz do repositório):
#    docker build -t pp-busca -f docker/Dockerfile .
#    docker run --rm -it pp-busca bash
#    # dentro do container:
#    make && make validar && make dados && make graficos
#
#  O código JÁ VAI dentro da imagem (ver COPY abaixo). Assim, mesmo quem
#  esquecer de montar o volume encontra os arquivos em /projeto.
#
#  Se você editar os arquivos no Windows e quiser ver as mudanças sem
#  reconstruir a imagem, monte a pasta atual como volume:
#
#    Git Bash:   MSYS_NO_PATHCONV=1 docker run --rm -it -v "$(pwd -W):/projeto" -w /projeto pp-busca bash
#    PowerShell: docker run --rm -it -v "${PWD}:/projeto" -w /projeto pp-busca bash
#    CMD:        docker run --rm -it -v "%cd%:/projeto" -w /projeto pp-busca bash
#
#  Atenção (Git Bash): o MSYS_NO_PATHCONV=1 é obrigatório. Sem ele, o caminho
#  /projeto é convertido para C:/Program Files/Git/projeto e o Docker falha.
#  Também não use "%cd%" no Git Bash: ele não expande e cria uma pasta vazia
#  com esse nome — era exatamente o motivo de /projeto aparecer vazio.
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

# Copia o projeto para dentro da imagem. O build é feito na raiz do repositório
# ("docker build -f docker/Dockerfile ."): o "." no fim é o contexto, por isso
# daqui a pasta de trabalho é a raiz e podemos copiar tudo com "COPY . .".
COPY . .

# Verificação rápida do toolchain e dos arquivos durante o build.
RUN g++ --version | head -1 \
    && mpicxx --version | head -1 \
    && python3 --version \
    && test -f Makefile && test -f docs/baseline_sequencial.cpp && echo "projeto copiado OK"

CMD ["bash"]
