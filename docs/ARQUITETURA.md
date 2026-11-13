# Arquitetura do D-Sig-PS5

Este documento fixa as decisões de desenho antes do Alvo 3. Cada decisão tem um
estado: **decidido** (vale para o código), **proposto** (recomendação, ainda não
aceita), ou **a medir** (depende de execução no console, e nada aqui foi executado lá).

---

## 1. Decisões

| tema | decisão | estado |
|---|---|---|
| Escopo | Turbo, macro, sequência, acorde e domínios são o núcleo. Jogo local entra como acessibilidade. Vantagem online fica fora (`CLAUDE.md`, seção 2) | decidido |
| Perfil por jogo | Um perfil por título. O título em execução escolhe o perfil ativo | decidido |
| Quem identifica o jogo | O **daemon**. O gancho recebe o perfil pronto e não faz chamada de sistema | decidido |
| Como o jogo é identificado | Pelo identificador de título do processo | a medir |
| Quantidade de slots | Sem limite de produto. O usuário cria os que julgar necessários | decidido |
| Salvamento | Arquivo por título, com slots, em caminho próprio. Não usa o savedata oficial | decidido |
| Gravação | Atômica (temporário e depois renomeia). Nunca dentro do quadro | decidido |
| Como o perfil chega ao gancho | O daemon **escreve na memória do processo do jogo**, no mesmo mecanismo que instala o gancho. Não há memória compartilhada nova | decidido, a medir |
| Privilégio | Começa sem escalonar. Herda o acesso que o carregador entrega. Eleva só o próprio processo, e só se a rede falhar | proposto |
| Ícone | **Não inicia** o D-Sig. Abre a página de configuração quando o residente já está de pé | decidido |
| Início do residente | O autoloader do PS5 Payload Manager ou do ProsperoMgr sobe o D-Sig, a pedido do usuário ou no boot | proposto |
| Segunda subida | **Não sobe.** Pede ao residente que recarregue o perfil pela porta local, e termina. Sem matar processo e sem elevação | decidido |
| Celular | PWA na rede local, conectando por IP do console, com código de pareamento mostrado na TV | proposto |
| Interface do celular | Escrita do zero, licença GPL-3.0. Não reaproveita código do Cronus | decidido |
| Modelo de MOD | Vários sinais simultâneos em ALFA, GAMA e DELTA, com turbo e combo. Ver seção 9 | proposto |

---

## 2. Dois processos, duas regras

O gancho precisa rodar **dentro do processo do jogo**, porque é lá que a entrada PLT
aponta. Qualquer código que roda ali está sujeito à regra de estabilidade do `CLAUDE.md`,
seção 6. Por isso o sistema tem duas partes:

| parte | onde roda | o que faz | regra |
|---|---|---|---|
| **Gancho** | dentro do processo do jogo | recebe o pacote, aplica o perfil ativo, devolve | sem alocação, sem bloqueio, sem arquivo, sem chamada de sistema |
| **Daemon** | processo próprio | identifica o jogo, carrega perfis, interface, salvamento, rede com o celular | pode alocar, abrir arquivo e rede |

Nenhum dos dois fala com o outro por arquivo ou socket no caminho do quadro.

### Entrega do perfil ao gancho

**Decidido:** buffer duplo com contador de versão, escrito pelo daemon dentro da
memória do jogo.

1. O daemon escreve o perfil novo no buffer **inativo**.
2. Só depois publica o número da versão nova, em uma escrita só.
3. O gancho lê a versão, copia o buffer ativo, e lê a versão de novo. Se mudou durante
   a cópia, descarta a cópia e usa o perfil anterior nesse quadro. Isso impede ler um
   perfil pela metade.
4. A tentativa de releitura é limitada a uma por quadro. Sem laço de espera.

**Alternativa, se a escrita pelo daemon falhar no console:** reinstalar o gancho a cada
troca de perfil. Mais simples, mas com uma janela de troca mais longa.

**A medir:** que a escrita pelo daemon é possível e que a cópia pelo gancho cabe no
quadro. Nenhum dos dois foi executado.

---

## 3. Fluxo de dados

```
controle ──► libScePad ──► [gancho no jogo] ──► jogo
                              ▲
                              │ perfil ativo (buffer duplo, versão),
                              │ escrito pelo daemon na memória do jogo
                              │
                          [daemon] ◄── perfis e slots (arquivo, gravação atômica)
                              ▲
                              │ HTTP na rede local, pareado por código
                              │
                          [celular: PWA]

ícone ──► página em 127.0.0.1 servida pelo daemon (só com o residente de pé)
autoloader (PS5 Payload Manager / ProsperoMgr) ──► sobe o daemon
segunda subida ──► pede recarga ao residente pela porta local, e termina
```

---

## 4. Perfil

Um perfil é o conteúdo que o daemon publica para o gancho e que o celular edita. Os
campos seguem o modelo da seção 9.

| campo | conteúdo |
|---|---|
| `titulo` | identificador do jogo ao qual o perfil pertence |
| `versao` | número de formato, para leitura futura |
| `slots` | lista de slots por título, cada um com nome e um conjunto de MODs |
| `mods` | lista de MODs no formato da seção 9, até o teto da seção 9 |
| `soma` | verificação de integridade do conteúdo |

**Decidido:** formato binário de tamanho fixo, com versão e soma. O gancho não interpreta
texto. A conversão para texto, se o celular precisar, acontece no daemon.

**Carga inválida:** como no `dsig_carregar`, nada é trocado se o perfil não validar.

---

## 5. Privilégio

**Proposto, em ordem:**

1. Começar sem escalonar. O carregador entrega acesso ao payload antes do `main()`
   (`ESTUDO.md`, seção 4). O gancho, a leitura de arquivos e a escrita na memória do
   jogo pelo mesmo mecanismo do gancho não precisam de elevação adicional.
2. Medir se o servidor de rede abre no processo do daemon, sem elevação.
3. Só se a rede falhar: elevar **o próprio processo**, e só o que for necessário. Nunca
   o processo do jogo.
4. Ordem, se a elevação for necessária: elevar antes de abrir a rede.
5. Não encerrar instância anterior por PID. A segunda subida pede recarga (seção 6), e
   assim não é preciso matar nada.

**A medir:** tudo acima.

---

## 6. Celular, e segunda subida

**Decidido:** PWA escrita do zero, GPL-3.0, servida pelo daemon.

**Proposto:**

- O console publica uma porta na rede local. O celular conecta pelo IP do console.
- A conexão exige o código de pareamento, mostrado na TV, uma vez por celular.
- A porta só aceita conexão da rede local. Nada sai para a internet.
- A interface edita perfis e slots, e troca de perfil ao vivo.

**Decidido, segunda subida:** se o D-Sig for carregado de novo com o residente já de
pé, a segunda instância não sobe. Ela manda à porta local um pedido de recarga e
termina. O residente relê o perfil do slot ativo e reescreve o buffer.

**A medir:** se a porta abre sem elevação, e se a troca de perfil pelo celular chega ao
gancho dentro de poucos quadros.

---

## 7. Fases

| fase | o que entrega | onde se verifica |
|---|---|---|
| **3a** | Modelo de MOD da seção 9, formato de perfil, serialização, validação, buffer duplo com versão. Teste do buffer com threads no Linux | bancada no Linux, com sabotagens |
| **3b** | Daemon: identificação do jogo, carga de perfis, slots, gravação atômica, pareamento, troca de perfil, segunda subida. Sem gancho | bancada e console (rede, arquivos, autoloader) |
| **3c** | Gancho: instalação na PLT, escrita e leitura do perfil ativo, encadeamento com o PoorDS4 | console. A bancada só cobre a lógica |
| **3d** | Ícone e página de configuração embutida | console |

A fase 3a não exige console, e é a próxima a ser escrita.

---

## 8. Decisões tomadas sobre as perguntas em aberto

| pergunta | decisão | motivo |
|---|---|---|
| Identificador do jogo | O daemon identifica. O gancho não | O gancho não faz chamada de sistema. A forma exata de obter o identificador ainda é a medir |
| Memória para o perfil | Daemon escreve na memória do jogo, com buffer duplo | Usa o mesmo mecanismo que instala o gancho (`ESTUDO.md`, seção 2). Evita memória compartilhada não verificada |
| Ícone com o D-Sig desligado | Não inicia. Só abre a configuração | Sem residente de pé não há página para servir. O início a frio é do autoloader |
| Segunda subida | Pede recarga e termina | Não mata processo e não precisa de elevação |
| Quantidade de slots | Em aberto, escolhido pelo usuário | Não há teto de produto. O limite real está no perfil ativo, não no número de slots |

---

## 9. Modelo de MOD — versão anterior, substituída pela seção 10

*Esta seção fica como registro. O modelo vigente é o da seção 10.*

Cada MOD tem cinco partes. A diferença em relação ao Cronus é que cada parte aceita
**vários itens ao mesmo tempo**, e DELTA aceita turbo e combo, não só botão.

| parte | o que define | novo nesta versão |
|---|---|---|
| **ALFA** (fontes) | quais entradas o MOD observa | Vários sinais simultâneos. Botões, analógicos e gatilhos na mesma lista |
| **BETA** (condição) | a partir de quando um sinal conta como acionado | Limiar por analógico e por gatilho |
| **GAMA** (gatilho) | o que liga e desliga o MOD | Vários gatilhos: segurar, duplo toque, sequência |
| **DELTA** (saídas) | o que o MOD entrega | Vários destinos. Cada destino pode ser botão fixo, **turbo** (repete com período), ou **combo** (sequência de botões com tempos) |
| **ZETA** (relações) | como o MOD se relaciona com outros | Exclusão e bloqueio, como no Cronus, mais **alternativas novas a definir** |

### Regras que permanecem

- **A leitura é sempre na entrada.** Um MOD não enxerga a saída de outro. Por isso a
  ordem da tabela não muda o resultado (já verificado na bancada do Alvo 2).
- **Conflito tem regra fixa:** quando um sinal é solto e outro é apertado no mesmo
  botão, o aperto vence.
- **Carga valida tudo antes de trocar.** Nada muda se o perfil não validar.

### Estado que vive entre quadros

Turbo e combo precisam guardar posição e tempo entre quadros. Esse estado:

- fica em uma tabela de tamanho fixo, pré-alocada na carga do perfil;
- é zerado a cada troca de perfil;
- não é gravado em arquivo.

### Tempo

- O tempo de um sinal (segurar, duplo toque, duração de combo) usa o **carimbo de 64
  bits** do pacote, em microssegundos.
- A saída só muda a cada quadro do jogo. Então a precisão efetiva de uma sequência é de
  um quadro. Isso é consequência do gancho e precisa ser medido no console antes de ser
  prometido a alguém.

### Custo por quadro

O custo de um quadro cresce com o número de MODs e de itens por MOD. Por isso:

- o teto de MODs é `DSIG_MAX_MODS`, já definido;
- o teto de itens por MOD em cada parte ainda não foi fixado. Será fixado **depois de
  medir** o custo, contando operações por quadro na bancada. Não será um número escolhido
  no escuro.

### Perguntas em aberto deste modelo

1. **Alternativas novas de ZETA.** Você mencionou que existem. Quais são?
2. **Condições combinadas em ALFA.** Quando vários botões estão em ALFA, a regra é "todos"
   (como no acorde do Alvo 2) ou "qualquer um"? Pode ser as duas, por MOD?
3. **Turbo e combo em um mesmo destino.** Um destino de DELTA pode ter turbo e combo
   juntos, ou um por destino?

---

## 10. Modelo de MOD — base, revisada pela seção 11

Este modelo vem das decisões do autor. Substitui a seção 9. Onde o autor não definiu,
o item aparece na lista de pendências, sem palpite.

### Vocabulário

- **PASSIVO sai do vocabulário.** Não há substituto definido ainda (pendência 2).
- **ATIVO** e **OCULTO** passam a ser os estados de cada fonte em ALFA.
- O Cronus usa o termo PASSIVO. A troca também precisa acontecer lá (pendência 6).

### ALFA — fontes e montagem da sequência

| item | regra |
|---|---|
| fontes | todas as entradas do controle, ao mesmo tempo: botões, eixos e gatilhos |
| estado de cada fonte | **ATIVO** ou **OCULTO** (significado a confirmar, pendência 1) |
| acionamento | **sequencial** ou **simultâneo**, escolhido por ALFA |
| delay | continua em ALFA |
| BETA (eixos e gatilhos) | limiar continua, agora com **INV** (inverter) |

ALFA é onde a sequência é montada. Quando o acionamento é simultâneo, o MOD se comporta
como o acorde já verificado na bancada do Alvo 2: todas as fontes ATIVAS juntas.

### GAMA — gatilho

- As **5 situações** do GAMA aceitam qualquer botão, eixo ou gatilho.
- Lista exata das 5 situações: a confirmar, pendência 3.

### DELTA — saída

- Cada resposta aceita qualquer botão, eixo ou gatilho.
- Cada resposta também pode ser uma **sequência**.

### ZETA — relações

- As alternativas são as mesmas do Cronus.
- No **EXCL**, o autor escolhe o momento da exclusão:
  - no instante em que o DELTA **sobe**; ou
  - junto com a **finalização** do DELTA.
- Isso responde à pergunta 1 da seção 9: as alternativas novas de ZETA são o momento
  do EXCL.

### Consequências no desenho

- O `DsigMod` atual (origem e destino com máscara) é um subconjunto do modelo vigente.
  A bancada do Alvo 2 continua válida para o caso simples, mas o formato do perfil
  muda na fase 3a.
- Sequência e delay precisam de estado entre quadros, na tabela pré-alocada da seção 9.
- O teto de itens por MOD continua **a medir**, e agora cresce com as cinco partes.
- Fontes ATIVAS e OCULTAS e a ordem sequencial precisam de regra de precedência escrita
  antes da implementação.

### Pendências

1. **ATIVO e OCULTO.** Proposta de leitura, para o autor confirmar: uma fonte **ATIVO**
   participa do MOD. Uma fonte **OCULTO** é retirada da saída que o jogo recebe enquanto
   o MOD está ativo. Está certo?
2. **Substituto de PASSIVO.** Qual termo entra no lugar?
3. **As 5 situações do GAMA.** Liste quais são.
4. **Combinação em ALFA.** O modo sequencial e o simultâneo cobrem a regra "todos". Falta
   definir a regra "qualquer um", ou ela não existe?
5. **Turbo e combo no mesmo destino** (pergunta 3 da seção 9): continua em aberto.
6. **Repositório do Cronus.** A troca de PASSIVO precisa ser feita lá. O `CLAUDE.md` diz
   que nada daqui volta ao repositório do Cronus, e esta sessão não tem acesso de escrita
   a ele. Quem faz essa alteração, e em qual branch?

---

## 11. Revisão do autor: gatilhos, tempo e vocabulário

Esta seção substitui o que a seção 10 dizia sobre GAMA, tempo e PASSIVO. O restante da
seção 10 continua valendo. A referência é o D-Sig do Cronus **1.0.17**, o arquivo enviado
pelo autor. Não verifiquei se essa versão está no GitHub.

### Vocabulário

| antes | agora | estado |
|---|---|---|
| PASSIVO | **OCULTO** | confirmado pelo autor |
| ATIVO / OCULTO em ALFA | mantido | confirmado pelo autor |

A troca de PASSIVO também precisa ser feita no Cronus, onde o termo aparece em
`LEIAME.md` e no código. Ver pendência 7.

### GAMA: lista de gatilhos, não cinco fixos

Hoje o Cronus tem cinco gatilhos fixos no menu. Aqui, **cada GAMA é uma lista de gatilhos**.
Um mesmo GAMA pode ter dois ou mais gatilhos, do mesmo tipo ou de tipos diferentes.

| tipo | o que faz | variantes |
|---|---|---|
| **2CLK** | duplo toque dentro de uma janela | FINALIZAR / ALTERNAR |
| **TOGGLE** | cada acionamento liga ou desliga o MOD | FINALIZAR / ALTERNAR |
| **HOLD_AT** | segurar o botão **até** um tempo | FINALIZAR / ALTERNAR |
| **HOLD_AP** | segurar o botão **após** um tempo | FINALIZAR / ALTERNAR |
| **AFTER** | disparar um tempo depois de um evento | FINALIZAR / ALTERNAR |
| **BLOK** | bloqueia o MOD por um botão | a confirmar |

