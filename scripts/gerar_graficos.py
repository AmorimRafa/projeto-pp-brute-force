#!/usr/bin/env python3
# ============================================================================
#  gerar_graficos.py — Agrega as medições e gera os gráficos do Marco 2
#
#  Entrada : dados/bruto.csv  (uma linha por execução, gerado por coletar.sh)
#  Saídas  : dados/resultados.csv  (medianas/mínimos/médias por configuração)
#            graficos/*.svg        (tempo, speedup, eficiência, forte e fraca)
#
#  Usa SOMENTE a biblioteca padrão do Python 3 (csv, math, statistics).
#  Não depende de matplotlib, numpy nem de conexão com a internet — assim roda
#  no laboratório e no container sem instalar nada além do python3.
#
#  Uso: make graficos   (ou: python3 scripts/gerar_graficos.py)
# ============================================================================
import csv
import math
import statistics
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
ENTRADA = BASE / "dados" / "bruto.csv"
SAIDA_CSV = BASE / "dados" / "resultados.csv"
DIR_GRAFICOS = BASE / "graficos"

# Fração paralelizável estimada para a curva de Amdahl. O laço do hash é ~100%
# do tempo; adotamos p = 0,999 para representar o custo residual de
# inicialização/finalização e deixar a curva visível no gráfico.
P_AMDAHL = 0.999

CORES = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#17becf"]


# ----------------------------------------------------------------------------
#  Leitura e agregação
# ----------------------------------------------------------------------------
def para_float(valor, padrao=0.0):
    try:
        return float(valor)
    except (TypeError, ValueError):
        return padrao


def para_int(valor, padrao=0):
    try:
        return int(float(valor))
    except (TypeError, ValueError):
        return padrao


def ler_bruto():
    if not ENTRADA.exists():
        sys.exit(f"ERRO: {ENTRADA} não existe. Rode 'make dados' primeiro.")

    linhas = []
    with ENTRADA.open(encoding="utf-8") as arquivo:
        leitor = csv.DictReader(arquivo)
        for bruta in leitor:
            if not bruta.get("versao"):
                continue
            linhas.append({
                "versao": bruta["versao"],
                "politica": bruta["politica"],
                "modo": bruta["modo"],
                "escala": bruta["escala"],
                "espaco": para_int(bruta["espaco"]),
                "alvo": bruta["alvo"],
                "chave": para_int(bruta["chave"], -1),
                "threads": para_int(bruta["threads"], 1),
                "processos": para_int(bruta["processos"], 1),
                "tempo_busca": para_float(bruta["tempo_busca"]),
                "tempo_total": para_float(bruta["tempo_total"]),
                "tempo_coletivas": para_float(bruta["tempo_coletivas"]),
                "mensagens": para_int(bruta["mensagens"]),
                "execucao": para_int(bruta["execucao"]),
                "tempo_referencia": para_float(bruta.get("tempo_referencia")),
            })

    if not linhas:
        sys.exit(f"ERRO: {ENTRADA} está vazio.")

    grupos = {}
    for linha in linhas:
        chave = (linha["versao"], linha["politica"], linha["modo"],
                 linha["escala"], linha["espaco"], linha["threads"],
                 linha["processos"])
        grupos.setdefault(chave, []).append(linha)

    resultados = []
    for chave, execucoes in grupos.items():
        tempos = sorted(e["tempo_busca"] for e in execucoes)
        mensagens = sorted(e["mensagens"] for e in execucoes)
        coletivas = sorted(e["tempo_coletivas"] for e in execucoes)
        referencias = sorted(e["tempo_referencia"] for e in execucoes)
        razoes = [e["tempo_referencia"] / e["tempo_busca"]
                  for e in execucoes
                  if e["tempo_busca"] > 0 and e["tempo_referencia"] > 0]
        resultados.append({
            "versao": chave[0],
            "politica": chave[1],
            "modo": chave[2],
            "escala": chave[3],
            "espaco": chave[4],
            "threads": chave[5],
            "processos": chave[6],
            "chave": execucoes[0]["chave"],
            "repeticoes": len(execucoes),
            "tempo_mediana": statistics.median(tempos),
            "tempo_minimo": tempos[0],
            "tempo_medio": statistics.fmean(tempos),
            "tempo_maximo": tempos[-1],
            "mensagens_mediana": statistics.median(mensagens),
            "tempo_coletivas_mediana": statistics.median(coletivas),
            # Speedup medido na mesma janela de tempo da referência (T1 da
            # própria implementação), o que neutraliza a deriva de frequência.
            "speedup": statistics.median(razoes) if razoes else 1.0,
            "referencia_mediana": statistics.median(referencias),
        })

    resultados.sort(key=lambda r: (r["versao"], r["modo"], r["politica"],
                                   r["escala"], r["espaco"], r["threads"],
                                   r["processos"]))
    return resultados


