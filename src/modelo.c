/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver modelo.h e docs/ARQUITETURA.md, secao 21.
 *
 * Abas de nivel (LIVRE): alimentam o DELTA com o seu nivel quando o DELAY e OFF.
 * Abas de pulso (SEQUENCIA, 2CLK, HOLD_AT, HOLD_AP): alimentam o DELTA com o pulso.
 * BLOCK: nao alimenta o DELTA. Suspende o MOD.
 */
#include "modelo.h"
#include <string.h>

static int aba_alimenta(unsigned a) {
    return a != ABA_BLOCK;
}

static int aba_e_nivel(unsigned a) {
    return a == ABA_LIVRE || a == ABA_BLOCK;
}

int modelo_validar(const ModCfg *m) {
    unsigned a, alimentam = 0;
    uint8_t origem_pulso = 0;
    DeltaCfg d;

    if (m == 0) return -1;
    if (alfa_validar(&m->alfa) != 0) return -1;

    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &m->alfa.abas[a];
        if (ab->n == 0) continue;
        if (m->acao[a] > ACAO_FINALIZA) return -1;

        if (!aba_alimenta(a)) {
            /* BLOCK: suspende, nao tem acao sobre o DELTA */
            if (m->acao[a] != ACAO_LIGA) return -1;
            continue;
        }
        alimentam++;
        if (!aba_e_nivel(a)) origem_pulso = 1;

        if (m->delta.tipo == DELTA_DIRECT) {
            if (!aba_e_nivel(a)) return -1;                     /* DIRECT: so nivel */
            if (ab->delay != ALFA_DELAY_OFF && ab->delay != ALFA_DELAY_NAO_ESCOLHIDO) return -1;
        }
    }
    if (alimentam == 0) return -1;                              /* sem origem, sem resposta */

    d = m->delta;
    d.origem_pulso = origem_pulso;
    return delta_validar(&d);
}

void modelo_iniciar(ModEstado *e) {
    alfa_iniciar(&e->alfa);
    delta_iniciar(&e->delta);
}

void modelo_avaliar(const ModCfg *m, ModEstado *e, const Entrada *in, ModSaida *out) {
    AlfaSaida as;
    DeltaEntrada de;
    DeltaSaida ds;
    unsigned a;
    uint8_t bloqueio = 0;

    memset(out, 0, sizeof *out);
    memset(&de, 0, sizeof de);

    alfa_avaliar(&m->alfa, &e->alfa, in, &as);
    out->ocultar_botoes = as.ocultar_botoes;
    out->ocultar_analog = as.ocultar_analog;

    for (a = 0; a < ALFA_ABAS; a++) {
        if (m->alfa.abas[a].n == 0) continue;
        if (!aba_alimenta(a)) {
            if (as.ativa[a]) bloqueio = 1;                      /* BLOCK */
            continue;
        }
        /* o disparo de cada aba (com DELAY) alimenta o DELTA pela sua acao */
        switch (m->acao[a]) {
        case ACAO_LIGA:     de.liga |= as.disparo[a];     break;
        case ACAO_ALTERNA:  de.alterna |= as.disparo[a];  break;
        case ACAO_FINALIZA: de.finaliza |= as.disparo[a]; break;
        default:            break;
        }
    }

    de.congelado = bloqueio;
    de.t_us = in->t_us;
    delta_avaliar(&m->delta, &e->delta, &de, &ds);

    out->destinos = bloqueio ? 0 : ds.destinos;
    out->terminou = ds.terminou;
    out->bloqueado = bloqueio;
}
