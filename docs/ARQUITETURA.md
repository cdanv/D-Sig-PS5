# Arquitetura do D-Sig-PS5

Este documento fixa as decisões de desenho antes do Alvo 3. Cada decisão tem um
estado: **decidido** (vale para o código), **proposto** (recomendação minha, ainda não
aceita) ou **a medir** (depende de execução no console, e nada aqui foi executado lá).

---

## 1. Decisões

| tema | decisão | estado |
|---|---|---|
| Escopo | Turbo, macro, sequência, acorde e domínios são o núcleo. Jogo local entra como acessibilidade. Vantagem online fica fora (`CLAUDE.md`, seção 2) | decidido |
| Perfil por jogo | Um perfil por título. O título em execução escolhe o perfil ativo | decidido |
| Identificar o jogo | Pelo identificador de título do processo do jogo | a medir |
| Salvamento | Arquivo por título, com slots, em caminho próprio. Não usa o savedata oficial | decidido |
| Gravação | Atômica (temporário e depois renomeia). Nunca dentro do quadro | decidido |
| Privilégio | Começa sem escalonar. Herda o acesso que o carregador entrega. Eleva só o próprio processo, e só se a rede falhar | proposto |
| Ícone | Abre a configuração. Não inicia o D-Sig sozinho | proposto |
| Início do residente | O autoloader do PS5 Payload Manager ou do ProsperoMgr sobe o D-Sig, a pedido do usuário ou no boot | proposto |
| Celular | PWA na rede local, conectando por IP do console, com código de pareamento mostrado na TV | proposto |
| Interface do celular | Escrita do zero, licença GPL-3.0. Não reaproveita código do Cronus | decidido |

**Sobre o ícone.** Ele abre uma página em `127.0.0.1` de uma porta. Essa página só existe
se alguém a estiver servindo. Por isso o ícone não resolve sozinho o início a frio do
D-Sig. A solução proposta é o autoloader subir o residente, e o ícone abrir a
configuração. Se o residente estiver desligado, o ícone precisa de outro caminho.
**A medir:** o que acontece quando o ícone aponta para uma porta sem servidor.

---

## 2. Dois processos, duas regras

O gancho precisa rodar **dentro do processo do jogo**, porque é lá que a entrada PLT
aponta. Qualquer código que roda ali está sujeito à regra de estabilidade do `CLAUDE.md`,
seção 6. Por isso o sistema tem duas partes:

| parte | onde roda | o que faz | regra |
|---|---|---|---|
| **Gancho** | dentro do processo do jogo | recebe o pacote, aplica a tabela ativa, devolve | sem alocação, sem bloqueio, sem arquivo, sem chamada de sistema |
| **Daemon** | processo próprio | interface, perfis, salvamento, rede com o celular, troca de perfil | pode alocar, abrir arquivo e rede |

Nenhum dos dois fala com o outro por arquivo ou socket no caminho do quadro.

### Como o perfil chega ao gancho

**Proposto:** buffer duplo com contador de versão.

- O daemon escreve a tabela nova no buffer inativo, e só depois publica o número da versão.
- O gancho lê a versão. Se ela mudou desde o último quadro, copia a tabela e atualiza a referência.
- Nenhuma trava. A escrita é sempre no buffer que o gancho não está lendo.
- Tamanho fixo: o teto de `DSIG_MAX_MODS`, já pré-alocado.

**A medir:** que a leitura da versão e a cópia não custam o quadro. E que a memória
compartilhada entre os dois processos é possível no contexto de payload. Se não for, o
perfil é copiado para o gancho no momento da instalação, e a troca de perfil reinstala
o gancho. Isso é mais simples e menos desejável, mas está registrado como alternativa.

---

## 3. Fluxo de dados

```
controle ──► libScePad ──► [gancho no jogo] ──► jogo
                              ▲
                              │ tabela ativa (buffer duplo, versão)
                              │
                          [daemon] ◄── perfis e slots (arquivo, gravação atômica)
                              ▲
                              │ HTTP na rede local, pareado por código
                              │
                          [celular: PWA]

ícone ──► página em 127.0.0.1 servida pelo daemon
autoloader (PS5 Payload Manager / ProsperoMgr) ──► sobe o daemon
```