**Leitura do que o Cronus já faz (a confirmar com o autor):**

- HOLD com modo ATÉ e modo APÓS existe no Cronus (`MODO: ATE / APOS`). HOLD_AT e HOLD_AP
  seguem esse modo, separados em dois tipos.
- O modo **FINALIZA** dos gatilhos existe no Cronus e só desliga. Leio FINALIZAR como esse
  modo, e ALTERNAR como o liga e desliga normal.

### Tempo: uma tabela única

O Cronus já segue a regra de que todo tempo é um `define` na seção 2, com o nome do que
mede. Aqui a mesma regra vira **uma tabela de tempos no perfil**, usada por todo item que
tem ajuste de tempo.

| item que usa tempo | parte | referência no Cronus |
|---|---|---|
| janela do 2CLK | GAMA | **fixa, não ajustável: 300 ms** (manual, seção 13.5). Não entra na tabela |
| tempo do HOLD_AT / HOLD_AP | GAMA | tabela de **63 valores**, de 50 ms a 4 s, escalonada (manual, seção 13.3) |
| tempo do AFTER | GAMA | tabela de **8 valores**, de 50 ms a 2 s (manual, seção 13.4) |
| delay | ALFA | tabela de **4 valores**: OFF, 300 ms, 700 ms, 1,2 s (manual, seção 17) |
| tempo de passo do TURBO | DELTA | tabela de **8 valores**, de 30 ms a 1 s (manual, seção 14.5) |
| latência (LAT) do TURBO | DELTA | tabela de **16 valores**, com "sem latência" e de 10 ms a 750 ms (manual, seção 14.5) |

**Verificado por cálculo** (`scratchpad/tempos/tabela.py`): a união dessas tabelas tem
**67 entradas**. AFTER e DELAY são subconjuntos da tabela do HOLD. Os únicos valores que
não estão no HOLD são 10, 20, 30 e 40 ms, todos vindos do LAT e do passo do TURBO. Não é
preciso um esquema de tabelas separadas: uma tabela de 67 entradas basta, e cada item
guarda só um índice.

Ficam **fora** da tabela os tempos fixos de comando e de tela, como `TIME_SHORT_PRESS`
(250 ms, janela do duplo clique de domínio e touchpad), `TIME_HOLD` (500 ms, para cair em
D3 e D4), `TIME_CIRCLE_CLOSE`, `TIME_SQ_RESET`, `TIME_SHARE_SAVE`, `TIME_TRI_DELETE` e
`OLED_TIMEOUT`. Eles não são ajustáveis, e a tabela única é só para os que são.

**Proposta de leitura, a confirmar:** a tabela é um **catálogo**. Cada entrada tem nome,
unidade e valor. Cada MOD aponta para uma entrada. Assim, mudar uma entrada muda todos os
itens que a usam. A alternativa é um valor fixo por item, sem compartilhamento.

**Unidade proposta:** milissegundos, inteiro. A precisão efetiva segue a seção 10, "Tempo":
um quadro.

### Consequências no desenho

- O perfil ganha a tabela de tempos. O gancho lê a entrada pelo índice, em tempo constante.
- Um MOD passa a ter listas: fontes em ALFA, gatilhos em GAMA e respostas em DELTA. O teto
  de cada lista continua **a medir**.
- O modelo do Alvo 2 (`DsigMod`) continua sendo um caso particular, sem GAMA em lista,
  sem tempo e sem OCULTO.

### Pendências desta revisão

1. **FINALIZAR e ALTERNAR** valem para os seis tipos, inclusive BLOK?
2. **BLOK.** O que exatamente ele faz? Liga e desliga o MOD, ou só bloqueia enquanto
   estiver pressionado?
3. **Tabela de tempos.** Catálogo com referência por MOD, ou valor fixo por item?
4. ~~**Valor de referência do HOLD_AT** é o mesmo `TIME_HOLD` de 500 ms?~~ **Respondida
   pelo manual:** não. `TIME_HOLD` é a duração para cair em D3 e D4. O HOLD dos MODs usa
   a tabela de 63 valores.
5. **Teto de gatilhos por GAMA.** A definir depois de medir o custo por quadro.
6. **Turbo e combo no mesmo destino** (pendência 5 da seção 10): continua em aberto.
7. **Cronus.** PASSIVO → OCULTO precisa ser feito no repositório do Cronus. Você faz, ou me
   dá acesso de escrita e confirma a branch?

---

## 12. Decisões do autor: estado de cada item e sequência em ALFA

Esta seção responde as pendências 1 a 3 da seção 11 e fixa o que é sequência em ALFA.
Onde o autor não definiu, o item aparece como pergunta, sem palpite.

### Estados e gatilhos (confirmados)

| item | decisão |
|---|---|
| PASSIVO | é **OCULTO**. O mesmo comportamento: a fonte e os gatilhos são suprimidos do jogo |
| BLOK | **não** ganha FINALIZAR nem ALTERNAR. Continua suspendendo enquanto segurado. A mudança é só poder escolher vários botões, eixos ou gatilhos |

### ALFA: sequência

A sequência é definida pela opção **Sequencial: SIM / NÃO**, por MOD.

**Sequencial = NÃO.** As fontes valem juntas, como o acorde do Alvo 2.

**Sequencial = SIM.**

| item | regra |
|---|---|
| ordem | a ordem em que o usuário coloca as fontes é a ordem de acionamento |
| resposta | DELTA só responde **depois que a sequência termina** |
| espera entre passos | cada passo pode ter um tempo de espera até o próximo. O valor vem da tabela única de tempo |
| ATIVO / OCULTO | vale para **a sequência inteira**, não por passo |
| DELAY | conta a partir do **fim da sequência**, e não do início |

**Relação com o DELAY do Cronus.** No Cronus, o DELAY atrasa o início da resposta, e
fica amarrado à RESPOSTA. Aqui ele continua em ALFA, como o autor pediu, e passa a contar
do fim da sequência. Para MOD sem sequência, o comportamento é o do Cronus.

**Relação com a seção 9 (estado entre quadros).** A sequência é um índice de passo e um
tempo de espera, guardados na tabela pré-alocada. Não tem relógio próprio. Isso segue a
lição do combo do Cronus, seção 3.1 de `REFERENCIA_CRONUS.md`.

### Perguntas sobre sequência (respondidas na seção 13)

1. **Espera entre passos é máxima?** Se o usuário demora mais que o tempo de espera, a
   sequência reinicia, ou fica esperando?
2. **Passo errado.** Se o usuário pressiona uma fonte fora de ordem, a sequência reinicia,
   ignora o botão, ou volta um passo?
3. **Gatilho durante a sequência.** Se o gatilho for acionado de novo com a sequência em
   andamento, a sequência reinicia, ou o acionamento é ignorado? O Cronus ignora em
   silêncio um combo que já está rodando, e o manual aponta isso como bug (seção 29).
4. **Sequência com OCULTO.** Enquanto a sequência roda, os botões dos passos somem do jogo?
   Ou só depois que ela termina?

---

## 13. Decisões do autor: modos de ALFA, sequência e EXCL

Responde as perguntas da seção 12 e define "finalizar" para cada tipo de resposta.

### 13.1 Modo de acionamento em ALFA (três opções, não duas)

O modo de acionamento tem três valores, por MOD:

| modo | regra | interpretação |
|---|---|---|
| **LIVRE** | cada fonte age sozinha; basta uma | OU entre as fontes. Aperta um, ou dois juntos, e o MOD responde |
| **CONJUNTO** | todas as fontes devem estar acionadas juntas | E entre as fontes, no mesmo quadro. É o acorde do Alvo 2 |
| **SEQUÊNCIA** | as fontes em ordem, cada uma com espera até a próxima | a sequência descrita na seção 12 |

**Confirmado pelo autor:** LIVRE é OU, CONJUNTO é E. Exemplo: com QUADRADO, CROSS e
TRIÂNGULO na fonte, em LIVRE basta apertar um deles, ou todos, em qualquer ordem, e o
DELTA responde. Em CONJUNTO, o DELTA só sobe se os três forem apertados juntos.

**Nomes decididos (seção 13.5):** QUALQUER (antes LIVRE), SIMULTÂNEO (antes CONJUNTO) e SEQUÊNCIA.

**Pendência de medição.** "Juntos" em CONJUNTO precisa de uma janela de tolerância. Três
botões apertados por uma pessoa caem em quadros diferentes, e a regra de mesmo quadro
do Alvo 2 seria estrita demais para uso real. A janela entra na tabela única de tempo, e
o valor só será escolhido depois de medir (seção 13.5).

### 13.2 Sequência: as quatro respostas

| pergunta | decisão |
|---|---|
| Espera entre passos estoura | **reinicia** a sequência |
| Passo errado | **reinicia** a sequência |
| Gatilho durante a sequência | **reinicia** a sequência. O passo esperado tem prioridade sobre o gatilho, se os dois forem o mesmo botão |
| Botões dos passos durante a sequência | **ficam OCULTOS durante a sequência**, desde o primeiro passo. O jogo só vê o que sai da sequência, quando ela termina |

**Justificativa para jogos de luta (Mortal Kombat, Street Fighter).** Um movimento de luta
é uma sequência curta e rígida. Quem usa o D-Sig precisa que o movimento saia igual toda
vez, e que um erro seja visível e recomeçável. "Reiniciar" dá essas duas coisas. "Ignorar
em silêncio" faria o usuário apertar uma vez e não saber se a sequência está correndo, que
é o defeito do combo do Cronus. A escolha tem um custo que fica registrado: um botão
apertado um instante antes do passo certo também reinicia. Esse risco é controlado pela
espera entre passos e só será medido no console.

### 13.3 O que significa "finalizar" para cada tipo de resposta

O EXCL passa a ter duas opções de momento: **SOBE** (o Cronus) e **FINALIZA** (novo).
Para definir "finalizar", usamos uma regra única: **subir** é o instante em que a resposta
começa; **finalizar** é o instante em que ela termina.

| resposta | sobe (começa) | finaliza (termina) |
|---|---|---|
| **DIRECT** | a fonte entra na zona ativa e o destino acompanha | a fonte sai da zona, ou o gatilho solta e o destino cai a zero |
| **STATIC** | o destino trava no alto | a trava é solta: por gatilho, por relação, ou pela saída da zona |
| **TURBO** | a primeira fase ativa começa | a máquina para: ao fim do último ciclo, ou quando a resposta é desligada. Fases de silêncio contam como parte da resposta, como o manual já diz |
| **SEQUÊNCIA** (ALFA) | a sequência termina, e a resposta começa | a resposta termina, pelo tipo de DELTA que a sequência alimenta |

**Consequência para EXCL:** com SOBE, o alvo que começa encerra o MOD (comportamento do
Cronus). Com FINALIZA, o MOD só é encerrado quando o alvo termina. Isso permite, por
exemplo, um MOD que espera um turbo acabar antes de agir. "Disparo único" continua valendo
nos dois casos.

### 13.4 Pontos que ficam para o autor

1. Confirmar a leitura de LIVRE como OU e de CONJUNTO como E (seção 13.1).
2. Para TURBO, a definição de "finalizar" inclui o caso de a resposta ser desligada antes
   do fim dos ciclos. Confirmar que isso deve encerrar a máquina.
3. Confirmar se "sequência como resposta" em DELTA é o mesmo que TURBO. Pela seção 14 do
   manual, é: o TURBO é a sequência de até quatro passos. Se for, a tabela de DELTA continua
   com três tipos, e não quatro.

### 13.5 Nome do modo CONJUNTO e janela de simultaneidade

**Sugestão de nome: SIMULTÂNEO.** Os três modos descrevem a relação entre as fontes:

| modo | relação no tempo |
|---|---|
| LIVRE | independentes: qualquer uma basta |
| SIMULTÂNEO | ao mesmo tempo: todas juntas |
| SEQUÊNCIA | uma depois da outra, em ordem |

"Simultâneo" nomeia o mecanismo exato e fica simétrico a "sequência", que é o outro eixo
de tempo. "Conjunto" descreve o resultado, e não a regra de tempo. Alternativas
aceitáveis: TODAS (curto, mas pode ser lido como "todas as fontes, em qualquer ordem"),
e JUNTAS. Se o nome de CONJUNTO mudar, LIVRE pode virar QUALQUER, para formar par com TODAS.

**Janela de simultaneidade.** Proposta: SIMULTÂNEO aceita as fontes apertadas dentro de
uma janela, medida a partir da primeira. O valor sai da tabela única de tempo, com um
piso de 10 ms (o menor valor da tabela). O valor real só será escolhido depois de medir
quanto espaço um humano deixa entre dois botões apertados juntos. Isso é uma medição que
o Linux não faz, e fica para o console ou para um teste com dados de pessoas.

### 13.6 Decisão final dos nomes e valor provisório da janela

**Nomes:** QUALQUER (OU), SIMULTÂNEO (E no mesmo instante) e SEQUÊNCIA (em ordem).
LIVRE e CONJUNTO saem do vocabulário.

**Janela de simultaneidade:** **50 ms, provisório.** É um valor já existente na tabela
única de tempo, escolhido porque cobre o intervalo entre dois botões apertados juntos por
uma pessoa, na estimativa de quem desenhou o modo. Essa estimativa **não foi medida**. Fica
marcada como provisória até a medição no console ou com dados de pessoas reais. Enquanto
isso, a bancada do Alvo 2 continua testando o caso de mesmo quadro, que é o limite inferior.

---

## 14. Decisões do autor: tempo como catálogo, TURBO e a sequência de DELTA

Fecha as pendências 6 e 2-4 da seção 11 e a seção 13.4.

### 14.1 Tempo: catálogo com seleção por índice

**Decidido:** o padrão de tempo é o do HOLD. Existe uma lista de valores selecionáveis (a
tabela única da seção 11, com 67 entradas). Cada item que tem ajuste de tempo guarda
apenas um índice, e o usuário escolhe o valor na lista. Nenhum item guarda um número livre.

- Os itens ajustáveis são os que estão na tabela: HOLD_AT, HOLD_AP, AFTER, DELAY, passo e
  latência do TURBO, passo da sequência de DELTA (seção 14.3), e a janela de
  simultaneidade do modo SIMULTÂNEO (50 ms, provisório, seção 13.6).
- Os tempos fixos de comando e de tela (janela do 2CLK, `TIME_*`) ficam fora, porque não
  são ajustáveis.

### 14.2 TURBO desligado antes do fim dos ciclos

**Decidido, pela distinção que o autor deu:**

| o que desliga a resposta | efeito no TURBO |
|---|---|
| **BLOQ** (relação de bloqueio) | **pausa.** A máquina congela e retoma de onde parou. Segue o BLOQ do manual (seção 15.2) |
| **Finalizador** (gatilho em FINALIZA, ou relação EXCL) | **encerra.** A máquina é zerada, e a próxima vez é um começo novo. Segue o EXCL (seção 15.3) |

### 14.3 DELTA: quatro tipos de resposta

| tipo | origem |
|---|---|
| DIRECT | manual, seção 14 |
| STATIC | manual, seção 14 |
| TURBO | manual, seção 14. **Mantido**, é um tipo próprio |
| **SEQUÊNCIA** | **novo.** Construção de uma sequência de respostas dentro do DELTA |

**SEQUÊNCIA de DELTA.** Cada passo tem destino e tempo, do mesmo jeito que o TURBO. O
autor sugeriu **até 8**. Há duas leituras, e o autor precisa escolher (pendência A abaixo).

### 14.4 TURBO e SEQUÊNCIA podem coexistir no mesmo destino

**Decidido:** sim. Um destino pode receber saída de mais de um MOD, inclusive de um TURBO e
de uma sequência de DELTA. A regra de acumulação da seção 26 do manual vale: digital usa
OU, e eixo usa intensidade máxima (ver `REFERENCIA_CRONUS.md`, seção 3.3).