def gravar_resultados(resultados):
    SAIDA_CSV.parent.mkdir(parents=True, exist_ok=True)
    campos = ["versao", "politica", "modo", "escala", "espaco", "chave",
              "threads", "processos", "repeticoes", "tempo_mediana",
              "tempo_minimo", "tempo_medio", "tempo_maximo", "speedup",
              "referencia_mediana", "mensagens_mediana",
              "tempo_coletivas_mediana"]
    with SAIDA_CSV.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.DictWriter(arquivo, fieldnames=campos)
        escritor.writeheader()
        for r in resultados:
            escritor.writerow({c: r[c] for c in campos})


# ----------------------------------------------------------------------------
#  Filtros auxiliares
# ----------------------------------------------------------------------------
def filtrar(resultados, **filtros):
    return [r for r in resultados
            if all(r[campo] == valor for campo, valor in filtros.items())]


def por_eixo(resultados, campo_x):
    """Devolve [(x, tempo_mediana)] ordenado por x."""
    return sorted(((r[campo_x], r["tempo_mediana"]) for r in resultados),
                  key=lambda p: p[0])


def primeira(resultados, **filtros):
    achados = filtrar(resultados, **filtros)
    return achados[0] if achados else None


def pontos_speedup(linhas, campo_eixo):
    """[(P, speedup)] usando o speedup medido ao lado da referência T1."""
    return sorted(((r[campo_eixo], r["speedup"]) for r in linhas),
                  key=lambda p: p[0])


# ----------------------------------------------------------------------------
#  Desenho de SVG (sem dependências externas)
# ----------------------------------------------------------------------------
LARGURA = 860
ALTURA = 500
MARGEM_ESQ = 88
MARGEM_DIR = 30
MARGEM_TOPO = 66
MARGEM_BASE = 66


