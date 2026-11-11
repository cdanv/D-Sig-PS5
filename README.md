# D-Sig-PS5

**Roteador de sinais de acessibilidade para PlayStation 5 com jailbreak.**

Recebe a entrada do controle, aplica condição, gatilho, tempo e relação entre MODs, e
entrega o resultado ao jogo. É o [D-Sig](https://github.com/cdanv/D-Sig) — que funciona
há um ano no Cronus Zen — saindo do hardware e indo para o console.

> ⚠️ **Em construção.** Hoje existem o roteador vazio e a bancada de testes. A ponte com
> a `libScePad` ainda não existe. Não há nada para instalar.

## Por que sair do Cronus

O D-Sig no Cronus está terminado, e não por falta de vontade: ele bateu no teto do
aparelho. A EEPROM fechou em **64 de 64 palavras**, o campo de destinos está a **99,4%**
da capacidade, e as duas coisas que faltam — gatilho de sequência entre botões
diferentes, e saída para os eixos do touchpad — exigem bits que não existem.

No PS5 essas paredes não existem:

| no Cronus | no PS5 |
|---|---|
| 20 MODs, limitados pela EEPROM | sem teto prático |
| 96 bits por MOD, base-19 no limite | a estrutura que for necessária |
| sem array, sem `for` | C de verdade |
| sem gatilho de sequência | um array de histórico resolve |
| sem saída de touchpad | `touchData` é escrevível |
| sem giroscópio | `quat`, `velocity`, `acceleration` |
| volta de ~10 ms que o script não media | **carimbo de tempo de 64 bits no pacote** |

Aquele último item encerra uma dúvida de setembro: no Cronus o script supunha 10 ms por
volta e não tinha como conferir. Numa sessão a VM rodou a 16,1 ms e todos os tempos
saíram 1,63× mais longos, sem ninguém perceber. Aqui o tempo chega como dado.

## O que é, e o que não é

O D-Sig é um **roteador de sinais e automatizador de sequências**, feito para quem não
alcança dois analógicos ao mesmo tempo, não sustenta um botão, ou precisa transformar
uma sequência em um toque.

Não há nele assistência de mira, leitura de memória do jogo nem interação com servidor.
Um PS5 com jailbreak não acessa a PSN, então também não há questão de jogo online.

**Jailbreak não é pirataria** — são coisas distintas, e este projeto não tem relação com
a segunda. A legalidade de destravar um console varia por país; informe-se sobre o seu.

## Estado

| peça | estado |
|---|---|
| **Roteador** | passagem pura — recebe o pacote e devolve intacto |
| **Bancada** | 4 verificações, rodando no Linux sem PS5 |
| **Ponte com a `libScePad`** | não existe ainda |
| **Configuração** | não existe ainda |

```
$ make bancada
  sizeof(ScePadData) = 120 bytes
  [ok ] a saida e identica a entrada   200000 pacotes aleatorios, byte a byte
  [ok ] a entrada nao foi tocada   o roteador so le
  [ok ] a ponte foi mesmo chamada   200000 quadros, 200000 passagens
  [ok ] ponteiro invalido e recusado, nao derruba
  => bancada aberta: 4 verificacoes passaram
```

**Por que o primeiro alvo é o que não faz nada.** O payload roda *dentro do processo do
jogo*. Um defeito aqui não faz o controle agir estranho como no Cronus — faz o jogo
fechar. Antes de qualquer MOD é preciso provar que a ponte, sozinha, é invisível. Sem
isso, todo MOD que vier depois está apoiado no vazio.

## Para quem for mexer

```bash
make bancada     # compila e roda os testes. Precisa só de gcc
make payload     # reservado para o console. Hoje só avisa que a ponte não existe
```

| arquivo | o que é |
|---|---|
| `src/pad.h` | o `ScePadData` — a estrutura que o PS5 entrega ao jogo |
| `src/dsig.h` | a interface do roteador. Estreita de propósito |
| `src/passagem.c` | o roteador. Hoje só copia |
| `ferramentas/testa_passagem.c` | a bancada |

**A regra de arquitetura: um fonte, dois alvos.** O `src/*.c` não tem nada de PS5
dentro dele — nem cabeçalho de SDK, nem chamada de sistema. Por isso o gcc do Linux o
compila como está, e a bancada o linka e roda. Quando a ponte com a `libScePad` existir,
ela entra num arquivo separado, e o roteador continua compilando dos dois lados.

É a lição mais cara do projeto do Cronus, onde cada conferência exigia o aparelho na
mesa. Aqui o aparelho só é necessário no dia em que a ponte entrar — e só para ela.

As três regras que o projeto segue, trazidas inteiras do D-Sig:

1. **Nada afirmado sem rodar.** Número em comentário que ninguém mediu é número errado
   esperando a hora.
2. **Teste que não consegue reprovar não prova nada.** Toda verificação da bancada foi
   conferida contra uma sabotagem deliberada — inclusive tirar a guarda de ponteiro
   nulo, que derruba o processo com SIGSEGV, que é o jogo fechando.
3. **A entrada é só leitura.** O roteador recebe `const`, e a bancada confere que ele
   não escreveu mesmo. No Cronus, confundir "o que entrou" com "o que vai sair" foi a
   origem de defeitos que não davam sinal.

## Licença

**[GPL-3.0-or-later](LICENSE)**, por herança: o conhecimento da plataforma — e o layout
do `ScePadData` — vem do [PoorDS4](https://github.com/ItsBlurf/PoorDS4) de ItsBlurf, que
é GPL-3.0. A atribuição está no cabeçalho de `src/pad.h`.

O D-Sig do Cronus é CC BY-NC 4.0 e continua como está; as duas licenças não se misturam,
e nada do PoorDS4 volta para lá.
