# Referência: o D-Sig do Cronus 1.0.17

Análise do manual completo do D-Sig 1.0.17 (`D-Sig_1.0.17_Manual.pdf`, 67 páginas, gerado
em 11/11/2026). Este documento diz o que o Alvo 3 deve manter, o que não se aplica ao PS5,
e onde o manual contradiz o que foi escrito antes neste repositório.

As referências de página são do manual. Nada aqui foi executado no PS5.

---

## 1. Mecânica que o PS5 deve manter

Esta é a parte do Cronus que descreve o comportamento do usuário. Não depende da EEPROM.

| tema | o que o Cronus faz | página |
|---|---|---|
| MOD | regra com cinco partes: ALFA (fonte), BETA (condição), GAMA (gatilho), DELTA (resposta), ZETA (relação) | 4-5 |
| Pipeline | ordem fixa por volta: snapshot, ASSOC, BLOQ, EXCL, aplicação dos acumuladores | 31 |
| Portões | cada MOD passa por 14 verificações em ordem; o DELAY é o último | 32 |
| Domínios | quatro conjuntos; só um liga por vez; trocar de domínio trava todos os MODs até o repouso | 7, 32 |
| Acumuladores | vários MODs num mesmo destino: vence o de maior intensidade absoluta | 33 |
| Destino ganha da supressão | um MOD suprime um botão; outro MOD que o escreve como destino vence | 13 |
| STATIC | trava o destino; para fonte de botão, trava em 100%; para eixo, segue a zona | 20 |
| TURBO | até quatro passos, cada um com destino e tempo; ciclos e autofinalização | 21-22 |
| BLOQ | suspende, não desmonta; ao voltar, retoma de onde parou | 24 |
| EXCL | encerra no instante em que o alvo sobe; disparo único | 24 |
| ASSOC | responde só enquanto o alvo responder; órfão herda o desligamento e a intensidade | 24-25 |
| Órfão adotado | MOD sem fonte e sem gatilho que liga, com ASSOC, sobe junto com o alvo | 25 |
| DELAY | atrasa o início da resposta, não da fonte; cadeia de MODs soma os atrasos | 26 |

### Regras derivadas que não aparecem na tela (seção 16)

- Com fonte, o MOD nasce ligado; sem fonte, nasce desligado.
- Com fonte, o gatilho só desliga; sem fonte, o gatilho alterna.
- DIRECT sem fonte encerra ao soltar o botão do gatilho.
- Um MOD sem nenhum gatilho configurado segue de pé.

### Regra sobre "resposta" (seção 15.1)

As relações valem de DELTA para DELTA. O que conta é a **resposta** estar de pé, nunca a
fonte nem o gatilho. Um MOD pode estar ligado e em silêncio, se a fonte estiver em repouso.
O manual chama essa distinção de importante e diz que ela "volta várias vezes".

---

## 2. O que o PS5 não precisa

| item do Cronus | por que não se aplica |
|---|---|
| EEPROM de 64 palavras, empacotamento em bits | Limite de hardware do Cronus. O PS5 não tem esse teto |
| Permutação, selo, migração de layout | Existem para sobreviver ao desligamento na EEPROM |
| Limite de 20 MODs | Vem da EEPROM (seção 30). No PS5 é uma escolha de custo por quadro, a medir |
| Limite de 18 destinos e 23 fontes | Vêm do controle do Cronus. O DualSense tem mais sinais (seção 4) |
| Arrays, variáveis locais, rodízio de temporárias | Limitações da linguagem GPC (seções 28, 54) |
| Trio de botões empacotado, cache de 3 palavras | Consequência da EEPROM |
| Relógio de ~10 ms por volta | No PS5 o carimbo de 64 bits é o relógio (seção 22) |

**Consequência para o Alvo 3:** a seção 2 do manual inteira não vira código. O que vira
código é a seção 1 deste documento, e a regra de que o comportamento depende de estado
(travas, atrasos, fontes em repouso) e não só de valores instantâneos.

