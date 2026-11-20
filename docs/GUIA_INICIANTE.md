# Guia para iniciantes — como o projeto funciona do início ao fim

> **Para quem é este arquivo:** para quem nunca mexeu com C++ e nunca
> paralelizou um programa. Ele foi escrito para ser lido **com o código aberto
> do lado** e responde a três perguntas: *o que o projeto faz*, *como ele
> funciona* e *onde aparecem os resultados*.
>
> O `README.md` traz os números e a defesa da arguição. Este guia traz o
> caminho, passo a passo, sem pular nada — inclusive coisas que parecem
> óbvias para quem já programa.

---

## Índice

1. [O que o projeto faz, em uma frase](#1-o-que-o-projeto-faz-em-uma-frase)
2. [O problema em linguagem simples](#2-o-problema-em-linguagem-simples)
3. [As três versões](#3-as-três-versões)
4. [Vocabulário mínimo de C++](#4-vocabulário-mínimo-de-c)
5. [Vocabulário mínimo de paralelização](#5-vocabulário-mínimo-de-paralelização)
6. [O fluxo completo, passo a passo](#6-o-fluxo-completo-passo-a-passo)
7. [Como os resultados aparecem](#7-como-os-resultados-aparecem)
8. [Mapa de arquivos](#8-mapa-de-arquivos)
9. [Como saber que está certo](#9-como-saber-que-está-certo)
10. [Roteiro para apresentar em 5 minutos](#10-roteiro-para-apresentar-em-5-minutos)
11. [Perguntas e respostas curtas](#11-perguntas-e-respostas-curtas)

---

## 1. O que o projeto faz, em uma frase

Ele encontra **a mesma chave que o programa do professor encontra**, só que
mais rápido, usando várias unidades de processamento ao mesmo tempo.

O programa do professor (o *baseline*) procura um número entre `0` e
`ESPACO - 1`, um por um. Com `ESPACO = 4.800.000.000` ele gasta cerca de
**1,35 segundo**. O nosso, com 12 threads, gasta **0,236 segundo**. A resposta
é idêntica nos dois.

---

## 2. O problema em linguagem simples

### 2.1 A "máquina de misturar números" (hash)

A função `hash_chave(k)` é uma caixa-preta:

```
entra um número k  ->  [ mistura os bits ]  ->  sai outro número de 64 bits
```

Ela tem duas propriedades importantes:

* **Determinística:** o mesmo `k` sempre gera a mesma saída.
* **Mistura bem:** números parecidos (como `100` e `101`) geram saídas bem
  diferentes. É como uma impressão digital do número.

O código da função está em `src/comum/busca.hpp` e é uma **cópia exata** da
função `h()` do baseline. Não inventamos nada: se mudássemos um único sinal,
a resposta mudaria.

### 2.2 O alvo

O professor escolhe o número procurado assim:

```c
alvo = hash(ESPACO / 3);
```

Ou seja: ele pega o número que está um terço do caminho, joga na máquina de
misturar e guarda o resultado. Isso é uma sacada inteligente porque:

1. **garante que sempre existe resposta** — a chave `ESPACO/3` com certeza
   produz aquele alvo;
2. **coloca a resposta no meio do caminho** — nem no começo (fácil demais),
   nem no fim (demorado demais).

Exemplo real:

```
ESPACO = 60000000
ESPACO / 3 = 20000000
alvo = hash(20000000) = 14850082816872249796
percorrendo k = 0, 1, 2, 3, ... o primeiro que bate é k = 20000000
```

### 2.3 Por que isso se chama "força bruta"

Porque o programa não tem atalho: ele testa `k = 0`, depois `1`, depois `2`...
até encontrar. É a busca exaustiva. O único "truque" é o
**término antecipado**: parar no primeiro acerto, sem terminar de varrer o
espaço.

### 2.4 O que muda quando paralelizamos

Se uma pessoa testa 100 chaves por segundo, quatro pessoas testando ao mesmo
tempo deveriam testar 400. A ideia é essa. O cuidado está em duas coisas:

* **não deixar ninguém ocioso:** se déssemos a cada pessoa um pedaço gigante,
  quem ficasse com o pedaço errado trabalharia muito mais. Por isso dividimos
  em **blocos pequenos e entregues sob demanda**;
* **não errar a resposta:** quando alguém acha, todos precisam parar e o
  resultado tem que ser o mesmo de sempre. Aqui entram `atomic`, `critical`
  e `Allreduce`, explicados mais adiante.

---

## 3. As três versões

| Versão | Arquivo | Quem faz o trabalho | Campo de tempo na saída |
|---|---|---|---|
| Sequencial | `docs/baseline_sequencial.cpp` | 1 processo, 1 laço | `tempo` |
| OpenMP | `src/openmp/openmp.cpp` | várias **threads** dentro de 1 processo | `tempo_busca` |
| MPI | `src/mpi/mpi.cpp` | vários **processos** (rank 0 coordena) | `tempo_busca` |

**O que é idêntico nas três:** a função de hash, o cálculo do alvo, a ordem da
varredura (`k` crescente), a regra de parar no primeiro acerto e a chave
impressa.

**O que muda:** apenas *quem* percorre os números e *como* eles recebem cada
pedaço do trabalho.

O baseline **não foi alterado**. Ele fica em `docs/` e é compilado direto pelo
Makefile. Ele é o gabarito: se as outras versões divergissem dele, estaríamos
errados — não ele.

---

## 4. Vocabulário mínimo de C++

Não precisa decorar nada disso. Serve só para você não travar quando ler o
código.

| O que aparece | O que significa, em português |
|---|---|
| `#include <cstdint>` | "importe esta biblioteca". É o equivalente a `import` em Python |
| `namespace busca { ... }` | uma "pasta" de nomes. Para chamar algo de dentro, escrevemos `busca::agora()` |
| `uint64_t` | número inteiro **sem sinal** de 64 bits: vai de 0 a ~1,8×10¹⁹ |
| `double` | número com casa decimal, usado para o tempo |
| `constexpr` | "esse valor pode ser calculado enquanto compila, não precisa calcular na hora" |
| `inline` | "pode colocar o código desta função aqui mesmo"; evita erro quando o `.hpp` é incluído em vários `.cpp` |
| `struct` | um pacote de variáveis com nome. `Configuracao` guarda espaço, alvo, threads... |
| `std::atomic<bool>` | um booleano com acesso seguro entre threads (explicado na seção 5) |
| `Configuracao& cfg` | o `&` é uma **referência**: a função mexe na variável original, não numa cópia |
| `argc`, `argv` | dados que o sistema operacional entrega ao `main`: quantos argumentos e quais são |
| `cond ? sim : nao` | um `if` curto: "se cond, pegue `sim`; senão, pegue `nao`" |
| `nullptr` | "não aponta para nada" |
| `printf("chave=%lld", x)` | imprime texto com espaço para variáveis; `%lld` é o código para número inteiro longo |
| `PRIu64` | o código do `printf` para `uint64_t` (o nome muda entre sistemas) |
| `enum Tag { ... }` | dar nomes a números: `TAG_PARAR` vale 4, mas é mais legível |
| `static_cast<uint64_t>(x)` | "converta `x` para `uint64_t` de forma explícita" |
| `for (;;)` | laço sem condição: só sai com um `break` |
| `break` / `continue` | `break` sai do laço; `continue` pula para a próxima volta |

Exemplo de `argc`/`argv` (importantíssimo para entender o `main`):

```
comando:  ./bin/openmp 60000000 --threads=4
argc   =  3
argv[0] = "./bin/openmp"     <- o próprio programa
argv[1] = "60000000"
argv[2] = "--threads=4"
```

O `ler_argumentos()` percorre `argv` e preenche a `struct Configuracao`.

---

## 5. Vocabulário mínimo de paralelização

### 5.1 Thread x processo

* **Thread** é como uma pessoa trabalhando **na mesma sala**, dividindo a
  mesma mesa (a memória). É o OpenMP. Vantagem: conversar é rápido. Cuidado:
  duas pessoas podem escrever no mesmo papel ao mesmo tempo (corrida).
* **Processo** é como uma pessoa em **sala separada**, com sua própria mesa.
  É o MPI. Ninguém vê o papel do outro: para trocar informação, é preciso
  mandar bilhetes (mensagens). Cada processo tem um **rank** (um crachá).

### 5.2 Como cada biblioteca é usada aqui

| | OpenMP | MPI |
|---|---|---|
| Compilação | `g++ -fopenmp` | `mpicxx` |
| Lançamento | `./bin/openmp 60000000 --threads=4` | `mpirun -np 4 ./bin/mpi 60000000` |
| Como pedimos 4 unidades | `--threads=4` | `-np 4` |
| Como dividimos o trabalho | `#pragma omp for schedule(dynamic, 1)` | mestre envia blocos por mensagem |

### 5.3 Palavras que aparecem na apresentação

| Termo | Tradução livre |
|---|---|
| **Speedup** | "quantas vezes ficou mais rápido": `T1 / Tp`. Se com 1 thread leva 1,0 s e com 4 leva 0,25 s, o speedup é 4 |
| **Eficiência** | `speedup / número de unidades`. Mede o aproveitamento: 100% seria perfeito |
| **Escalabilidade forte** | problema do **mesmo tamanho**, aumentamos as unidades. Queremos o tempo cair |
| **Escalabilidade fraca** | aumentamos o problema **junto com** as unidades, mantendo o trabalho por unidade constante. Queremos o tempo ficar igual |
| **Condição de corrida** | duas threads mexendo no mesmo dado sem combinação; o resultado depende de quem chega primeiro |
| **`atomic`** | operação indivisível: "ou faz tudo, ou não faz nada". Usamos no booleano `parar` |
| **`critical`** | "só uma thread por vez neste trecho do código". Usamos para juntar os resultados |
| **`reduction`** | recurso do OpenMP para juntar valores automaticamente. **Não usamos** porque ele exige que o laço termine sempre, e nós paramos antes |
| **False sharing** | duas threads disputam a mesma linha de cache por escreverem em variáveis vizinhas. **Não acontece aqui**: cada thread escreve só na sua variável local |
| **Lei de Amdahl** | diz que o ganho é limitado pela parte do programa que **não** pode ser paralelizada. Como 100% do nosso tempo está no laço do hash, o teto teórico é o ideal |
| **Mestre-escravo** | padrão em que um processo coordena e os outros trabalham |
| **Rank** | o número do processo no MPI (0, 1, 2...) |
| **Tag** | rótulo da mensagem MPI: diz se é um bloco, um pedido, um resultado ou um "pare" |
| **Coletiva** | operação em que **todos** os processos participam |

---

## 6. O fluxo completo, passo a passo

Vamos seguir **dois exemplos concretos**, do comando digitado até a linha
impressa.

### 6.1 Antes de tudo: entrar no container e rodar o `make`

Primeiro abrimos o ambiente Linux. A imagem Docker **já traz o código copiado**
dentro de `/projeto`:

```bash
docker build -t pp-busca -f docker/Dockerfile .
docker run --rm -it pp-busca bash
```

Se `/projeto` aparecer vazio, reconstrua a imagem com o `docker build` acima
(imagens antigas não copiavam o código). Para editar no Windows e ver as
mudanças sem reconstruir, use o comando de montagem do seu terminal — está no
`README.md`, seção 2. No Git Bash, o `MSYS_NO_PATHCONV=1` é obrigatório.

Agora sim, dentro do container:

```
você digita `make`
        |
        +--> g++ compila docs/baseline_sequencial.cpp  -> bin/sequencial
        +--> g++ -fopenmp compila src/openmp/openmp.cpp -> bin/openmp
        +--> mpicxx compila src/mpi/mpi.cpp             -> bin/mpi
```

"Compilar" significa traduzir o código escrito em C++ para um programa que o
computador executa. Depois do `make`, temos três arquivos executáveis em
`bin/`.

### 6.2 Exemplo A: OpenMP com 4 threads

Comando:

```bash
./bin/openmp 60000000 --threads=4
```

| # | O que acontece | Onde está no código |
|---:|---|---|
| 1 | O sistema chama `main` com `argc=3` e os três `argv` | `main()` |
| 2 | `ler_argumentos` preenche `cfg`: `espaco=60000000`, `threads=4`, `alvo=hash(20000000)` | `busca.hpp` |
| 3 | Calculamos `blocos = 60000000 / 16384 = 3663` (arredondado para cima) | `total_de_blocos()` |
| 4 | Criamos a flag `parar = false` e `melhor = "ainda não achei"` | `main()` |
| 5 | `inicio = agora()` liga o cronômetro | `busca.hpp` |
| 6 | **4 threads são criadas**. Cada uma executa o bloco entre `{ }` do `parallel`. Cada uma ganha sua própria `melhor_local` | `#pragma omp parallel` |
| 7 | O `for` vira uma **fila de 3663 blocos**. `dynamic, 1` significa: "quem acabou, pega o próximo bloco da fila, um de cada vez" | `#pragma omp for schedule(dynamic, 1)` |
| 8 | Para cada bloco `b`, calculamos `inicio_bloco = b*16384` e `fim_bloco` (o último pode ser menor). O laço interno testa `hash_chave(k) == alvo` | laço `for (k...)` |
| 9 | A thread que chega em `k = 20000000` guarda esse valor, escreve `parar = true` e sai do laço interno com `break` | `if (hash_chave(k) == alvo)` |
| 10 | As outras threads, ao pegarem seu próximo bloco, veem `parar == true` e usam `continue`: pulam sem trabalhar | `if (parar.load(...)) continue` |
| 11 | Cada thread entra **uma vez** no `critical` e compara seu resultado com o global. Menor índice = primeira ocorrência | `#pragma omp critical` |
| 12 | `fim = agora()`: `tempo_busca = fim - inicio` | `main()` |
| 13 | Uma única linha é impressa na tela | `imprimir_resultado()` |

**Por que 16.384 chaves por bloco?** Se o bloco fosse gigante, a thread que
ficasse com ele perderia muito tempo mesmo depois que outra já tivesse achado.
Se fosse minúsculo, a thread passaria mais tempo "pegando na fila" do que
calculando hash. 16.384 é um meio-termo simples.

**Onde está a corrida?** Na variável `parar`. Sem `atomic`, uma thread poderia
escrever `true` enquanto outra lia, e o compilador poderia ter guardado uma
cópia antiga em cache. Com `std::atomic<bool>`, o acesso é seguro. O
`critical` resolve o segundo ponto de disputa: o `melhor` global.

### 6.3 Exemplo B: MPI com 4 processos

Comando:

```bash
mpirun -np 4 ./bin/mpi 60000000
```

Aqui acontece uma coisa que assusta quem é iniciante: **o mesmo programa
começa a rodar 4 vezes**. Cada cópia recebe um crachá (`rank`) de 0 a 3.

| # | O que acontece | Onde está no código |
|---:|---|---|
| 1 | `mpirun` cria 4 cópias. `MPI_Init` abre o ambiente | `MPI_Init()` |
| 2 | Cada cópia descobre seu `rank` e que existem 4 processos | `MPI_Comm_rank/size` |
| 3 | Todas leem os argumentos e calculam `cfg` | `ler_argumentos()` |
| 4 | Todas calculam o mesmo tamanho de bloco: `60000000/(3*128) = 156250`. (Usamos `P-1 = 3` porque quem busca são os 3 escravos) | `main()` |
| 5 | **Coletiva 1:** o rank 0 fala alto e todos copiam `[espaco, alvo, bloco]` | `MPI_Bcast` |
| 6 | Papéis se separam: rank 0 vira **mestre**; ranks 1, 2 e 3 viram **escravos** | `if / else if / else` |
| 7 | O mestre manda um bloco para cada escravo: `[0,156250)`, `[156250,312500)`, `[312500,468750)` | `enviar_bloco()` |
| 8 | Cada escravo recebe seu bloco e varre. Se não acha, manda um `PEDIDO` (bilhete vazio) e espera outro bloco | `escravo()` |
| 9 | O mestre fica num laço recebendo bilhetes **de qualquer um** (`MPI_ANY_SOURCE`) e responde: ou com um novo bloco, ou com `PARAR` | `mestre()` |
| 10 | A chave 20000000 está no bloco 128 (`20000000 / 156250 = 128`). Quem estiver com ele manda `RESULTADO` | `TAG_RESULTADO` |
| 11 | O mestre guarda a chave, quebra o laço e manda `PARAR` para **todos** — alguns escravos podem estar parados esperando um bloco que não virá mais | `if (encontrou)` |
| 12 | Os escravos recebem `PARAR` e saem do laço infinito `for (;;)` | `escravo()` |
| 13 | **Coletiva 2:** `MPI_Allreduce` junta as respostas e todo mundo fica sabendo da menor chave. Usar `MIN` garante a ideia de "primeira ocorrência" | `MPI_Allreduce` |
| 14 | **Só o rank 0 imprime.** Se todos imprimissem, apareceriam 4 linhas iguais | `if (rank == 0)` |

**O que é uma mensagem na prática?** Um envelope com rótulo:

| Rótulo | De quem para quem | Conteúdo |
|---|---|---|
| `BLOCO` | mestre → escravo | 2 números: início e fim do intervalo |
| `PEDIDO` | escravo → mestre | nada (só o rótulo já diz "estou livre") |
| `RESULTADO` | escravo → mestre | a chave encontrada |
| `PARAR` | mestre → escravo | nada (só o rótulo já diz "pode sair") |

O mestre conta cada bilhete que envia e recebe. Por isso a coluna `mensagens`
do CSV já mede o custo de comunicação — não precisamos de uma coletiva extra
só para contar.

---

## 7. Como os resultados aparecem

Os resultados aparecem em **três níveis**, um derivado do outro:

```
nível 1: uma linha na tela (cada execução)
             |
             v  scripts/coletar.sh repete várias vezes e salva
nível 2: dados/bruto.csv  ->  dados/resultados.csv
             |
             v  scripts/gerar_graficos.py desenha
nível 3: 4 arquivos SVG em graficos/
```

### Nível 1 — a linha na tela

Exemplo real de execução do OpenMP:

```
versao=openmp espaco=60000000 alvo=14850082816872249796 chave=20000000 threads=4 processos=1 tempo_busca=0.004947 mensagens=0
```

Campo por campo:

| Campo | Significado |
|---|---|
| `versao=openmp` | qual programa rodou (`sequencial`, `openmp` ou `mpi`) |
| `espaco=60000000` | quantas chaves seriam testadas no pior caso |
| `alvo=14850082816872249796` | o hash que estamos procurando |
| `chave=20000000` | **a resposta**. É o campo que a validação compara |
| `threads=4` | quantas threads usamos |
| `processos=1` | quantos processos (no OpenMP é sempre 1) |
| `tempo_busca=0.004947` | segundos gastos **só na busca** |
| `mensagens=0` | mensagens MPI (sempre 0 fora do MPI) |

Quando não há resposta (acontece se `ESPACO = 0`), o programa imprime
`chave=-1`, exatamente como o baseline. Isso é o "sentinela": o maior número
possível internamente, convertido em `-1` na hora de mostrar.

> O campo `tempo_busca` mede só a região de busca. No baseline o campo se
> chama `tempo`. São comparáveis porque os dois cronômetros pegam o mesmo
> ponto: logo antes e logo depois do laço.

### Nível 2 — os CSVs

**`dados/bruto.csv`** — uma linha por execução (115 linhas). Cada configuração
roda 1 vez para aquecer + 5 vezes de verdade. Serve para ver a dispersão.

**`dados/perfilamento.csv`** — mostra onde o baseline gasta tempo:

```
espaco,tempo_total,tempo_laco,percentual_laco
4800000000,1.339528,1.346270,100.50
```

Ou seja: ~100% do tempo está no laço do hash. Daí vem a conclusão da Lei de
Amdahl: `p ≈ 1`, então o melhor possível seria `speedup ≈ P`.

**`dados/resultados.csv`** — uma linha por configuração, com a **mediana**
das 5 execuções:

```
versao,escala,n,espaco,chave,repeticoes,tempo_mediano,tempo_minimo,tempo_maximo,speedup,eficiencia,mensagens
openmp,forte,12,4800000000,1600000000,5,0.235784,0.229058,0.243651,5.737,47.8,0
mpi,forte,12,4800000000,1600000000,5,0.272404,0.242275,0.301037,4.952,41.3,945
```

Por que mediana e não média? Porque uma única execução pode ser atrasada por
um processo do sistema operacional. O valor do meio das 5 medições não é
puxado por esse pico.

Os dois últimos cálculos:

```
speedup    = T1 / Tp     = 1,352755 / 0,235784 = 5,737   (OpenMP, 12 threads)
eficiencia = speedup/P × 100 = 5,737 / 12 × 100 = 47,8%
```

`T1` é o tempo da **mesma versão** com 1 unidade. Assim comparamos maçã com
maçã: OpenMP 12 é comparado com OpenMP 1, não com o baseline.

### Nível 3 — os quatro gráficos

| Arquivo | O que mostra | Como ler |
|---|---|---|
| `graficos/tempo.svg` | tempo de cada configuração | linha mais baixa = mais rápido |
| `graficos/speedup.svg` | `T1/Tp` | quanto mais perto da reta cinza tracejada (ideal) melhor. A linha laranja é o teto do mestre-escravo (`P−1`) |
| `graficos/eficiencia.svg` | aproveitamento por unidade | quanto mais alto, melhor. A queda com muitos processos é esperada |
| `graficos/escalabilidade_fraca.svg` | tempo com trabalho por unidade constante | o ideal seria uma reta horizontal; se sobe, o custo extra está crescendo |

Os SVGs são gerados como **texto** por `scripts/gerar_graficos.py`, sem
matplotlib. Basta abrir no navegador.

> Atenção ao teto do MPI: com `P` processos, apenas `P−1` procuram. Então o
> melhor speedup possível é `P−1` e a melhor eficiência é `(P−1)/P`. Com 12
> processos isso dá no máximo 11× e 91,7%. É propriedade do padrão
> mestre-escravo, não erro de código.

---

## 8. Mapa de arquivos

| Arquivo | Papel |
|---|---|
| `docs/baseline_sequencial.cpp` | programa original do professor — **não mexer** |
| `docs/GUIA_INICIANTE.md` | este guia |
| `docs/DECISAO_MPI.md` | por que escolhemos só mestre-escravo |
| `docs/PLANO_MARCO2.md` | escopo e critérios de aceite |
| `docs/PENDENCIAS.md` | o que ainda depende do grupo |
| `src/comum/busca.hpp` | hash, alvo, argumentos e saída — igual nas 3 versões |
| `src/openmp/openmp.cpp` | versão com threads |
| `src/mpi/mpi.cpp` | versão com processos |
| `scripts/validar.sh` | compara as chaves com o baseline |
| `scripts/coletar.sh` | executa tudo e grava os CSVs |
| `scripts/gerar_graficos.py` | calcula medianas e desenha os SVGs |
| `Makefile` | compila e oferece `validar`, `dados`, `graficos` |
| `docker/Dockerfile` | instala g++, OpenMPI e Python dentro do Linux |
| `README.md` | números, metodologia e respostas para a arguição |
| `AUTORES.md` | identificação do grupo |

---

## 9. Como saber que está certo

Três verificações, todas automáticas:

```bash
make validar                 # 7 tamanhos × 10 configurações = 70 comparações
git diff -- docs/baseline_sequencial.cpp   # precisa não mostrar nada
make dados && make graficos  # 115 execuções e 4 SVGs
```

A validação testa tamanhos pequenos (`0, 1, 2, 3, 10, 1000`) de propósito:
neles, a resposta fica no começo e a corrida/término antecipado aparece na
hora. Se alguma versão parasse no lugar errado, a chave divergiria do
baseline. Resultado obtido: **70/70 OK**.

---

## 10. Roteiro para apresentar em 5 minutos

1. **(30 s)** Abra o baseline e mostre as 4 linhas: alvo = `h(ESPACO/3)`,
   laço de 0 a ESPACO, `break` no acerto. Diga que não o alteramos.
2. **(40 s)** Rode `./bin/sequencial 60000000`, `./bin/openmp 60000000
   --threads=4` e `mpirun -np 4 ./bin/mpi 60000000`. Aponte a **mesma
   chave** nas três linhas: `20000000`.
3. **(1 min)** No `openmp.cpp`, mostre o `schedule(dynamic, 1)` e desenhe a
   fila de blocos no quadro. Depois mostre `atomic` e `critical` e diga qual
   corrida cada um evita.
4. **(1 min)** No `mpi.cpp`, desenhe o rank 0 no meio e os escravos em volta,
   com setas `BLOCO` e `PEDIDO`. Aponte `Bcast` e `Allreduce` como as coletivas.
5. **(1 min)** Abra `speedup.svg` e `eficiencia.svg`. Mostre que o OpenMP
   chega a 5,74× e que o MPI tem teto `P−1`: com 12 processos, 11× é o máximo.
6. **(30 s)** Feche com Amdahl: quase 100% do tempo é o laço do hash, então a
   queda real vem de comunicação, término antecipado e hardware — não da lógica.

---

## 11. Perguntas e respostas curtas

**"Por que vocês não testaram `static`, `dynamic` e `guided`?"**
Porque o hash sempre faz o mesmo número de contas, sem ler memória. Todo mundo
leva o mesmo tempo por chave, então não existe carga desigual para medir. A
gente usa `dynamic` só para lidar com o término antecipado.

**"O que aconteceria se vocês não usassem `atomic`?"**
Poderíamos perder o aviso de "já achei" e continuar calculando à toa — ou, no
pior caso, uma thread leria um valor pela metade. O `atomic` torna essa
leitura/escrita indivisível.

**"Por que o `critical` está fora do laço?"**
Dentro do laço ele travaria as 4 threads a cada chave. Fora, cada thread entra
uma vez, no fim, para juntar o resultado.

**"Por que o MPI com 2 processos é mais lento que o sequencial?"**
Porque um dos dois processos só coordena (não busca) e ainda gasta tempo
mandando bilhetes. Com poucos processos esse custo pesa mais que o ganho.

**"O que é escalabilidade fraca e por que o espaço cresce com `P−1` no MPI?"**
É o teste em que aumentamos o problema junto com as unidades. Como no
mestre-escravo só `P−1` processos buscam, mantemos o trabalho **por
trabalhador** constante multiplicando o espaço por `P−1` (e não por `P`).

---

> **Dica final:** se você entendeu até aqui, consegue explicar o projeto. Não
> tente decorar código: decore a **história** — "procuramos uma chave,
> dividimos o trabalho em blocos, paramos no primeiro acerto e provamos que a
> resposta continua a mesma".
