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
