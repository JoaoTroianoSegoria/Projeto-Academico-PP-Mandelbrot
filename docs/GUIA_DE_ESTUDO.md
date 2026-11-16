# Guia de estudo — Conjunto de Mandelbrot em paralelo (OpenMP e MPI)

Projeto Acadêmico de Programação Paralela (CCO085) — IESB 2026/2 — Prof. Rodrigo Gonçalves.
Apresentação: 19/11/2026. Dupla: João Troiano Segoria (Frentes 1 e 3) e Bruno Nóbrega Souza (Frente 2 e análise experimental).

> Os números de medição deste guia são os do notebook da apresentação (Intel Core i5-9300H, 4 núcleos
> físicos / 8 lógicos, 16 GB, Linux), medidos com `make medir` em 16/11/2026 (mediana de 3 execuções,
> `resultados/resumo.md`). Se o número da tela for um pouco diferente no dia, o que vale é a
> **explicação**, não a casa decimal.

## Sumário

1. Como usar este guia
2. Matemática
3. Contexto: o que é, onde e para que serve
4. Tecnologias usadas
5. Fundamentos de paralelismo
6. Frente 1 — Baseline e perfilamento
7. Frente 2 — OpenMP
8. Frente 3 — MPI
9. Sequencial × paralelo — leitura dos resultados
10. Roteiro da demonstração ao vivo
11. Banco de perguntas e respostas
12. Glossário

---

## 1. Como usar este guia

### 1.1 As regras do jogo (seções 7, 8 e 9 do enunciado)

- A apresentação tem ~18 minutos: ~9 de **demonstração ao vivo** e ~9 de **arguição individual**.
- A arguição é **cruzada**: a pergunta sorteada é sobre a frente de **outro** colega. Você fez o baseline
  e o MPI, então a sua pergunta muito provavelmente será sobre **OpenMP** (seção 7 deste guia). O Bruno
  deve ser perguntado sobre baseline ou MPI.
- Você responde **sozinho**, sem IA e sem colegas, podendo consultar **apenas o repositório**. Este guia
  fica dentro do repositório (`docs/GUIA_DE_ESTUDO.md`), assim como os comentários no topo de cada
  arquivo `.cpp`, que resumem as decisões.
- A resposta à questão vale 3,5 pontos e "precisão técnica, terminologia e tempo" vale mais 1,0. Não
  saber explicar a frente perguntada **zera** o critério.

### 1.2 Ordem de estudo sugerida

1. Leia o "pitch" abaixo até conseguir falar sem olhar.
2. Seção 2 (matemática) e seção 5 (fundamentos): são a base de qualquer pergunta.
3. Seção 7 (OpenMP) duas vezes. É a sua pergunta provável.
4. Seções 6 e 8: você fez, mas precisa explicar em voz alta durante a demonstração.
5. Seção 9: saber ler as tabelas e os gráficos.
6. Seção 11: cubra as respostas e responda em voz alta. Se travar, volte à seção indicada.

Você **não** precisa decorar código. Precisa saber: o que cada parte faz, por que foi feita assim, e o que
aconteceria se fosse feita de outro jeito.

### 1.3 O projeto em 5 frases (pitch de 30 segundos)

1. O programa calcula o **Conjunto de Mandelbrot**: para cada pixel de uma imagem 2000×2000, repete a
   conta `z = z² + c` até o ponto "escapar" ou até 1000 iterações, e guarda quantas iterações foram.
2. O código sequencial do professor é o **gabarito**: ele não foi alterado, e as versões paralelas têm de
   dar exatamente o mesmo **checksum** (709817858 em 2000×2000).
3. O perfil mostrou que mais de 99% do tempo está no cálculo dos pixels e que o custo é **muito desigual**:
   as linhas do meio da imagem custam centenas de vezes mais que as das bordas.
4. No **OpenMP** dividimos as linhas entre threads com `parallel for`, somamos o checksum com
   `reduction` e usamos `schedule(dynamic, 1)` para equilibrar a carga; no **MPI** cada processo calcula
   linhas intercaladas (decomposição cíclica) e o rank 0 junta tudo com `MPI_Gatherv` e `MPI_Reduce`.
5. Resultado no notebook (4 núcleos físicos): com 4 threads o `dynamic` chega a speedup ≈ 3,90<!--num-->
   e o MPI cíclico a ≈ 3,9<!--num-->, enquanto a divisão em blocos fixos (`static` e MPI `bloco`) fica
   em ≈ 2<!--num-->, exatamente como o perfil previu.

---

## 2. Matemática

### 2.1 Número complexo

No conjunto dos números reais não existe raiz quadrada de número negativo. Os números complexos resolvem
isso criando uma unidade nova, `i`, definida por:

```
i² = −1
```

Um número complexo tem a forma `z = a + b·i`, em que `a` e `b` são números reais:

- `a` é a **parte real**;
- `b` é a **parte imaginária**.

Analogia: um número real é um ponto numa **reta**; um número complexo é um ponto num **plano**. O eixo
horizontal é o eixo real e o vertical é o eixo imaginário. Esse plano se chama **plano complexo** (ou
plano de Argand-Gauss). O número `3 + 2i` é o ponto de coordenadas (3, 2).

No código não existe um tipo "complexo": cada número complexo são **duas variáveis `double`**. `zr` e
`zi` são a parte real e a imaginária de `z`; `cr` e `ci` são as de `c`.

**Módulo.** É a distância do ponto até a origem (Pitágoras):

```
|z| = √(a² + b²)
```

**Soma.** Soma-se parte real com parte real e imaginária com imaginária:

```
(a + bi) + (c + di) = (a + c) + (b + d)i
```

**Multiplicação.** Faz-se a distributiva e troca-se `i²` por −1:

```
(a + bi)(c + di) = ac + adi + bci + bd·i² = (ac − bd) + (ad + bc)i
```

**Quadrado** (caso particular, é o que o programa usa). Com `z = zr + zi·i`:

```
z² = (zr + zi·i)(zr + zi·i)
   = zr² + 2·zr·zi·i + zi²·i²
   = (zr² − zi²) + (2·zr·zi)·i        porque i² = −1
```

Por isso, no código, um passo `z = z² + c` aparece como duas linhas:

```cpp
double t = zr*zr - zi*zi + cr;   // nova parte real:       (zr² − zi²) + cr
zi = 2*zr*zi + ci;               // nova parte imaginária: (2·zr·zi)  + ci
zr = t;
```

A variável temporária `t` existe porque a conta de `zi` precisa do `zr` **antigo**. Se escrevêssemos
direto em `zr`, a segunda linha usaria o valor já alterado e a conta ficaria errada.

**Conjugado.** O conjugado de `a + bi` é `a − bi` (espelha o ponto em relação ao eixo real). Vai ser
importante na simetria (2.7).

### 2.2 A iteração z ← z² + c

Escolhe-se um número complexo `c` (um ponto do plano). Começa-se com `z₀ = 0` e repete-se:

```
z₀ = 0
z₁ = z₀² + c = c
z₂ = z₁² + c
z₃ = z₂² + c
...
```

Isso gera uma sequência de pontos (a "órbita" de 0). Há dois destinos possíveis:

- a sequência **fica limitada** (os pontos nunca se afastam muito da origem);
- a sequência **foge para o infinito** (o módulo cresce sem parar).

Exemplos fáceis de fazer de cabeça:

| c | sequência | destino |
|---|---|---|
| 0 | 0, 0, 0, ... | limitada → pertence |
| −1 | 0, −1, 0, −1, ... | limitada (fica alternando) → pertence |
| 1 | 0, 1, 2, 5, 26, 677, ... | foge → não pertence |
| i | 0, i, −1+i, −i, −1+i, −i, ... | limitada → pertence |

### 2.3 Definição do conjunto

> O **Conjunto de Mandelbrot** é o conjunto de todos os números complexos `c` para os quais a sequência
> `z ← z² + c`, começando em `z = 0`, **permanece limitada**.

Ou seja: o conjunto é feito de valores de **c**, não de z. Cada pixel da imagem é um `c` diferente, e para
cada um perguntamos "a sequência foge ou não?".

### 2.4 Por que o critério de fuga é |z| > 2

Não dá para iterar para sempre. O teorema que salva o programa é:

> Se em algum passo `|z| > 2`, a sequência **com certeza** vai para o infinito.

Prova curta, em dois casos (usa a desigualdade triangular: `|a + b| ≥ |a| − |b|`):

**Caso A: |c| ≤ 2 e, em algum passo, |z| > 2.** Então `|z| > |c|`, e:

```
|z² + c| ≥ |z|² − |c| > |z|² − |z| = |z| · (|z| − 1)
```

Como `|z| > 2`, o fator `(|z| − 1)` é maior que 1. Então o módulo do próximo ponto é **maior** que o do
atual, multiplicado por algo maior que 1. No passo seguinte o módulo é maior ainda, então o fator também
é. O módulo cresce pelo menos como uma progressão geométrica: vai para o infinito.

**Caso B: |c| > 2.** O primeiro ponto é `z₁ = c`, então já temos `|z₁| > 2` e `|z₁| ≥ |c|`. A mesma conta
do caso A vale (com `|c| ≤ |z|`) e a sequência foge.

Consequências:

- O conjunto inteiro cabe no disco de raio 2 em volta da origem.
- O raio 2 é o **menor** raio que funciona para todos os pontos: `c = −2` pertence ao conjunto (a
  sequência é 0, −2, 2, 2, 2, ...) e tem módulo exatamente 2. Por isso o teste é `> 2` e não `≥ 2`.

**Por que o código compara com 4.** A condição do laço é:

```cpp
while (zr*zr + zi*zi <= 4.0 && it < MAXIT)
```

`zr² + zi²` é `|z|²`. Comparar `|z|² ≤ 4` é o mesmo que comparar `|z| ≤ 2`, mas **sem calcular a raiz
quadrada**, que é uma operação cara. Como esse teste roda centenas de milhões de vezes, economizar a raiz
importa.

### 2.5 O que MAXIT = 1000 significa

A definição pede "a sequência fica limitada **para sempre**". O computador não pode testar para sempre,
então o programa testa até **1000 iterações**:

- se `|z|` passou de 2 antes disso → o ponto **escapou**: com certeza está **fora** do conjunto;
- se chegou a 1000 sem escapar → o ponto é **tratado como dentro**.

Isso é uma **aproximação**: um ponto muito perto da borda pode precisar de 5000 iterações para escapar e
seria contado como "dentro". Aumentar MAXIT deixa a borda mais precisa e o programa mais lento. Não
podemos mudar: MAXIT faz parte da lógica do baseline.

### 2.6 Tempo de fuga: o que o programa guarda

O valor guardado para cada pixel é `it`, o **número de iterações executadas** até escapar (ou 1000). Isso
se chama **tempo de fuga** (*escape time*). É esse número que vira cor nas imagens famosas: preto para
quem chegou a MAXIT (dentro), e cores diferentes conforme a rapidez da fuga. Pontos longe do conjunto
escapam em 1 a 5 iterações; pontos perto da borda demoram centenas.

O nosso programa **não desenha** a imagem: ele guarda a matriz de contagens (`hist`, um vetor de `long`
com W·H posições) e imprime a soma. Os gráficos SVG do projeto são gráficos de **desempenho**. A figura
do fractal que está no Marco 1 (`docs/marco1/mandelbrot.png`) foi gerada à parte, com a **mesma fórmula**
do baseline, só para ilustrar: preto = chegou a 1000 iterações (caro), claro = escapou logo (barato).
Mostrar essa figura é a forma mais rápida de explicar por que as linhas do meio custam mais.

Em 2000×2000: média de 177,5 iterações por pixel, e 17,3% dos pixels chegam a 1000 (saída do perfil).

### 2.7 Como o pixel (x, y) vira o número c

A imagem tem W colunas e H linhas. O pixel da coluna `x` e linha `y` representa:

```
cr = −2,5  + 3,5 · x / W        (parte real)
ci = −1,25 + 2,5 · y / H        (parte imaginária)
```

É uma regra de três (interpolação linear):

- `x = 0` dá `cr = −2,5`; `x = W` daria `cr = 1,0`. Como `x` vai só até `W − 1`, o intervalo é
  `[−2,5; 1,0)`. A largura da janela é 3,5.
- `y = 0` dá `ci = −1,25`; `y = H` daria `ci = +1,25`. O intervalo é `[−1,25; 1,25)`. A altura é 2,5.

A janela `[−2,5; 1,0) × [−1,25; 1,25)` foi escolhida porque o conjunto inteiro cabe nela (no eixo real ele
vai de −2 a 0,25; no imaginário, de cerca de −1,12 a +1,12).

Observação: a proporção da janela é 3,5 : 2,5, mas a imagem é quadrada (2000×2000). Então os pixels não
são "quadrados" no plano (a figura sai um pouco "achatada" na horizontal). Para o desempenho isso não
importa: o que conta é quantas iterações cada pixel faz.

### 2.8 Simetria em relação ao eixo real

Se `c` vira o seu conjugado (troca-se o sinal de `ci`), todos os `z` da sequência também viram os seus
conjugados, e o módulo não muda. Então `c` e o conjugado de `c` escapam **na mesma iteração**. Resultado:
a figura é **espelhada** em relação ao eixo real (a metade de cima é igual à de baixo).

Na imagem, o eixo real (`ci = 0`) é a linha `y = H/2` (linha 1000). A linha `y` e a linha `H − y` são
espelhos uma da outra. O perfil mostra isso: a faixa de linhas 800–999 custa 24,81%<!--num--> e a faixa
1000–1199 custa 24,85%<!--num-->.

**Por que isso importa:** com **2 threads**, o `schedule(static)` dá as linhas 0–999 para uma thread e
1000–1999 para a outra. Pela simetria, as duas metades custam praticamente o mesmo, então o `static` com 2
threads fica equilibrado "por sorte" (speedup 1,96<!--num-->). Com 4 threads a sorte acaba: as duas
threads do meio ficam com quase todo o trabalho (ver 7.6).

Detalhe honesto: a simetria na grade de pixels é quase exata, não perfeita (a linha 0 não tem par, e a
linha 1000 é o próprio eixo real), mas a diferença é desprezível.

### 2.9 O checksum e por que ele prova a corretude

O **checksum** é a soma do número de iterações de todos os pixels:

```
checksum = soma de it(x, y) para todos os pixels
```

Para 2000×2000 o baseline dá **709817858**; para 1000×1000 dá **177507041**.

Por que serve de prova:

- O cálculo é **determinístico**: mesma entrada, mesma conta, mesmo resultado. Não há aleatoriedade.
- A soma é de **números inteiros**, e soma de inteiros é exata e não depende da ordem. (Soma de `double`
  dependeria da ordem, por causa do arredondamento; aqui esse problema não existe.)
- Se a versão paralela pular uma linha, calcular uma linha duas vezes, perder uma soma numa condição de
  corrida ou errar um pixel, a soma muda.

Limite honesto: o checksum é uma evidência muito forte, mas não é uma prova matemática absoluta (dois
erros poderiam, em teoria, se cancelar). Por isso o modo demonstração do OpenMP compara também a imagem
**pixel a pixel** com a sequencial, e o MPI confere se a imagem montada no rank 0 soma o mesmo valor que o
`MPI_Reduce` calculou.

### 2.10 Por que -ffp-contract=off

Ponto flutuante (`double`) tem precisão finita: cada operação **arredonda** o resultado.

Processadores modernos têm uma instrução chamada **FMA** (*fused multiply-add*), que calcula `a·b + c`
com **um arredondamento só**, em vez de dois (um na multiplicação, outro na soma). É mais rápida e mais
precisa, mas o resultado pode diferir no último bit do resultado feito em duas etapas.

No Mandelbrot, uma diferença no último bit de `zr` pode crescer ao longo de centenas de iterações (é um
sistema caótico) e fazer um pixel perto da borda escapar uma iteração antes ou depois. Aí o checksum muda
e a versão paralela "não bate" com o gabarito, mesmo sem estar errada na lógica.

`-ffp-contract=off` proíbe o compilador de fundir multiplicação e soma em FMA. Assim a conta é feita
**na mesma ordem e com os mesmos arredondamentos** do baseline.

Detalhe honesto: com `g++ -O2` puro em x86-64 o compilador normalmente nem gera FMA (a instrução não faz
parte do conjunto básico). A flag é uma **garantia**: protege se alguém compilar com `-march=native` ou
em outra arquitetura.

### 2.11 Complexidade

```
O(W · H · MAXIT)
```