def escapar(texto):
    return (str(texto).replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;"))


def fmt(valor, casas=None):
    if casas is None:
        grande = abs(valor)
        if grande >= 100:
            casas = 0
        elif grande >= 10:
            casas = 1
        else:
            casas = 2
    return f"{valor:.{casas}f}".replace(".", ",")


def passo_bonito(inicio, fim, alvo=6):
    if fim <= inicio:
        return 1.0
    bruto = (fim - inicio) / max(1, alvo)
    expoente = math.floor(math.log10(bruto)) if bruto > 0 else 0
    base = 10.0 ** expoente
    for multiplicador in (1, 2, 2.5, 5, 10):
        if bruto <= multiplicador * base:
            return multiplicador * base
    return 10 * base


def ticks(inicio, fim, passo):
    if passo <= 0:
        return [inicio]
    primeiro = math.ceil(inicio / passo - 1e-9) * passo
    valores = []
    v = primeiro
    while v <= fim + passo * 0.001:
        valores.append(round(v, 10))
        v += passo
    return valores


def gerar_svg(caminho, titulo, rotulo_x, rotulo_y, series,
              x0=0.0, x1=None, y0=0.0, y1=None,
              legenda="superior-esquerda", largura=LARGURA, altura=ALTURA):
    """series: lista de dicts com nome, pontos, cor, tracejada, marcador."""
    pontos_todos = [p for s in series for p in s["pontos"]]
    if not pontos_todos:
        return False

    if x1 is None:
        x1 = max(p[0] for p in pontos_todos)
    if x1 <= x0:
        x1 = x0 + 1
    if y1 is None:
        y1 = max(p[1] for p in pontos_todos) * 1.10
    if y1 <= y0:
        y1 = y0 + 1

    largura_plot = largura - MARGEM_ESQ - MARGEM_DIR
    altura_plot = altura - MARGEM_TOPO - MARGEM_BASE

    def px(x):
        return MARGEM_ESQ + (x - x0) / (x1 - x0) * largura_plot

    def py(y):
        return MARGEM_TOPO + (1 - (y - y0) / (y1 - y0)) * altura_plot

    partes = []
    partes.append('<?xml version="1.0" encoding="UTF-8"?>')
    partes.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{largura}" '
        f'height="{altura}" viewBox="0 0 {largura} {altura}">')
    partes.append(
        '<rect width="100%" height="100%" fill="#ffffff"/>')
    partes.append(
        f'<text x="{largura/2:.1f}" y="32" text-anchor="middle" '
        f'font-family="DejaVu Sans, Arial, sans-serif" font-size="18" '
        f'font-weight="bold" fill="#222">{escapar(titulo)}</text>')

    # Grade e eixos
    passo_y = passo_bonito(y0, y1, 6)
    passo_x = passo_bonito(x0, x1, 7)
    # Em eixos de contagem (threads/processos), prefere passos inteiros.
    if passo_x < 1:
        passo_x = 1
    passo_x = max(1, int(round(passo_x)))

    for valor in ticks(y0, y1, passo_y):
        y = py(valor)
        partes.append(
            f'<line x1="{MARGEM_ESQ}" y1="{y:.1f}" '
            f'x2="{MARGEM_ESQ + largura_plot}" y2="{y:.1f}" '
            f'stroke="#e6e6e6" stroke-width="1"/>')
        partes.append(
            f'<text x="{MARGEM_ESQ - 10}" y="{y + 4:.1f}" text-anchor="end" '
            f'font-family="DejaVu Sans, Arial, sans-serif" font-size="12" '
            f'fill="#444">{fmt(valor)}</text>')

    for valor in ticks(x0, x1, passo_x):
        x = px(valor)
        partes.append(
            f'<line x1="{x:.1f}" y1="{MARGEM_TOPO}" '
            f'x2="{x:.1f}" y2="{MARGEM_TOPO + altura_plot}" '
            f'stroke="#f0f0f0" stroke-width="1"/>')
        partes.append(
            f'<text x="{x:.1f}" y="{MARGEM_TOPO + altura_plot + 22}" '
            f'text-anchor="middle" font-family="DejaVu Sans, Arial, sans-serif" '
            f'font-size="12" fill="#444">{fmt(valor, 0)}</text>')

    partes.append(
        f'<line x1="{MARGEM_ESQ}" y1="{MARGEM_TOPO + altura_plot}" '
        f'x2="{MARGEM_ESQ + largura_plot}" y2="{MARGEM_TOPO + altura_plot}" '
        f'stroke="#333" stroke-width="1.5"/>')
    partes.append(
        f'<line x1="{MARGEM_ESQ}" y1="{MARGEM_TOPO}" x2="{MARGEM_ESQ}" '
        f'y2="{MARGEM_TOPO + altura_plot}" stroke="#333" stroke-width="1.5"/>')

    partes.append(
        f'<text x="{MARGEM_ESQ + largura_plot/2:.1f}" '
        f'y="{altura - 18}" text-anchor="middle" '
        f'font-family="DejaVu Sans, Arial, sans-serif" font-size="14" '
        f'fill="#222">{escapar(rotulo_x)}</text>')
    partes.append(
        f'<text x="22" y="{MARGEM_TOPO + altura_plot/2:.1f}" '
        f'text-anchor="middle" transform="rotate(-90 22 '
        f'{MARGEM_TOPO + altura_plot/2:.1f})" '
        f'font-family="DejaVu Sans, Arial, sans-serif" font-size="14" '
        f'fill="#222">{escapar(rotulo_y)}</text>')

    # Séries
    for serie in series:
        pontos = sorted(serie["pontos"])
        if not pontos:
            continue
        cor = serie["cor"]
        tracejada = serie.get("tracejada", False)
        dash = ' stroke-dasharray="8 5"' if tracejada else ""
        caminho_d = " ".join(
            f"{'M' if i == 0 else 'L'} {px(x):.2f} {py(y):.2f}"
            for i, (x, y) in enumerate(pontos))
        partes.append(
            f'<path d="{caminho_d}" fill="none" stroke="{cor}" '
            f'stroke-width="{serie.get("largura", 2.4)}"{dash} '
            f'stroke-linejoin="round" stroke-linecap="round"/>')
        if serie.get("marcador", True):
            for x, y in pontos:
                partes.append(
                    f'<circle cx="{px(x):.2f}" cy="{py(y):.2f}" r="4" '
                    f'fill="#ffffff" stroke="{cor}" stroke-width="2"/>')

    # Legenda
    if legenda:
        largura_texto = max(len(s["nome"]) for s in series) * 6.7 + 52
        altura_legenda = len(series) * 20 + 12
        if "direita" in legenda:
            lx = MARGEM_ESQ + largura_plot - largura_texto - 12
        else:
            lx = MARGEM_ESQ + 12
        if "inferior" in legenda:
            ly = MARGEM_TOPO + altura_plot - altura_legenda - 10
        else:
            ly = MARGEM_TOPO + 10

        partes.append(
            f'<rect x="{lx:.1f}" y="{ly:.1f}" width="{largura_texto:.1f}" '
            f'height="{altura_legenda}" rx="4" fill="#ffffff" fill-opacity="0.92" '
            f'stroke="#bbbbbb" stroke-width="1"/>')
        for i, serie in enumerate(series):
            yy = ly + 18 + i * 20
            dash = ' stroke-dasharray="6 4"' if serie.get("tracejada") else ""
            partes.append(
                f'<line x1="{lx + 10:.1f}" y1="{yy - 4:.1f}" '
                f'x2="{lx + 34:.1f}" y2="{yy - 4:.1f}" '
                f'stroke="{serie["cor"]}" stroke-width="2.4"{dash}/>')
            partes.append(
                f'<text x="{lx + 42:.1f}" y="{yy:.1f}" '
                f'font-family="DejaVu Sans, Arial, sans-serif" font-size="12" '
                f'fill="#222">{escapar(serie["nome"])}</text>')

    partes.append('</svg>')
    caminho.write_text("\n".join(partes), encoding="utf-8")
    return True


