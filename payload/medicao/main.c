/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PAYLOAD DE MEDICAO. So le o controle e grava um CSV. Nao tem gancho, nao e residente
 * e termina sozinho. Serve para confirmar, no PS5 FAT 11.20, o que o D-Sig ainda nao pode
 * afirmar (ver docs/ARQUITETURA.md e CLAUDE.md, secao 3): convencao vertical, faixa do
 * touch, escala dos sensores, gatilhos e os bits de PS e SHARE.
 *
 * SONDAGEM 5 (ver docs/ARQUITETURA.md, secao 38). A ordem segue o que o projeto
 * Ghostcontrol (GPL-3.0-or-later) faz antes de usar libScePad, lido em
 * core/src/bt/ps5_vpad.c; nenhum trecho foi copiado, so a ordem e o papel de cada chamada:
 *   1. dlopen da libSceMbus (a libScePad importa dela; sem isso scePadInit pode falhar);
 *   2. sceUserServiceInitialize e sceUserServiceGetInitialUser (o userId precisa ser de um
 *      usuario logado: scePadOpen devolve 0x809b0081 para quem nao esta logado);
 *   3. scePadInit, depois scePadSetProcessPrivilege(1);
 *   4. scePadOpen com o usuario inicial e param NULL.
 * Tudo e gravado no CSV antes e depois de cada chamada.
 *
 * AVISO: as assinaturas abaixo sao as mesmas que o Ghostcontrol declara, mas nao foram
 * conferidas num cabecalho oficial. O payload grava os codigos de retorno logo no inicio do CSV.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include "../../src/pad.h"

/* Declaracoes. Ver o aviso acima: a confirmar no console. */
int scePadInit(void);
int32_t scePadSetProcessPrivilege(int32_t privilege);
int32_t scePadOpen(int32_t userId, int32_t type, int32_t index, const void *param);
int scePadReadState(int32_t handle, ScePadData *data);
int32_t sceUserServiceInitialize(void *params);
int32_t sceUserServiceGetInitialUser(int32_t *user);
int32_t sceUserServiceGetLoginUserIdList(int32_t list[4]);
int32_t sceUserServiceGetForegroundUser(int32_t *user);

/* Destino do CSV: o primeiro pendrive ou HD que aceitar a gravacao (usb0 a usb3), depois o
 * armazenamento interno. */
#define USB_MAX     4
#define SAIDA_INT   "/data/dsig/medicao.csv"
#define DURACAO_S   90
#define PERIODO_US  10000          /* 10 ms: a mesma cadencia do quadro do jogo */

static const char *const libmbus[] = {
    "/system/common/lib/libSceMbus.sprx",
    "libSceMbus.sprx",
};

static double agora_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void passo(FILE *f, const char *msg) {
    /* grava e descarrega ja: se o processo travar depois, esta linha continua no arquivo */
    fprintf(f, "# passo %.3f: %s\n", agora_s(), msg);
    fflush(f);
}

