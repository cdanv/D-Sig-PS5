# O que aprendemos lendo os payloads existentes

Registro do estudo feito em 11/11/2026 sobre quatro projetos da cena de homebrew do PS5,
todos GPL-3.0. Nada foi copiado; o que está aqui é conhecimento de plataforma, com a
origem anotada para quem quiser conferir.

Existe para que a próxima sessão não precise reler 300 mil linhas.

---

## 1. O `ScePadData` está confirmado por duas fontes independentes

`src/pad.h` nasceu do `payload/pad_types.h` do **PoorDS4 v0.2.0**. O **etaHEN 2.5B** tem
a mesma estrutura em `daemon/include/pad.hpp`, com outra linhagem (OpenOrbis PS4
Toolchain) e outros nomes de campo.

As duas foram compiladas lado a lado e comparadas por `offsetof`:

```
  campo             nosso  etaHEN
  buttons              0     0   ok
  leftStick            4     4   ok
  rightStick           6     6   ok
  analogButtons        8     8   ok
  quat                12    12   ok
  velocity            28    28   ok
  acceleration        40    40   ok
  touchData           52    52   ok
  connected           76    76   ok
  timestamp           80    80   ok
  extension           88    88   ok
  count              104   104   ok

  sizeof             120   120   ok
```

O etaHEN declara `connected` seguido direto de `timestamp`; o nosso tem `reserved1[3]`
explícito no meio. O alinhamento do compilador faz os dois caírem no mesmo lugar. Os
`120 bytes` que a bancada imprime não são "o que o PoorDS4 disse".

## 2. A técnica do gancho, confirmada também por duas fontes

**etaHEN**, `daemon/source/msg.cpp`, função `HookGame`:

```c
UniquePtr<SharedLib> lib = hijacker->getLib("libScePad.sprx");
stuff.scePadReadState = hijacker->getFunctionAddress(lib.get(), nid::scePadReadState);
auto index = meta->getSymbolTable().getSymbolIndex(nid::scePadReadState);
for (const auto& plt : plttab) {
    if (ELF64_R_SYM(plt.r_info) == index) {
        uintptr_t hook_adr = hijacker->getEboot()->imagebase() + plt.r_offset;
        hijacker->write<uintptr_t>(hook_adr, code);   // a interceptacao
```

Localiza a `libScePad` dentro do processo do jogo, resolve `scePadReadState` pelo NID
(`YndgXqQVV7c`), acha a entrada **PLT** do jogo que aponta para ela e sobrescreve o
ponteiro. A biblioteca não é tocada. O **PoorDS4** usa a mesma técnica, de forma
independente.

Dois detalhes que custariam um fim de semana para descobrir:

**Suspender antes, retomar depois** — e o `usleep` não é enfeite:

```c
SuspendApp(appid);
... gancho ...
sleep(1);
ResumeApp(pid);
```

> *we need to sleep the thread after suspension because this will cause a kernel panic
> when user quits the process after sometime*

**Repetir até conseguir** — a `libScePad` do jogo pode não estar carregada no instante
em que o jogo abre:

```c
while (!HookGame(executable, text_base)) { sleep(1); }
```

## 3. O conflito com o PoorDS4

O PoorDS4 engancha, no mesmo processo, as funções:

```
scePadReadState   scePadRead   scePadReadStateExt   scePadReadExt
scePadGetDataInternal   scePadGetControllerInformation
scePadGetExtControllerInformation   scePadDeviceClassGetExtendedInformation
scePadSetVibration   scePadSetVibrationMode
scePadSetTriggerEffect   scePadGetTriggerEffectState
```

São exatamente as que o D-Sig precisa. O encadeamento está descrito em `CLAUDE.md`,
seção 7. Ainda é hipótese: não foi escrito nem verificado.

## 4. Privilégio — o que "o loader entrega kernel R/W" quer dizer

**ProsperoMgr**, `src/jailbreak.cpp`, 50 linhas:

```c
kernel_set_ucred_uid(pid, 0);  ruid, svuid, rgid, svgid...
kernel_set_ucred_authid(pid, 0x4801000000000013ULL);

uint8_t caps[16]; memset(caps, 0xff, sizeof(caps));
kernel_set_ucred_caps(pid, caps);

intptr_t root_vnode = kernel_get_root_vnode();
kernel_set_proc_rootdir(pid, root_vnode);
kernel_set_proc_jaildir(pid, root_vnode);
```

> *The loader hands every payload kernel R/W before main(); this spends it on our own
> credentials.*

