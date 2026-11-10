# Por que a versão MPI tem dois modos?

> Documento de decisão técnica. Complementa a seção de MPI do `README.md`.
> Responsável pela frente: **[NOME 3 — MPI]**.

---

## 1. Contexto

O problema 9 (busca exaustiva) é descrito na documentação da disciplina como um
caso de **mestre-escravo**: existe um espaço de chaves particionável e um
critério de parada que pode ocorrer antes de varrer todo o espaço. A implementação
em MPI precisa:

1. decompor o trabalho entre processos;
2. usar pelo menos uma operação coletiva;
3. manter o **mesmo resultado** do baseline para qualquer número de processos;
4. permitir medir speedup e discutir o overhead de comunicação.

Há duas formas naturais de decompor o problema, e cada uma responde a uma
pergunta diferente da análise experimental. Por isso a versão MPI implementa
**as duas**, selecionáveis por `--modo=`.

---

## 2. Modo `mestre-escravo` (padrão)

### 2.1 Funcionamento

- O rank 0 é o **mestre**: não varre chaves, apenas distribui blocos e coordena
  o término.
- Os ranks 1..P−1 são **escravos**: pedem um bloco, varrem o intervalo, e
  devolvem `PEDIDO` (quero mais trabalho) ou `RESULTADO` (encontrei a chave).
- Os blocos são entregues **em ordem crescente** e sob demanda, o que dá
  balanceamento automático mesmo se o custo por chave variar.
- Ao receber o primeiro `RESULTADO`, o mestre envia `PARAR` a todos e o
  resultado final é consolidado com `MPI_Allreduce(MPI_MIN)`.

### 2.2 Custo estrutural: um processo ocioso

Com `P` processos, apenas `P−1` realizam trabalho útil. No melhor caso
(ignorando comunicação):

```
T(P) ≈ T(1) / (P − 1)        ⇒        S(P) ≈ P − 1
E(P) = S(P) / P ≈ (P − 1) / P
```

Consequências práticas:

| P | Speedup máximo | Eficiência máxima |
|---:|---:|---:|
| 2 | 1,00× | 50,0% |
| 4 | 3,00× | 75,0% |
| 8 | 7,00× | 87,5% |
| 12 | 11,00× | 91,7% |

Ou seja: **a curva do mestre-escravo nunca encosta na reta ideal** — e isso não
é um defeito de implementação, é uma propriedade da decomposição. É exatamente
esse o tipo de afastamento que o Marco 2 pede para explicar (junto com o
overhead de comunicação e o tempo de inicialização do MPI).

Além disso, há um segundo custo: o mestre é um ponto de serialização. Cada
bloco gera pelo menos duas mensagens (pedido + resposta) e o mestre processa
esses pedidos sequencialmente. Com blocos grandes o bastante, esse custo é
amortizado; com blocos pequenos, o mestre vira gargalo.

### 2.3 Quando é a escolha certa

- Quando o **custo por unidade de trabalho é variável** ou imprevisível
  (balanceamento dinâmico vale mais que a perda de um processo).
- Quando o problema é naturalmente uma **fila de tarefas** (não é o caso aqui,
  mas é o padrão clássico).
- Quando se quer demonstrar o padrão descrito pelo enunciado do problema 9.

---

## 3. Modo `estatico`

### 3.1 Funcionamento

- **Todos** os ranks participam da busca, incluindo o rank 0.
- Decomposição de domínio **cíclica em blocos**: o espaço é dividido em
  blocos de `--intervalo` chaves (padrão 65.536) e o rank `r` processa os
  blocos `r, r+P, r+2P, ...`, em ordem crescente.
- Periodicamente, cada rank verifica com `MPI_Iprobe` se recebeu uma mensagem
  `PARAR` de outro rank. Quem encontra a chave envia `PARAR` a todos os demais
  e o resultado é consolidado com `MPI_Allreduce(MPI_MIN)`.
- A verificação é não bloqueante e acontece uma vez por bloco, então o custo
  no caminho quente é desprezível.

> **Por que blocos e não "uma chave por rank" (`k = r + i·P`)?** A versão
> inicial usava a intercalação por chave, mas o laço com passo variável impede
> o compilador de desenrolar e de sobrepor as multiplicações do hash: a versão
> por chave chegou a ser **30% mais lenta** que a versão OpenMP com 1 thread,
> o que falsearia a comparação entre as implementações. A versão em blocos
> mantém o laço interno contíguo (`k++`), igual ao da versão OpenMP, e ainda
> preserva o balanceamento (todos os ranks avançam juntos e param com
> diferença de no máximo um bloco).

### 3.2 Custo estrutural: nenhum processo ocioso

```
T(P) ≈ T(1) / P              ⇒        S(P) ≈ P
E(P) ≈ 1
```

O limite prático passa a ser: overhead do `MPI_Iprobe`, latência da mensagem
de parada (no máximo um intervalo de chaves), tempo de inicialização/finalização
do MPI e a fração serial do programa (Bcast, Allreduce e impressão).

