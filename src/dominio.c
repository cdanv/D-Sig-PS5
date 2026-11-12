/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver dominio.h e docs/ARQUITETURA.md, secao 23.
 *
 * DECISAO DE IMPLEMENTACAO, NAO DO AUTOR: o modificador (L1 ou R1) precisa estar apertado
 * no primeiro clique e no segundo. Se o modificador solta antes, o clique nao conta.
 */
#include "dominio.h"
#include "pad.h"
#include <string.h>

void dominio_iniciar(Dominio *d) {
    memset(d, 0, sizeof *d);
}

/* Liga o dominio `alvo`; se ele ja estava ligado, desliga. Marca a troca. */
static void alterna(Dominio *d, uint8_t alvo, DominioSaida *out) {
    uint8_t novo = (uint8_t)((d->atual == alvo) ? 0 : alvo);
    if (novo != d->atual) {
        d->atual = novo;
        out->trocou = 1;
    }
}

void dominio_avaliar(Dominio *d, const Entrada *in, DominioSaida *out) {
    unsigned k;
    memset(out, 0, sizeof *out);

    for (k = 0; k < 2; k++) {
        uint32_t mod_bit = (k == 0) ? DS_BTN_L1 : DS_BTN_R1;
        uint32_t btn = (k == 0) ? DS_BTN_L3 : DS_BTN_R3;
        uint8_t mod_apertado = (uint8_t)((in->botoes & mod_bit) != 0);
        uint8_t nivel = (uint8_t)((in->botoes & btn) != 0);
        uint8_t alvo_duplo = (k == 0) ? 1 : 2;     /* D1 para L1+L3, D2 para R1+R3 */
        uint8_t alvo_segurar = (k == 0) ? 3 : 4;   /* D3 para L1+L3, D4 para R1+R3 */

        /* segundo clique dentro da janela, com o modificador apertado: duplo clique */
        if (nivel && !d->ant[k]) {
            if (d->pend[k] && in->t_us - d->t_pend[k] <= DOMINIO_DUPLO_US && mod_apertado) {
                alterna(d, alvo_duplo, out);
                d->pend[k] = 0;
            } else if (mod_apertado) {
                d->pend[k] = 1;
                d->t_pend[k] = in->t_us;
            } else {
                d->pend[k] = 0;
            }
            d->t_press[k] = in->t_us;
            d->hold_feito[k] = 0;
        }

        /* segurar: com o modificador apertado, passa do tempo: D3 ou D4, uma vez por aperto */
        if (nivel && !d->hold_feito[k] && mod_apertado &&
            in->t_us - d->t_press[k] >= DOMINIO_HOLD_US) {
            alterna(d, alvo_segurar, out);
            d->hold_feito[k] = 1;
            d->pend[k] = 0;
        }

        /* um clique pendente que passou da janela expira */
        if (d->pend[k] && in->t_us - d->t_pend[k] > DOMINIO_DUPLO_US) d->pend[k] = 0;

        d->ant[k] = nivel;
    }
}