- W·H pixels; cada um faz no máximo MAXIT iterações; cada iteração tem custo constante.
- É o **pior caso**. O custo real é `W · H · (média de iterações)` = 4.000.000 × 177,5 ≈ 7,1 × 10⁸
  iterações em 2000×2000 (o próprio checksum é esse número).
- Dobrar o lado da imagem **quadruplica** o trabalho. As medidas do baseline confirmam: 1000×1000 leva
  ≈ 0,58 s<!--num-->, 2000×2000 ≈ 2,31 s<!--num-->, 4000×4000 ≈ 9,23 s<!--num-->.
- Memória: O(W·H). Em 2000×2000 são 4 milhões de `long` de 8 bytes = 32 MB.

---

## 3. Contexto: o que é, onde e para que serve

### 3.1 História em poucas linhas

- **Gaston Julia e Pierre Fatou** (França, por volta de 1918) estudaram a iteração de funções complexas
  como `z² + c`, sem computador para desenhar.
- **Benoît Mandelbrot** (matemático polonês-francês-americano, pesquisador da **IBM**) criou a palavra
  **"fractal"** em **1975** e, em **1980**, usando os computadores da IBM, visualizou e estudou o conjunto
  que hoje leva o seu nome. (Uma primeira figura rudimentar do conjunto apareceu em 1978, de Brooks e
  Matelski; foi Mandelbrot quem o estudou a fundo e o tornou conhecido.)
- O nome "conjunto de Mandelbrot" foi dado pelos matemáticos **Douady e Hubbard**, que provaram, nos anos
  1980, que o conjunto é **conexo** (uma peça só).

### 3.2 Fractal e auto-semelhança

Um **fractal** é uma forma com detalhe em **todas as escalas**: ao dar zoom, aparecem estruturas novas,
muitas vezes parecidas com o todo (**auto-semelhança**). Exemplos da natureza: litoral, samambaia,
brócolis romanesco, ramificação de rios, raios, vasos sanguíneos.

No Mandelbrot, ao ampliar a borda aparecem cópias pequenas (e levemente deformadas) do conjunto inteiro.
A auto-semelhança dele é aproximada, não exata.

### 3.3 Anatomia do conjunto

- **Cardioide principal**: a região grande em forma de coração. São os `c` cuja sequência converge para
  um ponto fixo.
- **Bulbos**: os discos grudados na cardioide. O maior, à esquerda, é um disco de centro −1 e raio 1/4;
  nele a sequência oscila entre 2 valores (como no exemplo `c = −1`). Cada bulbo corresponde a um período
  diferente.
- **Borda**: infinitamente complexa. Por mais zoom que se dê, ela nunca fica lisa. É na borda que estão os
  pixels "caros e imprevisíveis": demoram muito para escapar.
- No eixo real, o conjunto ocupa o intervalo de −2 a 0,25.

Curiosidade que confere com o nosso perfil: a área do conjunto é estimada em cerca de 1,51. A nossa
janela tem área 3,5 × 2,5 = 8,75, e 17,3% disso é ≈ 1,51.

### 3.4 Relação com os conjuntos de Julia

Usam a **mesma fórmula** `z ← z² + c`, mas com papéis trocados:

| | o que fica fixo | o que varia (o pixel) |
|---|---|---|
| Conjunto de **Julia** | `c` (um valor escolhido) | `z₀`, o ponto de partida |
| Conjunto de **Mandelbrot** | `z₀ = 0` | `c` |

Existe **um conjunto de Julia para cada `c`**, e só **um** conjunto de Mandelbrot. O Mandelbrot funciona
como um **mapa** dos conjuntos de Julia: se `c` está dentro do Mandelbrot, o Julia daquele `c` é conexo
(uma peça só); se `c` está fora, o Julia é uma "poeira" de pontos soltos.

### 3.5 Para que serve (resposta honesta)

O conjunto de Mandelbrot **em si** é usado principalmente como:

1. **Objeto de estudo da matemática**: sistemas dinâmicos e teoria do caos. É o exemplo clássico de como
   uma regra muito simples (`z² + c`) gera comportamento extremamente complexo, e de sensibilidade às
   condições iniciais (um `c` minimamente diferente muda o destino da sequência).
2. **Benchmark e exemplo didático de computação paralela** (ver 3.6).
3. **Arte e computação gráfica**: imagens e animações de zoom.
4. **Ensino**: números complexos, iteração, convergência, ponto flutuante.

Já a **geometria fractal** em geral (a área que Mandelbrot fundou) tem aplicações práticas reais:

- **Computação gráfica**: geração procedural de **terrenos**, montanhas, nuvens e **texturas** em filmes
  e jogos (ruído fractal).
- **Antenas fractais**: o formato fractal permite antenas compactas que funcionam em várias faixas de
  frequência; são usadas em aparelhos sem fio.
- **Compressão fractal de imagens**: aproveita a auto-semelhança dentro da imagem (teve uso comercial nos
  anos 1990; hoje é pouco usada).
- **Modelagem de fenômenos naturais**: litorais, redes de rios, crescimento de plantas, turbulência,
  estruturas ramificadas como pulmões e vasos; e séries irregulares, como preços de mercado (tema
  estudado pelo próprio Mandelbrot).

Se o professor perguntar "onde o Mandelbrot é usado no mundo real?", a resposta correta é: **o conjunto
é mais um modelo e um benchmark do que uma ferramenta; quem tem aplicação direta é a geometria fractal.**
Não invente aplicação.

### 3.6 Por que é um benchmark clássico de computação paralela

1. **Pixels independentes**: o resultado de um pixel não depende de nenhum outro. É o caso
   "embaraçosamente paralelo".
2. **Carga desigual e imprevisível**: não dá para saber quanto um pixel custa antes de calcular. Isso
   testa de verdade o **balanceamento de carga** (é exatamente o motivo de o professor ter colocado o
   problema na lista: "desbalanceamento natural de carga; motiva `schedule(dynamic)`").
3. **Limitado por CPU** (*compute-bound*): quase não usa memória durante a conta, então mede o
   processador e o escalonamento, sem a interferência do cache ou da banda de memória.
4. **Fácil de verificar**: o resultado é determinístico.
5. **Escala bem para GPU**: milhares de pixels calculados ao mesmo tempo. É exemplo comum em tutoriais de
   GPU.

---

## 4. Tecnologias usadas

| Tecnologia | O que é | Papel no projeto |
|---|---|---|
| **C++17** | linguagem compilada, rápida, com controle de memória | linguagem de todas as versões; é a do baseline do professor |
| **g++ (GCC)** | compilador C++ | transforma o código em executável |
| **-O2** | nível de otimização do compilador | pedido pelo professor; todas as versões usam o mesmo nível para a comparação ser justa |
| **-ffp-contract=off** | proíbe fundir multiplicação+soma (FMA) | garante o mesmo arredondamento do baseline (2.10) |
| **OpenMP** | padrão de paralelismo em **memória compartilhada** por diretivas `#pragma omp` | Frente 2: threads dividindo as linhas |
| **-fopenmp** | flag do g++ | liga as diretivas `#pragma omp` e linka a biblioteca de execução |
| **libgomp** | biblioteca de execução do OpenMP no GCC | cria a equipe de threads, distribui as iterações do laço, faz a redução |
| **MPI** | padrão de **troca de mensagens** entre processos (memória distribuída) | Frente 3 |
| **OpenMPI** | uma implementação do padrão MPI (versão 4.1.6 no contêiner) | fornece `mpicxx` e `mpirun` |
| **mpicxx** | o g++ "embrulhado" com os includes e bibliotecas do MPI | compila a versão MPI |
| **mpirun -np N** | lançador | inicia N cópias do programa e conecta umas às outras |
| **Make / Makefile** | automação de compilação | `make` compila tudo; `make medir`, `make graficos`, `make demo` |
| **Linux** | sistema operacional | ambiente de referência do laboratório; fornece o relógio `clock_gettime` |
| **Contêiner** (Docker no roteiro; **podman** no notebook) | ambiente isolado e reproduzível | mesmo Ubuntu 24.04 + g++ + OpenMPI em qualquer máquina |
| **Git / GitHub** | controle de versão | histórico de commits (faz parte da nota), issues por frente, `AUTORES.md` |
| **bash** | linguagem de script do terminal | `scripts/medir.sh` (medições) e `scripts/demo.sh` (roteiro) |
| **CSV** | tabela em texto separado por vírgulas | `resultados/medicoes.csv`: dados brutos das medições |
| **SVG** | formato de imagem vetorial em texto (XML) | os gráficos, gerados por `ferramentas/graficos.cpp` |

Detalhes que podem ser perguntados:

- **OpenMP × OpenMPI**: nomes parecidos, coisas diferentes. Open**MP** = threads, memória compartilhada.
  Open**MPI** = uma implementação de MPI, processos, troca de mensagens.
- **Por que os gráficos são gerados em C++?** Para o projeto depender só do que o ambiente da disciplina
  já tem (g++ e make), sem precisar instalar Python ou outra ferramenta. SVG é texto, então um programa
  C++ consegue escrever com `fprintf`.
- **Por que contêiner?** Para ter em casa o mesmo ambiente do laboratório. Não é uma máquina virtual
  completa: o contêiner usa o kernel do próprio Linux, então o custo de desempenho é praticamente nulo
  para um programa que só faz conta.
- **Docker × podman**: são equivalentes para este uso (mesmos comandos, mesma imagem). O podman é o que
  já vem no Bluefin.
- **`--shm-size=1g`**: o OpenMPI, numa máquina só, troca mensagens entre processos por **memória
  compartilhada** (`/dev/shm`). O padrão do contêiner é pequeno demais para a imagem de 32 MB.
- **`-DOMPI_SKIP_MPICXX`**: usamos a API em C do MPI; essa definição só desliga cabeçalhos antigos de C++
  do OpenMPI que geram avisos.
- **O baseline é compilado só com `g++ -O2`**, exatamente como diz o cabeçalho do arquivo do professor.

---

## 5. Fundamentos de paralelismo

### 5.1 Concorrência × paralelismo

- **Concorrência**: várias tarefas **em andamento** no mesmo período, possivelmente se revezando num único
  núcleo. Analogia: um cozinheiro cuidando de três panelas.
- **Paralelismo**: várias tarefas executando **no mesmo instante**, em núcleos diferentes. Analogia: três
  cozinheiros, um por panela.

Paralelismo exige hardware com vários núcleos; concorrência não. O projeto busca paralelismo: terminar
mais rápido.

### 5.2 Processo × thread

| | Processo | Thread |
|---|---|---|
| Memória | cada um tem a **sua** | todas as threads do processo **compartilham** a mesma |
| Criação | mais cara | mais barata |
| Comunicação | por mensagens (ou outro mecanismo do sistema) | direta: leem e escrevem as mesmas variáveis |
| Risco típico | custo de comunicação | condição de corrida |
| No projeto | **MPI** (cada rank é um processo) | **OpenMP** |

Analogia: processos são **casas separadas** (para passar algo ao vizinho, é preciso mandar pelo correio);
threads são **pessoas na mesma casa** (todos usam a mesma geladeira, e podem brigar por ela).

### 5.3 Memória compartilhada × memória distribuída

- **Compartilhada (OpenMP)**: todas as threads enxergam o mesmo vetor `hist`. Ninguém precisa copiar
  nada, mas é preciso cuidar para duas threads não escreverem no mesmo lugar. Limite: **uma máquina só**.
- **Distribuída (MPI)**: cada processo tem o seu vetor. Não há corrida de dados (ninguém enxerga a memória
  do outro), mas tudo o que for compartilhado tem de ser **enviado por mensagem**, e isso custa tempo.
  Vantagem: funciona em **várias máquinas** (cluster).

### 5.4 Taxonomia de Flynn (resumo)

Classifica as arquiteturas pelo número de fluxos de instruções e de dados:

| Sigla | Significado | Exemplo |
|---|---|---|
| SISD | uma instrução, um dado | processador de um núcleo, programa sequencial |
| SIMD | uma instrução, muitos dados | instruções vetoriais (SSE/AVX), GPU |
| MISD | muitas instruções, um dado | raro; sistemas redundantes tolerantes a falhas |
| MIMD | muitas instruções, muitos dados | multicore e clusters: **OpenMP e MPI** |

O estilo de programação do MPI é chamado **SPMD** (*single program, multiple data*): o **mesmo programa**
roda em todos os processos, cada um com a sua parte dos dados.

### 5.5 Embaraçosamente paralelo, mas com carga desigual

Um problema é **embaraçosamente paralelo** quando pode ser dividido em partes que **não dependem umas das
outras** e não precisam se comunicar durante o cálculo. O Mandelbrot é assim **entre pixels**: cada pixel
só depende das suas coordenadas.

Dois cuidados:

1. **Dentro** de um pixel, o cálculo é **sequencial por natureza**: a iteração 500 precisa do resultado
   da 499. Não dá para paralelizar o laço `while`. Paraleliza-se **entre** pixels (na prática, entre
   linhas).
2. A carga é **muito desigual**. Do perfil em 2000×2000: a linha mais cara leva 4,159 ms<!--num--> e a
   mais barata 0,008 ms<!--num--> (cerca de 520 vezes<!--num-->); as 20% de linhas centrais (800–1199)
   concentram ≈ 50%<!--num--> do custo. Dividir "em partes iguais de linhas" **não** divide o trabalho em
   partes iguais.

Então: fácil de dividir, difícil de dividir **bem**. É isso que decide quase tudo no projeto.

### 5.6 Fork-join

É o modelo de execução do OpenMP:

```
thread principal ──► [fork: cria a equipe] ══► várias threads trabalham ══► [join: todas terminam] ──► segue sozinha
```

O programa começa com uma thread. Ao chegar em `#pragma omp parallel`, ela cria (ou acorda) a equipe
(*fork*); no fim da região, todas se encontram numa barreira e só a principal continua (*join*).

### 5.7 Speedup e eficiência

```
speedup:     S(p) = T_sequencial / T_paralelo(p)
eficiência:  E(p) = S(p) / p
```

- `S(p) = p` é o **speedup linear (ideal)**: p vezes mais recursos, p vezes mais rápido.
- A eficiência diz quanto de cada núcleo foi aproveitado: 100% é ideal.
- Exemplo do projeto: `dynamic` com 4 threads, S = 3,90<!--num-->, E = 3,90 / 4 = 97,5%<!--num-->.
- `T_sequencial` é o tempo do **baseline do professor** (o melhor sequencial), não o tempo da versão
  paralela com 1 thread (ver 7.9).

### 5.8 Lei de Amdahl

Todo programa tem uma parte que não se paraleliza (a **fração serial**, `f`). Com p núcleos:

```
S(p) = 1 / ( f + (1 − f) / p )

quando p → infinito:   S_max = 1 / f
```

Analogia: uma viagem com um trecho de estrada de terra (serial) e um de rodovia (paralelizável). Mesmo
com um carro infinitamente rápido na rodovia, o tempo da estrada de terra continua lá.

No projeto, o perfil mede a fração serial (alocar e zerar o vetor, somar o checksum, imprimir):
`f ≈ 0,0048` (0,48%)<!--num-->. Conta para p = 4:

```
S(4) = 1 / (0,0048 + 0,9952 / 4) = 1 / (0,0048 + 0,2488) = 1 / 0,2536 ≈ 3,94
```

| p | 2 | 4 | 8 | 16 | infinito |
|---|---|---|---|---|---|
| S de Amdahl | 1,99<!--num--> | 3,94<!--num--> | 7,74<!--num--> | 14,93<!--num--> | ≈ 209<!--num--> |

O que Amdahl **supõe** (e por isso é um teto otimista): a parte paralela se divide **perfeitamente** entre
os p núcleos, **sem nenhum overhead**, e os p núcleos são todos **de verdade**. Ela só enxerga a fração
serial. Não enxerga desbalanceamento, comunicação, hyper-threading. Por isso o real fica abaixo (seção 9).

### 5.9 Lei de Gustafson

Amdahl mantém o **problema fixo** e aumenta os núcleos. Gustafson observa que, na prática, quem ganha
mais núcleos costuma **aumentar o problema** (imagem maior, mais resolução) para o mesmo tempo de espera.
Nesse cenário, a parte paralela cresce com p e a serial fica igual. O "speedup escalado" é:

```
S(p) = p − f · (p − 1)          (o mesmo que  f + (1 − f) · p)
```

Com f = 0,0048 e p = 4: S ≈ 3,99. Com Gustafson o speedup cresce quase linearmente e não tem teto.

Ligação com o projeto: **Amdahl ↔ escalabilidade forte**; **Gustafson ↔ escalabilidade fraca**.

### 5.10 Escalabilidade forte × fraca

