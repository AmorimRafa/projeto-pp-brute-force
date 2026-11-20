# Decisão da versão MPI: por que apenas mestre-escravo

> Substitui o antigo `DECISAO_MPI_MODOS.md`, que descrevia duas decomposições.
> O projeto agora tem **um único modo de execução**.

---

## 1. O que o enunciado pede

Na lista de problemas, o item 9 (busca exaustiva) tem como aspecto
interessante: **"mestre-escravo; efeito do término antecipado na medição"**.
Além disso, a frente MPI deve:

* usar decomposição de domínio ou de tarefas justificada;
* usar pelo menos uma operação coletiva;
* analisar o overhead de comunicação em função do número de processos;
* produzir a mesma chave do baseline para qualquer `-np`.

---

## 2. A escolha

O rank 0 é o **mestre**: não busca, apenas distribui blocos e coordena o
término. Os ranks 1..P−1 são **escravos**: recebem um intervalo `[início, fim)`,
varrem e respondem com `PEDIDO` (pedem outro bloco) ou `RESULTADO` (acharam a
chave). Quando o mestre recebe o primeiro `RESULTADO`, envia `PARAR` a todos.

Essa decomposição é a escolha natural porque:

1. o espaço de chaves é facilmente particionável;
2. o custo por chave é uniforme, mas o **término antecipado** torna o trabalho
   total imprevisível — distribuir sob demanda evita que um processo fique
   preso a um bloco gigante;
3. é exatamente o padrão destacado no enunciado.

Não mantivemos uma segunda decomposição (domínio estático): ela responderia a
uma pergunta que a apresentação não precisa responder e adicionaria tags,
`MPI_Iprobe` e uma documentação inteira só para explicar a comparação.

---

## 3. Coletivas usadas

| Coletiva | Para que serve |
|---|---|
| `MPI_Bcast` | Rank 0 espalha espaço, alvo e tamanho do bloco — fonte única da verdade |
| `MPI_Allreduce(MPI_MIN)` | Consolida a menor chave; garante a semântica de "primeira ocorrência" do baseline |

O `MPI_Reduce(SUM)` que existia apenas para somar contagens de mensagens foi
removido: **toda mensagem passa pelo mestre**, então ele mesmo conta envios e
recebimentos. A coluna `mensagens` do CSV já mede o overhead de comunicação.

---

## 4. Custo estrutural (ponto importantíssimo na arguição)

Com `P` processos, apenas `P−1` fazem trabalho útil:

```
T(P) ≈ T(1) / (P − 1)
speedup ≈ P − 1
eficiência ≈ (P − 1) / P
```

| P | Teto do speedup | Teto da eficiência |
|---:|---:|---:|
| 2 | 1× | 50,0% |
| 4 | 3× | 75,0% |
| 8 | 7× | 87,5% |
| 12 | 11× | 91,7% |

Isso **não é defeito de implementação**: é propriedade da decomposição pedida.
Na medição real, `P = 2` fica abaixo de 1× porque um núcleo é gasto apenas com
coordenação e ainda há o custo das mensagens. O afastamento do teto se deve a
comunicação, granularidade da parada e hardware/SMT.

---

## 5. Protocolo ponto a ponto

| Tag | Direção | Conteúdo |
|---|---|---|
| `BLOCO` | mestre → escravo | 2 × `uint64`: `[início, fim)` |
| `PEDIDO` | escravo → mestre | 0 bytes: "terminei, me dê outro" |
| `RESULTADO` | escravo → mestre | 1 × `uint64`: chave encontrada |
| `PARAR` | mestre → escravo | 0 bytes: encerrar |

As mensagens são pequenas (no máximo 16 bytes) e usam o protocolo *eager* do
MPI: não existe espera circular de buffer, portanto não há deadlock.
