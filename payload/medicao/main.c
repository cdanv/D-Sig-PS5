/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PAYLOAD DE MEDICAO. So le o controle e grava um CSV. Nao tem gancho, nao e residente
 * e termina sozinho. Serve para confirmar, no PS5 FAT 11.20, o que o D-Sig ainda nao pode
 * afirmar (ver docs/ARQUITETURA.md e CLAUDE.md, secao 3): convencao vertical, faixa do
 * touch, escala dos sensores, gatilhos e os bits de PS e SHARE.
 *
 * AVISO: as assinaturas de libScePad abaixo NAO foram conferidas num cabecalho oficial
 * (o SDK nao traz um). O payload grava os codigos de retorno logo no inicio do CSV: se
 * uma assinatura estiver errada, isso aparece na primeira execucao, e nao depois.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../src/pad.h"

/* Declaracoes de libScePad. Ver o aviso acima: a confirmar no console. */
int scePadInit(void);
int32_t scePadOpen(int32_t userId, int32_t type, int32_t index, const void *param);
int scePadReadState(int32_t handle, ScePadData *data);
/* Sondagem 4: mesma forma de scePadOpen, assumida; NAO confirmada (o SDK so tem o nome). */
int32_t scePadOpenExt(int32_t userId, int32_t type, int32_t index, const void *param);

/* Destino do CSV: o primeiro pendrive que aceitar a gravacao (o numero N de /mnt/usbN muda
 * conforme a ordem de conexao; no PS5 do autor apareceu como usb1), depois o armazenamento
 * interno. Assim o arquivo sai do console sem FTP. */
#define USB_MAX     4                  /* testa /mnt/usb0 a /mnt/usb3 */
#define SAIDA_INT   "/data/dsig/medicao.csv"
#define DURACAO_S   90
#define PERIODO_US  10000          /* 10 ms: a mesma cadencia do quadro do jogo */
#define USER_ID     0              /* a confirmar: usuario que possui o controle */
/* Sondagem: o teste de 14:03 travou DENTRO de scePadInit (o CSV parou em "antes de scePadInit",
 * sem a linha de retorno). Com esta macro em 0, scePadInit NAO e chamada e o payload tenta
 * scePadOpen direto. Para voltar ao comportamento anterior, defina USAR_SCEPADINIT como 1. */
#ifndef USAR_SCEPADINIT
#define USAR_SCEPADINIT 0      /* 0: sondagem 3, sem scePadInit (ela trava neste console) */
#endif

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
    int32_t h;
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

#if USAR_SCEPADINIT
    passo(f, "antes de scePadInit");
    rc_init = scePadInit();
    fprintf(f, "# scePadInit retornou %d\n", rc_init);
    fflush(f);
#else
    rc_init = -999;                       /* marca: scePadInit nao foi chamada nesta sondagem */
    (void)rc_init;
    fprintf(f, "# scePadInit NAO chamada (sondagem, USAR_SCEPADINIT=0)\n");
    fflush(f);
#endif

    /* Sondagem 3: sem scePadInit. userId 0 ja falhou na sondagem 1, entao testa 1..3 em ordem;
     * para no primeiro handle >= 0. Cada tentativa fica registrada no CSV. */
    /* Sondagem 4: param aponta para um buffer zerado nosso (256 bytes, chute: o tamanho real
     * da estrutura de parametros nao foi confirmado). Testa scePadOpen com userIds 0..3; se
     * nenhum funcionar, testa scePadOpenExt com o mesmo buffer. */
    static char param_zero[256];
    static const int32_t ids_usuario[] = { 0, 1, 2, 3 };
    h = -1;
    for (size_t k = 0; k < sizeof ids_usuario / sizeof ids_usuario[0]; k++) {
        int32_t hk;
        fprintf(f, "# antes de scePadOpen(userId=%d)\n", (int)ids_usuario[k]);
        fflush(f);
        hk = scePadOpen(ids_usuario[k], 0, 0, param_zero);
        fprintf(f, "# scePadOpen(userId=%d) retornou %d\n", (int)ids_usuario[k], (int)hk);
        fflush(f);
        if (hk >= 0) { h = hk; break; }
    }
    for (size_t k = 0; k < sizeof ids_usuario / sizeof ids_usuario[0] && h < 0; k++) {
        int32_t hk;
        fprintf(f, "# antes de scePadOpenExt(userId=%d)\n", (int)ids_usuario[k]);
        fflush(f);
        hk = scePadOpenExt(ids_usuario[k], 0, 0, param_zero);
        fprintf(f, "# scePadOpenExt(userId=%d) retornou %d\n", (int)ids_usuario[k], (int)hk);
        fflush(f);
        if (hk >= 0) { h = hk; break; }
    }
    rc_open = (int)h;
    fprintf(f, "# handle usado: %d (negativo = nenhum userId funcionou)\n", rc_open);
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