Usa `<ps5/kernel.h>` do **PS5 Payload SDK**, o mesmo que o nosso Makefile referencia.
Ordem que importa: escalar privilégio **antes** de matar instância anterior (senão
`kill` falha com EPERM) e **antes** de subir rede (os serviços da Sony se prendem ao
contexto em que foram iniciados).

## 5. Lançar outro payload é trivial

Os dois gerenciadores fazem igual: TCP em `127.0.0.1:9021` e despejar os bytes do ELF.

**PS5 Payload Manager**, `src/ps5_launcher.c`:

```c
address.sin_port = htons(ELFLDR_PORT);           // 9021
address.sin_addr.s_addr = inet_addr("127.0.0.1");
...
sent = send(sock, buffer+offset, bytes-offset, send_flags);
```

É o protocolo inteiro.

## 6. O autoload

**PS5 Payload Manager** — `/data/pldmgr/autoload.txt`, texto puro:

```
kstuff-lite.elf
!2000
ftpsrv.elf
dsig-ps5.elf
```

`!NNNN` é pausa em milissegundos. O parser rejeita a lista inteira se alguma linha for
inválida, em vez de executar metade.

Durante a contagem regressiva ele detecta o botão PS **lendo o log do kernel**:

```c
klog_fd = open("/dev/klog", O_RDONLY|O_NONBLOCK);
if (strstr(log, "onPSButtonPressed")) { /* aborta */ }
```

**ProsperoMgr** faz o equivalente com `autoloader.ini`, e tem uma ideia que vale anotar:
usa `kern.boottime` para saber se já rodou a sequência **nesta mesma ligada**, e não
relança o que já está carregado se o ELF for reenviado.

## 7. O ícone na tela inicial — dois caminhos, um deles muito melhor

**ProsperoMgr** embute um PKG pronto de 10 MB (`bundled_tile_pkg.cpp`, gerado por
`tools/gen_tile_pkg.py`) e instala com `sceAppInstUtilAppInstallPkg`. Funciona, mas não
é reconstruível sem ferramenta de PKG.

**PS5 Payload Manager** faz o mesmo com dois arquivos. `assets/param.json`, inteiro:

```json
{
    "titleId": "PLDM00001",
    "applicationCategoryType": 65536,
    "deeplinkUri": "http://127.0.0.1:8084/",
    "localizedParameters": {
        "defaultLanguage": "en-US",
        "en-US": { "titleName": "Payload Manager" }
    }
}
```

E `src/app_installer.c`, ~180 linhas:

```c
install_file(param_path, param_json, param_json_size);  // /user/app/<TID>/sce_sys/param.json
install_file(icon_path,  icon0_png,  icon0_png_size);   // .../sce_sys/icon0.png
sceAppInstUtilAppInstallTitleDir(title_id, "/user/app/", 0);
```

Sem PKG, sem assinatura, sem ferramenta externa. `applicationCategoryType: 65536` é o
tipo "atalho de navegador": o ícone abre o browser no `deeplinkUri`.

**O ícone é a porta, não a casa** — ele não executa o payload, abre uma página. Só faz
sentido se o D-Sig servir uma página de configuração.

Os dois arquivos entram no binário por `.incbin` em assembly inline, sem gerar um `.cpp`
gigante:

```c
__asm__(".section .rodata\n.global " #name "\n.align 16\n" #name ":\n"
        ".incbin \"" file "\"\n" #name "_end:\n...");
```

## 8. Interface web embutida no ELF

Os dois gerenciadores servem a interface inteira de dentro do próprio binário: o
ProsperoMgr em `assets.cpp` (2,2 MB, React, gerado por `gen_assets.py`), o pldmgr pelo
mesmo mecanismo. Portas: ProsperoMgr 7070, pldmgr 8084.

É o caminho para a configuração do D-Sig: o PWA que já existe no projeto do Cronus pode
ser embutido do mesmo jeito, sem reescrever.

## 9. Persistência no boot — não existe, e a corrente resolve

O jailbreak do PS5 não persiste; o exploit roda a cada vez que o console liga. Nenhum
payload muda isso.

O que existe é a automação do disparo. O README do PS5 Payload Manager:

> *It is highly recommended to use Payload Manager together with an Autoloader:
> Y2JB | BD-JB | Lua. `pldmgr.elf` is already included as the default payload in the
> latest versions of the Autoloaders.*

Os autoloaders (Y2JB, BD-JB, Lua) **não foram lidos** — isto é o que a documentação deles
afirma.