# ----------------------------------------------------------------------------
#  Gráficos
# ----------------------------------------------------------------------------
def grafico_tempo_openmp(resultados, espaco):
    linhas_c = filtrar(resultados, versao="openmp", politica="ciclica",
                       modo="antecipado", escala="forte", espaco=espaco)
    linhas_d = filtrar(resultados, versao="openmp", politica="dinamica",
                       modo="antecipado", escala="forte", espaco=espaco)
    if not linhas_c:
        return

    pontos_c = por_eixo(linhas_c, "threads")
    t1 = pontos_c[0][1]
    max_threads = max(p[0] for p in pontos_c)
    serie_ideal = [(t, t1 / t) for t in range(1, max_threads + 1)]

    series = [
        {"nome": "cíclica (medido)", "pontos": pontos_c, "cor": CORES[0]},
        {"nome": "ideal (T1 / T)", "pontos": serie_ideal, "cor": "#777777",
         "tracejada": True, "marcador": False},
    ]
    if linhas_d:
        series.insert(1, {"nome": "dinâmica (medido)",
                          "pontos": por_eixo(linhas_d, "threads"),
                          "cor": CORES[1]})

    gerar_svg(DIR_GRAFICOS / "tempo_openmp.svg",
              "OpenMP — tempo de busca (término antecipado)",
              "threads", "tempo (s)", series,
              x1=max_threads + 0.5,
              legenda="superior-direita")


