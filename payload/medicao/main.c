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

#define SAIDA       "/data/dsig/medicao.csv"
#define DURACAO_S   90
#define PERIODO_US  10000          /* 10 ms: a mesma cadencia do quadro do jogo */
#define USER_ID     0              /* a confirmar: usuario que possui o controle */

static double agora_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    FILE *f;
    int32_t h;
    int rc_init, rc_open;
    double t0, t;
    ScePadData d;

    mkdir("/data/dsig", 0777);            /* ja existir nao e erro */
    f = fopen(SAIDA, "w");
    if (!f) return 1;

    rc_init = scePadInit();
    h = scePadOpen(USER_ID, 0, 0, 0);
    rc_open = (int)h;
    fprintf(f, "# medicao D-Sig: scePadInit=%d scePadOpen=%d (handle esperado >= 0)\n", rc_init, rc_open);
    fprintf(f, "# tempo_s,buttons,lx,ly,rx,ry,l2,r2,touch_fingers,touch1_x,touch1_y,touch1_finger,touch2_x,touch2_y,touch2_finger,quat_w,quat_x,quat_y,quat_z,acc_x,acc_y,acc_z,gyr_x,gyr_y,gyr_z,ts_us,connected\n");
    fflush(f);

    t0 = agora_s();
    while ((t = agora_s() - t0) < DURACAO_S) {
        int rc = scePadReadState(h, &d);
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
    fclose(f);
    return 0;
}