| | Forte | Fraca |
|---|---|---|
| Tamanho do problema | **fixo** | **cresce junto** com p (trabalho por núcleo constante) |
| Pergunta | "o mesmo problema fica quanto mais rápido?" | "se o problema cresce com a máquina, o tempo fica igual?" |
| Ideal | tempo cai para T/p | tempo **constante** |
| Eficiência | S(p) / p | T(1) / T(p) |
| Lei associada | Amdahl | Gustafson |
| No projeto | imagem 2000×2000; threads 1, 2, 4, 8, 16; processos 1, 2, 4, 8 | ≈ 1 milhão de pixels por thread/processo |

**Por que lado = 1000 · √p na fraca.** O trabalho é proporcional ao **número de pixels** = lado². Para o
número de pixels ser p vezes maior, o lado tem de ser multiplicado por √p:

| p | 1 | 2 | 4 | 8 | 16 |
|---|---|---|---|---|---|
| lado | 1000 | 1414 | 2000 | 2828 | 4000 |
| pixels | 1 milhão | ≈ 2 milhões | 4 milhões | ≈ 8 milhões | 16 milhões |

Funciona porque a média de iterações por pixel quase não muda com a resolução (a figura é a mesma, só
mais detalhada). O baseline confirma: o tempo sequencial dobra a cada passo (≈ 0,58<!--num-->,
1,15<!--num-->, 2,31<!--num-->, 4,62<!--num-->, 9,23 s<!--num-->).

Na fraca, o projeto mede o OpenMP `dynamic` e o MPI `ciclico`. Os números finais ficam em
`resultados/resumo.md`.

### 5.11 Overheads: o que come o speedup

| Overhead | O que é | No nosso projeto |
|---|---|---|
| Criação/gestão de threads | criar a equipe, acordar, barreira final | desprezível em 2000×2000 (milissegundos contra segundos); pesaria em imagem pequena |
| Sincronização | trancas, operações atômicas, fila do `dynamic` | `reduction` evita; a fila do `dynamic` custa uma operação atômica por linha |
| Comunicação | enviar dados entre processos (MPI) | a imagem inteira (32 MB) vai para o rank 0 |
| **Desbalanceamento** | uns terminam cedo e ficam parados enquanto outros trabalham | **a causa nº 1 aqui**: `static` e `bloco` |
| False sharing | threads escrevendo em variáveis diferentes na **mesma linha de cache** | quase não ocorre (dividimos por linha, não por pixel) |
| Banda de memória | todos os núcleos disputando o acesso à RAM | não é gargalo: o problema é *compute-bound* |
| SMT / oversubscription | mais threads que núcleos físicos / lógicos | aparece de 4 para 8 e de 8 para 16 threads |
| Turbo / temperatura | a frequência cai com mais núcleos ativos ou calor | notebook: afeta todas as medidas |

**Compute-bound × memory-bound.** Um programa *compute-bound* é limitado pela velocidade de fazer conta;
um *memory-bound* é limitado pela velocidade de trazer dados da RAM. O laço do Mandelbrot usa só meia
dúzia de variáveis (ficam em **registradores**) e escreve 8 bytes por pixel no fim de ~177 iterações. É
*compute-bound*. Multiplicação de matrizes grandes, por exemplo, é *memory-bound*: lá, mais núcleos
disputam a mesma banda de memória e o speedup satura cedo.

**False sharing.** O cache trabalha em blocos de 64 bytes (linhas de cache). Se duas threads escrevem em
variáveis **diferentes** que caem na **mesma** linha de cache, cada escrita invalida a cópia da outra
thread, e a linha fica "pingando" entre os núcleos. O programa continua correto, mas lento. Analogia:
duas pessoas escrevendo em linhas diferentes da **mesma folha**; têm de ficar passando a folha.

### 5.12 Hyper-threading (SMT), oversubscription, turbo e temperatura

**Núcleo físico × lógico.** O i5-9300H tem **4 núcleos físicos** e **8 lógicos**. O hyper-threading (nome
da Intel para SMT, *simultaneous multithreading*) faz cada núcleo físico aparecer como dois para o sistema
operacional. São dois "fluxos" dividindo **as mesmas unidades de cálculo**. Analogia: uma cozinha com
**um fogão** e **dois cozinheiros**; o segundo só adianta quando o primeiro deixa uma boca livre.

**Por que o hyper-threading ajuda no Mandelbrot.** O laço de um pixel é uma **cadeia de dependências**:
cada iteração precisa do `zr` e `zi` da anterior. O processador não consegue adiantar a iteração seguinte,
então uma thread sozinha deixa parte das unidades de ponto flutuante **ociosa**, esperando. A segunda
thread do mesmo núcleo aproveita esses intervalos. O ganho é parcial: no notebook, de 4 para 8 threads o
`dynamic` vai de 3,90<!--num--> para 6,30<!--num--> (cerca de 62%<!--num--> a mais, não 100%).

**Oversubscription.** É ter **mais threads/processos prontos do que núcleos lógicos**. Com 16 threads em 8
núcleos lógicos, o sistema operacional reveza as threads (troca de contexto). Não existe hardware novo
para usar, então quase não há ganho: `dynamic` vai de 6,30<!--num--> (8 threads) para 6,58<!--num-->
(16 threads), e a eficiência cai para 41,1%<!--num-->.

**Turbo boost e temperatura.** O processador sobe a frequência acima da nominal (2,4 GHz no i5-9300H)
quando há folga térmica e elétrica; com **um** núcleo ativo o turbo é o mais alto, e com **todos** ativos
é menor. Consequências:

- o **sequencial** roda com o turbo máximo; o **paralelo**, com todos os núcleos ativos, roda cada núcleo
  um pouco mais devagar. Isso puxa a eficiência um pouco para baixo, e não é culpa do programa;
- se a CPU **esquenta**, a frequência cai (*thermal throttling*) e as medidas pioram e variam;
- foi exatamente esse efeito que criou a "eficiência acima de 100%" no MPI antes da correção (8.9).

---

## 6. Frente 1 — Baseline e perfilamento

Responsável: João. Arquivos: `sequencial/03_mandelbrot.cpp` (do professor, intocado),
`sequencial/mandelbrot_perfil.cpp`, `comum/mandelbrot_comum.hpp`, `Makefile`, `Dockerfile`.

### 6.1 O que o baseline faz

1. Lê W e H da linha de comando.
2. Aloca o vetor `hist` com W·H posições (`long`).
3. Laço duplo: para cada linha `y` e cada coluna `x`, calcula o número de iterações e guarda em
   `hist[y*W + x]`. **Só esse trecho é cronometrado.**
4. Soma tudo (checksum) e imprime: `W=2000 H=2000  checksum=709817858  tempo=... s`.

`hist[y*W + x]`: a imagem é uma matriz guardada num vetor de uma dimensão, **linha por linha**. A linha
`y` ocupa as posições de `y*W` até `y*W + W − 1`. Isso será importante no OpenMP: cada linha da imagem é
um pedaço **contíguo** e separado do vetor.

### 6.2 O cabeçalho comum

A regra do enunciado é: "não é permitido alterar a lógica de cálculo nem a entrada". Para que as três
versões façam **exatamente** a mesma conta, o corpo do laço do baseline foi copiado, sem mudar nenhuma
expressão, para a função `iteracoes_pixel(x, y, W, H)` em `comum/mandelbrot_comum.hpp`. O perfil, o
OpenMP e o MPI chamam essa mesma função (por meio de `calcula_linha`, que calcula uma linha inteira e
devolve a soma dela). O arquivo do professor continua intocado e é compilado sozinho.

### 6.3 Protocolo de medição

| Regra | Por quê |
|---|---|
| Compilar com **-O2** | é o que o professor pede; comparar otimizado com não otimizado seria injusto |
| Medir **só o cálculo** dos pixels | é o que o baseline mede; alocação e impressão ficam fora |
| **CLOCK_MONOTONIC** | relógio que só anda para a frente; não é afetado se a hora do sistema for ajustada |
| **1 execução de aquecimento descartada** | a primeira carrega o programa e as bibliotecas do disco, "acorda" a CPU (sobe a frequência), cria as threads |
| **3 execuções medidas** | uma medida só pode ter sido atrapalhada por outro programa |
| Usar a **mediana** | ver abaixo |
| Conferir o **checksum** em toda execução | um speedup com resultado errado não vale nada |
| Registrar a **máquina** (`resultados/maquina.txt`) | tempo sem máquina não significa nada |

**Por que mediana e não média.** A mediana é o valor do meio (com 3 medidas: ordena e pega a segunda). Se
uma das execuções for atrapalhada pelo sistema (um pico), a **média** é puxada por esse valor; a
**mediana** ignora. Exemplo: tempos 2,30, 2,31 e 3,10 → média 2,57, mediana 2,31. A mediana representa
melhor o comportamento típico.

**Tempo de relógio × tempo de CPU.** Medimos tempo de **relógio de parede** (quanto o usuário espera).
Tempo de CPU somaria o de todas as threads e não mostraria ganho nenhum com paralelismo.

O script `scripts/medir.sh` aplica esse protocolo a tudo e grava `resultados/medicoes.csv`. No OpenMP o
relógio é `omp_get_wtime()` e no MPI é `MPI_Wtime()`: todos são relógios de parede.

### 6.4 Perfilamento: onde o tempo é gasto

**Perfilar** é medir **onde** o programa gasta o tempo. O trecho mais caro é o **hotspot** (ponto quente).
Só vale a pena paralelizar o hotspot.

O `mandelbrot_perfil` faz o mesmo cálculo com cronômetros entre os trechos (isso se chama
**instrumentação** manual). Resultado em 2000×2000 (`resultados/perfil.txt`):

| Trecho | Tempo (s) | % do total |
|---|---|---|
| alocação + zerar o vetor (serial) | 0,009<!--num--> | 0,39%<!--num--> |
| **cálculo dos pixels (hotspot)** | 2,307<!--num--> | **99,52%**<!--num--> |
| soma do checksum | 0,002<!--num--> | 0,09%<!--num--> |
| saída (printf) | ≈ 0 | ≈ 0% |

Conclusão: o hotspot é o laço duplo. É ele que se paraleliza.

Por que instrumentar à mão e não usar uma ferramenta como o `gprof`? Porque o programa é um laço só: uma
ferramenta por função diria apenas "100% no main". Os cronômetros entre os trechos dão exatamente a
tabela "tempo por trecho" que o enunciado pede, e ainda permitem medir **cada linha da imagem**.

### 6.5 Carga por faixa de linhas

O perfil mede o tempo de cada uma das 2000 linhas e agrupa em 10 faixas:

| Linhas | % do cálculo |
|---|---|
| 0–199 | 0,10%<!--num--> |
| 200–399 | 1,21%<!--num--> |
| 400–599 | 7,54%<!--num--> |
| 600–799 | 16,39%<!--num--> |
| **800–999** | **24,81%**<!--num--> |
| **1000–1199** | **24,85%**<!--num--> |
| 1200–1399 | 16,25%<!--num--> |
| 1400–1599 | 7,53%<!--num--> |
| 1600–1799 | 1,20%<!--num--> |
| 1800–1999 | 0,10%<!--num--> |

Leitura:

- As duas faixas centrais (20% das linhas) têm ≈ 50% do custo. As duas das pontas (20% das linhas) têm
  0,2%.
- A tabela é **simétrica** (2.8).
- Motivo: as linhas do meio atravessam o conjunto, onde cada pixel faz 1000 iterações; as das bordas
  estão longe do conjunto e escapam em poucas iterações.

### 6.6 Previsão para divisão em blocos

Com o tempo de cada linha em mãos, o perfil **simula** o que aconteceria se as linhas fossem divididas em
p blocos contíguos iguais (o que o `static` e o MPI `bloco` fazem). O tempo paralelo seria o do **bloco
mais caro**, pois todos têm de esperar por ele:

```
speedup máximo em blocos = tempo total do cálculo / tempo do bloco mais caro
```

| p | bloco mais caro (s) | speedup máximo em blocos | ideal |
|---|---|---|---|
| 2 | 1,155<!--num--> | 2,00<!--num--> | 2 |
| 4 | 1,086<!--num--> | 2,12<!--num--> | 4 |
| 8 | 0,677<!--num--> | 3,41<!--num--> | 8 |
| 16 | 0,374<!--num--> | 6,17<!--num--> | 16 |

Repare no p = 4: o bloco mais caro (1,086 s) é quase igual ao de p = 2 (1,155 s). Passar de 2 para 4
blocos **quase não ajuda**, porque cada bloco do meio continua com quase metade do trabalho.

Essa previsão foi feita **antes** de paralelizar, só com o sequencial, e as medidas depois confirmaram:
`static` com 4 threads deu 2,09<!--num--> e com 8 deu 3,34<!--num-->. Esse é o ponto forte da Frente 1
para falar na apresentação: **o perfil previu o comportamento da versão paralela**.

### 6.7 Amdahl a partir do perfil

```
f = (tempo total − tempo do cálculo) / tempo total ≈ 0,0048
```

Tudo o que não é o cálculo dos pixels conta como serial. É uma estimativa **conservadora**: no OpenMP a
soma do checksum também é paralelizada (pela `reduction`), então o f real é ainda menor. Com esse f, o
speedup máximo teórico é 1/f ≈ 209<!--num-->, e S(4) ≈ 3,94, S(8) ≈ 7,74 (tabela em 5.8).

Mensagem para a apresentação: **Amdahl diz que o problema é quase 100% paralelizável; o perfil por linhas
avisa que o perigo não é a fração serial, é o desbalanceamento.**

Observação: f depende do tamanho da imagem e da máquina (em outra máquina o valor medido foi um pouco
diferente, na mesma ordem de grandeza, menos de 1%). A conclusão não muda.

---

## 7. Frente 2 — OpenMP

Responsável: Bruno. **É a sua pergunta provável.** Arquivos: `openmp/mandelbrot_omp.cpp` e
`openmp/corrida_demo.cpp`.

### 7.1 A ideia em uma frase

Todas as threads enxergam o mesmo vetor da imagem; o laço das **linhas** é dividido entre elas; cada
thread calcula linhas inteiras e escreve só nas posições dessas linhas; o checksum é somado com
`reduction`.

O núcleo da versão é isto:

```cpp
long soma = 0;
#pragma omp parallel num_threads(nt) reduction(+:soma)
{
    #pragma omp for schedule(dynamic, 1) nowait
    for (int y = 0; y < H; y++)
        soma += calcula_linha(y, W, H, &hist[y * W]);
}
```

Leitura, parte por parte:

| Trecho | O que faz |
|---|---|
| `#pragma omp parallel` | cria a equipe de threads (fork); o bloco `{ }` é executado por **todas** |
| `num_threads(nt)` | quantas threads |
| `reduction(+:soma)` | cada thread ganha uma `soma` privada que começa em 0; no fim, todas são somadas na original |
| `#pragma omp for` | divide as **iterações** do laço entre as threads da equipe (sem isso, cada thread faria o laço inteiro) |
| `schedule(dynamic, 1)` | como dividir: uma linha por vez, por ordem de chegada |
| `nowait` | tira a barreira do fim do laço (7.8) |
| `}` final | barreira implícita: espera todas terminarem (join) e faz a redução |

`#pragma` é uma instrução para o **compilador**. Sem a flag `-fopenmp`, o compilador ignora as diretivas e
o programa roda sequencial, com o mesmo resultado. Essa é uma característica do OpenMP: paraleliza-se
**acrescentando** diretivas, sem reescrever o algoritmo.

### 7.2 Por que laço paralelo, e por que sobre as linhas

O enunciado pede para escolher o padrão: **laço paralelo, seções ou tasks**.

- **Laço paralelo (`parallel for`)**: para um laço com número de iterações conhecido e iterações
  independentes. É exatamente o nosso caso: `for (y = 0; y < H; y++)`. **Escolhido.**
- **Seções (`sections`)**: para um número pequeno e fixo de blocos de código **diferentes** rodando ao
  mesmo tempo (por exemplo, "ler arquivo A" e "ler arquivo B"). Aqui todo o trabalho é o mesmo código
  repetido; não há blocos diferentes.
- **Tasks**: para trabalho **irregular ou recursivo**, em que as tarefas são descobertas durante a
  execução (divisão e conquista, como o mergesort; percorrer árvores e listas). Aqui o conjunto de
  tarefas é conhecido de antemão (H linhas). Daria para criar uma task por linha, mas o resultado seria
  equivalente ao `schedule(dynamic, 1)`, com mais código e mais overhead.

**Por que dividir por linhas, e não por pixels?**

