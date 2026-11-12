/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver delta.h e docs/ARQUITETURA.md, secao 20.
 *
 * DECISOES DE IMPLEMENTACAO, NAO DO AUTOR (marcadas para confirmacao):
 *  - LIGA com a resposta ja rodando e ignorado.
 *  - Latencia so entre passos e entre ciclos, nunca depois do ultimo ciclo.
 *  - STATIC com ALTERNA: alterna a trava a cada subida de ALTERNA.
 */
#include "delta.h"
#include "tempo.h"
#include <string.h>

static uint64_t ms_us(uint16_t ms) { return (uint64_t)ms * 1000u; }

int delta_validar(const DeltaCfg *c) {
    unsigned i;
    if (c == 0) return -1;
    if (c->tipo > DELTA_COMBO) return -1;
    if (c->origem_pulso > 1) return -1;

    /* DIRECT acompanha a fonte: pulso nao sustenta isso */
    if (c->tipo == DELTA_DIRECT && c->origem_pulso) return -1;

    if (c->tipo == DELTA_DIRECT || c->tipo == DELTA_STATIC) {
        if (c->n_dest == 0 || c->n_dest > DELTA_MAX_DEST) return -1;
        if (c->ciclos != 0) return -1;                     /* sem ciclos nesses tipos */
    } else if (c->tipo == DELTA_TURBO) {
        if (c->n_dest != 1) return -1;
    } else {                                               /* COMBO */
        if (c->n_dest == 0 || c->n_dest > DELTA_MAX_PASSOS) return -1;
    }

    for (i = 0; i < c->n_dest; i++) {
        if (c->dest[i] >= DELTA_DESTINOS) return -1;
    }

    if (c->tipo == DELTA_TURBO || c->tipo == DELTA_COMBO) {
        for (i = 0; i < c->n_dest; i++) {
            if (c->tempo[i] >= tempo_quantos()) return -1;
        }
        if (c->lat != DELTA_SEM_LAT && c->lat >= tempo_quantos()) return -1;
        if (!(c->ciclos == DELTA_CICLOS_ATIVO || (c->ciclos >= 1 && c->ciclos <= 7))) return -1;
    }
    return 0;
}

void delta_iniciar(DeltaEstado *e) {
    memset(e, 0, sizeof *e);
}

/* duracao, em microssegundos, da fase corrente */
static uint64_t duracao_fase(const DeltaCfg *c, const DeltaEstado *e) {
    uint16_t ms = 0;
    if (e->em_latencia) {
        if (c->lat == DELTA_SEM_LAT) return 0;
        tempo_ms(c->lat, &ms);
    } else {
        tempo_ms(c->tempo[e->passo], &ms);
    }
    return ms_us(ms);
}

/* Instante em que a proxima fase comeca. Sem deriva: a fase seguinte comeca no instante
 * PLANEJADO (inicio anterior + duracao), e nao no quadro em que a anterior foi detectada.
 * So ressincroniza com o relogio se o quadro atrasou mais de uma fase inteira. */
static uint64_t inicio_proxima(const DeltaEstado *e, uint64_t dur, uint64_t now) {
    if (now - e->t_fase >= 2u * dur) return now;
    return e->t_fase + dur;
}

/* Passa para o proximo passo, ou encerra o ciclo. Devolve 1 se a resposta terminou. */
static int proximo(const DeltaCfg *c, DeltaEstado *e, uint64_t inicio, int n_passos) {
    int ultimo_passo = (e->passo + 1 >= n_passos);
    if (ultimo_passo) {
        e->ciclos_feitos++;
        if (c->ciclos != DELTA_CICLOS_ATIVO && e->ciclos_feitos >= c->ciclos) {
            e->ligado = 0;
            return 1;
        }
        e->passo = 0;
    } else {
        e->passo++;
    }
    e->em_latencia = 0;
    e->t_fase = inicio;
    return 0;
}

void delta_avaliar(const DeltaCfg *c, DeltaEstado *e, const DeltaEntrada *in, DeltaSaida *out) {
    uint8_t liga_borda = (uint8_t)(in->liga && !e->liga_ant);
    uint8_t alterna_borda = (uint8_t)(in->alterna && !e->alterna_ant);
    uint8_t finaliza_borda = (uint8_t)(in->finaliza && !e->finaliza_ant);
    unsigned i;

    memset(out, 0, sizeof *out);

    switch (c->tipo) {

    case DELTA_DIRECT:
        /* acompanha o nivel de LIGA */
        e->ligado = in->liga ? 1 : 0;
        if (e->ligado) {
            for (i = 0; i < c->n_dest; i++) out->destinos |= 1u << c->dest[i];
        }
        break;

    case DELTA_STATIC:
        if (liga_borda && !e->ligado) e->ligado = 1;
        if (alterna_borda) e->ligado = (uint8_t)!e->ligado;
        if (finaliza_borda) e->ligado = 0;
        if (e->ligado) {
            for (i = 0; i < c->n_dest; i++) out->destinos |= 1u << c->dest[i];
        }
        break;

    default: /* TURBO e COMBO */
        if (!e->ligado) {
            if (liga_borda) {                          /* LIGA comeca a resposta */
                e->ligado = 1;
                e->passo = 0;
                e->em_latencia = 0;
                e->ciclos_feitos = 0;
                e->t_fase = in->t_us;
            } else if (alterna_borda) {                /* ALTERNA tambem liga */
                e->ligado = 1;
                e->passo = 0;
                e->em_latencia = 0;
                e->ciclos_feitos = 0;
                e->t_fase = in->t_us;
            }
        } else {
            if (finaliza_borda || alterna_borda) {     /* FINALIZA, ou ALTERNA desligando */
                e->ligado = 0;
            }
        }

        if (e->ligado) {
            uint64_t dur = duracao_fase(c, e);
            if (in->t_us - e->t_fase >= dur) {
                uint64_t ini = inicio_proxima(e, dur, in->t_us);
                if (e->em_latencia) {
                    if (proximo(c, e, ini, c->n_dest)) out->terminou = 1;
                } else if (c->lat == DELTA_SEM_LAT) {
                    if (proximo(c, e, ini, c->n_dest)) out->terminou = 1;
                } else {
                    /* fim do passo: latencia, ou fim do ciclo sem latencia depois do ultimo */
                    int ultimo_passo = (e->passo + 1 >= c->n_dest);
                    int encerra = ultimo_passo && c->ciclos != DELTA_CICLOS_ATIVO &&
                                  (e->ciclos_feitos + 1 >= c->ciclos);
                    if (encerra) {
                        if (proximo(c, e, ini, c->n_dest)) out->terminou = 1;
                    } else {
                        e->em_latencia = 1;
                        e->t_fase = ini;
                    }
                }
            }
            if (e->ligado && !e->em_latencia) {
                out->destinos |= 1u << c->dest[e->passo];
            }
        }
        break;
    }

    e->liga_ant = in->liga;
    e->alterna_ant = in->alterna;
    e->finaliza_ant = in->finaliza;
}