### 14.5 Pendências que restam

- **A.** Os "até 8" da sequência de DELTA são 8 **passos** (como o TURBO tem 4), ou 8
  **sequências** configuráveis por DELTA?
- **B.** Confirmar que "finalizadores" são gatilhos em FINALIZA e a relação EXCL, e que BLOQ
  só pausa (seção 14.2).

Com A e B respondidas, a fase 3a fica sem bloqueio de modelo.

---

## 15. Decisões do autor: COMBO, TURBO simplificado e EXCL com memória

Fecha a pendência A e a pendência B da seção 14.

### 15.1 Tipos de DELTA (substitui a seção 14.3)

| tipo | o que é | origem |
|---|---|---|
| DIRECT | acompanha a fonte | manual, seção 14 |
| STATIC | trava o destino | manual, seção 14 |
| **TURBO** | repete uma sequência curta, com menos passos que hoje | manual, seção 14, **simplificado** |
| **COMBO** | sequência de até **10** passos. É o TURBO atual, renomeado e ampliado | manual, seção 14, **renomeado** |

- **COMBO:** até 10 passos, cada um com destino e tempo. Vem do TURBO atual, que tem até 4.
- **TURBO:** continua repetindo, mas com menos passos que o COMBO. O número exato é a
  pendência 1 abaixo.

**Vocabulário.** Neste documento, "passo" é cada etapa de uma sequência. O manual usa
"sequência" para uma volta completa do conjunto de passos, e o COMBO não tem essa leitura.
Por isso, de agora em diante, o número máximo de passos do COMBO é **10 passos**, e não
"10 sequências".

### 15.2 TURBO desligado e finalizadores (confirmado)

- **BLOQ** suspende e pausa: a máquina congela e retoma (seção 14.2).
- **Finalizadores** encerram: gatilho em FINALIZA, e a relação EXCL.

### 15.3 EXCL com memória (muda o manual)

O manual diz que EXCL é disparo único e que voltar é um começo novo. O autor acrescenta
uma regra de rearme:

- **Quando o EXCL dispara**, o MOD fica finalizado.
- **Ele só volta** quando um **novo acionamento** de subida acontece em um MOD ligado por
  ASSOC: na fonte, no gatilho, ou na resposta. Isso é o **rearme**.

Isso é uma mudança de modelo, e não só de nome: o EXCL passa a ter um estado que sobrevive
entre quadros, e que é desfeito por um evento de outro MOD.

### 15.4 Os dois momentos do EXCL: nomes propostos

Você pediu nomes para as duas opções:

| opção | significado | nome sugerido |
|---|---|---|
| o EXCL dispara quando a resposta **sobe** | o alvo começa | **SUBIDA** |
| o EXCL dispara quando a resposta **finaliza** | o alvo termina | **FIM** |

**Por que esses nomes.** Usam o mesmo vocabulário de "subir" e "finalizar" da seção 13.3.
"FINALIZA" não serve, porque já é o nome do modo de gatilho (só desliga). Usar a mesma
palavra nos dois casos confundiria o usuário e o código. "FIM" é curto e cabe na tela
do OLED.

### 15.5 Pendências que restam

1. **Passos do TURBO simplificado.** Quantos passos? O autor disse "menos", sem número.
2. **Confirmar os nomes SUBIDA e FIM** para as opções do EXCL, ou escolher outros.
3. **Confirmar o rearme:** um novo acionamento de subida em qualquer MOD ligado por ASSOC
   rearma o EXCL (fonte, gatilho ou resposta). Confirmar se "resposta" conta.

### 15.6 Decisões do autor: TURBO com um passo, nomes INIC e FIM, rearme

**TURBO = 1 passo.** Um destino e um tempo, intercalados com a latência (LAT). É o turbo
clássico de um botão: liga pelo tempo do passo, fica em silêncio pela latência, e repete.
O COMBO é o que tem sequência de até 10 passos.

**Nomes do EXCL: INIC e FIM.** INIC quando a resposta começa, FIM quando ela termina.
Substituem SUBIDA e FIM da seção 15.4.

**Rearme, em linguagem simples.** O EXCL tem dois MODs ligados: o MOD que se encerra
(chamado aqui de X) e o MOD que dá o sinal de volta, ligado por ASSOC (chamado Y).

1. X tem EXCL apontando para um alvo. Quando esse alvo começa (INIC), X se encerra.
2. X fica encerrado. Ele não volta sozinho quando o alvo termina.
3. X só volta quando **Y sobe de novo**, ou seja, quando a **resposta de Y** começa outra
   vez (borda de subida do DELTA de Y).

Ou seja, rearme = Y sobe de novo. Não basta apertar o botão de Y, nem o gatilho de Y.
Só a resposta de Y, que é o destino acionado, conta.

**Pendência de confirmação:** confirme se o rearme é só pela resposta de Y (DELTA
subindo), como acima. A seção 15.3 listava também fonte e gatilho, e isso ficou de fora.

### 15.7 Rearme confirmado

Confirmado pelo autor: o rearme do EXCL vem **só da resposta** do MOD ligado por ASSOC
(a borda de subida do DELTA dele). Fonte e gatilho não rearmam.

---

## 16. Fase 3a: o que está feito, e o que falta

### 16.1 Feito

| peça | arquivo | bancada | sabotagens |
|---|---|---|---|
| tabela única de tempo (67 valores) | `src/tempo.c`, `src/tempo.h` | `ferramentas/testa_tempo.c` | 4, todas pelo motivo certo |
| buffer duplo com versão | `src/perfil.c`, `src/perfil.h` | `ferramentas/testa_perfil.c` | 2, reproduzidas 3 de 3 |

**Tabela de tempo.** Os valores vêm da união das tabelas do manual, verificada por script.
A bancada escreve as listas do manual a mão, sem usar o gerador, e confere a tabela contra
elas. O item guarda um índice, e `tempo_ms` recusa índice fora da lista.

**Buffer duplo.** O escritor grava no buffer inativo, troca o ativo, e incrementa a versão.
O leitor lê a versão, lê o ativo, copia, e lê a versão de novo. Se mudou, descarta a cópia.
A bancada roda duas threads: 200 mil publicações e leituras concorrentes. Em três execuções,
a bancada aceitou centenas de milhares de cópias e descartou outras tantas, e nenhuma
cópia aceita saiu pela metade.

**Limite honesto.** A bancada prova o comportamento no x86-64 do Linux. O PS5 é x86-64 também,
mas o caminho de risco é o do gancho, e a leitura dentro do jogo só será confirmada no console.

### 16.2 Mudança de padrão C: de C99 para C11

- **O que mudou:** o Makefile passa a compilar as bancadas com `-std=c11`.
- **Por quê:** o buffer duplo usa `stdatomic.h`, que é C11. Em C99, a leitura e a escrita
  compartilhadas entre threads não teriam garantia de ordem.
- **Comparado ao que substituiu:** o C99 valia para `passagem.c` e para os bancos do
  Alvo 1 e 2. Esses arquivos não usam nada do C11, e continuam compilando sem alteração,
  como o `make bancada` confirma.

### 16.3 Falta na fase 3a

1. **Modelo de MOD v3** em C: ALFA com os três modos (QUALQUER, SIMULTÂNEO, SEQUÊNCIA),
   GAMA como lista, DELTA com quatro tipos (DIRECT, STATIC, TURBO, COMBO), e ZETA com
   BLOQ, EXCL (INIC ou FIM) e ASSOC.
2. **Formato binário do perfil** que vai dentro do buffer duplo.
3. **Teto de itens por lista:** a medir, por contagem de operações por quadro na bancada.

Depois da 3a, as fases 3b a 3d precisam de console.

---

## 17. Fase 3a: a ALFA do modelo v3

### 17.1 O que foi feito

| peça | arquivo | bancada | sabotagens |
|---|---|---|---|
| ALFA: fonte, BETA, os três modos, sequência, OCULTO | `src/alfa.c`, `src/alfa.h` | `ferramentas/testa_alfa.c` | 6, todas pelo motivo certo |

A bancada tem 21 verificações. Os resultados esperados foram escritos a mão a partir das
regras do autor, e não vêm do código de `alfa.c`.

**Como a ALFA funciona, por quadro:**

1. Calcula o nível de cada fonte. Para botão é o bit apertado. Para eixo e gatilho é a
   condição BETA (MULT, zona, DIR, INV).
2. Calcula a **borda** de cada fonte: o instante em que o nível sobe.
3. Suprime do jogo as fontes OCULTO.
4. Aplica o modo: QUALQUER (OU), SIMULTÂNEO (E dentro de uma janela) ou SEQUÊNCIA.

### 17.2 Decisões que eram do autor e foram implementadas como ele definiu

- QUALQUER é OU. SIMULTÂNEO é E dentro de uma janela. SEQUÊNCIA é ordem com espera.
- Espera estourada reinicia. Passo errado reinicia. Gatilho durante a sequência reinicia.
- Passo esperado tem prioridade sobre o gatilho, se forem o mesmo botão.
- OCULTO suprime a fonte do jogo o tempo todo.
- BETA: MULT escala o valor antes da zona, com limite de ±100. INV inverte a zona.

### 17.3 Decisões que NÃO eram do autor, e que ele precisa confirmar

Estas foram escolhas minhas para fechar a implementação. Estão marcadas no código.

1. **O passo de uma sequência é a borda de subida**, não o nível. Segurar um botão conta
   uma vez. A bancada tem um teste para isso.
2. **Passos no mesmo quadro** são processados pela ordem dos índices da sequência.
3. **Repetir o passo anterior** (por exemplo, apertar CROSS duas vezes seguidas numa
   sequência de CROSS, CIRCLE, SQUARE) **reinicia**. A regra é estrita: qualquer botão
   que não seja o passo esperado reinicia. Isso pode ser duro em jogo de luta, e vale
   revisar com o autor.
4. **Depois de uma SIMULTÂNEO completa**, o conjunto fica ativo enquanto todas as fontes
   estiverem apertadas. Soltar uma desmonta.

### 17.4 Perguntas abertas que a ALFA não responde sozinha

- **Resposta da sequência.** Quando a sequência conclui, a ALFA sinaliza um pulso
  (`concluida` e `ativa` por um quadro). Não está decidido por quanto tempo a resposta
  dura depois disso. Isso depende de como o DELTA alimenta o COMBO e o TURBO, e fica para
  a próxima fatia.
- **Passo errado em jogo de luta.** Como descrito em 17.3, item 3. Só se resolve com o
  autor e, depois, com teste com pessoas.
- **Janela do SIMULTÂNEO.** Continua em 50 ms, provisório (seção 13.6).

### 17.5 Limites

- `ALFA_MAX_FONTES = 8` é **provisório**. Não foi medido o custo por quadro.
- A bancada roda no Linux. O custo no gancho do PS5 não foi medido.

### 17.6 Próximas fatias da fase 3a

1. GAMA como lista de gatilhos (2CLK, TOGGLE, HOLD_AT, HOLD_AP, AFTER, BLOK).
2. DELTA: DIRECT, STATIC, TURBO (um passo) e COMBO (até dez passos).
3. ZETA: BLOQ, EXCL (INIC ou FIM, com rearme pela resposta de ASSOC) e ASSOC.
4. Formato binário do perfil que carrega tudo isso.

### 17.7 Pergunta aberta: DELAY quando a fonte é uma sequência

O autor propôs que o DELAY seja obrigatório quando a fonte é uma sequência. Duas leituras:

- **A. DELAY é o atraso até a resposta começar** (seção 12: conta do fim da sequência).
  Então falta uma **duração da resposta**, obrigatória para a sequência, com valor da tabela
  única de tempo. Recomendada.
- **B. DELAY é a duração da resposta.** Sem DELAY, a resposta teria duração zero. Isso muda
  a regra da seção 12 para o DELAY.

Pendente de decisão do autor.

### 17.8 Decisão do autor: DELAY é o atraso, e a resposta é TURBO ou COMBO

**Decidido pelo autor:** o DELAY é o atraso até a resposta começar (opção A da seção 17.7).
A resposta de uma sequência passa pelo **TURBO** ou pelo **COMBO** do DELTA. Não há um
parâmetro de duração separado.

**Consequências propostas, pendentes de confirmação:**

1. **TURBO com sequência como fonte:** o campo CICLOS aceita só 1 a 7. ATIVO fica proibido,
   porque o pulso da sequência dura um quadro e o MOD não fica respondendo. A duração da
   resposta é o número de ciclos.
2. **COMBO com sequência como fonte:** encaixa bem, porque o COMBO toca uma vez por
   acionamento e termina sozinho.
3. **DIRECT e STATIC com sequência como fonte:** proposto **não permitir**. Os dois dependem
   de a fonte continuar acionada, e um pulso não sustenta isso.
4. **DELAY quando a fonte é sequência:** obrigatório. O valor OFF (índice 0) é uma escolha
   válida, e significa começar logo após a conclusão.

**Perguntas:**
- Confirma as consequências 1 a 3?
- A consequência 4 (DELAY obrigatório) vale mesmo com OFF como escolha?

### 17.9 Pergunta aberta: sequência como fonte de STATIC, e ALTERNA/FINALIZA na sequência

**Proposta do autor:** STATIC aceita sequência como fonte, porque a ALFA ganha as variáveis
ALTERNA/FINALIZA.

**Consequência técnica:** com ALTERNA ou FINALIZA, a sequência deixa de ser um pulso de um
quadro e passa a ser um **estado** que persiste entre quadros. A ALFA atual só emite pulso,
então isso exige mudança em `src/alfa.c` e na bancada.

**Leitura proposta, a confirmar:**
- **ALTERNA:** cada sequência concluída liga o estado se estiver desligado, e desliga se
  estiver ligado.
- **FINALIZA:** a sequência concluída só desliga. **Pendente:** com a sequência como única
  fonte, FINALIZA nunca liga. Ou FINALIZA não se aplica à sequência, ou precisa de outro
  acionamento para ligar.

**Tabela proposta de compatibilidade (fonte = sequência):**

| tipo de DELTA | aceita? | condição |
|---|---|---|
| TURBO | sim | pulso; CICLOS de 1 a 7 |
| COMBO | sim | pulso; toca uma vez |
| STATIC | sim | com ALTERNA: a sequência trava e destrava o destino |
| DIRECT | a decidir | não, ou sim com ALTERNA |

**Perguntas:**
1. A leitura de ALTERNA e FINALIZA está certa? Como FINALIZA liga, se a sequência é a única fonte?
2. DIRECT aceita sequência com ALTERNA?

---

## 18. Mudança estrutural do autor: GAMA se funde com a ALFA (proposta v4)

**Decidido pelo autor:** a GAMA deixa de existir como seção. Os gatilhos passam para dentro da
ALFA. Cada tipo de gatilho tem as mesmas duas partes:

- **Fontes:** qualquer botão, gatilho ou eixo. O TOUCH é reconhecido como botão. O arrasto
  de dedo no touchpad é reconhecido como eixo X e eixo Y, e tratado como eixo.
- **Condições:** ATIVO/OCULTO, FINALIZA/ALTERNA, DELAY (escolhido na tabela única de tempo).
  Para eixo e gatilho, entra a BETA.

| tipo | fontes | condições |
|---|---|---|
| 2CLK | botão, gatilho, eixo | ATIVO/OCULTO, FINALIZA/ALTERNA, DELAY |
| TOGGLE | botão, gatilho, eixo | ATIVO/OCULTO, FINALIZA/ALTERNA, DELAY |
| HOLD_AT | botão, gatilho, eixo | ATIVO/OCULTO, FINALIZA/ALTERNA, DELAY |
| HOLD_AP | botão, gatilho, eixo | ATIVO/OCULTO, FINALIZA/ALTERNA, DELAY |
| BLOCK | botão, gatilho, eixo | ATIVO/OCULTO (sem FINALIZA/ALTERNA e sem DELAY) |

**AFTER sai.** O DELAY já cobre o atraso depois de soltar.

**Vocabulário de toque:** TOUCH é botão. O arrasto é eixo X e eixo Y.