1. **Granularidade**: uma linha custa em média ≈ 1,15 ms<!--num-->; um pixel custa menos de um
   microssegundo. Pegar trabalho da fila tem um custo fixo. Por linha, são 2000 idas à fila; por pixel,
   seriam 4 milhões, e o custo de distribuir ficaria comparável ao de calcular.
2. **False sharing**: uma linha ocupa 2000 × 8 bytes = 16 KB contíguos, ≈ 250 linhas de cache só dela. Com
   divisão por pixel, threads vizinhas escreveriam o tempo todo na mesma linha de cache (8 pixels cabem em
   64 bytes).
3. 2000 linhas para 4 a 16 threads já são pedaços suficientes para um balanceamento fino.

**Por que não `collapse(2)`?** `collapse(2)` junta os dois laços (y e x) num só de W·H iterações e
distribui **pixels**. Com `dynamic, 1` cairia nos problemas 1 e 2 acima; com `static` não resolveria o
desbalanceamento (os blocos continuariam sendo faixas contíguas da imagem). Não traz vantagem aqui.
`collapse` é útil quando o laço externo tem **poucas** iterações para o número de threads.

**Por que não paralelizar o laço interno (`x`)?** Criaria e encerraria uma região de trabalho 2000 vezes
(uma por linha) em vez de uma. Regra geral: paralelize o laço **mais externo** possível.

### 7.3 Variáveis compartilhadas × privadas

Regra do OpenMP: o que foi declarado **fora** da região paralela é **compartilhado** (todas as threads
veem a mesma variável); o que é declarado **dentro** é **privado** (cada thread tem a sua). A variável de
controle do `omp for` é privada automaticamente.

| Variável | Tipo | Por que é seguro |
|---|---|---|
| `W`, `H`, `nt` | compartilhadas | só **lidas**; leitura simultânea nunca dá corrida |
| `hist` (a imagem) | compartilhada | escrita, mas cada thread em **posições diferentes** (linhas diferentes) |
| `y` | privada | variável do laço paralelo |
| `x`, `cr`, `ci`, `zr`, `zi`, `it`, `t` | privadas | declaradas dentro das funções chamadas: ficam na **pilha** de cada thread |
| `soma` | **reduction** | cópia privada por thread, juntadas no fim |
| `t_thread` | privada | declarada dentro da região paralela |
| `m.tempo_por_thread` | compartilhada | cada thread escreve só na **sua** posição (índice `omp_get_thread_num()`) |

Cada thread tem a sua **pilha**. Toda variável local de uma função chamada dentro da região paralela vive
na pilha de quem chamou, então é privada sem precisar declarar nada.

### 7.4 Condição de corrida: onde há e onde não há

**Condição de corrida** (*race condition*; no caso de memória, *data race*): duas ou mais threads acessam
a **mesma** posição de memória **ao mesmo tempo**, **pelo menos uma escreve**, e **não há
sincronização**. O resultado passa a depender de quem chega primeiro: fica errado e **muda a cada
execução**.

No projeto há dois candidatos:

**1. A imagem `hist` — NÃO há corrida.** O `omp for` entrega cada valor de `y` a **uma única thread**. A
linha `y` ocupa as posições `y*W` até `y*W + W − 1`, que não se sobrepõem às de nenhuma outra linha.
Então duas threads **nunca** escrevem na mesma posição. Não precisa de proteção nenhuma. (Proteger com
`critical` seria correto, mas destruiria o desempenho à toa.)

**2. A soma do checksum — HÁ corrida.** Se todas as threads fizessem `soma += ...` na mesma variável,
teríamos o problema clássico. `soma += it` parece uma operação, mas são **três passos**:

```
1. ler o valor de soma da memória
2. somar it
3. escrever o resultado de volta
```

Duas threads podem se intercalar:

```
thread A: lê soma (100)                 thread B: lê soma (100)
thread A: calcula 100 + 7 = 107         thread B: calcula 100 + 5 = 105
thread A: escreve 107                   thread B: escreve 105      ← o +7 sumiu
```

O valor final deveria ser 112 e ficou 105. Isso se chama **atualização perdida**. Analogia: duas pessoas
consultam o saldo da mesma conta (R$ 100), cada uma faz um depósito e anota "o novo saldo"; a última a
anotar apaga o depósito da outra.

### 7.5 critical × atomic × reduction

O `corrida_demo` calcula o checksum de 1000×1000 (gabarito: 177.507.041) de quatro maneiras, com 8
threads:

| Versão | Correta? | Speedup (8 threads) | Como funciona |
|---|---|---|---|
| sem proteção | **não**: faltam ≈ 155 milhões<!--num-->, valor diferente a cada vez | — | todas escrevem na mesma variável |
| `critical` | sim | 3,87<!--num--> | tranca: uma thread por vez no bloco |
| `atomic` | sim | 6,46<!--num--> | a CPU faz ler-somar-escrever como uma instrução indivisível |
| `reduction` | sim | 7,04<!--num--> | cada thread soma numa cópia privada; junta no fim |

**critical.** `#pragma omp critical` marca uma **seção crítica**: um trecho em que só **uma thread por
vez** pode estar (exclusão mútua). É implementado com uma **tranca** (*lock*, *mutex*): quem chega e
encontra ocupado, espera. Analogia: banheiro com uma chave só.

- Vantagem: serve para **qualquer** trecho de código, de qualquer tamanho.
- Desvantagem: é o mais caro. Pegar e soltar a tranca custa, e as threads fazem **fila**. Aqui são 1
  milhão de entradas na seção crítica (uma por pixel).
- Por que ainda assim acelerou 3,87 vezes: o cálculo pesado (`iteracoes_pixel`) ficou **fora** da tranca;
  só a soma fica dentro. Se o cálculo estivesse dentro, o programa viraria sequencial.

**atomic.** `#pragma omp atomic` protege **uma única operação simples** sobre uma variável (`x += v`,
`x++`, e parecidas). O compilador usa uma instrução do processador que faz o ler-modificar-escrever de
forma **indivisível** (em x86, uma instrução com prefixo `lock`). Não há tranca nem fila no sistema
operacional.

- Vantagem: bem mais barato que `critical`.
- Desvantagens: só vale para uma operação simples; e **todas as threads continuam disputando a mesma
  variável**. A linha de cache onde `soma` mora fica "pingando" de núcleo em núcleo a cada pixel. Por isso
  não chega ao desempenho da `reduction`.

**reduction.** `reduction(+:soma)` faz três coisas:

1. cria uma cópia **privada** de `soma` para cada thread, iniciada com o elemento neutro da operação (0
   para a soma);
2. cada thread acumula na **sua** cópia, sem disputa nenhuma (é uma variável local, fica em registrador);
3. no fim da região, o OpenMP combina as cópias na variável original: **p somas** (uma por thread), em vez
   de W·H operações sincronizadas.

Analogia: contar os votos de uma eleição. Em vez de todos os mesários brigarem por **um** quadro (critical
ou atomic), cada um conta a **sua** urna numa folha própria e, no final, somam-se as folhas.

- Vantagem: correta e a mais rápida; a sincronização praticamente desaparece.
- Limite: só serve para operações de **acumulação** que sejam associativas (`+`, `*`, `min`, `max`, `&&`,
  `||`...). O nosso caso (uma soma de inteiros) é o exemplo perfeito.
- Com inteiros o resultado é **exatamente** o mesmo do sequencial, em qualquer ordem. (Com `double`, a
  ordem diferente das somas poderia mudar o último dígito.)

**Quando usar cada um:**

| Situação | Use |
|---|---|
| acumular um valor (soma, máximo, contagem) | `reduction` |
| uma atualização simples que não se encaixa em redução (ex.: incrementar a posição `i` de um histograma) | `atomic` |
| um trecho com várias instruções, ou estrutura de dados complexa (inserir numa lista, imprimir) | `critical` |

**Por que a escolha foi `reduction`:** é a única das três que elimina a disputa em vez de organizá-la.
Com `critical` e `atomic` todas as threads ainda brigam pela mesma variável a cada pixel; com `reduction`
ninguém briga.

**Pegadinha: por que sem proteção perde tanto (≈ 155 de 177 milhões)?** Com `-O2`, o compilador supõe
que só aquela thread mexe em `soma` (o padrão C++ diz que corrida de dados é **comportamento
indefinido**, então ele pode supor isso). Em vez de ler e gravar na memória a cada pixel, ele guarda
`soma` num **registrador** durante um trecho longo do laço e só grava na memória no final do trecho. A
"janela" entre ler e escrever, que seria de nanossegundos, passa a durar o trecho inteiro. Quando a
thread finalmente grava, ela **apaga** tudo o que as outras gravaram nesse intervalo. O que se perde são
trechos inteiros, não pixels isolados. Repare que o que sobra (≈ 22 milhões<!--num-->) é da ordem de 1/8
do total: mais ou menos a contribuição de **uma** thread das 8. O erro existe **com ou sem** otimização; a
otimização só o deixa maior e mais visível.

### 7.6 Escalonamento: static × dynamic × guided

A cláusula `schedule` decide **qual thread calcula qual linha**.

Analogia: uma pilha de 2000 provas para corrigir, em que as do meio da pilha são longas e as das pontas
estão quase em branco.

| | static | dynamic, 1 | guided |
|---|---|---|---|
| Como divide | blocos contíguos de ≈ H/p linhas, decididos **antes** de começar | **fila**: cada thread pega 1 linha; quando termina, pega a próxima | fila, mas com pedaços que **começam grandes e vão diminuindo** |
| Analogia | divide a pilha em p montes iguais na hora de começar | cada corretor pega uma prova por vez da pilha | cada corretor pega um maço proporcional ao que resta |
| Custo de distribuir | praticamente zero | uma operação atômica por linha | menos idas à fila que o dynamic |
| Equilibra carga desigual? | **não** | **sim** | parcialmente |
| Quando é bom | iterações de custo igual | custo desigual ou imprevisível | custo desigual, quando ir à fila é caro |

**O chunk.** O segundo parâmetro (`schedule(tipo, chunk)`) é o tamanho do pedaço. `dynamic, 1` entrega
**uma linha** por vez (1 já é o padrão do `dynamic`; escrevemos para deixar explícito). No `static` sem
chunk, cada thread recebe um bloco único de H/p. No `guided`, o tamanho do pedaço é proporcional às
iterações que restam dividido pelo número de threads.

**Números do notebook, 2000×2000 (speedup em relação ao sequencial):**

| Threads | static | dynamic | guided |
|---|---|---|---|
| 1 | 1,00<!--num--> | 1,00<!--num--> | 1,00<!--num--> |
| 2 | 1,96<!--num--> | 1,97<!--num--> | 1,97<!--num--> |
| 4 | 2,09<!--num--> | **3,90**<!--num--> | 2,71<!--num--> |
| 8 | 3,34<!--num--> | **6,30**<!--num--> | 4,99<!--num--> |
| 16 | 5,32<!--num--> | **6,58**<!--num--> | 6,00<!--num--> |

Eficiência e desbalanceamento (desbalanceamento = maior tempo de thread ÷ tempo médio; 1,00 = perfeito):

| Threads | static: eficiência | static: desbal. | dynamic: eficiência | dynamic: desbal. | guided: eficiência |
|---|---|---|---|---|---|
| 4 | 52,2%<!--num--> | 1,88<!--num--> | 97,5%<!--num--> | ≈ 1,00<!--num--> | 67,7%<!--num--> |
| 8 | 41,7%<!--num--> | 2,34<!--num--> | 78,8%<!--num--> | ≈ 1,00<!--num--> | 62,4%<!--num--> |
| 16 | 33,2%<!--num--> | 2,50<!--num--> | 41,1%<!--num--> | ≈ 1,00<!--num--> | 37,5%<!--num--> |

**Como ler cada coluna:**

- **static com 2 threads vai bem (1,96)**: simetria em relação ao eixo real (2.8). Cada metade da imagem
  custa o mesmo.
- **static com 4 threads empaca (2,09)**: as threads ficam com as linhas 0–499, 500–999, 1000–1499 e
  1500–1999. As duas do meio pegam quase todo o trabalho; as duas das pontas terminam logo e ficam
  **paradas**. O programa só termina quando a mais lenta termina. O perfil tinha previsto 2,12 (6.6).
  O desbalanceamento de 1,88 significa: a thread mais lenta trabalhou 1,88 vez a média.
- **static com 8 (3,34)**: de novo colado na previsão do perfil (3,41).
- **dynamic com 4 threads (3,90; 97,5%)**: quase ideal. Nenhuma thread fica parada enquanto houver linha
  na fila; desbalanceamento ≈ 1,00.
- **dynamic de 4 para 8 (6,30)**: o notebook tem 4 núcleos físicos; as threads 5 a 8 usam o
  hyper-threading. Ganho parcial (5.12). **Não é falha do programa**: o desbalanceamento continua 1,00.
- **dynamic com 16 (6,58)**: oversubscription; quase nada a ganhar.
- **guided fica no meio**: os primeiros pedaços são grandes. Com 4 threads, os primeiros pedaços têm
  centenas de linhas (≈ 500, depois ≈ 375, depois ≈ 280...), e um desses pedaços grandes cai justamente
  na região central cara. Quem o pegou fica preso nele enquanto as outras terminam. O guided supõe que
  ir à fila é caro e que o custo das iterações não está concentrado; aqui é o contrário.
- **static com 16 (5,32) é melhor que static com 8 (3,34)**, mesmo sem núcleos novos: com 16 blocos
  menores, as threads dos blocos baratos terminam logo e liberam os núcleos lógicos para as threads dos
  blocos caros. O sistema operacional acaba fazendo um "balanceamento" indireto. Continua abaixo do
  `dynamic`.

**Por que `dynamic` com chunk 1 não é caro aqui.** O custo do `dynamic` é uma operação atômica cada vez
que uma thread pega um pedaço (fração de microssegundo). São 2000 idas à fila no total. Uma linha custa em
média ≈ 1,15 ms<!--num-->. O custo de distribuir é milhares de vezes menor que o trabalho distribuído.
Chunk 1 seria caro se cada iteração fosse **minúscula** (por exemplo, se o laço paralelo fosse sobre
pixels).

**Por que não um chunk maior no dynamic (por exemplo 16)?** Diminuiria as idas à fila (que já são
desprezíveis) e pioraria um pouco o balanceamento no final. Sem ganho.

### 7.7 O desbalanceamento medido

O programa mede quanto tempo **cada thread** ficou calculando (`tempo_por_thread`) e calcula:

```
desbalanceamento = maior tempo de thread / tempo médio das threads
```

- 1,00: todas trabalharam o mesmo tempo (perfeito).
- 1,88 (static, 4 threads): a mais lenta trabalhou quase o dobro da média; as outras ficaram esperando.

É a evidência direta de que a perda do `static` é **desbalanceamento**, e não overhead de threads.

### 7.8 nowait e a barreira implícita

Uma **barreira** é um ponto em que as threads **esperam umas pelas outras**: só passam quando todas
chegaram.

O OpenMP coloca barreiras **implícitas** em dois lugares:

1. no fim de um `omp for`;
2. no fim da região `parallel`.

`nowait` remove a primeira. No projeto ela foi removida por um motivo de **medição**: logo depois do
laço, cada thread anota o seu tempo. Com a barreira, todas esperariam a mais lenta **antes** de anotar,
todas anotariam o mesmo tempo, e o desbalanceamento daria sempre 1,00 (falso). Com `nowait`, cada thread
anota o instante em que **ela** terminou.

A corretude não depende disso: a barreira do fim do `parallel` continua existindo e garante que, depois
da região, todas terminaram e a `soma` já está reduzida. Não se pode remover essa segunda barreira.

### 7.9 Medição do tempo e cálculo do speedup

- `omp_get_wtime()` devolve o tempo de **relógio de parede** em segundos (`double`). Mede-se antes e
  depois da região paralela. O intervalo inclui criar/acordar as threads, o cálculo e a barreira final.
- O speedup é calculado contra o **sequencial**, e não contra a versão OpenMP com 1 thread:
  - no modo demonstração (`./bin/mandelbrot_omp 2000 2000`), contra o laço sequencial medido no mesmo
    programa, sem OpenMP;
  - nos gráficos e no `resumo.md`, contra o **baseline do professor** (`mandelbrot_seq`).
- Por quê: comparar com "1 thread" esconderia o overhead da própria versão paralela e daria um speedup
  mais bonito do que o real (isso se chama speedup **relativo**). O correto é comparar com o melhor
  sequencial (speedup **absoluto**). Aqui a diferença é pequena (1 thread dá 1,00<!--num-->), o que
  mostra que o OpenMP não acrescentou custo relevante.
- Antes da tabela, o programa faz uma execução de **aquecimento** com 2 threads, para criar o conjunto de
  threads (a libgomp reaproveita as threads entre regiões paralelas).