def grafico_speedup_openmp(resultados, espaco):
    linhas_c = filtrar(resultados, versao="openmp", politica="ciclica",
                       modo="antecipado", escala="forte", espaco=espaco)
    if not linhas_c:
        return

    def pontos(linhas):
        return pontos_speedup(linhas, "threads")

    max_threads = max(r["threads"] for r in linhas_c)
    ideal = [(t, float(t)) for t in range(1, max_threads + 1)]
    amdahl = [(t, 1.0 / ((1.0 - P_AMDAHL) + P_AMDAHL / t))
              for t in range(1, max_threads + 1)]

    series = [
        {"nome": "cíclica (medido)", "pontos": pontos(linhas_c), "cor": CORES[0]},
        {"nome": "ideal (linear)", "pontos": ideal, "cor": "#777777",
         "tracejada": True, "marcador": False},
        {"nome": f"Amdahl (p={fmt(P_AMDAHL, 3)})", "pontos": amdahl,
         "cor": "#2ca02c", "tracejada": True, "marcador": False},
    ]
    linhas_d = filtrar(resultados, versao="openmp", politica="dinamica",
                       modo="antecipado", escala="forte", espaco=espaco)
    if linhas_d:
        series.insert(1, {"nome": "dinâmica (medido)", "pontos": pontos(linhas_d),
                          "cor": CORES[1]})

    gerar_svg(DIR_GRAFICOS / "speedup_openmp.svg",
              "OpenMP — speedup (T1 medido ao lado de cada configuração)",
              "threads", "speedup  (T1 / Tp)", series,
              x1=max_threads + 0.5,
              y1=max(max_threads, max(p[1] for p in series[0]["pontos"])) * 1.08,
              legenda="superior-esquerda")


def grafico_eficiencia_openmp(resultados, espaco):
    linhas_c = filtrar(resultados, versao="openmp", politica="ciclica",
                       modo="antecipado", escala="forte", espaco=espaco)
    if not linhas_c:
        return

    def pontos(linhas):
        return sorted(((r["threads"], 100.0 * r["speedup"] / r["threads"])
                       for r in linhas), key=lambda p: p[0])

    max_threads = max(r["threads"] for r in linhas_c)
    series = [
        {"nome": "cíclica (medido)", "pontos": pontos(linhas_c), "cor": CORES[0]},
        {"nome": "100% (ideal)", "pontos": [(1, 100.0), (max_threads, 100.0)],
         "cor": "#777777", "tracejada": True, "marcador": False},
    ]
    linhas_d = filtrar(resultados, versao="openmp", politica="dinamica",
                       modo="antecipado", escala="forte", espaco=espaco)
    if linhas_d:
        series.insert(1, {"nome": "dinâmica (medido)",
                          "pontos": pontos(linhas_d), "cor": CORES[1]})

    gerar_svg(DIR_GRAFICOS / "eficiencia_openmp.svg",
              "OpenMP — eficiência (speedup / threads)",
              "threads", "eficiência (%)", series,
              x1=max_threads + 0.5, y1=110,
              legenda="inferior-esquerda")


def grafico_speedup_mpi(resultados, espaco):
    def pontos(politica):
        linhas = filtrar(resultados, versao="mpi", politica=politica,
                         modo="antecipado", escala="forte", espaco=espaco)
        return pontos_speedup(linhas, "processos")

    p_me = pontos("mestre-escravo")
    p_es = pontos("estatico")
    if not p_me and not p_es:
        return

    max_p = max([p[0] for p in p_me + p_es])
    ideal = [(p, float(p)) for p in range(1, max_p + 1)]
    teto_me = [(p, float(p - 1)) for p in range(2, max_p + 1)]

    series = [{"nome": "ideal (linear)", "pontos": ideal, "cor": "#777777",
               "tracejada": True, "marcador": False}]
    if p_me:
        series.append({"nome": "mestre-escravo (medido)", "pontos": p_me,
                       "cor": CORES[1]})
        series.append({"nome": "teto mestre-escravo (P-1)",
                       "pontos": teto_me, "cor": "#ff7f0e", "tracejada": True,
                       "marcador": False})
    if p_es:
        series.append({"nome": "estático (medido)", "pontos": p_es,
                       "cor": CORES[2]})

    gerar_svg(DIR_GRAFICOS / "speedup_mpi.svg",
              "MPI — speedup (T1 medido ao lado de cada configuração)",
              "processos", "speedup  (T1 / Tp)", series,
              x1=max_p + 0.5,
              y1=max(max_p, max(p[1] for s in series for p in s["pontos"])) * 1.08,
              legenda="superior-esquerda")