### 18.1 Perguntas que precisam de resposta antes de codar

1. **Relação com os modos QUALQUER, SIMULTÂNEO e SEQUÊNCIA.** Esses modos continuam valendo
   dentro de cada tipo de gatilho? Por exemplo, um 2CLK com SEQUÊNCIA de duas fontes? Ou o
   tipo define o comportamento, e os modos desaparecem?
2. **Quantas entradas por MOD.** Antes a GAMA tinha uma lista de gatilhos. Agora cada entrada
   da ALFA tem tipo, fontes e condições. Qual é o teto de entradas? Continua provisório?
3. **Arrasto: absoluto ou relativo.** O eixo do touchpad é a posição absoluta do dedo, mapeada
   para -100 a +100? Ou é o deslocamento desde o toque? O manual do Cronus não cobre isso,
   porque o Cronus tinha touch só como categoria de fonte.

Enquanto essas três não forem respondidas, não altero `src/alfa.c`.

### 18.2 Respostas do autor às perguntas da seção 18.1

**Pergunta 1 (modos).** As alternativas de ALFA que são **fontes** são: **SEQUÊNCIA**, **2CLK**,
**HOLD_AT**, **HOLD_AP** e **BLOCK**. Nas alternativas de fonte, as escolhas de botão, gatilho
e eixo ficam abertas. O sinal **+** adiciona uma entrada à lista.

- **2CLK não é sequência.** Ele não entra como passo de SEQUÊNCIA.

**Pergunta 2 (entradas por MOD).** Respondida parcialmente: 2CLK não é sequência. O teto de
entradas por MOD **não foi respondido**.

**Pergunta 3 (arrasto).** O arrasto do dedo define os eixos X e Y. O sinal **trava no último
toque**: ao soltar o dedo, o eixo continua no último valor. Isso é o mesmo comportamento de
STATIC para eixo.

### 18.3 O que ainda precisa de confirmação

1. **O "+" muda o significado conforme a alternativa?** Em SEQUÊNCIA, a ordem dos "+" é a ordem
   da sequência. Em 2CLK, HOLD_AT, HOLD_AP e BLOCK, o "+" junta entradas que devem acontecer
   juntas, como o SIMULTÂNEO? Ou cada entrada é independente, como o QUALQUER?
2. **QUALQUER e SIMULTÂNEO continuam como alternativas de fonte?** Só SEQUÊNCIA foi citado.
3. **Teto de entradas por MOD.** Não foi respondido.
4. **Eixo do touch antes do primeiro toque.** Vale zero? Proposta: zero, e a posição absoluta
   do dedo mapeada para -100 a +100.

### 18.4 Decisões do autor: abas da ALFA, ordem e teto

**Decidido:**

- A ALFA tem abas: **LIVRE**, **SEQUÊNCIA**, **2CLK**, **HOLD_AT**, **HOLD_AP** e **BLOCK**.
  Em cada aba, o **+** adiciona botões, gatilhos e eixos.
- **Só a SEQUÊNCIA obriga a ordem.** As outras abas aceitam acionamento simultâneo.
- **Teto provisório de 30 modos.**
- **Eixo do touch:** zero antes do primeiro toque. Depois, posição absoluta do dedo mapeada
  para -100 a +100, e o valor trava no último toque (confirmado na seção 18.2, item 4).

### 18.5 Perguntas que faltam para codar

1. **"Acionamento simultâneo" nas abas que não são SEQUÊNCIA.** Os botões do mesmo "+" valem
   como OU (basta um) ou como E (todos juntos no mesmo instante)? Exemplo: numa aba 2CLK com
   L1 + R1, o 2CLK conta quando L1 é clicado duas vezes, ou quando L1 e R1 são clicados juntos?
   A aba LIVRE é o OU?
2. **Teto de 30.** É o total de entradas da ALFA, somando todas as abas? Ou por aba?

### 18.6 Decisões do autor: E e OU por aba, e teto de 30

**Decidido:**

- **2CLK com "+" é OU, por botão.** Cada botão conta pelo próprio duplo clique. Apertar duas
  vezes L1 e duas vezes R1 conta, porque cada botão fez a própria ação dupla. Não é a soma
  de L1 com R1.
- **LIVRE aceita E e OU.** O autor escolhe, na própria aba, qual dos dois vale.
- **Teto de 30 entradas na ALFA, no total.** São cinco por aba. Como são seis abas, o total
  bate com o teto de 30.

**Lacunas identificadas pelo código (não respondidas pelo autor):**

1. **TOGGLE não aparece nas abas.** Proposta: TOGGLE é a condição ALTERNA aplicada a uma fonte,
   e não uma aba própria.
2. **HOLD_AT, HOLD_AP e BLOCK com "+".** Presumo OU por botão, como o 2CLK. Confirmar.
3. **O E da aba LIVRE** exige que as entradas aconteçam dentro da janela de simultaneidade
   (50 ms, provisório), como no SIMULTÂNEO. Confirmar.

### 18.7 Confirmações do autor

- **TOGGLE sai das abas.** ALTERNA, uma das condições, faz o papel dele.
- **HOLD_AT, HOLD_AP e BLOCK aceitam E ou OU**, escolhido pelo autor, como o LIVRE. Padrão
  de implementação: OU, até o autor escolher outro.
- **E exige a janela de simultaneidade (50 ms provisório)**, em todas as abas que o aceitam,
  inclusive HOLD_AT, HOLD_AP e BLOCK.
- **2CLK é sempre OU**, por botão. Não aceita E.
- **SEQUÊNCIA é sempre ordenada.** Não aceita E nem OU.

### 18.8 Regra de cada aba (resumo para implementação)

| aba | fonte | E ou OU | acionamento |
|---|---|---|---|
| LIVRE | botão, gatilho, eixo | escolhido | nível: a aba fica ativa enquanto o acionamento vale |
| SEQUÊNCIA | botão, gatilho, eixo | ordem | pulso ao concluir a sequência |
| 2CLK | botão, gatilho, eixo | OU, por botão | pulso no segundo clique, dentro de 300 ms (fixo) |
| HOLD_AT | botão, gatilho, eixo | escolhido | pulso na soltura, se a pressão ficou até o tempo (toque curto) |
| HOLD_AP | botão, gatilho, eixo | escolhido | pulso quando a pressão passa do tempo, ainda apertado |
| BLOCK | botão, gatilho, eixo | escolhido | suspende o MOD enquanto o acionamento vale |

**Limite:** 5 entradas por aba. Teto total de 30 na ALFA.

### 18.9 O que ainda não tem regra, e fica fora desta fatia

- **ALTERNA, FINALIZA e DELAY** nas abas. Dependem de como uma aba de nível interage com o
  DELAY, e do que FINALIZA faz quando a fonte é a própria aba. Pendente de decisão.
- **Gatilho que reinicia a sequência.** A GAMA saiu, então não há mais um gatilho externo.
  Pendente: qual aba reinicia a SEQUÊNCIA, se alguma.

---

## 19. ALFA v4 implementada na bancada

### 19.1 O que foi feito

| peça | arquivo | bancada | sabotagens |
|---|---|---|---|
| ALFA v4: seis abas, 5 entradas por aba, teto de 30 | `src/alfa.c`, `src/alfa.h` | `ferramentas/testa_alfa.c` | 7, todas pelo motivo certo |

A v4 **substitui** a v3 (QUALQUER, SIMULTÂNEO e SEQUÊNCIA como modos de MOD). Os modos
viraram abas, e o E ou OU é escolhido por aba, como manda a seção 18.8.

### 19.2 Como cada aba se comporta

- **LIVRE:** OU ou E. Em E, todas as entradas precisam cair na janela de 50 ms.
- **SEQUÊNCIA:** ordem obrigatória. Espera por passo, do catálogo. Pulso ao concluir.
  Espera estourada e passo errado reiniciam.
- **2CLK:** OU por botão. Pulso no segundo clique, dentro de 300 ms (fixo). Cada botão tem
  o seu pendente: o exemplo de L1 e R1 da seção 18.6 está na bancada, com instantes exatos.
- **HOLD_AP:** pulso uma vez por pressão, quando a pressão passa do tempo do catálogo.
- **HOLD_AT:** pulso na soltura, só se a pressão não passou do tempo (toque curto).
- **BLOCK:** nível. Suspende enquanto a entrada (OU) ou todas (E) estiverem apertadas.
- **OCULTO:** suprime os botões da aba, o tempo todo.

### 19.3 Decisões que não eram do autor, marcadas para confirmação

1. **HOLD_AP e HOLD_AT, em E:** a janela de 50 ms vale para o conjunto de pulsos, do mesmo
   jeito que vale para o LIVRE.
2. **BLOCK em E:** o bloqueio vale enquanto todas as entradas estiverem apertadas, dentro da
   janela.
3. **Repetir uma fonte** é recusado em SEQUÊNCIA e em E, mas aceito em OU.

### 19.4 Ainda fora desta fatia

- **ALTERNA, FINALIZA e DELAY** nas abas (seção 18.9).
- **Gatilho que reinicia a SEQUÊNCIA** (seção 18.9).
- **Teto de 5 por aba e de 30 no total** são constantes provisórias. O custo por quadro no
  PS5 não foi medido.

### 19.5 Decisões do autor: ALTERNA, FINALIZA, DELAY e reinício da sequência

**Decidido:**

1. **FINALIZA desliga a saída ligada por outra aba** (opção b). Não é a própria aba que se
   desliga.
2. **LIVRE e SEQUÊNCIA também aceitam ALTERNA e FINALIZA.** BLOCK continua só com
   ATIVO/OCULTO.
3. **DELAY é contado do fim do pulso**, e o valor sai da tabela padrão de 67 valores.
4. **OFF é uma opção fora da tabela**, como "sem latência" no TURBO.
5. **DELAY é obrigatório quando a fonte é SEQUÊNCIA.** Nas outras abas, OFF é o padrão.
6. **Se a fonte solta durante o atraso, o atraso é cancelado.**
7. **Sem gatilho externo de reinício.** A SEQUÊNCIA reinicia só por espera estourada ou passo
   errado.

**Pendências que travam a implementação:**

A. **Alvo do FINALIZA.** Se FINALIZA desliga a saída de outra aba, é preciso saber qual. Proposta:
   um campo "alvo" na aba, com o índice da aba cuja saída é desligada. Pode ser assim?

B. **Cancelamento em abas de pulso.** Em 2CLK, HOLD_AT, HOLD_AP e SEQUÊNCIA, a fonte solta
   normalmente logo depois do pulso. Se "soltar cancela", todo atraso de aba de pulso
   seria cancelado. Proposta: o cancelamento vale só para abas de nível (LIVRE e BLOCK).
   Nas abas de pulso, o atraso não é cancelado pela soltura. Pode ser assim?

C. **DELAY em abas de nível.** "Fim do pulso", em aba de nível, é a descida do nível, ou a
   subida? Proposta: conta da subida, e o cancelamento é pela descida antes do fim do atraso.

### 19.6 Correção de 19.5 e fluxo: ALFA dispara o DELTA, que gera a resposta

**Correção.** Na seção 19.5, eu escrevi que FINALIZA desliga "a saída de outra aba". Está
errado. O FINALIZA atua sobre a **resposta do DELTA**.

**Fluxo:** ALFA (abas, inclusive as antigas da GAMA) → DELAY → DELTA (DIRECT, STATIC, TURBO
ou COMBO) → resposta → destinos.

**Como cada tipo de DELTA termina:**

| tipo | termina sozinho? | observação |
|---|---|---|
| DIRECT | sim | quando a fonte sai da zona, ou o gatilho solta |
| STATIC | não | só FINALIZA encerra |
| TURBO sem ciclos | não | só FINALIZA encerra |
| TURBO com ciclos | sim | ao completar os ciclos |
| COMBO | sim | ao concluir os passos |

**Consequência para FINALIZA:** só tem efeito com STATIC e TURBO sem ciclos. Com DIRECT,
TURBO com ciclos e COMBO, FINALIZA não faz nada, porque a resposta já termina sozinha.

**Pendências novas:**

A'. **Como o FINALIZA escolhe a resposta que encerra?** Opções: (a) ele encerra a resposta
do DELTA da própria aba; (b) ele encerra uma resposta de outra aba, escolhida por uma lista,
como as relações ZETA.

B'. **Quais tipos de DELTA aceitam cada aba de origem?** Proposta:
- DIRECT: só com fonte de nível (LIVRE, BLOCK). Um pulso não acompanha a fonte.
- STATIC: qualquer origem com ALTERNA, ou com nível.
- TURBO e COMBO: qualquer origem. São sequências que tocam sozinhas.

C'. **Cancelamento do DELAY (19.5, B e C)** continua valendo, porque ocorre antes do DELTA.

### 19.7 Decisões do autor sobre FINALIZA, STATIC, DIRECT, TURBO e COMBO

**Decidido:**

- **A'. FINALIZA encerra a resposta do DELTA da própria aba** (opção a).
- **DIRECT aceita só fonte de nível (LIVRE ou BLOCK).** Um pulso não acompanha a fonte.
- **TURBO e COMBO aceitam qualquer origem.**
- **COMBO ganha CICLOS**, com as mesmas regras de finalização do TURBO: ATIVO, ou de 1 a 7.

**Correção de minha proposta B':** eu tinha escrito que STATIC aceita "com ALTERNA ou com nível".
A regra é mais ampla. Pelo manual (seção 14.4), STATIC é uma **chave**:

| origem | comportamento do STATIC |
|---|---|
| pulso (2CLK, HOLD, SEQUÊNCIA) | o pulso **trava** o destino no alto. A soltura não destrava. Só FINALIZA destrava |
| nível (LIVRE, BLOCK) | a fonte trava o destino enquanto está na zona. Ao sair, o destino cai, a menos que a trava esteja presa |
| com ALTERNA | cada pulso alterna a trava: trava, destrava, trava |

Então STATIC aceita **qualquer origem**. Não precisa de ALTERNA.

**Pendência nova, que a decisão de COMBO com CICLOS levanta:** a seção 17.8 proibia ATIVO em
TURBO quando a origem é sequência, porque o pulso não mantém o MOD respondendo. Mas, com
FINALIZA, um TURBO em ATIVO iniciado por pulso continua oscilando até FINALIZA encerrar. A
proibição pode não ser necessária. Você quer manter a proibição, ou liberar ATIVO com pulso?

### 19.8 Decisão do autor: ATIVO liberado com origem de pulso

**Decidido:** TURBO e COMBO aceitam CICLOS em ATIVO com qualquer origem, inclusive pulso. Um
TURBO iniciado por pulso oscila até FINALIZA encerrar. Fica revogada a proibição da seção
17.8, item 1.

**Pendências que ainda travam a implementação da ALFA:**

- **B. Cancelamento do DELAY em abas de pulso.** Proposta: o cancelamento vale só para abas de
  nível (LIVRE e BLOCK). Nas abas de pulso, soltar a fonte não cancela o atraso.
- **C. DELAY em aba de nível.** Proposta: conta da subida. A descida antes do fim do atraso
  cancela.

### 19.9 Decisão do autor: DELAY conta a partir da descida do pulso

**Decidido:** o DELAY é escolhido pelo usuário, e sempre conta a partir da descida do pulso.

**Consequências:**

- **B (cancelamento por soltura) fica resolvido.** O atraso só começa quando o pulso desce,
  então não há soltura durante o atraso para cancelar.
- **C (subida ou descida em aba de nível) fica resolvido.** Conta da descida.
- **Consequência a confirmar:** em LIVRE e BLOCK, a descida é a soltura. Com DELAY configurado,
  um botão mantido apertado não dispara a resposta. Ela só começa depois da soltura e do atraso.

**Pendência nova:** se um novo pulso chegar enquanto o atraso corre, o atraso atual continua,
ou é reiniciado? Proposta: continua, e o novo pulso é ignorado.

### 19.10 Decisões do autor: exceção de LIVRE e BLOCK, e reinício do atraso

**Regra geral:** o DELAY conta a partir da **descida do pulso**.