---

## 3. Lições de engenharia que o PS5 deve levar

O manual registra erros reais do Cronus. Dois deles afetam diretamente o que estamos
desenhando.

### 3.1 Combo com relógio próprio

O Cronus **não tem combo**. A seção 29 explica por quê: um combo com relógio próprio tem
quatro problemas, e todos saíram em bugs.

1. **Rearmar.** Pedir um combo que já está rodando é ignorado em silêncio.
2. **Estado visível.** "Está rodando?" só é respondido em um ponto do script.
3. **Reset.** Zerar as quatro saídas de uma vez exige um `stop` por combo, e o esquecido
   vira bug.
4. **Variáveis.** O Zen reserva variáveis por combo.

A solução do Cronus é o molde **Disparar / Manter**: um contador em milissegundos, uma
função que aplica o primeiro instante, e outra chamada **a cada volta** que aplica o
restante e faz a virada.

**Implicação direta.** Vamos ter sequências em ALFA e TURBO em DELTA. Se a sequência
tiver relógio próprio, ela herda os quatro problemas. A escolha para o PS5 é a mesma
do Cronus: a sequência é um estado que o quadro atualiza, não um processo separado.

### 3.2 Travas que sobrevivem a quadros

O STATIC mantém a trava entre quadros. O snapshot é tirado antes dos MODs rodarem, então
ele lê o estado do quadro anterior. A seção 24.1 chama isso de solução e não de defeito:
"o snapshot que seria preciso construir já existe de graça."

**Implicação.** O buffer duplo do Alvo 3 precisa dizer em que quadro cada estado foi
lido. Se o gancho usar o perfil de um quadro e o estado de outro, o STATIC trava errado.

### 3.3 Eixos não são somados, são comparados

Para eixos, o acumulador vence pelo valor de maior intensidade absoluta (seção 26). Um
digital é uma escrita de sim ou não, e pode usar OU. Um eixo não pode: dois MODs com
40% e 70% resultam em 70%, não em 110%.

**Correção ao Alvo 2.** A bancada atual usa OU para os destinos, o que está certo para
botões. Para eixos, o modelo precisa de intensidade máxima. O Alvo 2 não tem eixo, então
nada quebra hoje, mas a regra tem de entrar antes do primeiro MOD de eixo.

### 3.4 Trocar de contexto trava tudo

Trocar de domínio trava todos os MODs. Eles só voltam a operar depois de passar por
repouso, com a fonte fora da zona e nenhum gatilho pressionado (seção 25.1). Sem isso, um
MOD com fonte no L1 armaria o AFTER na primeira volta do domínio novo.

**Implicação.** A troca de perfil pelo celular, e a troca de domínio pelo controle, precisam
do mesmo tipo de trava de repouso.

### 3.5 Timer com teto

O timer do TURBO tem 11 bits, então esperas acima de 2047 ms eram cortadas em silêncio.
A correção foi recarregar em fatias sem sair da fase (seção 58.2).

**Implicação.** No PS5 o carimbo de 64 bits em microssegundos elimina o problema do teto. Um
temporizador que usa o carimbo não precisa de fatiamento. Ainda assim, o teste de duração
longa tem de existir na bancada.

---

## 4. O que o DualSense acrescenta

O Cronus tem 23 fontes. O manual lista todas (seção 11.1). O DualSense traz sinais que o
Cronus não tinha, e que o pacote `ScePadData` já entrega:

| sinal | no Cronus | no PS5 |
|---|---|---|
| botões, eixos, gatilhos | sim | sim |
| touchpad (dois dedos) | sim, como touch X e Y | sim, com dois dedos |
| giroscópio, acelerômetro, quaternion | não | sim |
| botão OPTIONS e PS | OPTIONS não é fonte | a confirmar |