def grafico_eficiencia_mpi(resultados, espaco):
    def pontos(politica):
        linhas = filtrar(resultados, versao="mpi", politica=politica,
                         modo="antecipado", escala="forte", espaco=espaco)
        return sorted(((r["processos"], 100.0 * r["speedup"] / r["processos"])
                       for r in linhas), key=lambda p: p[0])

    p_me = pontos("mestre-escravo")
    p_es = pontos("estatico")
    if not p_me and not p_es:
        return
    max_p = max([p[0] for p in p_me + p_es])
    teto_me = [(p, 100.0 * (p - 1) / p) for p in range(2, max_p + 1)]

    series = [{"nome": "100% (ideal)",
               "pontos": [(1, 100.0), (max_p, 100.0)],
               "cor": "#777777", "tracejada": True, "marcador": False}]
    if p_me:
        series.append({"nome": "mestre-escravo (medido)", "pontos": p_me,
                       "cor": CORES[1]})
        series.append({"nome": "teto (P-1)/P", "pontos": teto_me,
                       "cor": "#ff7f0e", "tracejada": True,
                       "marcador": False})
    if p_es:
        series.append({"nome": "estático (medido)", "pontos": p_es,
                       "cor": CORES[2]})

    gerar_svg(DIR_GRAFICOS / "eficiencia_mpi.svg",
              "MPI — eficiência (speedup / processos)",
              "processos", "eficiência (%)", series,
              x1=max_p + 0.5, y1=110,
              legenda="inferior-esquerda")


def grafico_politicas(resultados, espaco):
    series = []
    for i, politica in enumerate(["estatica", "dinamica", "guiada"]):
        linhas = filtrar(resultados, versao="openmp", politica=politica,
                         modo="completo", escala="forte", espaco=espaco)
        if not linhas:
            continue
        series.append({"nome": f"{politica} (medido)",
                       "pontos": pontos_speedup(linhas, "threads"),
                       "cor": CORES[i]})

    if not series:
        return
    max_t = max(p[0] for s in series for p in s["pontos"])
    series.insert(0, {"nome": "ideal (linear)",
                      "pontos": [(t, float(t)) for t in range(1, max_t + 1)],
                      "cor": "#777777", "tracejada": True,
                      "marcador": False})

    gerar_svg(DIR_GRAFICOS / "speedup_politicas.svg",
              "OpenMP — políticas de escalonamento (modo completo)",
              "threads", "speedup  (T1 / Tp)", series,
              x1=max_t + 0.5,
              y1=max(max_t, max(p[1] for s in series for p in s["pontos"])) * 1.08,
              legenda="superior-esquerda")



def grafico_escalabilidade_fraca(resultados):
    series = []

    linhas = filtrar(resultados, versao="openmp", politica="ciclica",
                     modo="antecipado", escala="fraca")
    if linhas:
        series.append({"nome": "OpenMP (cíclica)",
                       "pontos": por_eixo(linhas, "threads"), "cor": CORES[0]})

    linhas = filtrar(resultados, versao="mpi", politica="estatico",
                     modo="antecipado", escala="fraca")
    if linhas:
        series.append({"nome": "MPI (estático)",
                       "pontos": por_eixo(linhas, "processos"), "cor": CORES[2]})

    linhas = filtrar(resultados, versao="mpi", politica="mestre-escravo",
                     modo="antecipado", escala="fraca")
    if linhas:
        series.append({"nome": "MPI (mestre-escravo)",
                       "pontos": por_eixo(linhas, "processos"), "cor": CORES[1]})

    if not series:
        return

    base = min(s["pontos"][0][1] for s in series)
    max_p = max(p[0] for s in series for p in s["pontos"])
    series.append({"nome": "ideal (tempo constante)",
                   "pontos": [(1, base), (max_p, base)], "cor": "#777777",
                   "tracejada": True, "marcador": False})

    gerar_svg(DIR_GRAFICOS / "escalabilidade_fraca.svg",
              "Escalabilidade fraca — trabalho por unidade constante",
              "threads / processos", "tempo (s)", series,
              x1=max_p + 0.5, legenda="superior-esquerda")


