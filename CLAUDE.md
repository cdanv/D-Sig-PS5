# D-Sig-PS5 — contexto do projeto

Leia este arquivo antes de qualquer trabalho neste repositório. Ele diz o que o projeto
é, o que ele não é, como se trabalha nele e onde ele está.

**Idioma: português do Brasil**, em toda conversa, comentário de código e documento.

---

## 1. O que é

O D-Sig é um **roteador de sinais de controle para acessibilidade**. Ele recebe o estado
do controle, aplica condição, gatilho, tempo e relação entre MODs, e entrega o resultado
ao jogo.

Ele existe há um ano e está **terminado e em uso** no Cronus Zen
([cdanv/D-Sig](https://github.com/cdanv/D-Sig), CC BY-NC 4.0). Este repositório é a sua
reescrita como payload para PlayStation 5 com jailbreak, porque o Cronus bateu no teto
do aparelho.

**Para quem:** pessoas que não alcançam dois analógicos ao mesmo tempo, não sustentam um
botão pelo tempo que o jogo exige, ou precisam transformar uma sequência de comandos em
um toque só. É software de acessibilidade, escrito por quem precisa dele.

## 2. O que não é, e os limites do trabalho

Isto não é uma ressalva decorativa. É a definição do escopo, e ela é respeitada.

**O que o D-Sig não contém, e não vai conter:**

- assistência de mira, correção de recuo, ou qualquer ajuda que dê vantagem sobre outra
  pessoa em jogo online
- leitura ou escrita na memória do jogo
- qualquer interação com servidor ou com a rede do jogo

Um PS5 com jailbreak não acessa a PSN, então não existe jogo online para afetar. O
roteamento acontece entre o controle e o jogo, no mesmo lugar onde um controle adaptado
de hardware atuaria.

**O que o D-Sig contém, e é o núcleo do projeto:** turbo, macro, sequência, acorde,
domínios e tudo o que permitir jogar sem depender de ajuda de terceiros. O público inclui
quem não tem movimento dos dedos e sustenta o controle só com as mãos. Esse público
também joga **localmente contra outra pessoa**, com limitação ou não. Nesse caso, o
D-Sig existe para tornar a partida adaptativa e quase equivalente: sem ele, uma pessoa
com limitação severa não conseguiria jogar com quem não tem limitação. Jogo local é
usado com esse propósito, e não é tratado como vantagem competitiva.

A linha do projeto é a **finalidade**: acessibilidade para jogar sem depender de outra
pessoa, ou com ela em condição de igualdade. Não é o mecanismo. Turbo e sequência
existem nos dois casos, e a finalidade é o que decide.

**Jailbreak não é pirataria.** São coisas distintas e este projeto não tem relação com a
segunda. A legalidade de destravar um console varia por país e este repositório não
afirma nada a respeito.

**O que este projeto pede de quem trabalha nele:**

| sim | não |
|---|---|
| o roteador, a bancada, a configuração, a documentação | desenvolver ou melhorar o exploit do console |
| estudar payloads existentes para entender a plataforma | contornar DRM, carregar jogo pirata, fraudar licença |
| a ponte com a `libScePad` | assistência que dê vantagem sobre outra pessoa em jogo online |

Se um pedido sair dessa coluna da esquerda, a resposta correta é dizer isso com clareza
e seguir com o que está dentro — não inventar um jeito de atender.

## 3. Estado atual

| peça | estado |
|---|---|
| **Alvo 1 — passagem pura** | **feito.** `src/passagem.c`, 4 verificações, 4 sabotagens |
| **Alvo 2 — o primeiro MOD de verdade** | **feito na bancada.** Troca de botão e acorde. `src/passagem.c`, `ferramentas/testa_mod.c`, 9 sabotagens |
| Alvo 3 — o vigia residente + a ponte | arquitetura decidida em `docs/ARQUITETURA.md`. Fase 3a (perfil e buffer duplo) é a próxima, sem console. Fases 3b a 3d precisam de console |
| Alvo 4 — ícone na tela inicial | caminho conhecido e barato (ver `docs/ESTUDO.md`) |

```
src/pad.h                     o ScePadData — 120 bytes, confirmado por duas fontes
src/dsig.h                    a interface do roteador. Estreita de propósito
src/passagem.c                o roteador. Copia, e aplica MOD de botão quando carregado
ferramentas/testa_passagem.c  a bancada do Alvo 1 (passagem)
ferramentas/testa_mod.c       a bancada do Alvo 2 (o primeiro MOD)
docs/ESTUDO.md                o que aprendemos lendo PoorDS4, etaHEN, ProsperoMgr, pldmgr
docs/ARQUITETURA.md           decisões de desenho do Alvo 3: processos, perfil, privilégio, celular
```

```bash
make bancada     # compila e roda os testes. Precisa só de gcc
make payload     # reservado para o console. Hoje só avisa que a ponte não existe
```

## 4. Como se trabalha aqui

Estas regras vieram do projeto do Cronus, cada uma paga com um defeito real. Elas valem
aqui inteiras.

**1. Nada afirmado sem rodar.** Número em comentário que ninguém mediu é número errado
esperando a hora. Se não deu para medir, o texto diz que não deu.

**2. Teste que não consegue reprovar não prova nada.** Toda verificação é conferida
contra uma sabotagem deliberada: quebra-se a coisa de propósito e confere-se que o teste
reprova — *e que reprova pelo motivo certo*. Sabotagem que reprova por outro motivo não
provou nada.

**3. A entrada é só leitura.** `dsig_rotear` recebe `const ScePadData *`. No Cronus,
confundir "o que entrou" com "o que vai sair" foi origem de defeitos silenciosos.

**4. Nenhum deslocamento dependente de firmware entra no D-Sig.** Ver seção 5.

**5. O que roda dentro do jogo é código de risco máximo.** Ver seção 6.

**6. Toda mudança estrutural é explicada: como, por quê, e comparada ao que substituiu.**
O autor do projeto está aprendendo a escrever código desta forma; entregar o resultado
sem o raciocínio desperdiça metade do trabalho.

**7. Erro próprio se admite e se corrige na hora**, inclusive erro já publicado. Já
aconteceu mais de uma vez neste projeto e é o comportamento esperado, não a exceção.

## 5. Dois consoles, um binário

Os dois aparelhos de destino:

| | PS5 FAT, com leitor | PS5 Pro, com leitor |
|---|---|---|
| firmware | **11.20** | **13.40** |

O PoorDS4 tem tabela de perfis por firmware (6.02, 8.60, 10.01, 11.60, 12.40) — e
**nenhum dos dois está nela**. Mas essa tabela serve para ler a *tabela interna privada*
da `libScePad`, coisa que o PoorDS4 precisa porque inventa um controle que não existe.

O D-Sig toca só a **ABI pública**: `scePadReadState(int32_t, ScePadData *)`. Os 120 bytes
estão confirmados por duas fontes independentes e são estáveis de 6.02 a 12.40. O endereço
da função vem de resolução por NID em tempo de execução, não de tabela.

**Por isso o mesmo binário deve servir aos dois.** Isso é uma propriedade do desenho, não
uma sorte — e é por isso que a regra 4 existe. Se um recurso exigir deslocamento por
firmware, ele fica de fora ou se resolve por outro caminho.

Ainda não há prova em 13.40. O que se afirma é que nada no desenho deveria quebrar, o que
é diferente de garantir que não quebra.

## 6. Estabilidade — a prioridade declarada

"Estável depois de iniciado" é a exigência número um do projeto. Ela tem dois endereços
com consequências muito diferentes:

| onde o código roda | o que um defeito causa |
|---|---|
| o daemon D-Sig (processo próprio) | o D-Sig para. O jogo não percebe |
| **o gancho, dentro do processo do jogo** | **o jogo fecha** |

O gancho roda a cada quadro, na thread do jogo. No caminho do quadro:

- **sem alocação** — nada de `malloc`/`new`; tudo pré-alocado quando o gancho se instala
- **sem bloqueio** — sem arquivo, sem rede, sem trava que possa esperar
- **sem chamada de sistema**
- **sem recursão e sem laço de limite indefinido**

É a mesma disciplina do GPC no Cronus: espaço fixo, sem memória dinâmica, toda volta
termina. Por isso o Alvo 1 foi o que não faz nada, e por isso a bancada existe antes do
roteador: tudo que vai rodar dentro do jogo tem que ser provável no Linux.

Na bancada, uma das sabotagens (tirar a guarda de ponteiro nulo) produziu **SIGSEGV**. É
exatamente a classe de falha que, dentro do jogo, é tela preta.

## 7. Controle DS4 por Bluetooth — e o conflito que isso cria

Requisito do projeto: funcionar com **DualSense (PS5) e DualShock 4 (PS4)**, por
Bluetooth e por cabo.

O D-Sig **não implementa Bluetooth**, e não deve. Ele intercepta `scePadReadState`, que
fica depois de tudo: qualquer controle que o sistema já entenda chega ali pronto.

- **DualSense** — o PS5 trata nativamente. O D-Sig roteia.
- **DS4** — o PS5 não o reconhece em jogos de PS5. Quem resolve isso é o
  [PoorDS4](https://github.com/ItsBlurf/PoorDS4) (GPL-3.0). O D-Sig roteia o que ele
  entrega.

Os dois se compõem, mas **com um conflito concreto**: o PoorDS4 engancha exatamente as
mesmas funções —

```
scePadReadState   scePadRead   scePadReadStateExt   scePadReadExt   scePadGetDataInternal
```

Dois ganchos no mesmo ponto. O encadeamento tem de ser explícito:

1. o D-Sig **lê o ponteiro atual** da entrada PLT antes de escrever o seu
2. guarda esse valor como "o próximo da corrente"
3. o gancho do D-Sig chama o guardado em vez de chamar a `libScePad` direto
4. instalar duas vezes sem desfazer cria laço ou perde o elo — tem de ser idempotente

Isso ainda não está escrito. É parte do Alvo 3 e precisa de console para ser verificado.
Até lá, é hipótese documentada, não solução.

## 8. Licenças

| projeto | licença | relação |
|---|---|---|
| **D-Sig-PS5** (este) | **GPL-3.0-or-later** | — |
| PoorDS4 | GPL-3.0 | origem do layout do `ScePadData`; atribuído em `src/pad.h` |
| etaHEN, ProsperoMgr, PS5 Payload Manager | GPL-3.0 | estudados; nada copiado até aqui |
| **D-Sig do Cronus** | **CC BY-NC 4.0** | repositório separado |

**Regra de mão única:** código GPL pode vir para cá; **nada daqui volta para o
repositório do Cronus**, que é CC BY-NC. O autor é o mesmo pessoa nos dois, mas as
licenças não se misturam.

Toda origem externa leva atribuição no cabeçalho do arquivo que a usa.

## 9. Onde o D-Sig vai morar no console

Não precisa de infraestrutura própria. A corrente já existe:

```
liga o console
  → autoloader (Y2JB / BD-JB / Lua) dispara o exploit
    → carrega o gerenciador (PS5 Payload Manager ou ProsperoMgr)
      → o gerenciador lê sua lista de autoload
        → lança o D-Sig
```

O jailbreak do PS5 **não é persistente** e nenhum payload muda isso — o exploit roda a
cada vez que o console liga. Isso é entendido e aceito; não é problema a resolver.

Para o D-Sig entrar nessa corrente bastam três coisas, e só a segunda falta:

| requisito | estado |
|---|---|
| ser um `.elf` que o elfldr aceita | o alvo `payload` do Makefile produz isso |
| **não terminar — ficar residente** | **falta.** É o Alvo 3 |
| ícone próprio (opcional) | caminho conhecido, ~180 linhas (ver `docs/ESTUDO.md`) |

## 10. Interlocução

O autor é **Daniel** (`cdanv`), de Patos de Minas, MG. Ele escreveu o D-Sig do Cronus
inteiro e conhece o comportamento do projeto de cor, mas está aprendendo C e a disciplina
de engenharia junto com o trabalho — e pediu explicitamente que fosse assim.

O que ele espera:

- **português do Brasil**, direto, sem rodeio
- **listas e tabelas comparativas** para assunto técnico
- **avaliação honesta com o raciocínio à mostra** — inclusive discordar quando for o caso
- **nada afirmado sem medir**, e dizer com todas as letras quando não deu para medir
- quando ele trouxer conhecimento próprio sobre o PS5 ou sobre o D-Sig, **levar a sério
  antes de corrigir** — ele já esteve certo e o assistente errado mais de uma vez