O etaHEN acrescenta sobrevivência ao **modo de descanso** (`Rest_Mode_Delay_Seconds`,
`Game_rest_kill`), que é como se convive com isso na prática: não desligar o console.

## 10. Comparação dos dois gerenciadores

| | ProsperoMgr 1.2 | PS5 Payload Manager 0.6.3 |
|---|---|---|
| tamanho | ~295 mil linhas, C++17 | ~20 mil linhas, C |
| porta | 7070 | 8084 |
| autoload | `.ini` | texto puro |
| ícone | PKG embutido de 10 MB | 2 arquivos + 1 chamada |
| catálogo de terceiros | sim | sim, JSON documentado |
| como material de estudo | muito, mas disperso | **direto ao ponto** |

O pldmgr documenta um formato de catálogo (`CUSTOM_REPOSITORIES.md`) que permitiria o
`D-Sig-PS5` se publicar como repositório próprio, com verificação SHA-256 e aviso de
versão nova — sem construir infraestrutura. Não é para agora; é para saber que existe.

---

## Fontes

| projeto | versão estudada | licença |
|---|---|---|
| [PoorDS4](https://github.com/ItsBlurf/PoorDS4) | v0.2.0 | GPL-3.0 |
| [etaHEN](https://github.com/LightningMods/etaHEN) | 2.5B (com fonte) | GPL-3.0 |
| ProsperoMgr | 1.2 | GPL-3.0 |
| [PS5 Payload Manager](https://github.com/itsPLK/ps5-payload-manager) | 0.6.3 | GPL-3.0 |

---

## 11. PoorDS4 (GPL-3.0): o que estudamos do gancho de entrada

Fonte: `ItsBlurf/poords4`, commit `aa568a5`, leitura de `docs/ARCHITECTURE.md`, `payload/wireless_ds4.c`
(trechos de instalação e restauração). Nenhum trecho foi copiado. Atribuição: PoorDS4, GPL-3.0.

**O que o PoorDS4 faz:**
- Não abre controles: observa handles que já existem. O próprio código diz que `scePadOpen` em índices
  ausentes pode esgotar o limite de dispositivos Bluetooth do console. Isso confirma o que a seção 38 de
  ARQUITETURA.md mediu.
- Redireciona os slots de importação (PLT/GOT) das cinco funções de leitura do controle para funções próprias.
  Não altera o código da `libScePad`.
- Guarda, para cada slot, o endereço original e um "gateway" que chama o original, para os controles que não
  são o dele. É o encadeamento que o CLAUDE.md, seção 7, descreve.
- Valida ABI da biblioteca e do controle antes de instalar.
- Na remoção, restaura o slot somente se o valor atual ainda for o do gancho ou o original, e recusa
  restaurar se encontrar outro valor. Não apaga um gancho de outra camada.

**O que não aproveitamos:** a escrita na memória do jogo a partir do kernel (mapeamento físico e módulo de
sistema). Fica fora da exceção da ARQUITETURA.md, seção 39.

## 12. O caminho do autoloader: ele não entrega código ao processo do jogo

Fontes: `docs/ESTUDO.md`, seções 5 e 6 (PS5 Payload Manager e ProsperoMgr); buscas por `autoload` nos
repositórios PoorDS4 e Ghostcontrol (nenhum deles usa autoload para entrar em jogo).

**O que o autoloader faz:** a corrente (CLAUDE.md, seção 9) sobe o gerenciador, que lê uma lista e lança
cada ELF como **processo próprio**, pelo mesmo protocolo de envio (TCP `127.0.0.1:9021`). A pausa `!NNNN`
é só espera entre um ELF e o próximo. Não há nenhum passo que abra um jogo e carregue código nele.

**O que isso resolve:** a residência do daemon (fase 3b). O daemon sobe sozinho a cada ligada, como processo
separado, e fica disponível para o gancho. Persistência de processo, não de gancho.

**O que não resolve:** a instalação do gancho dentro do processo do jogo. Nenhuma das duas referências
usa autoload para isso; o PoorDS4 instala de fora, por escrita de memória a partir do kernel, que está
fora da exceção da ARQUITETURA.md, seção 39.

**Conflito que este estudo revelou:** a ARQUITETURA.md, linhas 20 e 161, prevê que o daemon escreve o perfil
na memória do processo do jogo. Isso é escrita de fora do processo, com o mesmo mecanismo de instalação. Pela
seção 39, não pode continuar assim. Precisa ser revisto: o perfil passaria a ser entregue por um canal que o
gancho lê dentro do próprio processo, ou por outro meio ainda a definir.