**Exceção, só para LIVRE e BLOCK:** o DELAY pode contar a partir do **acionamento**. Se o
acionamento descer antes do fim do atraso, a contagem **zera**. Com isso:

- um botão mantido apertado **dispara** a resposta depois do atraso, sem esperar a soltura;
- se o botão for solto antes do fim do atraso, nada é disparado.

**Correção de 19.9:** a consequência ali descrita ("não dispara enquanto estiver apertado")
estava errada. Ela vale só para a regra geral, e LIVRE e BLOCK são a exceção.

**Novo pulso durante o atraso:** a contagem **zera**. Não é ignorado.

**Para implementação:** SEQUÊNCIA exige DELAY diferente de OFF. As demais abas aceitam OFF.
Com OFF, a resposta sai no mesmo quadro do acionamento.

### 19.11 DELAY implementado na ALFA

**Feito:** o estágio do DELAY na ALFA, com a regra geral e a exceção de LIVRE e BLOCK, OFF,
zeramento por novo pulso ou por soltura antes do fim, e obrigatoriedade na SEQUÊNCIA.
A bancada da ALFA tem agora as verificações de DELAY, e a SEQUÊNCIA recusa o DELAY não
escolhido.

**Como o DELAY é escolhido:** o valor `ALFA_DELAY_NAO_ESCOLHIDO` (254) é o padrão explícito.
Zero não serve como padrão, porque seria o índice 0 do catálogo, ou seja, 10 ms. As abas
que não são SEQUÊNCIA tratam o não escolhido como OFF. A SEQUÊNCIA o recusa.

**Sabotagens do DELAY:** quatro. Três reprovaram pelo motivo certo. A primeira versão de uma
delas sobreviveu: o caso de novo pulso não tocava o instante em que o atraso antigo venceria,
então o zeramento não fazia diferença. O caso foi ajustado, e a sabotagem agora reprova.

**Fica para o estágio do DELTA:** ALTERNA e FINALIZA. A ALFA entrega `disparo[aba]`, o pulso
que vai ao DELTA. O DELTA decide, por tipo, o que fazer com ele.

---

## 20. DELTA: a resposta do MOD (fase 3a)

### 20.1 Leitura usada na implementação (a confirmar)

- **Um DELTA por MOD.** As abas da ALFA alimentam esse DELTA.
- **Cada disparo de aba tem uma ação:** LIGA (padrão), ALTERNA ou FINALIZA. FINALIZA encerra a
  resposta do próprio MOD.
- **TURBO é um COMBO de um passo.** A mesma máquina de execução serve aos dois. O TURBO
  limita a um passo, e o COMBO vai até dez.
- **DIRECT** segue o nível da origem, e não tem ação própria.
- **STATIC** trava na subida de LIGA, alterna com ALTERNA e destrava com FINALIZA.
- **TURBO e COMBO** começam com LIGA e param com FINALIZA, ou ao completar os ciclos
  (quando CICLOS não é ATIVO). LIGA enquanto já está rodando é ignorado.
- **Latência:** entre passos e entre ciclos. Não há latência depois do último ciclo.

### 20.2 Limites desta fatia

- Destinos são booleanos (ligado ou não). A intensidade dos eixos, o MULT e a acumulação
  por intensidade máxima ficam para depois.
- Os 18 destinos são os do Cronus. Os sinais extras do DualSense não entram ainda.
- A integração com as abas da ALFA (quem produz LIGA, ALTERNA e FINALIZA) fica para a
  próxima fatia.

### 20.3 DELTA implementado e verificado

| peça | arquivo | bancada | verificações | sabotagens |
|---|---|---|---|---|
| DELTA: DIRECT, STATIC, TURBO, COMBO | `src/delta.c`, `src/delta.h` | `ferramentas/testa_delta.c` | 11 | 6, todas pelo motivo certo |

**Sem deriva.** Cada fase começa no instante planejado (início anterior mais a duração), e
não no quadro em que a anterior foi detectada. Assim o erro não se acumula. Só ressincroniza
com o relógio se o quadro atrasou mais de uma fase inteira. A bancada tem um caso com quadros
de 7 ms, em que o término tem de sair no quadro de 112 ms. A sabotagem que usa o quadro da
detecção reprova.

**Duas falhas da bancada que eu corrigi, e que estão registradas aqui:**
- A bancada juntava dois casos com a mesma variável de resultado, então uma falha num caso
  aparecia no outro.
- Pulsos consecutivos sem quadro zero entre eles não geram borda. A bancada precisa de quadros
  zero entre pulsos, como o sistema real tem.

**Limites desta fatia:** destinos booleanos; sem intensidade de eixo nem MULT; sem os sinais
extras do DualSense; a integração com as abas da ALFA (quem produz LIGA, ALTERNA e FINALIZA)
fica para a próxima fatia.

---

## 21. Integração ALFA → DELTA: o MOD completo

### 21.1 Decisões de integração (as três primeiras são do autor; as outras, minhas)

- **Cada aba com LIGA, ALTERNA ou FINALIZA** alimenta o DELTA do MOD. O padrão é LIGA.
- **FINALIZA** encerra a resposta do próprio MOD.
- **BLOCK suspende o MOD** (decisão de 20.1 e manual, seção 15.2). Enquanto o BLOCK estiver
  ativo, o DELTA congela: não responde, não corre os tempos, e não processa eventos. Ao soltar,
  retoma de onde parou.
- *Minha:* **DIRECT não aceita DELAY** nas abas de origem. Ele acompanha o nível, e um atraso
  quebraria essa leitura. Recusar na validação é melhor que ignorar em silêncio.
- *Minha:* **DIRECT só aceita origem de nível** (LIVRE ou BLOCK). Já estava em 19.7.
- *Minha:* o MOD precisa de pelo menos uma aba que alimente o DELTA.
- *Minha:* a ação ALTERNA/FINALIZA/LIGA é configurada por aba, fora da ALFA, no `ModCfg`.

### 21.2 Integração implementada e verificada

| peça | arquivo | bancada | verificações | sabotagens |
|---|---|---|---|---|
| MOD completo: ALFA + DELTA, com validação | `src/modelo.c`, `src/modelo.h` | `ferramentas/testa_modelo.c` | 6 | 5, 4 pelo motivo certo; 1 é mutante equivalente |
| congelamento (BLOCK) no DELTA | `src/delta.c` | `ferramentas/testa_delta.c` (teste novo) | 11 no total | 1 nova, reprova |

**Como o MOD funciona, por quadro:** a ALFA avalia as abas. Cada aba com LIGA, ALTERNA ou
FINALIZA entrega o seu disparo (já com DELAY) ao DELTA, pela ação dela. Uma aba BLOCK ativa
congela o DELTA. A saída do DELTA vira o destino do MOD, e as ocultações da ALFA seguem para o
jogo.

**Validação.** O MOD é recusado se: DIRECT tiver origem de pulso ou DELAY; BLOCK tiver ação que
não seja suspender; não houver nenhuma aba que alimente o DELTA; ou a ALFA for inválida.

**Erros da bancada que aparecem no caminho e foram corrigidos:**

- O helper de teste punha ciclos ATIVO em todo MOD. STATIC e DIRECT recusam ciclos, então
  todos os MODs de STATIC eram inválidos. Isso só apareceu porque a validação era verificada.
- Um instante de 119 ms não era amostrado (a bancada amostra de 10 em 10 ms), e a bancada
  reaproveitava o valor da chamada anterior. Agora uma amostra não feita reprova.
- O caso de congelamento do DELTA tinha uma expectativa de 150 ms, calculada a partir do cenário
  do MOD. A conta certa, para o cenário do DELTA, dá 130 ms.
- **Vazamento na pasta de sabotagem:** a primeira rodada de sabotagens copiava só os arquivos
  alterados. Uma sabotagem anterior ficou na cópia, e a M3 pareceu reprovar um caso que não
  tocava. O script foi corrigido para copiar o projeto limpo a cada sabotagem.

**Mutante equivalente (M4).** A linha que zera a saída do MOD quando há BLOCK é redundante,
porque o DELTA já zera a saída quando está congelado. A garantia está coberta no DELTA, pelo
teste de congelamento. A sabotagem no MOD não muda o comportamento, e isso fica registrado
aqui, em vez de ser escondido.

**Limites desta fatia:** os destinos são booleanos; não há intensidade de eixo nem MULT; a
validação entre abas não impede um destino de ser escrito por mais de um MOD (isso é tratado
pela acumulação, que ainda não foi implementada); e a ZETA continua de fora.

---

## 22. ZETA: as relações entre MODs (fase 3a)

### 22.1 Leitura usada (a confirmar)

- **Resposta de pé** = o DELTA do MOD está ligado: DIRECT pelo nível, STATIC pela trava,
  TURBO e COMBO enquanto rodam, incluindo as fases de silêncio. (Manual, seção 15.1. A
  alternativa "ou a fonte está na zona" do STATIC fica de fora desta fatia.)
- **BLOQ:** enquanto algum alvo responde, o MOD fica congelado (como o BLOCK).
- **ASSOC:** o inverso. O MOD só responde enquanto algum alvo responder. No resto do tempo,
  fica congelado.
- **EXCL:** encerra o MOD quando o alvo sobe (INIC) ou quando termina (FIM). Disparo único.
  Volta (rearme) quando um MOD ligado por ASSOC tem a resposta subindo de novo. Esse rearme
  vem só da resposta, conforme você confirmou.
- **Encerrado** zera o DELTA e suprime a saída. A ALFA continua avançando.
- **Defasagem de um quadro:** as relações olham o estado do quadro anterior. Assim o resultado
  não depende da ordem dos MODs (manual, seção 24.1).
- **Limite:** ZETA não tem domínio nesta fatia. Os domínios D1 a D4 ficam para depois, e o
  órfão adotado (manual, seção 15.5) também.

### 22.2 ZETA implementada e verificada

| peça | arquivo | bancada | verificações | sabotagens |
|---|---|---|---|---|
| rede de MODs com BLOQ, ASSOC e EXCL | `src/rede.c`, `src/rede.h` | `ferramentas/testa_rede.c` | 6 | 7, todas pelo motivo certo |
| avaliação do MOD com congelamento e encerramento vindos da rede | `src/modelo.c` (`modelo_avaliar_ext`) | — (coberto pela bancada da rede) | — | — |

**Como funciona, por quadro:** a rede guarda quem está respondendo, quem subiu e quem caiu
no quadro anterior. Cada MOD recebe, então, dois sinais: "congelado" (por BLOQ ou por ASSOC)
e "encerrado" (por EXCL). Um MOD encerrado zera o DELTA e fica sem saída. O rearme acontece
quando um MOD ligado por ASSOC sobe de novo.

**O que a bancada cobre:** validação (relação consigo, índice fora da rede); BLOQ com
defasagem; ASSOC congelando e liberando; EXCL INIC com disparo único; EXCL FIM; e o rearme,
que só acontece pela subida de um MOD ligado por ASSOC. O caso de rearme usa quatro MODs, para
que a queda do alvo não congele o MOD e esconda o estado de encerramento.

**Sabotagens (7):**

- Z1 (BLOQ ignorado), Z2 (ASSOC ignorado), Z3 (EXCL sem disparo único), Z4 (sem rearme),
  Z5 (INIC usando a queda), Z6 (FIM ignorado): cada uma reprovou a verificação correspondente.
- Z7 apaga o quadro anterior, então nenhuma relação reage. É uma sabotagem mais forte que
  "sem defasagem", que ficaria para depois, porque exige recalcular a rede no mesmo quadro.

**Limites desta fatia:** sem domínios D1 a D4; sem o órfão adotado (manual, seção 15.5); a
resposta de pé do STATIC é só a trava, sem a alternativa "ou a fonte está na zona"; sem
acumulação de destinos entre MODs.

---

## 23. Domínios: separação de MODs e desativação (fase 3a)

### 23.1 Leitura usada (a confirmar)

- **Um domínio ligado por vez.** D1 a D4. Um MOD pode estar em um, vários ou nenhum. Sem
  domínio, o MOD nunca roda, e é assim que se desativa um MOD sem apagá-lo (manual, seção 10).
- **Comandos fixos** (manual, seção 6): L1 + duplo clique em L3 = D1; R1 + duplo clique em R3
  = D2; L1 + segurar L3 = D3; R1 + segurar R3 = D4. O mesmo comando no domínio ligado desliga.
- **Janela do duplo clique de domínio: 250 ms** (`TIME_SHORT_PRESS`, manual seção 18). Não é a
  janela de 300 ms dos MODs. **Segurar: 500 ms** (`TIME_HOLD`).
- **Trocar de domínio:** reinicia o estado de execução de todos os MODs (manual, seção 40,
  `Reset_Perfil`), e trava cada MOD até a fonte voltar ao repouso (manual, seção 25.1).
- **MOD travado:** não detecta nem responde, mas continua suprimindo os seus botões (manual,
  seção 25.1). MOD fora do domínio atual: não roda e não suprime nada.

### 23.2 Dominios implementados e verificados

| peça | arquivo | bancada | verificações | sabotagens |
|---|---|---|---|---|
| comandos de dominio (D1 a D4) | `src/dominio.c`, `src/dominio.h` | `ferramentas/testa_dominio.c` | 8 | 8, todas pelo motivo certo |
| rede com dominios, trava e reinicio | `src/rede.c` (alterado), `src/alfa.c` (`alfa_em_repouso`), `src/modelo.c` (`modelo_so_ocultar`) | `ferramentas/testa_dominio.c` e `ferramentas/testa_rede.c` | — | — |

**Como funciona:** a rede lê os comandos de domínio a cada quadro. Um MOD roda só se o seu
PL_HAB (máscara de D1 a D4) contém o domínio ligado. Trocar de domínio reinicia o estado de
todos os MODs e os trava. Um MOD travado não detecta nem responde, mas continua suprimindo os
seus botões, e é destravado quando a sua fonte volta ao repouso.

**Como as sabotagens foram conferidas:** na primeira rodada, três sabotagens sobreviveram:
- D1 (troca não reinicia o estado), porque a trava escondia o efeito. Agora a bancada confere o
  instante depois de a trava soltar, quando o estado antigo apareceria.
- D4 (o modificador não é exigido no duplo clique), porque nenhum caso soltava o modificador
  entre os dois cliques. Agora há um caso para isso.
- D8 (janela de 300 ms em vez de 250 ms), porque nenhum caso tinha intervalo entre 250 e 300 ms.
  Agora há um caso com 280 ms.

**Limites desta fatia:** a validação entre domínios e relações não é verificada (por exemplo,
um alvo de ZETA que não divide o domínio, que o manual aponta como aviso); os comandos são fixos
(o manual também os trata como fixos); e a suspensão de cascata de domínio não foi testada
além do caso simples.

### 23.3 Acumulação de destinos entre MODs

| peça | arquivo | bancada | verificações | sabotagens |
|---|---|---|---|---|
| acumulação e saída do controle | `src/rede.c` (`rede_avaliar`), `src/rede.h` (`RedeSaida.botoes`) | `ferramentas/testa_acumula.c` | 4 | 4, todas pelo motivo certo |

**Regras (manual, seção 26):**

- **Digital escreve só quando liga, por OU.** Dois MODs no mesmo botão funcionam como OU.
- **Saída desligada não força zero.** O sinal cru do jogador passa, porque o MOD não afirma zero.
- **O destino vence a supressão.** A supressão das fontes OCULTO vem antes; os destinos são
  escritos por último.
- **MOD fora do domínio não suprime.** Só os MODs que rodam participam da supressão.

**Saída:** `out->botoes = (entrada & ~suprimidos) | acumulados`.

**Limite desta fatia:** os destinos 10 a 13 são eixos (LX, LY, RX, RY). Eles ainda não viram
botão, porque o modelo não tem intensidade de eixo. Eixos ficam para a próxima fatia, com a regra
de "maior intensidade absoluta" do manual (seção 26.1), que é diferente do OU dos digitais.

**Correção nas bancadas:** o destino de TRIANGLE é o 9, e não o 12 (que é RX). As bancadas da
ZETA e dos domínios usavam 12. Os resultados não mudaram, porque os testes comparavam bits de
destino entre si, mas o nome agora bate com o manual.