### 3.3 Quando é a escolha certa

- Quando o custo por chave é **uniforme** (é o nosso caso: o hash custa o mesmo
  para qualquer chave).
- Quando se quer a **melhor escalabilidade** e a curva mais próxima da ideal.
- Quando se quer medir **escalabilidade fraca** de forma limpa: basta fazer
  `ESPACO = base × P` para manter o trabalho por processo constante.

---

## 4. Por que não escolher apenas um?

| Pergunta da análise | Modo que responde |
|---|---|
| O padrão mestre-escravo do enunciado funciona? | `mestre-escravo` |
| Qual o custo de dedicar um processo à coordenação? | Comparação entre os dois |
| A decomposição de domínio escala melhor? | `estatico` |
| O overhead de comunicação cresce com P? | Mensagens e tempos medidos em ambos |
| Por que a curva não é linear? | Eficiência `(P−1)/P`, latência de parada, startup, Amdahl |
| O resultado muda com o modo ou com P? | Validação: mesma chave nos dois modos |

Implementar os dois custa pouco (o núcleo da busca é o mesmo) e transforma uma
afirmação genérica ("MPI acelera") em uma **comparação experimental com causa
identificada**. É exatamente o que a rubrica do Marco 2 chama de "análise
experimental e discussão do afastamento em relação à Lei de Amdahl".

---

## 5. Protocolo de mensagens (mestre-escravo)

| Tag | Direção | Conteúdo | Significado |
|---|---|---|---|
| `BLOCO` | mestre → escravo | 2 × `uint64` | intervalo `[inicio, fim)` a varrer |
| `PEDIDO` | escravo → mestre | 0 bytes | "terminei o bloco, me dê outro" |
| `RESULTADO` | escravo → mestre | 1 × `uint64` | chave encontrada (parada antecipada) |
| `PARAR` | mestre → escravo | 0 bytes | encerrar imediatamente |
| `FIM` | mestre → escravo | 0 bytes | não há mais blocos (varredura completa) |

Regras de encerramento:

1. **Com término antecipado**: o mestre encerra ao receber o primeiro
   `RESULTADO`; envia `PARAR` a todos; em seguida todos chamam
   `MPI_Allreduce(MIN)`.
2. **Sem término antecipado (`--completo`)**: o mestre distribui todos os
   blocos; cada escravo que esgota recebe `FIM`; quando todos terminaram, o
   `MPI_Allreduce(MIN)` consolida o menor resultado.
3. **Sem solução**: `MPI_Allreduce` devolve o sentinela e o programa imprime
   `chave=-1`, igual ao baseline.

Mensagens de 0 byte e de 8 bytes usam o protocolo *eager* do MPI; não há
dependência circular de buffers que possa causar deadlock. Mesmo que um escravo
envie um `PEDIDO` depois de o mestre já ter saído do laço, a mensagem fica
enfileirada e é ignorada — o escravo recebe o `PARAR` que já estava a caminho.

---

## 6. O que esperamos medir

- **Mestre-escravo**: speedup ≈ `P−1` (menos overhead), eficiência subindo de
  50% (P=2) para ~90% (P=12). É a curva que **não** deve ser comparada com a
  reta ideal sem explicação.
- **Estático**: speedup próximo de `P`, com queda suave em P alto por
  inicialização, `Allreduce` e SMT.
- **Tempo de comunicação**: cresce com P no mestre-escravo (mais mensagens e
  mais participantes), mas permanece uma fração pequena do tempo total com
  blocos de ~128 por escravo.
- **Validação**: a chave é a mesma nos dois modos e em todos os `-np`,
  confirmando que a decomposição não altera o resultado.

### 6.1 O que foi medido de fato (ver `README.md`, seção 7)

| P | mestre-escravo | estático | mensagens (mestre-escravo) |
|---:|---:|---:|---:|
| 1 | 0,98× | 0,96× | 0 |
| 2 | **0,91×** | 1,72× | 174 |
| 4 | 2,06× | 2,49× | 520 |
| 8 | 3,42× | 3,39× | 1.224 |
| 12 | **3,66×** | **4,51×** | 1.908 |

Dois pontos merecem destaque na defesa:

1. **Com P = 2 o mestre-escravo é mais lento que o sequencial (0,91×).** Com
   um único escravo, o mestre ocupa um núcleo sem buscar e ainda existe o
   custo de coordenação. É a demonstração mais direta de que a *decomposição*
   — e não apenas a contagem de processos — determina o speedup.
2. **O modo estático supera o mestre-escravo em P = 4 e P = 12.** O motivo
   medido é o volume de mensagens (1.908 contra 22 com P = 12): cada bloco
   entregue pelo mestre exige um par pedido/resposta, e o mestre vira um ponto
   de serialização.

Os números ficam em `dados/resultados.csv` e nos gráficos de `graficos/`; a
discussão completa (inclusive o efeito do hardware) está no `README.md`,
seções 7 e 8.