---

## 4. Perfil

Um perfil é o conteúdo que o daemon publica para o gancho e que o celular edita.

| campo | conteúdo |
|---|---|
| `titulo` | identificador do jogo ao qual o perfil pertence |
| `versao` | número de formato, para leitura futura |
| `slots` | um ou mais slots por título, cada um com nome e tabela de MODs |
| `mods` | lista de `DsigMod` (origem e destino), com o teto `DSIG_MAX_MODS` |
| `soma` | verificação de integridade do conteúdo |

**Proposto:** formato binário de tamanho fixo, com versão e soma. Motivo: o gancho não
pode interpretar texto, e um formato fixo é o que a bancada consegue testar exaustivamente.
Se o celular precisar de texto, a conversão acontece no daemon, nunca no gancho.

**Carga inválida:** como no `dsig_carregar`, nada é trocado se o perfil não validar.

---

## 5. Privilégio

**Proposto, em ordem:**

1. Começar sem escalonar. O carregador entrega acesso ao payload antes do `main()`
   (`ESTUDO.md`, seção 4). O gancho e a leitura de arquivos não precisam de elevação.
2. Medir se o servidor de rede abre no processo do daemon, sem elevação.
3. Só se a rede falhar: elevar **o próprio processo**, e só o que for necessário. Nunca
   o processo do jogo.
4. Ordem, se a elevação for necessária: elevar antes de abrir a rede, e antes de
   qualquer tentativa de encerrar uma instância anterior. O `ESTUDO.md` registra que a
   ordem inversa falha com EPERM e deixa os serviços presos ao contexto antigo.
5. Não encerrar instância anterior por PID. Usar arquivo de bloqueio. Assim a elevação
   para isso não é necessária.

**A medir:** tudo acima. Nenhum desses passos foi executado no console.

---

## 6. Celular

**Decidido:** PWA escrita do zero, GPL-3.0, servida pelo daemon.

**Proposto:**

- O console publica uma porta na rede local. O celular conecta pelo IP do console.
- A conexão exige o código de pareamento, mostrado na TV, uma vez por celular.
- A porta só aceita conexão da rede local. Nada sai para a internet.
- A interface edita perfis e slots. Também troca de perfil ao vivo, para uso durante o
  jogo.

**A medir:** se a porta abre sem elevação (seção 5), e se a troca de perfil pelo celular
chega ao gancho dentro de poucos quadros.

---

## 7. Fases

| fase | o que entrega | onde se verifica |
|---|---|---|
| **3a** | Formato de perfil, serialização, validação, buffer duplo com versão | bancada no Linux, com sabotagens |
| **3b** | Daemon: carga de perfis, slots, gravação atômica, pareamento e troca de perfil. Sem gancho | bancada e console (rede, arquivos, autoloader) |
| **3c** | Gancho: instalação na PLT, leitura da tabela ativa, encadeamento com o PoorDS4 | console. A bancada só cobre a lógica |
| **3d** | Ícone e página de configuração embutida | console |

A fase 3a não exige console. Por isso é a próxima a ser escrita, depois que você
aprovar este documento.

---

## 8. Perguntas em aberto

1. **Identificador do jogo.** Qual API do sistema devolve o identificador do título em
   execução, e funciona de dentro do gancho ou só do daemon?
2. **Memória compartilhada.** O buffer duplo exige memória compartilhada entre o daemon e
   o processo do jogo. Isso é possível no contexto de payload? Se não for, a alternativa
   da seção 2 vale.
3. **Início a frio pelo ícone.** O ícone precisa funcionar com o D-Sig desligado, ou basta
   ele abrir a configuração quando o residente já está de pé?
4. **Duas instâncias.** Um segundo carregamento do D-Sig deve substituir o primeiro, ou
   recusar a subida?
5. **Número de slots por título.** Quantos slots o usuário precisa? O Cronus tinha cinco
   scripts, e o D-Sig um perfil por jogo. Isso ainda não foi definido.