- Antes de cada execução, a imagem é **zerada**, para nenhuma linha "sobrar" certa da execução anterior, e
  depois é comparada **pixel a pixel** com o gabarito (coluna `resultado`: `igual`).

### 7.10 Resumo da Frente 2 para falar em 1 minuto

"O OpenMP usa memória compartilhada. Paralelizamos o laço das linhas com `parallel for`, porque as
linhas são independentes e são um pedaço de trabalho grande o bastante. A imagem não tem condição de
corrida, porque cada linha é escrita por uma thread só. O checksum teria, porque todas somariam na mesma
variável; usamos `reduction`, que dá uma cópia privada para cada thread e junta no fim, e medimos que é
mais rápida que `critical` e `atomic`. Como as linhas do meio custam centenas de vezes mais que as das
bordas, o `static` deixa threads paradas e fica em speedup 2,1 com 4 threads; o `dynamic` com uma linha
por vez equilibra e chega a 3,90. De 4 para 8 threads o ganho é só do hyper-threading, porque o notebook
tem 4 núcleos físicos."

---

## 8. Frente 3 — MPI

Responsável: João. Arquivo: `mpi/mandelbrot_mpi.cpp`.

### 8.1 Conceitos básicos

- **MPI** (*Message Passing Interface*): um **padrão** de troca de mensagens entre processos. OpenMPI é
  uma implementação dele.
- **Processo**: cada cópia do programa. `mpirun -np 4 ./bin/mandelbrot_mpi` inicia 4 processos.
- **Rank**: o número de identificação de cada processo, de 0 a p − 1. `MPI_Comm_rank` devolve o meu rank;
  `MPI_Comm_size` devolve p.
- **Comunicador**: um grupo de processos que podem trocar mensagens. `MPI_COMM_WORLD` é o comunicador
  padrão, com **todos** os processos.
- **Root**: o processo que coordena as coletivas; aqui é o rank 0.
- **SPMD**: o **mesmo executável** roda em todos os processos. O que diferencia o comportamento é o
  rank: `if (meu_rank == 0) { ... }`.
- **`MPI_Init` / `MPI_Finalize`**: a primeira e a última chamada MPI de todo programa. `MPI_Init` conecta
  o processo aos outros; `MPI_Finalize` encerra.

Ponto central: **cada processo tem a sua memória**. O rank 2 não enxerga o vetor do rank 0. Toda variável
é "privada" por natureza. Consequências:

- **não existe corrida de dados** entre processos;
- tudo o que precisa ser compartilhado tem de ser **enviado**. A imagem completa só existe no rank 0.

O programa segue três passos: **decompor** (quem calcula o quê), **calcular** (cada um a sua parte, sem
conversar), **juntar** (comunicação).

### 8.2 Decomposição de domínio: bloco × cíclica

**Decomposição de domínio** é dividir os **dados** (aqui, as linhas da imagem) entre os processos.

| | Bloco | Cíclica |
|---|---|---|
| Quem calcula o quê | rank r calcula um bloco **contíguo** de ≈ H/p linhas | rank r calcula as linhas r, r+p, r+2p, ... (**intercaladas**) |
| Com p = 4 | r0: 0–499, r1: 500–999, r2: 1000–1499, r3: 1500–1999 | r0: 0, 4, 8, ...; r1: 1, 5, 9, ...; r2: 2, 6, 10, ...; r3: 3, 7, 11, ... |
| Analogia | cortar o baralho em 4 montes | **distribuir as cartas** uma a uma, em roda |
| Carga | **desbalanceada** | **equilibrada** |
| É o equivalente a | `schedule(static)` | `schedule(static, 1)` |

**Por que a cíclica resolve.** O custo das linhas muda **suavemente** de uma linha para a vizinha (as
linhas 1000, 1001, 1002 e 1003 custam quase o mesmo). Na distribuição cíclica, cada rank recebe uma
amostra de **todas** as regiões da imagem: um quarto das linhas caras e um quarto das baratas. As cargas
ficam praticamente iguais.

**Números do notebook, -np 4, 2000×2000:**

| Decomposição | Speedup | Eficiência | Desbalanceamento | Tempo de cálculo por rank |
|---|---|---|---|---|
| bloco | 1,92<!--num--> | 48,1%<!--num--> | 1,89<!--num--> | r0 ≈ 0,07 s<!--num-->, r1 ≈ 1,2 s<!--num-->, r2 ≈ 1,2 s<!--num-->, r3 ≈ 0,07 s<!--num--> |
| cíclico | 3,84<!--num--> | 95,9%<!--num--> | 1,00<!--num--> | os quatro praticamente iguais |

No bloco, os ranks 0 e 3 terminam em 0,07 s e ficam **esperando** os ranks 1 e 2 por mais de um segundo.
O tempo do programa é o do mais lento.

**Por que cíclica (estática) e não um esquema dinâmico (mestre-escravo)?** No MPI, um esquema dinâmico
exigiria um processo mestre distribuindo linhas sob demanda: uma troca de mensagens por linha, e um
processo ocupado só em coordenar. A cíclica não tem **nenhuma** comunicação durante o cálculo e já
equilibra a carga neste problema (desbalanceamento 1,00). Se a carga variasse de forma brusca e
imprevisível, o mestre-escravo passaria a valer a pena.

**Decomposição de domínio × de tarefas.** De domínio divide os **dados**; de tarefas divide **trabalhos
diferentes**. Aqui todos fazem a mesma conta sobre dados diferentes, então é de domínio.

### 8.3 As operações coletivas usadas

**Ponto-a-ponto × coletiva.**

- **Ponto-a-ponto**: um processo envia (`MPI_Send`), outro recebe (`MPI_Recv`). Um remetente, um
  destinatário.
- **Coletiva**: **todos** os processos do comunicador participam, chamando a **mesma função**. O MPI
  decide internamente como distribuir (em geral em árvore, com log p etapas, em vez de p envios em
  sequência).

O projeto usa **somente coletivas**: o código fica mais curto, sem laços de `Send`/`Recv`, e a biblioteca
otimiza a distribuição. O enunciado exige pelo menos uma.

| Coletiva | Direção | O que faz no projeto | Por quê |
|---|---|---|---|
| `MPI_Bcast` | um → todos | o rank 0 lê W, H e o modo da linha de comando e transmite aos outros | todos precisam dos mesmos parâmetros; o padrão é "o root lê e espalha" (não se deve contar que todos os processos recebam os argumentos) |
| `MPI_Gatherv` | todos → um | cada rank envia as suas linhas; o rank 0 recebe todas | a imagem completa tem de existir em algum lugar |
| `MPI_Reduce` com `MPI_SUM` | todos → um, **combinando** | soma os checksums parciais | não precisamos dos p valores, só da soma |
| `MPI_Reduce` com `MPI_MAX` | todos → um, combinando | pega o tempo do rank **mais lento** | é o tempo do programa (8.5) |
| `MPI_Gather` | todos → um | traz o tempo de cálculo de cada rank | queremos ver **cada** valor (para o desbalanceamento), por isso Gather e não Reduce |
| `MPI_Barrier` | sincronização | todos esperam todos antes de ligar o cronômetro | para os ranks largarem juntos e a medida ser justa |
| `MPI_Ibarrier` | sincronização não bloqueante | espera sem gastar CPU no modo demonstração | ver 8.9 |

**Gather × Reduce.** `Gather` **junta** os valores (o root recebe um vetor com p pedaços). `Reduce`
**combina** os valores com uma operação (o root recebe **um** valor: a soma, o máximo...). O `MPI_Reduce`
é o equivalente, no MPI, da `reduction` do OpenMP.

### 8.4 Por que Gatherv e não Gather

- `MPI_Gather` exige que **todos** os ranks enviem a **mesma quantidade** de elementos.
- `MPI_Gatherv` (o "v" é de **variável**) aceita quantidades **diferentes** por rank. Ele recebe dois
  vetores:
  - `quantidades[r]`: quantos elementos vêm do rank r;
  - `deslocamentos[r]`: a partir de que posição do buffer do root colocar o que veio do rank r.

Quando H não é divisível por p, os ranks têm números de linhas diferentes. Exemplo: 2000 linhas com 3
processos dá 667, 667 e 666 linhas. `Gather` não funcionaria; `Gatherv` funciona para **qualquer** H e p.
(Com 2000 linhas e 4 processos as quantidades são iguais, 500 cada, mas o programa tem de valer para o
caso geral.)

**A reorganização no root.** O `Gatherv` coloca os dados no buffer **rank por rank**: primeiro tudo do
rank 0, depois tudo do rank 1, e assim por diante. No bloco, essa ordem já é a ordem da imagem. No
cíclico, as linhas chegam agrupadas por rank (0, 4, 8, ... depois 1, 5, 9, ...), então o root copia cada
linha para a posição certa da imagem com `memcpy`, usando a mesma função `linha_global` que disse a cada
rank qual linha calcular.

**Por que não `MPI_Scatter`?** `Scatter` distribui **dados de entrada** do root para os outros. Aqui não
há dados de entrada: cada rank calcula as suas linhas a partir de W, H e do próprio rank. Basta o `Bcast`
de três inteiros. Enviar menos é sempre melhor.

### 8.5 Por que o tempo é o do rank mais lento

O resultado só existe quando **o último** processo termina e a imagem está montada no root. Não adianta
três ranks terminarem em 0,07 s se o quarto leva 1,2 s. Por isso:

- cada rank mede o seu tempo desde a largada (depois do `MPI_Barrier`) até o fim do `MPI_Reduce` do
  checksum, **incluindo a comunicação**;
- `MPI_Reduce` com `MPI_MAX` leva ao root o **maior** desses tempos.

Incluir a comunicação é o honesto: sem ela o resultado não está no rank 0, ou seja, o programa não
terminou. `MPI_Wtime()` é o relógio de parede do MPI.

### 8.6 Overhead de comunicação

**Definição usada no projeto:**

```
comunicação = tempo total − maior tempo de cálculo entre os ranks
```

É o que sobra depois que o rank mais lento terminou a conta: as mensagens (`Gatherv` + `Reduce`) e a
reorganização das linhas no rank 0.

**Quanto se comunica.** A imagem tem W·H valores `long` de 8 bytes: 2000 × 2000 × 8 = **32 MB** em
2000×2000. O root recebe a imagem inteira e depois a copia para o lugar certo.

**No notebook, com -np 4:** a comunicação é ≈ 1,8%<!--num--> do tempo no cíclico e ≈ 0,8%<!--num--> no bloco. É pouco, por três motivos:

1. **não há comunicação durante o cálculo** (os pixels são independentes); só no fim;
2. numa máquina só, o OpenMPI passa as mensagens por **memória compartilhada**: é uma cópia de memória,
   não uma rede;
3. o cálculo é pesado (segundos) e a cópia é leve (milissegundos).

**Como ela varia com o número de processos** (o enunciado pede essa análise; os valores estão na coluna
`comunicacao_s` do CSV e na tabela de overhead do `resultados/resumo.md`):

- o volume de dados é **fixo** (a imagem tem o mesmo tamanho para qualquer p), então o custo absoluto da
  comunicação **não diminui** com mais processos;
- o tempo de cálculo **diminui** com p;
- logo, a comunicação **cresce em proporção** ao tempo total. É uma parte do programa que não acelera:
  na prática, entra na conta como "fração serial" e limita o speedup, como diz Amdahl.

Num **cluster de verdade** (várias máquinas ligadas por rede), enviar 32 MB custaria muito mais que numa
máquina só, e esse overhead seria bem mais visível.

**O OpenMP não tem esse custo**: as threads escrevem direto no vetor compartilhado. É a principal
diferença de desempenho entre as duas versões numa máquina só.

**Bloco × cíclico na comunicação.** O volume é o mesmo nos dois (a imagem inteira). O cíclico tem a cópia
de reorganização fazendo trabalho de verdade (linhas intercaladas), mas ela é barata. Em **porcentagem**,
a comunicação tende a pesar mais no cíclico simplesmente porque o tempo total dele é a metade do tempo do
bloco: o mesmo custo dividido por um total menor. É exatamente o que a medição mostrou:

| processos | bloco: comunicação | bloco: % do tempo | cíclico: comunicação | cíclico: % do tempo |
|---|---|---|---|---|
| 1 | 15,7 ms<!--num--> | 0,7%<!--num--> | 15,6 ms<!--num--> | 0,7%<!--num--> |
| 2 | 10,8 ms<!--num--> | 0,9%<!--num--> | 12,1 ms<!--num--> | 1,0%<!--num--> |
| 4 | 9,5 ms<!--num--> | 0,8%<!--num--> | 11,0 ms<!--num--> | 1,8%<!--num--> |
| 8 | 8,9 ms<!--num--> | 1,1%<!--num--> | 12,8 ms<!--num--> | 3,5%<!--num--> |

O custo absoluto fica na faixa de 9 a 16 ms (não diminui com p), e a **porcentagem** do cíclico dobra a
cada vez que o cálculo encolhe. Com 1 processo ele é até um pouco maior, porque o rank 0 copia a imagem
inteira para si mesmo no `Gatherv` e depois reorganiza tudo sozinho. Na escalabilidade fraca (imagem
crescendo com p) o custo absoluto cresce: 3,8 ms<!--num--> com 1 processo → 18 ms<!--num--> com 8. Gráfico:
`graficos/comunicacao_mpi.svg`.

### 8.7 Validação no MPI

1. O checksum do `MPI_Reduce` é comparado com o do sequencial (coluna `checksum`: `igual`).
2. O root soma a imagem que **montou** e confere com o valor do `Reduce`. Isso prova que o `Gatherv` e a
   reorganização colocaram cada linha no lugar certo, e não só que as somas parciais estão certas.