---

## 24. Conferência da lista de fontes do controle (PS5)

Lista enviada pelo autor, conferida contra `src/pad.h`, `src/alfa.h`, `src/rede.c` e a fonte
pública do PoorDS4 (`payload/*.c`, que cita os bits de botão).

| entrada | no autor | no código | situação |
|---|---|---|---|
| L1, L3, R1, R3 | botão | `DS_BTN_*` em `pad.h`; bits confirmados no PoorDS4 | ok |
| L2, R2 | gatilho | `GATILHO_L2/R2` em `alfa.h`; bits 0x100 e 0x200 confirmados no PoorDS4 | ok |
| UP, DOWN, LEFT, RIGHT | botão | `DS_BTN_*` em `pad.h` | ok |
| TRIANGLE, CIRCLE, CROSS, SQUARE | botão | `DS_BTN_*` em `pad.h` | ok |
| OPTIONS | botão | `DS_BTN_OPTIONS` = 0x08 em `pad.h` | ok (bit não conferido no PoorDS4) |
| TOUCH | botão | `DS_BTN_TOUCH` = 0x100000 em `pad.h` | ok (bit não conferido no PoorDS4) |
| LX, LY, RX, RY | eixo | `EIXO_LX..RY` em `alfa.h` | ok |
| **PS** | botão | **não definido** | **falta.** O bit não está em `pad.h` nem no PoorDS4. Não chutar |
| **SHARE** | botão | **não definido** | **falta.** Mesmo caso do PS |
| **FINGER1, FINGER2** (botão) | botão | **não definido** | a confirmar: seria "há dedo tocando", derivado de `touchData.fingers` |
| **FINGER1X/Y, FINGER2X/Y** | eixo | **não definido** | falta. Vêm de `touchData.touch[].x/y` (16 bits). Pelo que você disse, são eixos estáticos (travam no último toque) |

**Perguntas:**

1. **Destinos.** O Cronus tem 18 destinos, sem PS, SHARE, OPTIONS, TOUCH nem dedos. No PS5, esses
   entram como destino ou só como fonte? Hoje a rede escreve só os 18.
2. **Bits de PS e SHARE.** Não estão no PoorDS4 nem no `pad.h`. Preciso de outra fonte, ou de você
   medindo no console com o que sai em `buttons`.
3. **FINGER como botão.** Confirme a leitura: "há dedo sobre o touchpad".

### 24.1 Segunda fonte: etaHEN (`daemon/include/pad.hpp`)

O enum `OrbisPadButton` do etaHEN (que cita o OpenOrbis PS4 Toolchain) confere com `pad.h` em
todos os botões que lá aparecem: L3 0x0002, R3 0x0004, OPTIONS 0x0008, UP 0x0010, RIGHT 0x0020,
DOWN 0x0040, LEFT 0x0080, L2 0x0100, R2 0x0200, L1 0x0400, R1 0x0800, TRIANGLE 0x1000,
CIRCLE 0x2000, CROSS 0x4000, SQUARE 0x8000, TOUCH_PAD 0x100000.

**PS e SHARE não estão nesse enum.** Isso não é prova de que o PS5 não os entrega, mas é um
indício forte: o campo `buttons` de `ScePadData`, na ABI pública, não os declara. Possivelmente o
sistema consome PS e SHARE antes do jogo. Não afirmo isso como fato: a confirmação é ler `buttons`
no console enquanto se aperta cada um, e isso depende do Alvo 3.

**Conferência com o Zen Studio:** não foi feita. O Zen Studio é um programa de Windows ligado ao
Cronus físico, e o monitor dele mostra o mapeamento do Cronus, não o `ScePadData` do PS5. Ver um
valor ali não confirmaria o bit do PS5.

### 24.2 Testes no Zen Studio (monitor do PS5 no Cronus)

Feitos pelo autor, com o Cronus conectado ao DualSense. Lidos na tela de entrada do PS5.

| teste | resultado |
|---|---|
| PS e SHARE apertados | vão direto para **100** (não para 1). Botões são 0 ou 100, como no manual |
| Dedo arrastado e solto no touchpad | FINGER1X/Y acompanham a posição e **ficam no último valor** depois de soltar. Confirma o "trava no último toque" |
| FINGER1 com dedo sobre o pad | vai para **100**; sem dedo, volta para **0**. Confirma FINGER1 e FINGER2 como "há dedo sobre o pad" |

**Ressalva importante:** o que o monitor mostra é a leitura do Cronus, com as entradas que o
Zen Studio rotula para o PS5. Ele confirma a existência e o comportamento no Cronus. Não confirma
que o `ScePadData` do PS5 traz esses bits, nem os valores exatos de PS e SHARE nesse campo. Isso
continua dependendo do console (Alvo 3).

**Correção de uma leitura minha:** eu tinha escrito "1" para os botões digitais. Pelo teste, o
valor é 100, como no manual do Cronus (botão é 0 ou 100). A rede e a ALFA tratam botão como bit,
então o modelo não muda.

**Lista final de fontes que o monitor mostra para o PS5** (além das já conferidas): acelerômetro
ACCX, ACCY, ACCZ e giroscópio GYROX, GYROY, GYROZ. Eles não estão na sua lista. Decidir se entram
no modelo é sua escolha.

---

## 25. Sensores de movimento como fonte (ACCX, ACCY, ACCZ, GYROX, GYROY, GYROZ)

**Decidido pelo autor:** entram no modelo como eixos. Com isso, é possível configurar um MOD com
movimentos do controle.

**Implementado:** `EIXO_ACCX..EIXO_GYROZ` (ids 4 a 9) em `src/alfa.h`. Eles usam a mesma BETA que os
eixos analógicos (MULT, zona, DIR, INV). A ocultação passou a usar 12 bits: eixos nos bits 0 a 9,
L2 no 10 e R2 no 11. Antes, L2 e R2 ficavam nos bits 4 e 5, e colidiriam com os novos eixos.

**Verificado:** bancada da ALFA com 3 novas verificações; 3 sabotagens, todas reprovaram pelo motivo
certo (colisão de bits, validação limitada, leitura do eixo errado).

**NÃO medido ainda (pendente):** a conversão do sensor para -100..+100. No `ScePadData`, a
aceleração e a velocidade angular são floats, em unidades que ainda não conferi. O monitor do Zen
Studio mostra valores pequenos em repouso (ACCX +1, GYRO +0), mas não diz a escala. Sem essa
conversão, a faixa de BETA para movimento não tem significado. Ela precisa ser medida: com o
controle parado, em movimento, e nas faixas usadas em jogo. A conversão ainda não está no código;
hoje a entrada recebe o valor já normalizado.

**Também pendente:** zona morta para o sensor. Movimento parado tem ruído, e sem zona morta um MOD
de movimento dispara por tremor.

---

## 26. Toque do touchpad: dedos como botão e como eixo (fase 3a)

**Confirmado no monitor do Zen Studio (24.2):** FINGER1 e FINGER2 são botões (100 com o dedo, 0
sem). FINGER1X/Y e FINGER2X/Y são eixos que acompanham a posição e **travam no último toque**.

**Implementado:** `src/toque.c` converte as coordenadas brutas dos dois dedos para a entrada da
ALFA. A ALFA ganhou `FONTE_DEDO` (presença, ids 0 e 1) e os eixos `EIXO_F1X..F2Y` (ids 10 a 13).
`EIXO_N` passou a 14, então L2 e R2 foram para os bits 14 e 15 da máscara de ocultação.

**Verificado:** bancada do toque com 8 verificações. 3 sabotagens (sem trava, sem limite, dedo 2
lendo o dedo 1), todas reprovaram pelo motivo certo.

**NÃO verificado:** a faixa real das coordenadas do touchpad do PS5. A calibração (`ToqueCal`)
é um parâmetro. A bancada usa 1000 x 500 como valor **arbitrário**, para testar a lógica, e não
afirma a faixa real. Ela precisa ser medida no console, lendo `touchData` com o dedo nas bordas.

**Pendente:** ocultar o toque do jogo (o Cronus tem `Util_Touchpad`). Hoje uma fonte de dedo
oculta não suprime nada.

---

## 27. Formato binário do perfil (fase 3a)

**O que é:** a configuração de uma rede de MODs como bytes, para salvar e carregar. Campo a campo,
em little-endian, sem depender do layout de memória do compilador.

**Layout:** cabeçalho de 8 bytes (magic `DSG1`, versão 1, n de MODs, 2 reservados). Por MOD: 302
bytes de ALFA, acoes e DELTA, mais 14 de ZETA (BLOQ, ASSOC, EXCL, INIC) e domínio. Rodapé: CRC32 de
tudo que vem antes. Com 2 MODs, o arquivo tem 644 bytes (medido: 8 + 2×316 + 4, rodado com `formato_gravar`).

**Leitura:** só aceita o arquivo se o magic, a versão e o CRC batem, se o tamanho é exato (sem
byte sobrando nem faltando), e se a rede passa em `rede_validar`. Qualquer falha devolve erro, e a
configuração não deve ser usada.

**Verificado:** bancada do formato com 11 verificações, incluindo o vetor padrão do CRC32
(`123456789` → `cbf43926`), ida e volta byte a byte, e comportamento igual antes e depois de
gravar e ler. 5 sabotagens, todas reprovaram pelo motivo certo: domínio não gravado, CRC não
conferido, versão não conferida, validação ignorada e constante de bloco errada.

**Limites:** o formato guarda a configuração, não o estado de execução. A rede carregada começa
parada, como ao ligar. A versão 1 é fixa; quando o formato mudar, a versão sobe e a leitura antiga
é recusada, em vez de ser interpretada errado.

---

## 28. Órfão adotado pela ZETA (fase 3a)

Fonte: manual detalhado da ZETA (`D-Sig_ZETA_Manual.pdf`), seções 3.2, 7, 8, 11.1, 12.1.

**Correção de rumo.** A primeira versão (commit `6cba8c2`) seguia o alvo em nível e desligava na
queda. **Isso contradizia o manual** e foi substituída. Eu tinha tirado "sobe junto e desce junto"
de uma linha resumida da análise do Cronus, sem conferir o texto. Errado: o manual diz que a
adoção é um **evento**, não um estado.

**O que é (manual, seção 7):** um MOD sem gatilho que ligue (nenhum LIGA ou ALTERNA; só FINALIZA,
ou nada). Sozinho, ele é inerte. Quem o liga é a **borda de subida** de um alvo da sua ASSOC.

**Como ficou:**
- `modelo_sem_ligar`: 1 se nenhuma aba com fonte tem acao LIGA ou ALTERNA. BLOCK não conta.
- `modelo_validar_adotado`: valida o MOD. Sem nenhuma aba, valida só o DELTA. Com abas (só
  FINALIZA), vale a validação normal.
- `rede_validar`: um MOD sem gatilho que ligue é válido só com ASSOC não vazia.
- `rede_avaliar`, passo 2 (adoção, manual seção 8): `adota = (assoc & sobe) != 0`. A adoção entra
  no DELTA como borda de LIGA. Depois dela, o MOD obedece ao próprio estado.
- A ASSOC congela o órfão **como qualquer MOD**: suspende e preserva o estado (manual, 12.1:
  TURBO pausa, STATIC permanece, ASSOC preserva). Não há regra especial.
- **DIRECT sem gatilho que ligue é um trinco.** Sobe com a adoção e cai só com a borda de
  FINALIZA. Não segue o nível do alvo (manual, 3.2: "ligado, se a fonte for ---").

**Verificado:** bancada da ZETA, 4 verificações de órfão:
- validação: sem ASSOC, recusado; com ASSOC, aceito;
- DIRECT: ligado na subida (150), silenciado na queda (250), volta na nova subida (310);
- DIRECT com FINALIZA: desligado pelo gatilho (160 e 180), não volta enquanto o alvo ainda
  responde, volta na nova subida (310). É o caso que distingue o trinco do nível;
- STATIC: ligado na subida, silenciado na queda com a trava preservada, ligado na nova subida.

Sabotagens, todas reprovaram pelo motivo certo:
- O1, sem recusa de órfão sem alvo: reprova na validação.
- O2, adoção por nível: reprova no caso de FINALIZA (volta a ligar em 180, com o alvo ainda de pé).
- O3, FINALIZA ignorada no trinco: reprova no caso de FINALIZA (fica ligado em 160).
- O4, sem silêncio pela ASSOC: reprova no DIRECT, no STATIC e na própria bancada da ASSOC.
- O5, sem adoção: reprova em três verificações.
- O6, sem trinco (o DIRECT segue só o pulso): reprova no DIRECT e no FINALIZA.

**Decisões tomadas (3 dúvidas anteriores):**

1. **FINALIZA durante o silêncio da ASSOC: ignorado.** O manual (seção 8.4) põe a ALFA (gatilhos)
   antes da ZETA e do BLOQ, e a ASSOC "sai" antes da máquina. A ALFA continua avançando; a máquina
   do DELTA não é alcançada. Por isso o evento não é guardado. Guardar comando seria fila escondida,
   o defeito que o manual do Cronus documenta (seção 3.1 da análise). **Observação:** para o órfão,
   o resultado é o mesmo com ou sem a regra, porque a próxima subida do alvo liga de novo. A
   diferença aparece só num MOD com fonte própria e ASSOC. **Não medi esse caso geral.**
   Se for preciso, é um cronograma novo na bancada.
2. **MOD inerte é alerta, não recusa.** A rede recusa só o malformado (índice fora da faixa, relação
   consigo, DELTA inválido). O semântico vira alerta em `rede_alertas`: `ALERTA_INERTE`,
   `ALERTA_ALVO_OUTRO_DOMINIO` e `ALERTA_EXCL_BLOQ_MESMO` (manual, seção 11). Um MOD com alerta roda e
   avisa. Verificado: 2 verificações novas, e 4 sabotagens reprovam pelo motivo certo.
   Não implementei "alvo sem saída": `delta_validar` já recusa destino zero, então o alerta nunca
   dispararia numa configuração válida.
3. **Trinco do DIRECT sem fonte.** A leitura que sustento: o manual diz que o MOD "de pé" é o que
   está **ligado** (seção 3.2), e que o órfão, depois de ligado, "obedece ao próprio estado" e só
   volta quando o alvo sobe de novo (seção 7.3). Logo, o estado é um trinco: sobe com a adoção, cai
   com FINALIZA. A regra da análise do Cronus, "DIRECT sem fonte encerra ao soltar o botão", vale para
   um gatilho que é botão. Na ALFA v4 o gatilho é fonte, então esse caso não se aplica ao órfão.
   **Ponto em aberto:** a seção 7.4 diz que o DIRECT órfão recebe a **intensidade** do alvo. Com
   booleanos, isso não aparece. Minha leitura: o trinco decide liga/desliga, e a intensidade, quando
   existir, vem do alvo. Ainda não é confirmado pelo manual.

**Pendências:**
- **Intensidade (seção 7.4).** Depende dos eixos com intensidade (destinos 10 a 13), que ainda são
  booleanos.
- **Cadeia de heranças (seção 7.5).** Até 4 níveis. Depende da intensidade.
- **Órfão com gatilho em FINALIZA, DIRECT (seção 7.6).** O valor-base vem do botão físico do gatilho.
  Depende da intensidade.

---

## 29. Eixos com intensidade e a herança do órfão (fase 3a)

Fontes: manual da ZETA, seções 7.4 a 7.6 e 8.4; REFERENCIA_CRONUS seção 3.3 (acumulação por
intensidade máxima, manual do Cronus 26.1). **Leitura minha onde o manual não diz; marcada abaixo.**

**Intensidade da fonte (ALFA, `intens[]`), 0..100:**
- botão e toque: 100 quando há nível;
- eixo: o valor depois do MULT (`beta_valor`), em módulo, **só na zona**;
- gatilho: o valor depois do MULT, **só na zona**.

**Intensidade com que o MOD responde (DELTA, `intensidade`):**
- DIRECT: segue a intensidade do quadro. *(leitura minha)*
- STATIC: botão trava em 100; eixo acompanha enquanto está na zona e segura o último valor fora dela.
  *(leitura minha: o manual diz que o eixo "segue a zona", sem dizer o que acontece fora dela)*