int main(void) {
    FILE *f;
    int32_t h = -1;
    int32_t init_user = -1;
    void *lib = NULL;
    int rc_init, rc_open;
    double t0, t;
    ScePadData d;

    /* sem pendrive, mkdir e fopen falham em silencio e o laco passa adiante */
    static char usb_arq[64];
    const char *saida = SAIDA_INT;
    f = NULL;
    for (int i = 0; i < USB_MAX && !f; i++) {
        snprintf(usb_arq, sizeof usb_arq, "/mnt/usb%d/dsig", i);
        mkdir(usb_arq, 0777);
        snprintf(usb_arq, sizeof usb_arq, "/mnt/usb%d/dsig/medicao.csv", i);
        f = fopen(usb_arq, "w");
        if (f) saida = usb_arq;
    }
    if (!f) {
        mkdir("/data/dsig", 0777);
        f = fopen(SAIDA_INT, "w");
    }
    if (!f) return 1;
    fprintf(f, "# destino: %s\n", saida);
    fflush(f);
    passo(f, "arquivo aberto");

    /* 1. libSceMbus antes de qualquer chamada a libScePad */
    passo(f, "antes de dlopen libSceMbus");
    for (size_t k = 0; k < sizeof libmbus / sizeof libmbus[0] && !lib; k++) {
        lib = dlopen(libmbus[k], RTLD_NOW | RTLD_GLOBAL);
        fprintf(f, "# dlopen %s -> %s\n", libmbus[k], lib ? "ok" : (dlerror() ? dlerror() : "falhou"));
        fflush(f);
    }

    /* 2. usuario logado */
    passo(f, "antes de sceUserServiceInitialize");
    fprintf(f, "# sceUserServiceInitialize retornou %d\n", (int)sceUserServiceInitialize(NULL));
    fflush(f);
    {
        int r = sceUserServiceGetInitialUser(&init_user);
        fprintf(f, "# sceUserServiceGetInitialUser retornou %d, usuario inicial = %d (0x%x)\n",
                r, (int)init_user, (unsigned)init_user);
        fflush(f);
    }

    /* 3. scePadInit, depois o privilegio do processo */
    passo(f, "antes de scePadInit");
    rc_init = scePadInit();
    fprintf(f, "# scePadInit retornou %d\n", rc_init);
    fflush(f);

    passo(f, "antes de scePadSetProcessPrivilege(1)");
    fprintf(f, "# scePadSetProcessPrivilege(1) retornou %d\n", (int)scePadSetProcessPrivilege(1));
    fflush(f);

    /* 4. escolhe um usuario logado: o inicial falhou (ver CSV), entao usa a lista de logados
     * e o usuario em primeiro plano, como o Ghostcontrol faz. Ignora -1 (nenhum) e 0. */
    {
        int32_t login[4] = { -1, -1, -1, -1 };
        int32_t fore = -1;
        int rl = sceUserServiceGetLoginUserIdList(login);
        int rf = sceUserServiceGetForegroundUser(&fore);
        fprintf(f, "# sceUserServiceGetLoginUserIdList retornou %d: %d %d %d %d\n",
                rl, (int)login[0], (int)login[1], (int)login[2], (int)login[3]);
        fprintf(f, "# sceUserServiceGetForegroundUser retornou %d, usuario = %d\n", rf, (int)fore);
        fflush(f);
        if (init_user <= 0) init_user = -1;
        if (fore <= 0) fore = -1;
        if (init_user < 0 && fore > 0) init_user = fore;
        for (int i = 0; i < 4 && init_user < 0; i++)
            if (login[i] > 0) init_user = login[i];
        fprintf(f, "# usuario escolhido para scePadOpen: %d\n", (int)init_user);
        fflush(f);
    }

    passo(f, "antes de scePadOpen");
    if (init_user > 0) {
        h = scePadOpen(init_user, 0, 0, NULL);
        fprintf(f, "# scePadOpen(userId=%d) retornou %d\n", (int)init_user, (int)h);
    } else {
        fprintf(f, "# scePadOpen nao chamada: nenhum usuario logado encontrado\n");
    }
    fflush(f);
    rc_open = (int)h;
    fprintf(f, "# handle usado: %d (negativo = nao abriu)\n", rc_open);
    fflush(f);

    fprintf(f, "# tempo_s,buttons,lx,ly,rx,ry,l2,r2,touch_fingers,touch1_x,touch1_y,touch1_finger,touch2_x,touch2_y,touch2_finger,quat_w,quat_x,quat_y,quat_z,acc_x,acc_y,acc_z,gyr_x,gyr_y,gyr_z,ts_us,connected\n");
    fflush(f);

    passo(f, "entrando no laco de leitura");
    t0 = agora_s();
    int primeira = 1;
    while ((t = agora_s() - t0) < DURACAO_S) {
        int rc = scePadReadState(h, &d);
        if (primeira) { fprintf(f, "# primeira scePadReadState retornou %d\n", rc); fflush(f); primeira = 0; }
        if (rc == 0) {
            fprintf(f, "%.3f,%08x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%llu,%u\n",
                    t, d.buttons,
                    d.leftStick.x, d.leftStick.y, d.rightStick.x, d.rightStick.y,
                    d.analogButtons.l2, d.analogButtons.r2,
                    d.touchData.fingers,
                    d.touchData.touch[0].x, d.touchData.touch[0].y, d.touchData.touch[0].finger,
                    d.touchData.touch[1].x, d.touchData.touch[1].y, d.touchData.touch[1].finger,
                    d.quat.w, d.quat.x, d.quat.y, d.quat.z,
                    d.acceleration.x, d.acceleration.y, d.acceleration.z,
                    d.velocity.x, d.velocity.y, d.velocity.z,
                    (unsigned long long)d.timestamp, d.connected);
        } else {
            fprintf(f, "# %.3f: scePadReadState retornou %d\n", t, rc);
        }
        usleep(PERIODO_US);
    }
    passo(f, "fim normal");
    fclose(f);
    return 0;
}