**Por que o resultado não muda com o número de processos** (o enunciado diz que isso seria "falha
objetiva"):

- cada pixel é calculado pela **mesma função**, com as mesmas entradas (x, y, W, H). O valor não depende
  de **quem** calcula;
- a decomposição cobre **cada linha exatamente uma vez**, para qualquer p (inclusive quando H não é
  divisível por p);
- o checksum é soma de **inteiros**: exata e independente da ordem. Se fosse soma de `double`, a ordem
  diferente poderia mudar o último dígito.

### 8.8 OpenMP × MPI

| | OpenMP | MPI |
|---|---|---|
| Unidade | threads | processos |
| Memória | compartilhada | distribuída |
| Como se programa | diretivas `#pragma` sobre o código sequencial | chamadas de função explícitas; quem programa decide o que enviar |
| Risco principal | condição de corrida | overhead de comunicação, espera infinita (*deadlock*) |
| Balanceamento no projeto | `schedule(dynamic, 1)`, em tempo de execução | decomposição cíclica, decidida de antemão |
| Escala até | os núcleos de **uma** máquina | **várias** máquinas (cluster) |
| Custo de juntar o resultado | nenhum (já está no vetor) | copiar a imagem (32 MB) para o root |
| Esforço | pequeno | maior |

**Quando usar cada um.** Uma máquina multicore: OpenMP (mais simples e sem cópia). Problema que não cabe
em uma máquina, ou que precisa de mais núcleos do que uma máquina tem: MPI.

**Híbrido (MPI + OpenMP).** Em clusters usa-se os dois: um processo MPI por máquina (ou por processador)
e, dentro de cada um, threads OpenMP nos núcleos. Diminui o número de processos e de mensagens. O projeto
não implementa o híbrido; é só o caminho natural para escalar além de uma máquina.

### 8.9 A correção da espera ativa (a pegadinha da eficiência acima de 100%)

**O sintoma.** No modo demonstração, o rank 0 mede o tempo sequencial sozinho e depois todos rodam as
versões paralelas. Na primeira versão do programa, o cíclico aparecia com **eficiência de 104% a
112%**<!--num-->: speedup maior que o número de processos. Isso é **impossível** neste problema (não há
efeito de cache que explique speedup superlinear; o cálculo usa só registradores).

**A causa.** Enquanto o rank 0 media o sequencial, os ranks 1, 2 e 3 esperavam num `MPI_Barrier`. No
OpenMPI, um processo bloqueado numa chamada MPI faz **espera ativa** (*busy-wait*): fica girando num laço
perguntando "já chegou a mensagem?", e ocupa **100% de um núcleo** sem produzir nada. Então, durante a
medida "sequencial", três núcleos estavam a 100% à toa. No notebook:

1. a CPU esquentava e, com todos os núcleos ocupados, **perdia o turbo**;
2. o sequencial ficava ≈ 12%<!--num--> mais lento (≈ 2,6 s<!--num--> em vez de ≈ 2,34 s<!--num-->);
3. como speedup = T_sequencial / T_paralelo, um numerador inflado dava um **speedup inflado**.

O erro estava na **medição**, não no cálculo: o checksum sempre esteve certo.

**A correção.** Trocar a barreira bloqueante por uma **não bloqueante**, e dormir enquanto se espera:

```cpp
MPI_Ibarrier(MPI_COMM_WORLD, &barreira);                 // começa a barreira e volta na hora
while (!todos_chegaram) {
    MPI_Test(&barreira, &todos_chegaram, MPI_STATUS_IGNORE);   // "todos já chegaram?"
    if (!todos_chegaram) usleep(1000);                         // não: dorme 1 ms (sem gastar CPU)
}
```

- `MPI_Ibarrier`: o "I" é de *immediate*. A chamada inicia a barreira e retorna imediatamente, devolvendo
  um "recibo" (`MPI_Request`).
- `MPI_Test`: pergunta se a operação daquele recibo terminou, sem bloquear.
- `usleep(1000)`: o processo dorme 1 milissegundo; dormindo, não usa CPU.

Agora os ranks 1 a 3 ficam **dormindo** enquanto o rank 0 mede o sequencial, o núcleo do rank 0 roda com
o turbo normal, e a eficiência do cíclico fica em 97–98%<!--num-->.

**Por que a espera ativa existe?** Porque em clusters cada processo tem um núcleo só para ele, e ficar
perguntando sem parar reage mais rápido do que dormir e ser acordado pelo sistema operacional. É uma
escolha boa para supercomputador e ruim para notebook.

**Por que o `MPI_Barrier` dentro de `rodar` não foi trocado?** Porque ali todos os ranks chegam
praticamente juntos; a espera é de microssegundos e não esquenta nada. Ele só alinha a largada do
cronômetro.

**Lição para falar ao professor:** "Desconfiamos de um número bom demais. Eficiência acima de 100% aqui
era sinal de erro de medição; investigamos, achamos a causa (espera ativa mais turbo) e corrigimos."

### 8.10 Resumo da Frente 3 para falar em 1 minuto

"No MPI cada processo tem a sua memória, então decidimos quais linhas cada rank calcula e depois juntamos
no rank 0. O rank 0 lê os parâmetros e espalha com `MPI_Bcast`. Comparamos duas decomposições: em bloco,
que dá a cada rank um pedaço contíguo e fica desbalanceada, com speedup perto de 2 com 4 processos; e
cíclica, que distribui as linhas como cartas, equilibra a carga e chega a 3,9. As linhas voltam com
`MPI_Gatherv`, porque cada rank pode ter uma quantidade diferente; o checksum com `MPI_Reduce` e
`MPI_SUM`; e o tempo é o do rank mais lento, com `MPI_MAX`, incluindo a comunicação. O overhead de
comunicação é a cópia da imagem de 32 MB para o rank 0: pequeno numa máquina só, mas não diminui com mais
processos."

---

## 9. Sequencial × paralelo — leitura dos resultados

### 9.1 Quadro geral (notebook, 2000×2000, checksum 709817858 em todas)

Baseline sequencial: ≈ 2,3 s<!--num-->.

| p | Amdahl (teto teórico) | Previsão em blocos (perfil) | OpenMP static | OpenMP guided | OpenMP dynamic | MPI bloco | MPI cíclico |
|---|---|---|---|---|---|---|---|
| 1 | 1,00 | — | 1,00<!--num--> | 1,00<!--num--> | 1,00<!--num--> | 0,99<!--num--> | 0,99<!--num--> |
| 2 | 1,99<!--num--> | 2,00<!--num--> | 1,96<!--num--> | 1,97<!--num--> | 1,97<!--num--> | 1,95<!--num--> | 1,95<!--num--> |
| 4 | 3,94<!--num--> | 2,12<!--num--> | 2,09<!--num--> | 2,71<!--num--> | **3,90**<!--num--> | 1,92<!--num--> | **3,84**<!--num--> |
| 8 | 7,74<!--num--> | 3,41<!--num--> | 3,34<!--num--> | 4,99<!--num--> | **6,30**<!--num--> | 2,81<!--num--> | 6,24<!--num--> |
| 16 | 14,93<!--num--> | 6,17<!--num--> | 5,32<!--num--> | 6,00<!--num--> | **6,58**<!--num--> | — | — |

(MPI foi medido até 8 processos. Todos os números são a mediana de 3 execuções do `make medir`; tabela
completa em `resultados/resumo.md`. Na demonstração ao vivo os números variam um pouco — veja 10.)

Uma observação fina: o `dynamic` com 4 threads (3,90) fica **muito perto** do Amdahl (3,94) — e às vezes,
na demonstração, aparece 3,97, um pouco acima. Não é contradição: o f do perfil é a estimativa
conservadora do **programa inteiro** (inclui alocar o vetor e imprimir), enquanto o tempo medido cobre
**só a região do cálculo**, onde a fração serial é praticamente zero; e há uma variação natural de ~1%
entre execuções. Dentro do que é medido, o teto é 4.

### 9.2 O que cada gráfico mostra

- **`speedup_openmp.svg`** (speedup × threads): a reta ideal (S = p), e as três curvas. A do `dynamic`
  acompanha a ideal até 4 threads e depois dobra para baixo (hyper-threading, depois oversubscription). A
  do `static` fica muito abaixo, seguindo a previsão em blocos do perfil. A do `guided` fica entre as duas.
- **`speedup_mpi.svg`** (speedup × processos): o cíclico perto da ideal até 4; o bloco perto de 2.
- **`tempo_forte.svg`** (tempo × p): o tempo caindo. No ideal, cai pela metade a cada vez que p dobra.
- **`eficiencia.svg`** (eficiência forte e fraca): quanto de cada núcleo foi aproveitado. Cai quando p
  passa de 4 (núcleos físicos).

Frase-chave para apontar no gráfico: **"a distância entre a curva do static e a do dynamic é o custo do
desbalanceamento; a distância entre o dynamic e a reta ideal, depois de 4 threads, é o hardware."**

### 9.3 Por que o real fica abaixo de Amdahl

Amdahl só considera a fração serial (f ≈ 0,48%) e promete S(8) ≈ 7,7 e S(16) ≈ 14,9. O real fica abaixo
por causa do que Amdahl ignora. Em ordem de importância **neste projeto**:

| # | Causa | O que é | Evidência no projeto |
|---|---|---|---|
| 1 | **Desbalanceamento de carga** | com divisão fixa, o tempo é o do pedaço mais caro | `static` com 4: 2,09<!--num--> contra 3,94 de Amdahl; desbalanceamento 1,88<!--num-->; MPI bloco: r1 e r2 com ≈ 1,2 s, r0 e r3 com ≈ 0,07 s. O perfil previu 2,12. `dynamic` e cíclico eliminam (desbalanceamento 1,00) |
| 2 | **Núcleos lógicos × físicos (SMT)** | as threads 5 a 8 dividem núcleos com as 1 a 4 | `dynamic`: eficiência 97,5%<!--num--> com 4 e 78,8%<!--num--> com 8, com desbalanceamento 1,00 nos dois casos |
| 3 | **Oversubscription** | 16 threads em 8 núcleos lógicos | `dynamic`: 6,30 → 6,58<!--num-->; eficiência 41,1%<!--num--> |
| 4 | **Overhead de comunicação (MPI)** | copiar a imagem (32 MB) para o rank 0 | ≈ 1,8%<!--num--> do tempo com 4 processos e 3,5% com 8 no cíclico; o cíclico fica em 3,84 contra 3,90 do `dynamic`. Não diminui com p |
| 5 | **Turbo e temperatura** | o sequencial roda com frequência mais alta que cada núcleo no paralelo | o caso da eficiência de 104–112% antes da correção (8.9) mostra o tamanho do efeito: ≈ 12% |
| 6 | **Criação e sincronização de threads** | fork, fila do `dynamic`, barreira | desprezível em 2000×2000: 1 thread dá speedup 1,00, e `dynamic` com 4 dá 97,5% |
| 7 | **False sharing** | threads escrevendo na mesma linha de cache | quase não ocorre, por projeto: cada thread escreve linhas inteiras (16 KB); só há contato na fronteira entre linhas |
| 8 | **Banda de memória** | núcleos disputando a RAM | não é gargalo: o problema é *compute-bound* (177 iterações em registradores para cada 8 bytes escritos); o speedup acompanha os núcleos físicos |

As causas 7 e 8 estão na lista do enunciado ("false sharing, limite de banda de memória"). A resposta
correta para este problema é: **foram analisadas e não são relevantes aqui, e sabemos explicar por quê.**
Dizer que um fator não pesa, com justificativa, também é análise.

### 9.4 Forte × fraca nos resultados

- **Forte** (imagem fixa): o trabalho por thread diminui quando p cresce, então os custos fixos pesam
  cada vez mais em proporção. A eficiência cai.
- **Fraca** (imagem cresce com p): o trabalho por thread fica constante; o ideal é o tempo não mudar.
  Espera-se eficiência alta até 4 (núcleos físicos) e queda com 8 e 16, pela mesma razão de hardware:
  com 8 threads, cada thread tem só "meio núcleo". No MPI, soma-se o fato de que a imagem a transmitir
  também cresce com p.
- Os valores medidos estão em `resultados/resumo.md` (tabela "Escalabilidade fraca").

### 9.5 Conclusões em três frases

1. O problema é quase 100% paralelizável (f < 1%), então o limite prático **não** é Amdahl.
2. O que decide o desempenho é o **balanceamento de carga**: a mesma máquina dá speedup ≈ 2 ou ≈ 4 com 4
   núcleos, dependendo só de **como** as linhas são distribuídas.
3. Com a carga equilibrada, o limite passa a ser o **hardware**: 4 núcleos físicos.

---

## 10. Roteiro da demonstração ao vivo (~9 min)

### 10.1 Comandos

```
cd ~/projetos/Projeto-Academico-PP-Mandelbrot
podman run --rm -it --shm-size=1g -v "$PWD:/projeto:Z" -w /projeto localhost/iesb/pp-lab:2026.2
make && make demo
```

O que cada parte do `podman run` significa (pode ser perguntado):

| Parte | Significado |
|---|---|
| `--rm` | apaga o contêiner ao sair |
| `-it` | terminal interativo |
| `--shm-size=1g` | aumenta a memória compartilhada (`/dev/shm`) que o OpenMPI usa para as mensagens |
| `-v "$PWD:/projeto:Z"` | espelha a pasta do projeto dentro do contêiner; `:Z` ajusta a permissão do SELinux (necessário no Fedora/Bluefin) |
| `-w /projeto` | pasta de trabalho inicial |
| `localhost/iesb/pp-lab:2026.2` | a imagem com Ubuntu, g++, make e OpenMPI |

`make demo` roda `scripts/demo.sh`, que pausa entre as etapas esperando ENTER. Variáveis úteis:
`PAUSA=0 make demo` (sem pausas) e `NP=8 make demo` (MPI com 8 processos; o padrão é 4).

### 10.2 O que falar em cada etapa

Combine antes com o Bruno quem fala em cada etapa. Sugestão: João na máquina, Frente 1 e Frente 3; Bruno
na Frente 2 e nos gráficos. Tempo total: ~9 minutos.

**Etapa 0 — Máquina (~30 s).** A tela mostra os núcleos lógicos (8) e o processador.
> "Core i5-9300H: 4 núcleos físicos e 8 lógicos, com hyper-threading. Guardem esse 4: ele explica onde as
> curvas dobram."

**Etapa 1 — Frente 1: baseline e perfil (~2 min).** Roda `mandelbrot_seq 2000 2000` e o perfil.
> "Este é o código do professor, sem nenhuma alteração. O checksum 709817858 é o gabarito: tudo o que
> rodar depois tem de dar esse número. O tempo sequencial fica perto de 2,3 segundos."
>
> "No perfil: mais de 99% do tempo está no cálculo dos pixels, então é isso que se paraleliza. A fração
> serial é menor que 1%, e Amdahl promete quase 4 com 4 núcleos. Mas olhem a carga por faixa de linhas:
> as duas faixas centrais têm metade do custo e as das pontas quase nada. O perfil já prevê que dividir
> em 4 blocos iguais dá speedup de só 2,1. Vamos ver essa previsão se confirmar."

**Etapa 2 — Frente 2 (1/2): condição de corrida (~1,5 min).** Roda `corrida_demo 1000 1000 8`.
> "A mesma soma do checksum, feita por 8 threads de quatro maneiras. Sem proteção, o resultado está
> errado e muda a cada execução: é a condição de corrida. `critical`, `atomic` e `reduction` acertam, mas
> com custos diferentes: a `reduction` é a mais rápida porque cada thread soma na sua cópia privada. Foi a
> escolhida."

**Etapa 3 — Frente 2 (2/2): speedup e escalonamento (~2 min).** Roda `mandelbrot_omp 2000 2000`.
> "Três escalonamentos, de 1 a 16 threads. Coluna `resultado`: todas iguais ao sequencial, pixel a pixel.
> `static` com 4 threads: speedup 2,1, exatamente o que o perfil previu, e desbalanceamento perto de 1,9.
> `dynamic` com 4: perto de 4, desbalanceamento 1,00. De 4 para 8 o ganho diminui porque são núcleos
> lógicos; com 16 já há mais threads que núcleos."

**Etapa 4 — Frente 3: MPI (~1,5 min).** Roda `mpirun -np 4 mandelbrot_mpi 2000 2000`.
> "Quatro processos, cada um com a sua memória. Mesmo checksum nas duas decomposições. Em bloco: speedup
> perto de 2; vejam o tempo por rank, os do meio trabalham mais de um segundo e os das pontas quase nada.
> Cíclica: perto de 3,9, com os quatro ranks iguais. A coluna comunicação mostra o custo de trazer a
> imagem para o rank 0 com `Gatherv`."

**Etapa 5 — Gráficos (~1 min).** `make graficos` gera os SVG a partir do CSV. Abra
`graficos/speedup_openmp.svg` no navegador (deixe as abas **já abertas** antes de começar; a pasta é a
mesma dentro e fora do contêiner).
> "Gráfico gerado dos dados medidos pelo `make medir`. A reta é o ideal. O `dynamic` acompanha até 4
> threads; o `static` fica embaixo, pelo desbalanceamento."

### 10.3 Dicas práticas

Antes do dia:

- Rode `make medir && make graficos` **com antecedência** (leva alguns minutos; não se faz ao vivo). O
  `make demo` só gera os gráficos se `resultados/medicoes.csv` existir.
- Confirme que a imagem do contêiner existe: `podman images | grep pp-lab`.
- Ensaie o roteiro inteiro com cronômetro pelo menos duas vezes, falando em voz alta.

No dia:

- **Notebook na tomada.** Na bateria o processador limita a frequência e os números pioram e variam.
- **Feche programas pesados** (navegador com muitas abas, jogos, Steam, wallpaper animado). Qualquer
  programa usando CPU rouba núcleo e turbo.
- **Rode `make && make demo` uma vez antes** de ser chamado (com `PAUSA=0`), para compilar, carregar tudo
  na memória e confirmar que funciona. Depois **espere um ou dois minutos** para o notebook esfriar.
- Aumente a **fonte do terminal** e deixe o terminal em tela cheia.
- Deixe uma segunda aba do terminal aberta na pasta do projeto, para consultar arquivos na arguição.

Se um número sair diferente do esperado:

- **Pequena variação** (3,8 em vez de 3,90): normal. Diga: "varia um pouco entre execuções por causa de
  temperatura e de outros processos; por isso, no protocolo, usamos a mediana de 3 medidas". Aponte para
  o que **não** muda: o checksum, a ordem (dynamic > guided > static) e o desbalanceamento.
- **Speedup geral baixo**: provavelmente o notebook está quente ou na bateria. Explique o turbo e siga.
- **Eficiência acima de 100%**: explique a seção 8.9 (medição do sequencial prejudicada). É uma chance de
  mostrar domínio.
- **Checksum diferente**: não deve acontecer nunca. Se acontecer, não esconda: é erro de corretude, e não
  de desempenho.
- **Erro de permissão na pasta**: faltou o `:Z` no `-v`. **Erro de memória compartilhada no MPI**: faltou
  o `--shm-size=1g`.
- O `corrida_demo` "sem proteção" **tem** de dar errado e diferente a cada vez: é o esperado. Se por acaso
  uma execução acertar, o programa escreve "OK (sorte!)"; explique que a corrida é não determinística.

---

## 11. Banco de perguntas e respostas

Respostas pensadas para falar em voz alta, em 20 a 40 segundos. Entre parênteses, a seção para rever.

### 11.1 Matemática e contexto — básico

**1. O que é um número complexo?**
É um número da forma `a + bi`, em que `a` é a parte real, `b` é a parte imaginária e `i` é a unidade
imaginária, definida por `i² = −1`. Pode ser visto como um ponto (a, b) de um plano, o plano complexo. No
código, cada complexo são dois `double`. (2.1)

**2. O que é o módulo de um número complexo?**
É a distância do ponto até a origem do plano: `|z| = √(a² + b²)`, por Pitágoras. O programa usa o módulo
ao quadrado, `zr² + zi²`, para não calcular a raiz. (2.1, 2.4)

**3. O que é o Conjunto de Mandelbrot?**
É o conjunto dos números complexos `c` para os quais a sequência `z ← z² + c`, começando em `z = 0`,
permanece limitada, isto é, não vai para o infinito. Cada pixel da imagem é um `c` diferente. (2.3)

**4. Por que no código aparece `zr*zr − zi*zi` e `2*zr*zi`?**
É o quadrado de um complexo. `(zr + zi·i)² = zr² + 2·zr·zi·i + zi²·i²`, e como `i² = −1` fica
`(zr² − zi²) + (2·zr·zi)·i`. A primeira expressão é a parte real e a segunda, a imaginária; depois soma-se
`cr` e `ci`. (2.1)

**5. O que o programa guarda para cada pixel?**
O número de iterações até o ponto escapar, ou 1000 se não escapou. Isso se chama tempo de fuga. É o valor
que normalmente vira cor nas imagens do fractal. (2.6)

**6. Quem foi Mandelbrot e quando o conjunto surgiu?**
Benoît Mandelbrot era matemático e pesquisador da IBM. Criou o termo "fractal" em 1975 e visualizou e
estudou o conjunto em 1980, com os computadores da IBM. A base matemática vem de Julia e Fatou, por volta
de 1918. (3.1)

**7. O que é um fractal?**
Uma forma com detalhe em todas as escalas, em que partes ampliadas lembram o todo (auto-semelhança). A
borda do Mandelbrot é infinitamente complexa: por mais zoom que se dê, nunca fica lisa. (3.2)

**8. Para que serve o Mandelbrot?**
O conjunto em si é mais um objeto de estudo (sistemas dinâmicos, caos), um benchmark de computação
paralela e uma fonte de arte e computação gráfica. Quem tem aplicação prática direta é a geometria
fractal: geração de terrenos e texturas, antenas fractais, compressão fractal, modelagem de formas
naturais. (3.5)

### 11.2 Matemática e contexto — intermediário e avançado

**9. Por que o critério de parada é |z| > 2?**
Porque está provado que, se o módulo passa de 2, a sequência vai para o infinito. Com `|z| > 2` e
`|z| ≥ |c|`, temos `|z² + c| ≥ |z|² − |c| ≥ |z|·(|z| − 1)`, que é maior que `|z|`; o módulo cresce a cada
passo, cada vez mais rápido. Então, passou de 2, o ponto está fora. (2.4)

**10. Por que o código compara com 4 e não com 2?**
Porque compara o módulo **ao quadrado**: `zr² + zi² ≤ 4` equivale a `|z| ≤ 2`. Assim evita a raiz
quadrada, que é cara, num teste que roda centenas de milhões de vezes. (2.4)

**11. O que significa MAXIT = 1000? O resultado é exato?**
É o limite de iterações por pixel. Quem não escapou em 1000 passos é tratado como pertencente ao
conjunto. É uma aproximação: um ponto muito perto da borda poderia escapar depois de 1000. Não podemos
alterar, porque faz parte da lógica do baseline. (2.5)

**12. Como um pixel vira um número complexo?**
Por interpolação linear: `cr = −2,5 + 3,5·x/W` e `ci = −1,25 + 2,5·y/H`. A coluna x percorre o eixo real
de −2,5 a 1,0 e a linha y percorre o eixo imaginário de −1,25 a 1,25. Essa janela contém o conjunto
inteiro. (2.7)

**13. Qual a complexidade do algoritmo?**
`O(W·H·MAXIT)` no pior caso: W·H pixels, até MAXIT iterações cada. O custo real é W·H vezes a média de
iterações, que é 177,5. Dobrar o lado da imagem quadruplica o tempo, como o baseline mostra. (2.11)

**14. Qual a relação entre Mandelbrot e Julia?**
A fórmula é a mesma. No Julia, `c` é fixo e o pixel é o ponto de partida `z₀`; no Mandelbrot, `z₀ = 0` e
o pixel é `c`. O Mandelbrot funciona como um mapa: se `c` está dentro dele, o conjunto de Julia daquele
`c` é conexo. (3.4)

**15. O que é o checksum e por que ele prova que a versão paralela está certa?**
É a soma das iterações de todos os pixels. O cálculo é determinístico e a soma é de inteiros, exata e
independente da ordem; qualquer linha pulada, repetida ou soma perdida muda o valor. Como reforço, o
OpenMP compara a imagem pixel a pixel e o MPI confere a imagem montada no rank 0. (2.9)

**16. Para que serve `-ffp-contract=off`? O que aconteceria sem?**
Proíbe o compilador de fundir multiplicação e soma na instrução FMA, que arredonda uma vez em vez de
duas. Sem a flag, se o compilador usasse FMA (por exemplo com `-march=native`), o último bit de algumas
contas mudaria; em pixels da borda isso pode mudar o número de iterações, e o checksum deixaria de bater
com o gabarito. (2.10)

**17. Por que o Mandelbrot é um bom benchmark de programação paralela?**
Porque os pixels são independentes (fácil de dividir), a carga é muito desigual (testa o balanceamento),
é limitado por CPU e não por memória (mede o processador de forma limpa) e o resultado é determinístico
(fácil de validar). (3.6)

### 11.3 Fundamentos de paralelismo

**18. Qual a diferença entre processo e thread?**
Processo tem memória própria; threads de um mesmo processo compartilham a memória. Threads são mais
baratas e se comunicam direto pelas variáveis, mas correm risco de condição de corrida. Processos
precisam trocar mensagens. OpenMP usa threads; MPI usa processos. (5.2)

**19. Memória compartilhada × distribuída?**
Na compartilhada, todos enxergam os mesmos dados: não há cópia, mas é preciso evitar corrida, e fica-se
limitado a uma máquina. Na distribuída, cada processo tem os seus dados: não há corrida, mas há custo de
comunicação, e pode-se usar várias máquinas. (5.3)

**20. O que é speedup e eficiência?**
Speedup é o tempo sequencial dividido pelo tempo paralelo: quantas vezes ficou mais rápido. Eficiência é
o speedup dividido pelo número de threads ou processos: quanto de cada núcleo foi aproveitado. No
notebook, `dynamic` com 4 threads: speedup 3,90, eficiência 97,5%. (5.7)

**21. O que diz a Lei de Amdahl?**
Que o speedup é limitado pela parte do programa que não se paraleliza: `S(p) = 1 / (f + (1 − f)/p)`, com
teto `1/f`. No projeto, f ≈ 0,5%, o que daria até S ≈ 3,94 com 4 núcleos e um teto por volta de 200. Ela
supõe divisão perfeita e nenhum overhead. (5.8)

**22. O que é a Lei de Gustafson?**
É a visão complementar: se o problema cresce junto com o número de núcleos, a parte paralela cresce e a
serial não, e o speedup escalado é `S = p − f·(p − 1)`, quase linear. Amdahl corresponde à escalabilidade
forte; Gustafson, à fraca. (5.9)

**23. Escalabilidade forte × fraca? Como o projeto mede?**
Forte: problema fixo (2000×2000), mais núcleos; o ideal é o tempo cair para T/p. Fraca: o problema cresce
com p, mantendo cerca de 1 milhão de pixels por thread; o ideal é o tempo ficar constante. Na fraca o
lado é `1000·√p`, porque o trabalho é proporcional à área. (5.10)

**24. O que é um problema embaraçosamente paralelo? O Mandelbrot é?**
É um problema cujas partes são independentes e não precisam se comunicar durante o cálculo. O Mandelbrot
é, entre pixels. Mas tem dois poréns: dentro de um pixel as iterações são sequenciais, e a carga por
linha é muito desigual, então dividir é fácil e dividir bem é o desafio. (5.5)

**25. O que é hyper-threading e por que ele ajuda aqui?**
É a técnica de fazer um núcleo físico aparecer como dois lógicos, que dividem as mesmas unidades de
cálculo. Ajuda no Mandelbrot porque o laço é uma cadeia de dependências (cada iteração espera a
anterior), o que deixa unidades ociosas que a segunda thread aproveita. O ganho é parcial: de 4 para 8
threads o speedup foi de 3,90 para 6,30, não para 8. (5.12)

**26. Quais as classes da taxonomia de Flynn? Onde o projeto se encaixa?**
SISD, SIMD, MISD e MIMD, conforme haja um ou vários fluxos de instruções e de dados. Um processador
multicore rodando OpenMP ou MPI é MIMD. O estilo do MPI, o mesmo programa em todos os processos, é
chamado SPMD. (5.4)

### 11.4 Frente 1 — Baseline e perfilamento

**27. O que é perfilamento e o que é hotspot?**
Perfilar é medir onde o programa gasta o tempo. Hotspot é o trecho que concentra o tempo. No projeto,
cronometramos cada trecho: o cálculo dos pixels é o hotspot, com mais de 99% do tempo. É ele que vale a
pena paralelizar. (6.4)

**28. Qual foi o protocolo de medição?**
Compilar com `-O2`; medir só o cálculo, com relógio monotônico; uma execução de aquecimento descartada e
três medidas; usar a mediana; conferir o checksum de toda execução; registrar a máquina. O mesmo
protocolo vale para as três versões. (6.3)

**29. Por que mediana e não média?**
Porque a mediana não é afetada por um valor fora da curva. Se uma das três execuções for atrapalhada pelo
sistema, a média sobe e a mediana não. Ela representa melhor o tempo típico. (6.3)

**30. Por que descartar a primeira execução?**
Porque a primeira paga custos que as outras não pagam: carregar o programa e as bibliotecas do disco,
subir a frequência do processador, criar as threads. Não representa o desempenho em regime. (6.3)

**31. Por que CLOCK_MONOTONIC?**
É um relógio que só anda para a frente e não é alterado se a hora do sistema for ajustada. É tempo de
relógio de parede, que é o que interessa: quanto o usuário espera. Tempo de CPU somaria todas as threads
e esconderia o ganho. (6.3)

**32. Como a fração serial foi estimada?**
Pelo perfil: `f = (tempo total − tempo do cálculo) / tempo total`. Conta como serial a alocação e a
zeragem do vetor, a soma do checksum e a impressão. Deu cerca de 0,5%. É conservadora, porque no OpenMP a
soma também é paralelizada. (6.7)

**33. O que o perfil mostrou sobre a carga, e o que ele previu?**
Que o custo por linha é muito desigual: as 20% de linhas centrais têm cerca de 50% do custo. Simulando a
divisão em blocos contíguos, previu speedup máximo de 2,12 com 4 blocos e 3,41 com 8. O `static` medido
deu 2,09 e 3,34: a previsão se confirmou. (6.5, 6.6)

**34. Por que existe o arquivo `mandelbrot_comum.hpp` se não se pode alterar o baseline?**
O baseline continua intocado e é compilado sozinho. O cabeçalho só copia o corpo do laço, com as mesmas
expressões na mesma ordem, para uma função que as outras versões chamam. Assim todas fazem exatamente a
mesma conta, e o checksum tem de bater. (6.2)

### 11.5 Frente 2 — OpenMP — básico

**35. O que é OpenMP e como ele funciona?**
É um padrão de paralelismo em memória compartilhada, baseado em diretivas `#pragma omp` acrescentadas ao
código. Segue o modelo fork-join: a thread principal cria uma equipe na região paralela e, no fim, todas
se juntam de novo. Compila-se com `-fopenmp`. (5.6, 7.1)

**36. Como o Mandelbrot foi paralelizado com OpenMP?**
Com um laço paralelo sobre as linhas da imagem: `parallel for` no laço de `y`. Cada thread calcula linhas
inteiras e escreve no vetor compartilhado. O checksum usa `reduction(+:soma)` e o escalonamento é
`schedule(dynamic, 1)`. (7.1)

**37. O que é condição de corrida?**
É quando duas ou mais threads acessam a mesma memória ao mesmo tempo, pelo menos uma escreve e não há
sincronização. O resultado depende de quem chega primeiro: fica errado e muda a cada execução. O exemplo
é `soma += it`, que são três passos: ler, somar e escrever. (7.4)

**38. Onde há condição de corrida no projeto e onde não há?**
Na imagem não há: cada linha é calculada por uma única thread e ocupa posições próprias do vetor. Na soma
do checksum haveria, porque todas as threads somariam na mesma variável. Tratamos com `reduction`. (7.4)

**39. Qual a diferença entre static, dynamic e guided?**
`static` divide as linhas em blocos iguais antes de começar: custo zero, mas não equilibra. `dynamic` é
uma fila: cada thread pega um pedaço quando termina o anterior; equilibra. `guided` é uma fila com pedaços
que começam grandes e diminuem. (7.6)

### 11.6 Frente 2 — OpenMP — intermediário

**40. Por que paralelizar as linhas e não os pixels?**
Porque a linha é um pedaço de trabalho de bom tamanho: cerca de 1 ms, contra menos de 1 microssegundo do
pixel. Por pixel, seriam 4 milhões de idas à fila em vez de 2000, e threads vizinhas escreveriam na mesma
linha de cache (false sharing). E 2000 linhas já bastam para balancear bem. (7.2)

**41. Por que laço paralelo e não tasks ou sections?**
`sections` serve para poucos blocos de código diferentes; aqui é o mesmo código repetido. Tasks servem
para trabalho recursivo ou descoberto durante a execução, como o mergesort; aqui as 2000 linhas são
conhecidas de antemão. O laço paralelo é o padrão natural e o mais simples. (7.2)

**42. Como funcionam critical, atomic e reduction?**
`critical` é uma tranca: só uma thread por vez executa o bloco; serve para qualquer código, mas é o mais
caro. `atomic` faz uma única operação simples de forma indivisível pela CPU; mais barato, mas todas
disputam a mesma variável. `reduction` dá a cada thread uma cópia privada e combina no fim. (7.5)

**43. Por que reduction e não atomic?**
Porque a `reduction` elimina a disputa, e o `atomic` só a organiza. Com `atomic`, a cada pixel as threads
brigam pela mesma variável, e a linha de cache dela fica passando de núcleo em núcleo. Com `reduction`,
cada thread soma na sua cópia e há só p somas no final. Medimos: 7,04 contra 6,46 com 8 threads. (7.5)

**44. Por que o static com 4 threads dá speedup de só 2,1?**
Porque divide em 4 blocos contíguos, e os dois blocos do meio ficam com quase todo o trabalho, já que as
linhas centrais atravessam o conjunto. As threads das pontas terminam logo e ficam paradas; o tempo é o
da mais lenta. O desbalanceamento medido foi 1,88, e o perfil tinha previsto 2,12. (7.6)

**45. Pegadinha: por que o static com 2 threads vai bem?**
Pela simetria do conjunto em relação ao eixo real. O `static` com 2 threads corta a imagem exatamente no
eixo real (linha 1000), e as duas metades custam o mesmo. O speedup dá 1,96. Com 4 threads a simetria não
ajuda mais. (2.8, 7.6)

**46. Pegadinha: dynamic com chunk 1 não é caro?**
Aqui não. O custo é uma operação atômica por linha, 2000 no total, contra cerca de 1 ms de cálculo por
linha: milhares de vezes menor. Seria caro se cada iteração do laço fosse minúscula, como um pixel. A
prova é a eficiência de 97,5% com 4 threads. (7.6)

**47. Por que o guided ficou pior que o dynamic?**
Porque os primeiros pedaços do `guided` são grandes, centenas de linhas, e um deles cai na região central
cara. A thread que o pega fica presa enquanto as outras terminam. O `guided` é bom quando ir à fila é caro
e o custo não é concentrado; aqui é o contrário. Com 4 threads deu 2,71 contra 3,90. (7.6)

**48. Quais variáveis são compartilhadas e quais são privadas?**
Compartilhadas: W, H (só lidas) e o vetor da imagem (escrito em posições diferentes). Privadas: a
variável do laço `y` e todas as locais do cálculo (`x`, `zr`, `zi`, `it`...), que ficam na pilha de cada
thread. A `soma` é de redução: privada durante o laço e combinada no final. (7.3)

**49. Para que serve o nowait?**
Tira a barreira implícita do fim do `omp for`. Sem ele, todas as threads esperariam a mais lenta antes de
anotar o próprio tempo, e o desbalanceamento medido seria sempre 1,00. A barreira do fim da região
`parallel` continua lá e garante a corretude. (7.8)

**50. O speedup é calculado contra o quê?**
Contra o sequencial: o baseline do professor nos gráficos, e o laço sequencial do próprio programa no
modo demonstração. Não é contra o OpenMP com 1 thread, porque isso esconderia o overhead da versão
paralela. (7.9)

### 11.7 Frente 2 — OpenMP — avançado e pegadinhas

**51. Por que a versão sem proteção perde tanto, e não só alguns pixels?**
Com `-O2` o compilador mantém a soma num registrador por um trecho longo e só grava na memória no fim,
porque supõe que ninguém mais mexe nela (corrida de dados é comportamento indefinido em C++). Quando
grava, apaga o que as outras threads gravaram nesse meio tempo. Perdem-se trechos inteiros: sobra mais ou
menos a parte de uma thread. (7.5)

**52. Por que de 4 para 8 threads o speedup não dobra?**
Porque o notebook tem 4 núcleos físicos. As threads 5 a 8 rodam nos núcleos lógicos do hyper-threading,
dividindo as unidades de cálculo com as outras. O ganho é parcial: 3,90 para 6,30. O desbalanceamento
continua 1,00, então a causa é o hardware, não o programa. (5.12, 7.6)

**53. Pegadinha: por que 16 threads quase não ajudam no notebook?**
Porque só existem 8 núcleos lógicos. Com 16 threads há oversubscription: o sistema operacional reveza as
threads nos mesmos núcleos, sem capacidade nova. O `dynamic` vai de 6,30 para 6,58 e a eficiência cai
para 43%. (5.12)

**54. Mas o static com 16 threads é melhor que com 8. Por quê?**
Porque 16 blocos são menores. As threads dos blocos baratos terminam logo e liberam os núcleos para as
dos blocos caros; o sistema operacional acaba redistribuindo o trabalho. É um balanceamento indireto, e
ainda fica abaixo do `dynamic`. (7.6)

**55. O que é false sharing e por que quase não acontece aqui?**
É quando threads escrevem em variáveis diferentes que estão na mesma linha de cache de 64 bytes; a linha
fica passando de um núcleo para outro e o programa fica lento, embora correto. Aqui cada thread escreve
linhas inteiras da imagem, 16 KB contíguos, então só há contato na fronteira entre duas linhas. E a soma
é privada pela `reduction`. (5.11, 9.3)

**56. A banda de memória limita o speedup aqui?**
Não. O problema é limitado por CPU: cada pixel faz em média 177 iterações só com registradores e escreve
8 bytes no final. A memória quase não é usada durante a conta. Por isso o speedup acompanha os núcleos
físicos. Seria diferente numa multiplicação de matrizes. (5.11)

**57. E se usássemos collapse(2)?**
O laço duplo viraria um laço de W·H iterações distribuídas por pixel. Com `dynamic, 1` seriam 4 milhões
de idas à fila e mais false sharing; com `static` o desbalanceamento continuaria. Não traz vantagem.
`collapse` é útil quando o laço externo tem poucas iterações. (7.2)

**58. Pegadinha: é possível eficiência acima de 100%?**
Em geral só com efeito de cache (speedup superlinear), que não existe neste problema. Aqui, se aparecer,
é erro de medição. Aconteceu no MPI: os processos em espera ativa esquentavam a CPU durante a medida do
sequencial, que ficava 12% mais lenta e inflava o speedup. Corrigimos fazendo os processos dormirem.
(8.9)

**59. O que mudaria se a imagem fosse 100×100?**
O trabalho cairia 400 vezes, para poucos milissegundos. Os custos fixos (criar threads, iniciar e
comunicar no MPI) passariam a ser comparáveis ao cálculo, e o speedup cairia bastante. Com só 100 linhas,
o balanceamento também ficaria mais grosseiro, e as medidas, mais ruidosas. Paralelismo compensa quando o
trabalho é grande perto do overhead. (5.11)

**60. Uma GPU seria melhor? Por quê?**
Para este problema, provavelmente sim: é embaraçosamente paralelo, e uma GPU calcula milhares de pixels
ao mesmo tempo. Os cuidados seriam o número diferente de iterações por pixel (uns terminam antes e
esperam), o desempenho em precisão dupla e garantir o mesmo arredondamento para o checksum bater. O
projeto pedia OpenMP e MPI, então não implementamos. (3.6)

### 11.8 Frente 3 — MPI

**61. O que é MPI? O que é rank e comunicador?**
MPI é um padrão de troca de mensagens entre processos, para memória distribuída. Rank é o número de cada
processo, de 0 a p − 1. Comunicador é o grupo de processos que se comunicam; `MPI_COMM_WORLD` contém
todos. O mesmo programa roda em todos os processos (SPMD). (8.1)

**62. Como o Mandelbrot foi paralelizado com MPI?**
Por decomposição de domínio: cada rank calcula um conjunto de linhas na sua própria memória. O rank 0
transmite os parâmetros com `MPI_Bcast`, cada rank calcula, e no fim as linhas vão para o rank 0 com
`MPI_Gatherv` e os checksums parciais são somados com `MPI_Reduce`. (8.1–8.3)

**63. Qual a diferença entre decomposição em bloco e cíclica?**
Em bloco, cada rank recebe um pedaço contíguo de H/p linhas. Na cíclica, o rank r recebe as linhas r,
r+p, r+2p...; é como distribuir cartas. A cíclica equilibra porque cada rank recebe um pouco de cada
região: speedup 3,9 contra perto de 2 do bloco, com 4 processos. (8.2)

**64. Pegadinha: por que Gatherv e não Gather?**
Porque `Gather` exige que todos enviem a mesma quantidade. Quando H não é divisível por p, os ranks têm
números diferentes de linhas (2000 linhas em 3 processos: 667, 667 e 666). O `Gatherv` recebe um vetor de
quantidades e um de deslocamentos, e funciona para qualquer H e p. (8.4)

**65. Para que servem o Bcast e o Reduce?**
`MPI_Bcast` envia um dado do root para todos: aqui, W, H e o modo. `MPI_Reduce` combina um valor de cada
processo com uma operação e entrega o resultado ao root: usamos `MPI_SUM` para o checksum e `MPI_MAX`
para o tempo. É o equivalente da `reduction` do OpenMP. (8.3)

**66. Qual a diferença entre comunicação ponto-a-ponto e coletiva?**
Ponto-a-ponto é entre dois processos: um faz `Send`, o outro `Recv`. Coletiva envolve todos os processos
do comunicador, numa única chamada, e a biblioteca otimiza a distribuição. Usamos só coletivas: código
mais simples e mais eficiente. (8.3)

**67. Por que o tempo do MPI é o do rank mais lento?**
Porque o programa só termina quando o último processo termina e a imagem está no rank 0. Cada rank mede o
seu tempo, incluindo a comunicação, e o `MPI_Reduce` com `MPI_MAX` pega o maior. (8.5)

**68. O que é o overhead de comunicação e quanto é aqui?**
É o tempo gasto trocando mensagens em vez de calcular. Medimos como o tempo total menos o maior tempo de
cálculo. O que trafega é a imagem: 32 MB em 2000×2000. Com 4 processos é cerca de 1,8% do tempo no cíclico (3,5% com 8); esse
custo não diminui com mais processos, então cresce em proporção quando o cálculo encolhe. (8.6)

**69. Pegadinha: o cíclico tem mais comunicação que o bloco?**
Em volume, não: nos dois a imagem inteira vai para o rank 0. O cíclico tem a reorganização das linhas
intercaladas, que é uma cópia barata. Em porcentagem a comunicação tende a pesar mais no cíclico, porque
o tempo total dele é a metade: o mesmo custo dividido por um total menor. (8.6)

**70. Pegadinha: por que o resultado do MPI não muda com o número de processos?**
Porque cada pixel é calculado pela mesma função com as mesmas entradas, não importa em qual processo; a
decomposição cobre cada linha exatamente uma vez para qualquer p; e o checksum é soma de inteiros, exata
e independente da ordem. (8.7)

**71. O que é espera ativa e como ela atrapalhou a medição?**
Espera ativa é o processo ficar num laço perguntando "já chegou?", usando 100% de um núcleo. O OpenMPI
faz isso em chamadas bloqueantes como o `MPI_Barrier`. No notebook, três processos esperando assim
esquentavam a CPU e tiravam o turbo do rank 0 durante a medida do sequencial. Trocamos por
`MPI_Ibarrier` com `MPI_Test` e `usleep`: os processos dormem. (8.9)

**72. Quando usar OpenMP e quando usar MPI?**
OpenMP numa máquina multicore: é mais simples e não copia dados. MPI quando é preciso usar várias
máquinas ou mais memória do que uma tem. Em clusters é comum o híbrido: MPI entre as máquinas e OpenMP
dentro de cada uma. (8.8)

**73. Por que não usar mestre-escravo no MPI, para balancear dinamicamente?**
Porque exigiria uma mensagem por linha e um processo só coordenando. A decomposição cíclica não tem
nenhuma comunicação durante o cálculo e já deixa o desbalanceamento em 1,00, porque o custo das linhas
varia suavemente. O mestre-escravo valeria a pena com carga imprevisível. (8.2)

### 11.9 Resultados e visão geral

**74. Por que o speedup real fica abaixo de Amdahl?**
Porque Amdahl só considera a fração serial e supõe divisão perfeita e sem custo. Aqui as causas são,
nesta ordem: desbalanceamento de carga (no `static` e no bloco), núcleos lógicos em vez de físicos acima
de 4 threads, oversubscription com 16, comunicação no MPI, e turbo e temperatura. False sharing e banda
de memória foram analisados e não pesam. (9.3)

**75. Qual o melhor resultado e por quê?**
`dynamic` com 4 threads: speedup 3,90, eficiência 97,5%, porque usa os 4 núcleos físicos com a carga
equilibrada. O maior speedup absoluto é com 16 threads (6,58), mas com eficiência de 41%. O MPI cíclico
com 4 processos chega a 3,84: um pouco abaixo, pelo custo de copiar a imagem. (9.1)

**76. Compare o OpenMP dynamic com o MPI cíclico.**
Os dois resolvem o mesmo problema, o desbalanceamento, por caminhos diferentes: o `dynamic` decide em
tempo de execução, com uma fila; o cíclico decide de antemão, intercalando as linhas. O desempenho é
parecido; o MPI paga a cópia da imagem e o OpenMP não. O MPI poderia escalar para várias máquinas. (8.8)

**77. O que você faria diferente ou a seguir?**
Testar num cluster de verdade, para ver o overhead de comunicação em rede; uma versão híbrida MPI mais
OpenMP; e uma versão em GPU. No código atual, daria para reduzir a comunicação enviando um tipo de dado
menor que `long`, já que o valor máximo é 1000, mas isso mudaria o formato do baseline.

**78. Qual foi a maior lição do projeto?**
Que paralelizar um problema "fácil" não garante speedup: com a mesma máquina e o mesmo código de cálculo,
o speedup com 4 núcleos vai de 2 a 4 só mudando a distribuição do trabalho. E que medir bem é tão
importante quanto programar: o perfil previu o resultado, e um número bom demais revelou um erro de
medição.

---

## 12. Glossário

| Termo | Em uma linha |
|---|---|
| Amdahl (lei de) | speedup limitado pela fração serial: `S = 1 / (f + (1 − f)/p)` |
| Aquecimento | execução inicial descartada, que paga custos de carga e de frequência da CPU |
| atomic | diretiva OpenMP que torna indivisível uma operação simples sobre uma variável |
| Barreira | ponto em que threads ou processos esperam todos chegarem |
| Baseline | a versão sequencial de referência (do professor); define o resultado correto |
| Bcast | coletiva MPI: o root envia o mesmo dado a todos |
| Bloco (decomposição) | cada processo recebe um pedaço contíguo de linhas |
| Busy-wait (espera ativa) | esperar girando num laço, ocupando 100% de um núcleo |
| Cardioide | a região principal, em forma de coração, do conjunto de Mandelbrot |
| Checksum | soma das iterações de todos os pixels; prova de corretude |
| Chunk | tamanho do pedaço de iterações entregue a uma thread pelo `schedule` |
| Cíclica (decomposição) | o rank r recebe as linhas r, r+p, r+2p, ...; como distribuir cartas |
| Coletiva | operação MPI de que todos os processos do comunicador participam |
| Comunicador | grupo de processos MPI que trocam mensagens (`MPI_COMM_WORLD` = todos) |
| Compute-bound | limitado pela velocidade de cálculo, não pela memória |
| Concorrência | várias tarefas em andamento, possivelmente se revezando |
| Condição de corrida | acesso simultâneo sem sincronização à mesma memória, com pelo menos uma escrita |
| Conjugado | `a − bi`; o espelho de `a + bi` em relação ao eixo real |
| critical | diretiva OpenMP de exclusão mútua: uma thread por vez no bloco |
| Decomposição de domínio | dividir os dados do problema entre os processos |
| Desbalanceamento | maior tempo de thread ou rank dividido pelo tempo médio (1,00 = perfeito) |
| dynamic | escalonamento por fila: quem termina pega o próximo pedaço |
| Eficiência | speedup dividido pelo número de threads ou processos |
| Embaraçosamente paralelo | problema com partes independentes, sem comunicação durante o cálculo |
| Escalabilidade forte | problema fixo, mais núcleos |
| Escalabilidade fraca | problema cresce com os núcleos; trabalho por núcleo constante |
| False sharing | threads escrevendo em variáveis diferentes na mesma linha de cache |
| FMA | instrução que faz `a·b + c` com um único arredondamento |
| Fork-join | modelo do OpenMP: cria a equipe, trabalha, junta |
| Fração serial (f) | parte do tempo que não se paraleliza |
| Fractal | forma com detalhe em todas as escalas e auto-semelhança |
| Gather / Gatherv | coletivas MPI que juntam dados de todos no root; o "v" aceita quantidades diferentes |
| guided | escalonamento por fila com pedaços que começam grandes e diminuem |
| Gustafson (lei de) | speedup escalado quando o problema cresce com p: `S = p − f·(p − 1)` |
| Hotspot | o trecho do programa que concentra o tempo de execução |
| Hyper-threading / SMT | um núcleo físico aparecendo como dois lógicos |
| Julia (conjunto de) | mesma fórmula, com `c` fixo e variando o ponto de partida |
| libgomp | biblioteca de execução do OpenMP no GCC |
| Linha de cache | bloco de 64 bytes; a unidade em que o cache trabalha |
| MAXIT | limite de iterações por pixel (1000) |
| Mediana | o valor do meio das medidas; resistente a valores fora da curva |
| Memory-bound | limitado pela velocidade de acesso à memória |
| Módulo | distância do número complexo à origem: `√(a² + b²)` |
| MPI | padrão de troca de mensagens entre processos (memória distribuída) |
| nowait | remove a barreira implícita do fim de um `omp for` |
| Núcleo físico × lógico | o hardware real × o que o sistema operacional enxerga com SMT |
| OpenMP | padrão de paralelismo por diretivas em memória compartilhada |
| OpenMPI | uma implementação do padrão MPI |
| Overhead | custo extra que o paralelismo acrescenta (criar threads, sincronizar, comunicar) |
| Oversubscription | mais threads ou processos do que núcleos lógicos |
| Paralelismo | várias tarefas executando no mesmo instante em núcleos diferentes |
| Perfilamento | medir onde o programa gasta o tempo |
| Plano complexo | plano em que o eixo horizontal é a parte real e o vertical, a imaginária |
| Processo | programa em execução, com memória própria |
| Rank | número de identificação de um processo MPI (0 a p − 1) |
| Reduce | coletiva MPI que combina um valor de cada processo (soma, máximo...) no root |
| reduction | cláusula OpenMP: cópia privada por thread, combinada no fim |
| Root | o processo que coordena as coletivas (rank 0) |
| schedule | cláusula OpenMP que define como as iterações são distribuídas |
| Seção crítica | trecho de código em que só uma thread pode estar por vez |
| Speedup | tempo sequencial dividido pelo tempo paralelo |
| SPMD | um mesmo programa rodando em vários processos com dados diferentes |
| static | escalonamento em blocos fixos, decididos antes de começar |
| Tempo de fuga | número de iterações até o ponto escapar |
| Thread | fluxo de execução dentro de um processo; compartilha a memória com as outras |
| Turbo boost | aumento automático de frequência quando há folga térmica; menor com todos os núcleos ativos |
