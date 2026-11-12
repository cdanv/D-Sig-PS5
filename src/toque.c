/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver toque.h e docs/ARQUITETURA.md, secao 26.
 */
#include "toque.h"
#include <string.h>

int toque_validar(const ToqueCal *c) {
    if (c == 0) return -1;
    if (c->x_max == 0 || c->y_max == 0) return -1;
    return 0;
}

void toque_iniciar(ToqueEstado *e) {
    memset(e, 0, sizeof *e);
}

/* origem (0) -> -100; maximo -> +100; fora da faixa, limita */
static int32_t normaliza(uint16_t v, uint16_t maximo) {
    int32_t r;
    if (v > maximo) v = maximo;
    r = (int32_t)v * 200 / (int32_t)maximo - 100;
    return r;
}

void toque_aplicar(const ToqueCal *c, ToqueEstado *e, const ToqueRaw *raw, Entrada *out) {
    unsigned k;
    for (k = 0; k < 2; k++) {
        out->dedo[k] = raw->presente[k] ? 1 : 0;
        if (raw->presente[k]) {
            /* enquanto ha dedo, a posicao acompanha; ao soltar, a ultima fica */
            e->pos[2 * k] = normaliza(raw->x[k], c->x_max);
            e->pos[2 * k + 1] = normaliza(raw->y[k], c->y_max);
        }
    }
    out->eixo[EIXO_F1X] = e->pos[0];
    out->eixo[EIXO_F1Y] = e->pos[1];
    out->eixo[EIXO_F2X] = e->pos[2];
    out->eixo[EIXO_F2Y] = e->pos[3];
}