A decisão sobre quais desses entram em ALFA é do autor. Por enquanto, o Cronus é a
referência.

---

## 5. Divergências entre o manual e o que escrevemos antes

Estas correções já estão aplicadas em `docs/ARQUITETURA.md`, seção 11.

| o que escrevemos | o que o manual diz | página |
|---|---|---|
| janela do 2CLK = `TIME_SHORT_PRESS` = 250 ms | janela do 2CLK dos MODs é **300 ms, fixa**. O 250 ms é dos domínios e do touchpad | 16, 27 |
| tempo do HOLD_AT = `TIME_HOLD` = 500 ms | HOLD dos MODs tem tabela de **63 valores**. `TIME_HOLD` é a duração para cair em D3/D4 | 16, 27 |
| o limite de 20 MODs é uma escolha nossa | o manual confirma que vem da EEPROM | 30, 67 |

A última linha afeta o `DSIG_MAX_MODS` em `src/dsig.h`. Hoje ele diz "herda o teto do
Cronus". O teto vem da memória do Cronus, e não tem relação com o custo no PS5.

---

## 6. Mudança de modelo que o autor já decidiu

Estas decisões vão além do Cronus. O manual não as cobre, e o Cronus não as tem.

| decisão | o que muda em relação ao Cronus |
|---|---|
| GAMA como lista, com mais de um gatilho do mesmo tipo | o Cronus tem um por tipo |
| FINALIZAR e ALTERNAR em todos os tipos | o Cronus só tem nos quatro primeiros (2CLK, TOGGLE, HOLD, AFTER) |
| HOLD separado em HOLD_AT e HOLD_AP | o Cronus tem um HOLD com modo ATÉ ou APÓS |
| sequência em ALFA, com ordem de acionamento | o Cronus não tem sequência de fontes |
| TURBO e combo em todas as respostas | o Cronus tem um tipo de DELTA por MOD (DIRECT, STATIC ou TURBO), aplicado a até quatro destinos |
| EXCL com dois momentos: na borda de subida, ou na finalização | o Cronus só tem a borda de subida |
| tabela única de tempo | o Cronus tem tabelas separadas por função |

---

## 7. Perguntas que o manual não responde

1. **ATIVO e OCULTO.** O manual não usa OCULTO. A confirmação do autor diz que OCULTO
   substitui PASSIVO. Pela seção 11.4, PASSIVO faz a fonte e os gatilhos serem suprimidos
   do jogo. Se o autor confirma que OCULTO é o mesmo comportamento, nada muda.
2. **Gatilho de BLOK.** O manual diz que BLOK não tem campo de modo: ele nem liga nem
   desliga, só suspende. A decisão do autor de dar FINALIZAR e ALTERNAR também ao BLOK é
   uma mudança e precisa ser confirmada.
3. **Combinação em ALFA.** O manual não tem "qualquer um" em ALFA. Só existe a regra de
   fontes com sequência ou sem.
4. **Sequência em ALFA.** O manual não descreve isso, porque o Cronus não tem. O modelo
   precisa definir o que conta como "passo" de uma sequência de fontes, e o que acontece
   quando um passo é OCULTO.
5. **EXCL na finalização.** No Cronus, a borda de subida é o único momento. Não há regra
   para "finalização" de DIRECT, STATIC, TURBO ou de sequência. Precisa ser definida.
6. **Tabela única de tempo.** A união tem 67 valores, verificado. Falta decidir se a tabela
   é um catálogo com referência por MOD, ou valores fixos por item.

---

## 8. Resumo

O manual confirma que o modelo do Cronus é uma máquina de estados por quadro, com regras
explícitas de precedência e travas de repouso. A parte que não serve ao PS5 é a parte de
empacotamento em memória. O PS5 deve manter as regras de comportamento e as lições de
engenharia da seção 3, especialmente o molde Disparar/Manter, a intensidade para eixos e
as travas de troca de contexto.
