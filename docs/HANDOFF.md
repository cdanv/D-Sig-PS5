# D-Sig-PS5: relatório de continuidade

Este documento resume o estado do projeto para quem vai dar continuidade. Ele não substitui
`CLAUDE.md` (escopo e regras de trabalho) nem `docs/ARQUITETURA.md` (decisões, seção por seção).
Ler os dois antes de qualquer alteração.

## 1. O projeto em uma frase

Roteador de sinais de controle para acessibilidade, para PS5 com jailbreak (FAT, firmware 11.20 primeiro).
Terminado e em uso no Cronus Zen (repositório separado, CC BY-NC 4.0). Esta é a reescrita em payload.

## 2. Escopo (resumo; o texto completo está no CLAUDE.md, seção 2)

Dentro: turbo, macro, sequência, acorde, domínios, e tudo que permita jogar sem depender de ajuda
de terceiros. Fora: vantagem sobre outra pessoa em jogo online, exploit do console, DRM, pirataria,
e qualquer escrita ou leitura de memória do jogo, salvo a exceção de gancho de entrada da seção 39 da
ARQUITETURA.md.

## 3. Estado por fase

| fase | estado |
|---|---|
| Alvo 1: passagem pura | feito |
| Alvo 2: primeiro MOD (troca de botão e acorde) | feito na bancada |
| Alvo 3, fase 3a: lógica do roteador | fechada na bancada (12 bancadas, todas passam) |
| Alvo 3, fases 3b a 3d | bloqueadas: dependem de instalar o gancho no jogo (ver seção 6) |
| Alvo 4: ícone na tela inicial | caminho conhecido (ESTUDO.md, seção 7) |

## 4. Como compilar e testar

```bash
make bancada     # compila e roda as 12 bancadas em ferramentas/. Requer gcc.
make payload     # reservado para o console. Hoje só avisa que a ponte não existe.
```

Payload de medição (console, não bancada), dentro de `payload/medicao/`:

```bash
export PS5_PAYLOAD_SDK=/caminho/para/ps5-payload-sdk
make             # usa prospero-clang; -Wall -Werror -O2 -lScePad -lSceUserService -ldl
```

O payload grava um CSV no primeiro pendrive ou HD em `/mnt/usb0` a `/mnt/usb3` (em `dsig/`), e
cai em `/data/dsig/` se nenhum aceitar. A primeira linha (`# destino:`) mostra o caminho usado.

## 5. Mapa de arquivos

| caminho | o que é |
|---|---|
| `src/pad.h` | ScePadData (120 bytes), confirmado por duas fontes; atribuído ao PoorDS4 |
| `src/dsig.h` | interface do roteador, deliberadamente estreita |
| `src/passagem.c` | roteador: copia e aplica MOD de botão quando carregado |
| `src/alfa.c/.h` | ALFA v4, seis abas (LIVRE, SEQUÊNCIA, 2CLK, HOLD_AT, HOLD_AP, BLOCK) |
| `src/delta.c/.h` | DELTA: DIRECT, STATIC, TURBO, COMBO |
| `src/modelo.c/.h` | MOD completo: ALFA ligada ao DELTA, com validação |
| `src/rede.c/.h` | rede de MODs com ZETA: BLOQ, ASSOC, EXCL |
| `src/dominio.c/.h` | domínios D1 a D4 |
| `src/toque.c/.h` | toque do touchpad: dedos como botão e como eixo |
| `src/tempo.c/.h` | tabela única de tempo (67 valores) |
| `src/perfil.c/.h` | buffer duplo com versão |
| `src/formato.c/.h` | formato binário do perfil: magic DSG1, versão 2, CRC32 |
| `ferramentas/testa_*.c` | 12 bancadas (acumula, alfa, delta, dominio, formato, mod, modelo, passagem, perfil, rede, tempo, toque) |
| `payload/medicao/main.c` | payload de medição: só lê o controle e grava CSV; roteiro em ROTEIRO.md |
| `docs/ARQUITETURA.md` | decisões de desenho, seções 1 a 41 |
| `docs/ESTUDO.md` | o que foi estudado de outros projetos (seções 1 a 12) |
| `docs/REFERENCIA_CRONUS.md` | análise do manual do Cronus 1.0.17 |

## 6. O bloqueio, em uma página

**Tudo que roda dentro do jogo depende de instalar o gancho nas funções de leitura do controle
(`scePadReadState` e variantes), no processo do jogo.** Hoje:

- O CLAUDE.md, seção 2, permite só o gancho de entrada (exceção da seção 39), e proíbe escrita de fora do
  processo, inclusive a partir do kernel.
- Os caminhos estudados não cabem nessa regra: o autoloader não entra no jogo (ESTUDO, seção 12); o PoorDS4
  instala de fora (ESTUDO, seção 11); o Ghostcontrol usa privilégio elevado para vincular o controle.
- Um payload independente não lê o controle: `scePadInit` e `scePadSetProcessPrivilege` passam, mas o usuário
  logado não é aceito por `scePadGetHandle` nem `scePadOpen` (ARQUITETURA, seções 38 e 41).

**Decisão pendente do autor (ARQUITETURA, seção 41):** ampliar a exceção da seção 39 para um instalador
externo restrito, com limites escritos, ou não. Sem ela, a fase 3b não tem caminho dentro da regra.

## 7. Medições de console já registradas

- `libSceMbus` carregada com `dlopen` antes das chamadas da `libScePad` faz `scePadInit` e
  `scePadSetProcessPrivilege(1)` retornarem 0 (ARQUITETURA, seção 38, addendum).
- `0x80920005` = biblioteca não inicializada (sem `scePadInit` efetiva).
- `0x809B0081` = usuário não logado para o pad. Nas seis tentativas, o usuário da lista de logados foi recusado.
- Com o controle conectado por Bluetooth, `scePadReadState` não devolveu dados em nenhuma execução.

## 8. Pendências de console que não dependem do gancho

Estas só precisam de leitura real do controle (ver CLAUDE.md, seção 3):
convenção vertical; faixa e calibração do touch; escala de ACC e GYRO e zona morta; conversão de
gatilhos 0..100 para 0..255; bits de PS e SHARE. Todas continuam abertas.

## 9. Licenças e atribuição

- D-Sig-PS5: GPL-3.0-or-later.
- PoorDS4 (GPL-3.0): origem do layout do ScePadData, atribuído em `src/pad.h`. Estudado, nada copiado.
- Ghostcontrol (repositório público, README indica GPL-3.0-or-later): estudado para a ordem de inicialização
  (ARQUITETURA, seção 38). Nada copiado.
- etaHEN, ProsperoMgr, PS5 Payload Manager: estudados (ESTUDO.md). Nada copiado.
- Regra de mão única: nada daqui volta para o repositório do Cronus (CC BY-NC).

## 10. Regras de trabalho

Do CLAUDE.md, seção 4: nada afirmado sem rodar; teste que não reprova não prova nada (sabotagem deliberada,
pelo motivo certo); entrada só leitura; toda mudança estrutural explicada; erro próprio se admite na hora.

## 11. Estado do repositório

- Branch de trabalho atual: `payload/medicao-usb-n`. Branch principal: `main`.
- Remoto: `https://github.com/cdanv/D-Sig-PS5`.
- Commits que documentam o caminho: a partir de `f2a88a5` (regra test do payload) até o commit desta seção.