- TURBO e COMBO: a intensidade do início, travada até o fim. Manual, 7.4: "o pulso digital sai com a
  intensidade do alvo". *(a parte "do início" é leitura minha)*

**Destinos de eixo (10 a 13 = LX, LY, RX, RY):**
- um MOD que escreve um eixo põe nele a sua intensidade, com sinal **positivo**. *(o sinal não está
  definido no manual que tenho; pendência)*
- acumulação: o **maior** valor vence entre os MODs que escrevem o mesmo eixo (manual 26.1);
- se ninguém escreve, o eixo passa cru;
- se um MOD suprime o eixo e ninguém o escreve, o eixo sai zerado (destino vence supressão, como nos
  botões).

**Órfão e a cadeia (manual 7.4 a 7.6):**
- o órfão responde com a intensidade do alvo: o primeiro alvo da ASSOC que tem fonte na zona;
- a busca sobe até **4 níveis**; um alvo mais longe não entra;
- sem fonte na cadeia, o valor é zero, e o órfão **se encerra** (manual, 7.6: "valor zero aciona o
  autoencerramento");
- a cadeia usa o **quadro anterior** (`vale`, `prof`), como o resto da ZETA. Cada salto leva um quadro.
  **Desvio do manual:** o manual sugere busca instantânea. Com quadros de 10 ms, um órfão no nível 4
  responde 4 quadros (40 ms) depois da fonte. Não medi isso no console.
- a fonte de um órfão é a cadeia: a intensidade própria de um órfão é zero (não entra na cadeia como fonte).

**Verificado (bancada, sem console):**
- ALFA: intensidade pelo módulo na zona; botão vale 100; o maior vence (70 → 70; -80 → 80; 30 fora da
  zona → 0; 60 com botão → 100). 2 sabotagens (eixo fixo em 100; o primeiro vence).
- DELTA: DIRECT segue (60, 30, 0); STATIC trava (100, 100 solto, 40 na zona, 40 fora dela); TURBO
  captura o início (50, e continua 50 com a fonte em 90). 3 sabotagens.
- Rede: eixos (70 contra 40 → 70; só 40 → 40; sem escrita → 25 cru). Cadeia: órfão 4 com 60 e
  ligado; órfão 5 com 0 e desligado; fonte fora da zona → órfão se encerra. 4 sabotagens: primeiro
  vence, sem limite de nível, sem autoencerramento, eixo sem passar o cru.

**Não feito:**
- o sinal dos eixos (positivo sempre, por enquanto);
- o órfão com gatilho em FINALIZA e DIRECT (manual 7.6, o valor-base vem do botão físico do gatilho);
- o MULT é aplicado à intensidade, mas as verificações não variam o MULT (um caso a mais se for preciso);
- a busca instantânea da cadeia (ver o desvio acima).

---

## 30. Toque oculto: a aba oculta com dedo suprime o toque do jogo (fase 3a)

**O que é:** uma aba OCULTO com `FONTE_DEDO` faz o toque do touchpad sair suprimido do jogo. É o
equivalente do `Util_Touchpad` do Cronus. Pendência desde a seção 26; a ALFA dizia "ainda não é
suportado nesta fatia".

**Como ficou:** um sinal `ocultar_toque` (0 ou 1) sobe pela cadeia: a ALFA marca a aba oculta
com dedo; o MOD copia o sinal; o MOD travado por domínio também (`modelo_so_ocultar`); a rede
faz o OU de todos os MODs que estão de pé no domínio. Como nos botões e eixos, um MOD fora do
domínio atual não suprime nada.

**Verificado:** 3 verificações, uma em cada ponto da cadeia: ALFA (aba oculta com dedo, e sem
ocultar, não); rede (um MOD suprime o toque na saída, e sem ocultar, passa); MOD travado
(só a aba oculta suprime). Sabotagens T1 a T3, todas reprovaram pelo motivo certo.

**Limite:** o sinal sai da rede, mas quem apaga o toque do pacote que vai ao jogo é a ponte
com a `libScePad`, que só existe no console. Aqui, o que se prova é que o sinal chega à saída
certa, não que o jogo deixa de ver o toque.

---

## 31. Revisão contra o manual de EIXOS (fase 3a)

Fonte: `D-Sig_EIXOS_Manual.pdf`, manual detalhado do D-Sig 1.0.17 (25 páginas). Esta seção
diz o que o código faz hoje contra o manual, e o que ficou para depois.

**Divergências que eram erros meus, corrigidas nesta passada:**

| tema | antes | manual | agora |
|---|---|---|---|
| STATIC de eixo | segurava a intensidade fora da zona | "trava? não"; responde enquanto a fonte está na zona (8.1) | não trava: fora da zona fica calado |
| valor do STATIC de eixo | a intensidade da fonte | 100 × MULT, com o lado atual (8.1, 8.4) | teto 100 × MULT, com sinal |
| DIR no touch | valia para TOUCH_X/Y | DIR só para os quatro analógicos e os sensores (2.1, 5.3) | não vale no touch |
| sinal no acumulador | sempre positivo | o valor guardado mantém o sinal; vence o de maior módulo (6.1, 10.1) | com sinal; empate em módulo fica o primeiro |

**Confere com o manual (sem mudança):** THR_IN/THR_OUT em módulo (4.1); DIR ABS inclui zero (4.2);
MULT aplicado com sinal e limitado a ±100 (4.3); PASSIVO zera o eixo para o jogo (4.4); destino de
eixo vence a supressão (4.4); botão em eixo vale 0 ou 100 (7.2); eixo para digital é o módulo (11.1);
DIRECT fora da zona fica calado (14.2); destinos 10 a 13 são LX, LY, RX, RY (2.2); os direcionais
são 14 a 17 (2.2); a intensidade do órfão vem do alvo (ZETA 7.4).

**Pendências (o manual define, o código ainda não faz):**
1. **TURBO em eixo escreve no silêncio.** O zero da latência e a fase de silêncio fazem parte da
   resposta (9.1). O código hoje não escreve no silêncio. Afeta a "máscara de eixo tocado" (6.2) e o
   que vaza do analógico cru entre os pulsos.
2. **Modos do TURBO em eixo: POS, NEG, ABS, ABSF** (9.2). Hoje só existe a intensidade positiva.
   ABS alterna entre +100 e −100 e não repousa, exceto ao terminar os ciclos (9.2, "exceção obrigatória").
3. **Digital recebe valor (6.1 e 11.1).** Eixo para digital entrega o valor absoluto (ex.: CROSS
   recebe 80). Os destinos digitais hoje são bits, sem valor. É a mudança mais larga desta lista.
4. **Gatilho STATIC trava, com teto que destrava** (8.3). Dentro de THR_IN trava; além de THR_OUT
   destrava; volta ao repouso sem destravar. Hoje o gatilho trava como botão, sem o teto.
5. **Órfão e sinal.** O manual dá o valor do alvo, mas não diz o sinal do órfão. Hoje o órfão sai
   sempre com +1. É uma leitura minha, a confirmar.

**Pontos que o manual não resolve:**
- o que acontece com FINALIZA num STATIC de eixo. Hoje o caso puro (só LIGA, só eixo, sem DELAY) é
  tratado como DIRECT, e qualquer outro caso fica como estava. O manual não diz.
- o sinal de DOWN/UP no console: a convenção "frente é negativo" (3.1) depende do valor que o
  ScePadData entrega. Só se confirma no console.

**Verificado nesta passada:** ALFA (sinal e DIR no touch), DELTA (sinal no DIRECT), MOD (STATIC de
eixo: não trava, 100 × MULT com o lado), rede (acumulador com sinal, com empate). 5 sabotagens,
todas reprovaram pelo motivo certo.

---

## 32. Achados do simulador do Cronus (D-Sig 1.0.17): o que vale para o PS5

Fonte: `D-Sig_Achados_1.0.17.pdf`, cinco achados medidos no simulador do próprio projeto do
Cronus (script GPC publicado, versão 1.0.17). Os números são saída literal de execução do
documento. **Não medi nada novo no PS5.** O que se aplica é o que o desenho do PS5 precisa.

**Achado 1: FINALIZA é inerte num MOD que tem ALFA. Decisão em aberto, não aplicada.**
No Cronus, um MOD com fonte e só gatilhos em FINALIZA nunca entra no estado "ligado", e o
FINALIZA não tira nada, porque a ação de finalizar executa o oposto. O documento propõe que
FINALIZA *conte* para decidir se o MOD obedece ao próprio estado.
- No PS5 o caso é outro: um MOD com fonte e só FINALIZA **não é órfão por padrão**; `rede_validar`
  o recusa sem ASSOC (`modelo_sem_ligar`). Isso é uma escolha minha, não a do Cronus.
- Não implementado. Três caminhos: (a) manter a recusa; (b) nascer ligado e FINALIZA desligar
  (a regra "com fonte, o MOD nasce ligado" do manual da ZETA, seção 16); (c) a proposta do documento.
  Preciso de uma decisão sua.

**Achado 2: o sentinela −1 colide, e a temporária é destruída. Aplicado no PS5.**
O órfão adotado tinha o mesmo tipo de problema: o valor de "não achei" era −1, que também é um
valor de saída legítimo. No PS5 o sentinela não existe: a herança devolve zero (sem fonte, e zero não
é intensidade válida) e o sinal vem num campo separado (`lado`). O que o documento mede sobre o
órfão, e que o desenho agora segue:

| tipo do órfão | sinal | intensidade herdada? |
|---|---|---|
| DIRECT → eixo | preservado | sim |
| STATIC | +, sempre | não: 100 fixo |
| TURBO → eixo | +, sempre | não: 100 fixo |
| TURBO → digital | descartado (módulo) | sim |

O caso do TURBO → eixo é o que o documento chama de "nunca vai ao negativo": a fonte do órfão é
zero, então o modo ABSF decide o polo, e com `raw` sempre zero a condição nunca é verdadeira.
No PS5 não há modo ABSF ainda (ver seção 31, pendência 2); o sinal + é o que o desenho escreve.

**Achado 3: THR_IN = 0 prende o MOD no portão. Aplicado no PS5.**
No Cronus, com THR_IN = 0 a zona ficava ativa em repouso, e o portão de entrada nunca liberava.
O mesmo defeito existia no nosso código: com THR_IN = 0, uma fonte de eixo parada estava "dentro da
zona" (verificado: `ativa=1` com o analógico em zero). Um DIRECT escreveria zero no eixo parado e
apagaria o movimento do jogador. **Correção:** sem movimento (módulo zero) a fonte fica fora da
zona; qualquer movimento conta. O caso invertido (THR_IN > THR_OUT), que o manual manda contar em
repouso, continua como está.
- **Nota sobre o documento:** o trecho proposto no achado, `if (Pipe_Thr > 0 && Work_N < Pipe_Thr)
  return 0;`, sozinho devolve 1 em repouso com THR_IN = 0. A intenção ("zona inativa em repouso") só
  se realiza com o restante da função, que não está no PDF. A correção aplicada é a minha, feita a
  partir da intenção do achado, e está verificada na bancada.

**Achado 4: o OLED esconde THR_IN e THR_OUT das fontes de touch. Não se aplica ao PS5 por ora.**
É um defeito da interface de configuração do Cronus. O PS5 ainda não tem interface. A lição fica
para quando houver: a tela de configuração tem de mostrar os limiares de todas as fontes que o motor
aplica, incluindo o touch.

**Achado 5: faixa errada no aviso de STATIC em eixo. Não se aplica ao PS5 por ora.**
Erro de índice no aviso do app do Cronus (`index.html`): a faixa 12..15 cobria UP como se fosse eixo.
Não afeta o script nem a EEPROM. A lição fica para a interface futura: avisos que dependem de faixas
de índice precisam de teste próprio, como qualquer outra verificação.

**Verificado nesta passada:** THR_IN = 0 (ALFA, com verificação e sabotagens T4 e T5); órfão DIRECT
com sinal, órfão STATIC em 100, órfão TURBO em eixo em 100 (rede, com sabotagens U1 a U3).
**Não verificado:** o COMBO órfão com destinos mistos (a regra usa o primeiro destino); o achado 1,
que é decisão.

---

## 33. Decisão: FINALIZA (achado 1 do simulador do Cronus)

**Decidido.** Duas situações, decididas de forma diferente porque são coisas diferentes.

**1. MOD com fonte e FINALIZA (o caso do achado): FINALIZA desliga; a fonte religa.**
É a regra que o próprio projeto já tinha escrito (achado 1, citando a regra derivada do Cronus):
"com fonte, o gatilho só DESLIGA (a fonte religa)". O Cronus não a cumpria: o MOD ficava
"sempre ativo". No PS5:
- STATIC e TURBO já cumpriam a regra.
- **DIRECT não cumpria** (ignorava FINALIZA, e portanto era inerte com esse gatilho). Corrigido
  nesta passada: FINALIZA liga a flag `finalizado`; a flag só cai na próxima subida da fonte.
- Verificado: DIRECT (desliga; fonte segue sem religar; solta; sobe de novo, religa) e STATIC
  (desliga; fonte segura não religa). Sabotagens V1 e V2 reprovaram pelo motivo certo.

**2. MOD sem nenhuma fonte que liga, só com FINALIZA: fica inerte e avisa. Não nasce ligado.**
Recusei a proposta do documento ("nasce ligado; FINALIZA derruba"). Motivo: um MOD que nasce ligado
sem nenhuma fonte que o ligue depois é exatamente o "sempre ativo" que o achado mediu como defeito.
Se ninguém pode ligá-lo, ele não deve estar ligado. O comportamento é o do alerta `ALERTA_INERTE`
(seção 31, rede): roda, nunca liga, e avisa. Não é recusa (a rede recusa só o malformado).
O órfão segue a mesma regra: só a subida de um alvo da ASSOC o liga (ZETA, 7.2).

**Consequência:** nenhuma configuração que hoje liga passa a ficar ligada sem ordem. A única mudança de
comportamento é o DIRECT com FINALIZA, que antes era inerte e agora desliga como o manual manda.

---

## 34. TURBO em eixo: modos e escrita no silêncio (manual do EIXOS, 9.1 e 9.2)

**O que mudou.** O TURBO com destino de eixo (10 a 13) agora escreve também no silêncio, e tem um
modo que define o sinal de cada fase. Os destinos digitais não mudam.

| modo | fase ligada | fase de silêncio |
|---|---|---|
| ABS | +100 | −100 |
| POS | +100 | 0 |
| NEG | −100 | 0 |
| ABSF | +100 ou −100, conforme o sinal atual da fonte | 0 |

Por que escreve no silêncio: o zero da latência faz parte da resposta. Se o TURBO ficasse calado ali,
o analógico cru do jogador vazaria entre os pulsos (manual, 9.1).

**Formato.** O DELTA ganhou o campo `modo`. Isso muda o formato binário: `FORMATO_VERSAO` passou de 1
para 2, e o bloco por MOD de 302 para 303 bytes. A bancada do formato foi ajustada: a verificação de
"versão diferente" passou a usar `FORMATO_VERSAO + 1`, porque a constante antiga (2) virou a versão
atual. Ela não é uma mudança de comportamento do formato, só da constante do teste.

**Verificado:** TURBO em eixo com os quatro modos, medidos na fase ligada (t = 50 ms) e no silêncio
(t = 120 ms), com fonte em +1 e −1 para o ABSF. Três sabotagens reprovaram pelo motivo certo: ABS sem
o −100 no silêncio, NEG com o sinal trocado, e silêncio que não escreve.

**Limites e decisões em aberto:**
1. **Fim dos ciclos.** O manual diz que, quando os ciclos terminam, todos os modos repousam em zero "de
   verdade", inclusive o ABS (9.2). Não implementado: não sei se o zero deve ser escrito no quadro do
   término ou continuar enquanto o MOD estiver desligado. Precisa de decisão sua.
2. **Órfão em TURBO com eixo.** O órfão usa o mesmo modo. Com o ABS (o padrão), ele alterna +100 e
   −100. O achado 2 do simulador do Cronus mediu só o ABSF. Os outros modos com órfão não foram medidos.
3. **COMBO com destinos mistos.** A regra de eixo vale pelo destino do passo corrente. Não verificado.
4. **Destino digital com valor** (manual, 6.1 e 11.1): não feito. Ver a seção 31, pendência 3.

---

## 35. Decisões: zero após os ciclos, órfão em TURBO com eixo, COMBO misto, gatilho com valor

**1. Zero depois dos ciclos (manual do EIXOS, 9.2): decidido enquanto o MOD estiver desligado.**
Quando os ciclos de um TURBO ou COMBO terminam, os destinos de eixo passam a escrever zero, e
continuam escrevendo enquanto o MOD estiver desligado. Motivo: sem isso, o eixo voltaria ao valor
cru do jogador logo após o fim, e o manual diz que o zero é "de verdade". Um MOD que nunca iniciou
continua em silêncio. Sabotagem X1 reprovou pelo motivo certo.

**2. Órfão em TURBO com eixo: mantém o modo configurado.**
Decisão minha. O órfão não muda o modo que o autor escolheu. Com ABSF, o sinal é +, porque a fonte
do órfão é zero e o sinal dela é +1: isso confere com o achado 2, que mediu exatamente esse caso.
Com ABS, o órfão alterna +100 e −100, e com POS ou NEG segue o modo. Os modos não medidos no
simulador do Cronus ficam como estão. Não há base para mudar.

**3. COMBO com destinos mistos: cada passo decide pelo próprio destino.**
- um passo de eixo escreve no eixo, e um passo digital escreve só no digital;
- a latência entre passos pertence ao passo que acabou de terminar (o `passo` não muda durante a
  latência), então ela escreve no eixo desse passo, se for de eixo;
- o modo é um só para o COMBO inteiro (`DeltaCfg.modo`); não há modo por passo;
- ao terminar os ciclos, todos os eixos do COMBO passam a escrever zero (decisão 1).
Não foi verificado com destinos mistos. A regra de cada passo é a mesma do TURBO, que foi verificada.

**4. Destinos de gatilho com valor (L2 e R2): sim, só para gatilhos e eixos.**
- L2 (destino 1) e R2 (destino 4) recebem o valor absoluto da intensidade (manual, 11.3).
- Sem escrita, o gatilho passa cru; a supressão de um gatilho o zera.
- O bit digital de L2 ou R2 segue o valor: pressionado se o valor for maior que zero. Isso é porque
  o ScePadData tem o bit e o campo analógico, e os dois precisam concordar.
- Botões (L1, CROSS e os outros) continuam booleanos. Não recebem valor.
- O valor fica em 0..100 internamente. A conversão para os 0..255 do ScePadData é da ponte no console.
  Não verificado no console.

**Verificado:** zero após o fim dos ciclos (DELTA, com sabotagem X1); gatilho como destino com valor e
passa-cru (rede, com sabotagens X2 e X3).

---

## 36. STATIC com gatilho: trava, teto e memória (manual do EIXOS, 8.3)

**O que faz.** Um STATIC cuja fonte é só um gatilho (L2 ou R2) vira chave com dois comandos, sem
depender de tempo: pressão leve liga, pressão forte desliga.

| leitura do gatilho | efeito |
|---|---|
| entra na zona (≥ THR_IN e ≤ THR_OUT) | trava (liga), se não estiver "destravado" |
| passa do THR_OUT | destrava (desliga), e marca "já destravou pelo teto" |
| desce pela zona, sem chegar ao repouso | não religa: a memória impede |
| volta ao repouso (< THR_IN) | rearma: pode travar de novo |

O valor é 100 × MULT, com sinal sempre +.

**Por que a memória.** Sem ela, soltar o gatilho faria a fonte atravessar a zona de volta e
retravar na hora, desfazendo o destrave no mesmo movimento (manual, 8.3).

**Como fica.** A ALFA expõe a leitura bruta do gatilho mais forte, dentro e fora da zona, com os
limiares e o teto (`bruto`, `bruto_in`, `bruto_out`, `bruto_teto`). A zona da ALFA não serve sozinha,
porque não distingue "acima do teto" de "repouso". O MOD mantém duas flags (`gz_ant`,
`gz_destravado`) e traduz a máquina em pulsos de LIGA e FINALIZA para o DELTA. O DELTA continua
sendo o STATIC de sempre.

**Decisões de leitura:**
- A máquina roda mesmo com o MOD congelado (BLOQ ou ASSOC). O gatilho é um estado físico, e a memória
  precisa acompanhá-lo. Por isso o que acontece durante o congelamento é diferente do que acontece com
  eventos de FINALIZA (seção 31): aqui a leitura não é um evento guardado, é o estado do gatilho.
- Só se aplica ao caso puro: STATIC, LIVRE, só gatilhos, só LIGA, nível (sem DELAY), sem outras abas.
  Qualquer outra combinação com gatilho fica como estava.

**Verificado:** sequência 0, 30, 40, 70, 40, 10, 30 com THR_IN 20 e THR_OUT 60, esperada como
off, on, on, off, off, off, on. Três sabotagens reprovaram pelo motivo certo: sem destrave, sem memória
(a descida religa no quadro 40), sem rearme (não volta a travar depois do repouso).

---

## 37. Fechamento das duas pendências de bancada: COMBO misto e órfão com gatilho em FINALIZA

**COMBO com destinos mistos: verificado.** Um COMBO com CROSS (digital), RX (eixo) e CIRCLE (digital),
medido no cronograma de 10 ms: a fase de CROSS escreve só CROSS; a latência do passo digital não
escreve nada; a fase ligada de RX escreve +100; a latência de RX escreve −100 (modo ABS), porque a
latência pertence ao passo que acabou de terminar; o passo de CIRCLE não toca no eixo. Duas sabotagens
reprovaram pelo motivo certo: o passo usado para escrever passa a ser sempre o primeiro (Z1), e a
latência de eixo deixa de escrever o silêncio (Z2).

**Órfão DIRECT com gatilho em FINALIZA (manual da ZETA, 7.6): sem efeito na saída. Não codificado.**
O manual diz, em quatro passos: (1) o valor-base vem do botão físico do gatilho, normalmente zero;
(2) a herança do alvo sobrescreve esse zero; (3) sem fonte na cadeia, o valor fica zero; (4) zero aciona
o autoencerramento. Lidos em conjunto, os passos 2 e 3 deixam o valor-base sem efeito:
- com alvo com fonte na cadeia, a herança sobrescreve o valor-base, então ele não aparece;
- sem alvo com fonte, o resultado é zero e o órfão se encerra (passo 4), que é o que o código já faz
  (cadeia de herança, seção 29).
Além disso, o gatilho do valor-base é o mesmo botão do FINALIZA. Apertá-lo dispara o FINALIZA e
desliga o órfão, então o valor-base só existiria enquanto o órfão já está desligado.
Decisão: o código atual já produz o resultado do manual (herança ou zero, com autoencerramento). O
valor-base não é implementado porque não altera nenhuma saída. Se o manual vier a mostrar um caso em
que o valor-base aparece, reabro esta decisão.

---

## 38. Medição no PS5 por payload independente: a abertura do controle não funciona (fase 3a, console)

**Pergunta:** o payload de medição, rodando sozinho no PS5 FAT 11.20, consegue abrir o controle e
ler os dados? Era o caminho para fechar as pendências de console (convenção vertical, toque, sensores,
gatilhos, PS e SHARE).

**Resultado medido** (CSV gravado no HD, `usb0/dsig/`, com o controle conectado por Bluetooth):

| execução | chamadas | retorno |
|---|---|---|
| 1 | `scePadOpen(0, 0, 0, NULL)`, sem `scePadInit` | `-2137915387` (0x80920005) |
| 2 | `scePadInit`, depois `scePadOpen(0..3, NULL)` | **travou em `scePadInit`** (três vezes; o CSV para em "antes de scePadInit") |
| 3 | `scePadOpen(1..3, NULL)`, sem `scePadInit` | `-2137915387` em todos |
| 4 | `scePadOpen(0..3, buffer zerado)` e `scePadOpenExt(0..3, buffer zerado)`, sem `scePadInit` | `-2137915387` em todos os oito |

Em todas as execuções que completaram, todas as leituras de `scePadReadState` também devolveram
`-2137915387`. Nenhuma linha de dados do controle foi gravada.

**O que fica estabelecido:**
- `scePadInit` trava quando chamada pelo payload neste console. Não é intermitente: aconteceu em todas
  as tentativas.
- Sem `scePadInit`, nenhuma forma de abertura testada funciona. O valor do userId e o `param` (NULL ou
  buffer zerado) não mudam o resultado.
- Os nomes `scePadOpen`, `scePadOpenExt`, `scePadInit` e `scePadReadState` existem no SDK (verificado
  com `nm -D`). A assinatura não foi confirmada: o SDK não traz cabeçalho, e o `.so` é um stub de link
  (todos os símbolos no mesmo endereço).

**O que não está estabelecido:**
- A causa do travamento de `scePadInit`. A hipótese mais provável é que a biblioteca já está inicializada
  pelo sistema, e a segunda chamada espera uma trava que não libera. Não há como confirmar daqui.
- Se a assinatura de `scePadOpen` é a usada. A única evidência é o retorno, que não diz se os argumentos
  foram interpretados como se espera.

**Decisão de desenho.** O D-Sig não deve abrir o controle por conta própria. O desenho do Alvo 3 já
previa isso: o daemon lê o estado que o jogo recebe, interceptando `scePadReadState` dentro do processo
do jogo, que já tem o controle aberto (seção 7 do CLAUDE.md e docs/ESTUDO.md). Um payload independente
não mede o que o D-Sig precisa medir. Por isso o teste desta seção não reprova o desenho. Ele mostra
que medir por um payload separado, neste console, não é o caminho.

**Consequências:**
- As pendências de console que dependem de leitura real (convenção vertical, faixa do touch, escala de
  ACC e GYRO, zona morta, conversão dos gatilhos, bits de PS e SHARE) continuam abertas. O caminho para
  fechá-las passa pela ponte dentro do processo do jogo, na fase 3b.
- O código de sondagem em `payload/medicao/main.c` fica como registro. Ele não é usado pelo D-Sig.
- Antes de qualquer nova tentativa de inicialização, é preciso estudar como o PoorDS4 e o etaHEN lidam
  com a biblioteca (docs/ESTUDO.md). Nada foi copiado.

**Addendum (sondagens 5 a 7, mesmo dia).** A `libSceMbus` carregada com `dlopen` antes das chamadas
resolveu o travamento: `scePadInit` e `scePadSetProcessPrivilege(1)` passaram a retornar 0. Pela
primeira vez, `scePadOpen` respondeu com um código que o próprio Ghostcontrol identifica (USER_NOT_LOGIN,
`0x809B0081`), e não com "não inicializada". Medido:

- `sceUserServiceGetInitialUser` falha (`-2137784316`), mas `sceUserServiceGetLoginUserIdList` e
  `sceUserServiceGetForegroundUser` devolvem o mesmo usuário, `513995998`.
- `scePadGetHandle(513995998, 0, 0)` devolve `0x80920008`, e `scePadOpen(513995998, 0, 0, NULL)` devolve
  `0x809B0081`, nas seis tentativas, com cinco segundos entre elas.

Conclusão: a lista de logados e o que a biblioteca de controle aceita não concordam para um processo de
medição. Não é uma questão de tempo. A hipótese mais forte, ainda não confirmada, é que o controle fica
vinculado ao usuário do processo que o abriu, e um payload independente não é esse processo. Obter esse
vínculo por outro caminho esbarra em elevação de privilégio do processo, fora do escopo do CLAUDE.md.

Decisão: a linha de sondagem com payload independente se encerra aqui. O caminho que sobra é o da fase 3b,
dentro do processo do jogo, com o estudo do PoorDS4 (que lê o controle a partir de um gancho no jogo) antes
de qualquer código.

---

## 39. Exceção do gancho de entrada: o que é permitido na memória do jogo

**Problema.** A seção 2 do CLAUDE.md dizia "leitura ou escrita na memória do jogo" como fora do projeto,
e o desenho do Alvo 3 (seções 2 e 7 deste documento, e o CLAUDE.md, seção 7) prevê justamente o
redirecionamento da entrada PLT do jogo. Os dois textos não batiam. Ao estudar o PoorDS4 (docs/ESTUDO.md,
seção 11), ficou claro que o método dele escreve na memória de outro processo a partir do kernel, o que
vai além do que o desenho precisava.

**Decisão do autor.** A proibição da seção 2 tem como alvo a alteração de estado do jogo para dar vantagem.
Um gancho que troca somente a entrada de controle (turbo, macro, sequência, acorde) muda o sinal que chega
ao jogo, e não o cálculo do jogo. Esse é o núcleo de acessibilidade do projeto. Então a exceção é:

- **permitido:** redirecionar somente as funções de leitura do controle, dentro do próprio processo do jogo,
  com encadeamento para o original;
- **proibido:** ler ou escrever o resto da memória do jogo, alterar estado do jogo, escrever em outro
  processo a partir do kernel, elevar privilégio além do próprio processo, e qualquer vantagem sobre outra
  pessoa.

**O que não está decidido.** O método de instalação do gancho. O PoorDS4 instala de fora do processo, e isso
fica fora da exceção. A alternativa a estudar é o caminho que o autoloader já usa para carregar código no
processo do jogo, sem escrita externa. Esse estudo ainda não foi feito.

**Consequência para o código.** Nada de escrita externa entra no D-Sig. A bancada e o payload de medição
não mudam com esta decisão.

---

## 40. Canal do perfil até o gancho, sem escrita na memória do jogo (proposta)

**Problema.** As seções 2 e 7 deste documento e a tabela de decisões (linhas 20 e 161) previam que o daemon
escreve o perfil na memória do processo do jogo. A seção 39 proíbe isso: escrita de fora do processo fica
fora da exceção. O perfil precisa chegar ao gancho por outro meio.

**Alternativas comparadas.**

| | A. Região compartilhada criada pelo daemon | B. Instantâneo na instalação |
|---|---|---|
| Como funciona | O daemon cria uma região. O gancho a mapeia no próprio processo, na instalação. Mudanças são publicadas nela com buffer duplo e versão, como `src/perfil.c` já faz. | O perfil é copiado uma vez para uma área que o gancho aloca. Não há canal depois. |
| Quem escreve no processo do jogo | Ninguém de fora. O daemon escreve só na sua região; o gancho lê a dele. | A cópia inicial. Se quem instala está fora do processo, é a escrita que a seção 39 proíbe. |
| Perfil muda com o jogo aberto | Sim, sem reinstalar. | Não: mudar exige reinstalar o gancho. |
| Custo no quadro | Uma leitura do buffer duplo. Sem chamada de sistema. | Nenhum. |
| Risco | As permissões do mapeamento no processo do jogo só se confirmam no console. | O mais simples de auditar. |
| Depende de decisão aberta | Sim: a instalação precisa ser dentro do processo, para o mapeamento acontecer lá. | Sim: a cópia inicial precisa vir de dentro do processo. |

**Proposta: A**, porque é a única que permite trocar um turbo ou uma macro durante a partida, que é o uso
real de acessibilidade. Ela também cumpre a seção 39: o daemon nunca escreve no processo do jogo.

**Reserva: B**, com a cópia inicial lida de um arquivo pelo próprio gancho, na instalação. Assim B também
fica dentro da exceção, sem escrita de fora.

**Regras para as duas.** Na seção 6 do CLAUDE.md, o caminho do quadro não aloca, não bloqueia e não faz
chamada de sistema. O mapeamento e a leitura do arquivo acontecem uma vez, na instalação, que é o único
momento em que chamada de sistema é aceita.

**O que fica pendente.** O método de instalação do gancho dentro do processo do jogo. A e B só podem ser
testadas no console depois que ele for definido. Esta seção não decide esse método.

**Consequência para o código.** Nada muda na bancada nem no payload de medição. A decisão afeta a fase 3c,
que depende de console.
