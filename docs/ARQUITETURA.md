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

## 10. Modelo de MOD — vigente

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