def grafico_comparativo(resultados, espaco):
    linhas_o = filtrar(resultados, versao="openmp", politica="ciclica",
                       modo="antecipado", escala="forte", espaco=espaco)
    linhas_m = filtrar(resultados, versao="mpi", politica="estatico",
                       modo="antecipado", escala="forte", espaco=espaco)
    if not linhas_o or not linhas_m:
        return

    pontos_o = pontos_speedup(linhas_o, "threads")
    pontos_m = pontos_speedup(linhas_m, "processos")
    max_p = max([p[0] for p in pontos_o + pontos_m])

    series = [
        {"nome": "OpenMP (cíclica)", "pontos": pontos_o, "cor": CORES[0]},
        {"nome": "MPI (estático)", "pontos": pontos_m, "cor": CORES[2]},
        {"nome": "ideal (linear)",
         "pontos": [(p, float(p)) for p in range(1, max_p + 1)],
         "cor": "#777777", "tracejada": True, "marcador": False},
    ]

    gerar_svg(DIR_GRAFICOS / "comparativo_speedup.svg",
              "OpenMP × MPI — speedup (cada um contra o seu T1)",
              "threads / processos", "speedup  (T1 / Tp)", series,
              x1=max_p + 0.5,
              y1=max(max_p, max(p[1] for s in series for p in s["pontos"])) * 1.08,
              legenda="superior-esquerda")


# ----------------------------------------------------------------------------
#  Resumo no terminal (para conferência rápida)
# ----------------------------------------------------------------------------
def imprimir_resumo(resultados):
    print("\n=== Baseline sequencial ===")
    print(f"{'ESPACO':>14} {'tempo (s)':>12} {'chave':>14}")
    for r in filtrar(resultados, versao="sequencial", modo="antecipado"):
        print(f"{r['espaco']:>14} {r['tempo_mediana']:>12.6f} {r['chave']:>14}")

    forte = [r for r in resultados if r["escala"] == "forte"
             and r["modo"] == "antecipado" and r["versao"] in ("openmp", "mpi")]
    if forte:
        print("\n=== Speedup (escalabilidade forte) ===")
        print(f"{'versao':>8} {'politica':>16} {'P':>3} "
              f"{'tempo (s)':>12} {'speedup':>8} {'efic. %':>8}")
        for r in sorted(forte, key=lambda x: (x["versao"], x["politica"],
                                              x["threads"], x["processos"])):
            p = r["processos"] if r["versao"] == "mpi" else r["threads"]
            print(f"{r['versao']:>8} {r['politica']:>16} {p:>3} "
                  f"{r['tempo_mediana']:>12.6f} {r['speedup']:>8.3f} "
                  f"{100.0 * r['speedup'] / p:>8.1f}")


# ----------------------------------------------------------------------------
#  Principal
# ----------------------------------------------------------------------------
def main():
    DIR_GRAFICOS.mkdir(parents=True, exist_ok=True)
    resultados = ler_bruto()
    gravar_resultados(resultados)

    # Espaço principal das curvas fortes: o maior espaço medido no baseline.
    linhas_base = filtrar(resultados, versao="sequencial", modo="antecipado")
    if not linhas_base:
        sys.exit("ERRO: nenhuma medição do baseline encontrada.")
    espaco_forte = max(r["espaco"] for r in linhas_base)

    # Espaço do estudo de políticas (modo completo).
    completos = [r["espaco"] for r in resultados if r["modo"] == "completo"]
    espaco_completo = max(completos) if completos else espaco_forte

    gerado = []
    for funcao, argumentos, nome in [
        (grafico_tempo_openmp, (resultados, espaco_forte), "tempo_openmp.svg"),
        (grafico_speedup_openmp, (resultados, espaco_forte), "speedup_openmp.svg"),
        (grafico_eficiencia_openmp, (resultados, espaco_forte), "eficiencia_openmp.svg"),
        (grafico_speedup_mpi, (resultados, espaco_forte), "speedup_mpi.svg"),
        (grafico_eficiencia_mpi, (resultados, espaco_forte), "eficiencia_mpi.svg"),
        (grafico_politicas, (resultados, espaco_completo), "speedup_politicas.svg"),
        (grafico_escalabilidade_fraca, (resultados,), "escalabilidade_fraca.svg"),
        (grafico_comparativo, (resultados, espaco_forte), "comparativo_speedup.svg"),
    ]:
        funcao(*argumentos)
        caminho = DIR_GRAFICOS / nome
        if caminho.exists():
            gerado.append(nome)

    imprimir_resumo(resultados)

    print(f"\nCSV agregado : {SAIDA_CSV}")
    print(f"Gráficos     : {', '.join(gerado) if gerado else 'nenhum'}")
    print(f"Total de configurações agregadas: {len(resultados)}")


if __name__ == "__main__":
    main()
