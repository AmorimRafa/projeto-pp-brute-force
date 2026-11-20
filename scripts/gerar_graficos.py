#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Agrega dados/bruto.csv e gera dados/resultados.csv + 4 gráficos SVG.

Usa somente a biblioteca padrão do Python: o SVG é escrito como texto, então
não é preciso instalar matplotlib nem outra dependência no laboratório.

Para quem está começando:
  * csv.DictReader lê o CSV e entrega cada linha como um dicionário
    (ex.: linha["tempo_busca"]);
  * statistics.median pega o valor do meio das 5 repetições, que não é
    puxado por um único pico de ruído do sistema;
  * um SVG nada mais é que um arquivo de texto com comandos de desenho
    (<line>, <circle>, <polyline>) que o navegador sabe interpretar.
"""
import csv
import statistics
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
BRUTO = RAIZ / "dados" / "bruto.csv"
RESULTADOS = RAIZ / "dados" / "resultados.csv"
GRAFICOS = RAIZ / "graficos"


def agrupar():
    """Agrupa execuções por (versão, escala, threads/processos, espaço).

    O espaço entra na chave porque o baseline é medido em três tamanhos
    diferentes; sem isso suas medianas seriam misturadas numa linha só.
    """
    grupos = {}
    with BRUTO.open(newline="", encoding="utf-8") as arquivo:
        for linha in csv.DictReader(arquivo):
            if linha["versao"] == "openmp":
                n = int(linha["threads"])
            elif linha["versao"] == "mpi":
                n = int(linha["processos"])
            else:
                n = 1
            chave = (linha["versao"], linha["escala"], n, int(linha["espaco"]))
            grupos.setdefault(chave, []).append(linha)
    return grupos


def resumir(grupos):
    """Calcula mediana, speedup T1/Tp e eficiência para cada configuração."""
    linhas = []
    for (versao, escala, n, espaco), execucoes in sorted(grupos.items()):
        tempos = [float(e["tempo_busca"]) for e in execucoes]
        mensagens = [int(e["mensagens"]) for e in execucoes]
        linhas.append({
            "versao": versao, "escala": escala, "n": n,
            "espaco": espaco,
            "chave": execucoes[0]["chave"],
            "repeticoes": len(execucoes),
            "tempo": statistics.median(tempos),
            "minimo": min(tempos),
            "maximo": max(tempos),
            "mensagens": statistics.median(mensagens),
            "speedup": "", "eficiencia": "",
        })

    # Referência: o T1 da própria versão, medido na mesma curva forte.
    referencia = {
        l["versao"]: l["tempo"]
        for l in linhas if l["escala"] == "forte" and l["n"] == 1
    }
    for linha in linhas:
        if linha["escala"] == "forte" and linha["versao"] != "sequencial":
            t1 = referencia.get(linha["versao"])
            if t1:
                linha["speedup"] = t1 / linha["tempo"]
                linha["eficiencia"] = 100 * linha["speedup"] / linha["n"]
    return linhas


def escrever_resultados(linhas):
    colunas = ["versao", "escala", "n", "espaco", "chave", "repeticoes",
               "tempo_mediano", "tempo_minimo", "tempo_maximo",
               "speedup", "eficiencia", "mensagens"]
    with RESULTADOS.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.writer(arquivo)
        escritor.writerow(colunas)
        for l in linhas:
            escritor.writerow([
                l["versao"], l["escala"], l["n"], l["espaco"], l["chave"],
                l["repeticoes"], f"{l['tempo']:.6f}", f"{l['minimo']:.6f}",
                f"{l['maximo']:.6f}",
                f"{l['speedup']:.3f}" if l["speedup"] != "" else "",
                f"{l['eficiencia']:.1f}" if l["eficiencia"] != "" else "",
                f"{l['mensagens']:.0f}",
            ])


def grafico(nome, titulo, eixo_x, eixo_y, series, formato="{:.2f}"):
    """Desenha um gráfico de linhas simples em SVG.

    Cada série é (rótulo, cor, pontos, tracejado), onde pontos é [(x, y), ...].
    """
    todos = [ponto for _, _, pontos, _ in series for ponto in pontos]
    if not todos:
        return

    largura, altura, margem = 660, 380, 60
    xmax = max(max(p[0] for p in pontos) for _, _, pontos, _ in series)
    ymax = max(max(p[1] for p in pontos) for _, _, pontos, _ in series)
    xmax = max(xmax, 1)
    ymax = max(ymax * 1.15, 0.001)

    def X(v):
        return margem + v / xmax * (largura - 2 * margem)

    def Y(v):
        return altura - margem - v / ymax * (altura - 2 * margem)

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{largura}" '
        f'height="{altura}" viewBox="0 0 {largura} {altura}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{largura/2:.0f}" y="28" text-anchor="middle" '
        f'font-family="Arial" font-size="17" font-weight="bold">{titulo}</text>',
    ]

    # Grade horizontal e rótulos do eixo Y.
    for i in range(5):
        valor = ymax * i / 4
        y = Y(valor)
        svg.append(f'<line x1="{margem}" y1="{y:.1f}" x2="{largura-margem}" '
                   f'y2="{y:.1f}" stroke="#e0e0e0"/>')
        svg.append(f'<text x="{margem-8}" y="{y+4:.1f}" text-anchor="end" '
                   f'font-family="Arial" font-size="11">{formato.format(valor)}</text>')

    # Eixos.
    svg.append(f'<line x1="{margem}" y1="{altura-margem}" x2="{largura-margem}" '
               f'y2="{altura-margem}" stroke="black"/>')
    svg.append(f'<line x1="{margem}" y1="{margem}" x2="{margem}" '
               f'y2="{altura-margem}" stroke="black"/>')
    svg.append(f'<text x="{largura/2:.0f}" y="{altura-18}" text-anchor="middle" '
               f'font-family="Arial" font-size="13">{eixo_x}</text>')
    svg.append(f'<text x="16" y="{altura/2:.0f}" text-anchor="middle" '
               f'font-family="Arial" font-size="13" transform="rotate(-90 16 {altura/2:.0f})">'
               f'{eixo_y}</text>')

    # Séries.
    for rotulo, cor, pontos, tracejado in series:
        pontos_ordenados = sorted(pontos)
        coords = " ".join(f"{X(x):.1f},{Y(y):.1f}" for x, y in pontos_ordenados)
        trace = ' stroke-dasharray="6,4"' if tracejado else ""
        svg.append(f'<polyline points="{coords}" fill="none" stroke="{cor}" '
                   f'stroke-width="2.5"{trace}/>')
        for x, y in pontos_ordenados:
            svg.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="4" fill="{cor}"/>')
        svg.append(f'<text x="{X(pontos_ordenados[-1][0])+8:.1f}" '
                   f'y="{Y(pontos_ordenados[-1][1])-8:.1f}" font-family="Arial" '
                   f'font-size="11" fill="{cor}">{rotulo}</text>')

    # Rótulos do eixo X (valores de threads/processos medidos).
    for x in sorted({ponto[0] for ponto in todos}):
        svg.append(f'<text x="{X(x):.1f}" y="{altura-margem+18}" text-anchor="middle" '
                   f'font-family="Arial" font-size="11">{x}</text>')

    svg.append("</svg>")
    (GRAFICOS / nome).write_text("\n".join(svg), encoding="utf-8")


def main():
    GRAFICOS.mkdir(exist_ok=True)
    linhas = resumir(agrupar())
    escrever_resultados(linhas)

    forte = [l for l in linhas if l["escala"] == "forte"]
    fraca = [l for l in linhas if l["escala"] == "fraca"]

    def serie(versao, campo, escala=forte):
        return sorted((l["n"], l[campo]) for l in escala
                      if l["versao"] == versao and l[campo] != "")

    ns = [n for n, _ in serie("openmp", "tempo")]

    # O baseline foi medido em três tamanhos; o gráfico de escalabilidade
    # forte deve usar o mesmo ESPACO das curvas paralelas.
    espaco_forte = next((l["espaco"] for l in forte
                         if l["versao"] == "openmp"), None)
    base = [l["tempo"] for l in forte
            if l["versao"] == "sequencial" and l["espaco"] == espaco_forte]
    tempo_base = base[0] if base else 0.0

    grafico("tempo.svg", "Tempo da região de busca — escalabilidade forte",
            "threads (OpenMP) / processos (MPI)", "tempo (s)", [
                ("sequencial", "#2ca02c", [(n, tempo_base) for n in ns], True),
                ("openmp", "#1f77b4", serie("openmp", "tempo"), False),
                ("mpi", "#d62728", serie("mpi", "tempo"), False),
            ])

    grafico("speedup.svg", "Speedup — escalabilidade forte",
            "threads (OpenMP) / processos (MPI)", "speedup (T1/Tp)", [
                ("ideal (T1/Tp = P)", "#888888", [(n, n) for n in ns], True),
                ("openmp", "#1f77b4", serie("openmp", "speedup"), False),
                ("mpi", "#d62728", serie("mpi", "speedup"), False),
                ("teto mestre-escravo (P-1)", "#ff7f0e",
                 [(n, n - 1) for n in ns if n > 1], True),
            ], "{:.1f}")

    grafico("eficiencia.svg", "Eficiência — escalabilidade forte",
            "threads (OpenMP) / processos (MPI)", "eficiência (%)", [
                ("openmp", "#1f77b4",
                 [(n, e) for n, e in serie("openmp", "eficiencia")], False),
                ("mpi", "#d62728",
                 [(n, e) for n, e in serie("mpi", "eficiencia")], False),
                ("teto mestre-escravo ((P-1)/P)", "#ff7f0e",
                 [(n, 100 * (n - 1) / n) for n in ns if n > 1], True),
            ], "{:.0f}%")

    grafico("escalabilidade_fraca.svg", "Escalabilidade fraca — tempo por unidade constante",
            "threads (OpenMP) / processos (MPI)", "tempo (s)", [
                ("openmp", "#1f77b4", serie("openmp", "tempo", fraca), False),
                ("mpi", "#d62728", serie("mpi", "tempo", fraca), False),
            ])

    print(f"OK: {RESULTADOS.relative_to(RAIZ)} e 4 gráficos em graficos/")


if __name__ == "__main__":
    main()
