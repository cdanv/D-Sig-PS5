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
    if (c->modo > DELTA_MODO_ABSF) return -1;

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
    uint8_t liga_borda, alterna_borda, finaliza_borda;
    unsigned i;

    memset(out, 0, sizeof *out);

    /* BLOCK ativo: o MOD para de responder. Os tempos param, e os eventos sao ignorados.
     * Ao soltar, o tempo decorrido na pausa e descontado: a resposta retoma de onde parou. */
    if (in->congelado) {
        if (!e->congelado_ant) e->t_pausa = in->t_us;
        e->congelado_ant = 1;
        e->liga_ant = in->liga;
        e->alterna_ant = in->alterna;
        e->finaliza_ant = in->finaliza;
        return;
    }
    if (e->congelado_ant) {
        e->t_fase += in->t_us - e->t_pausa;
        e->congelado_ant = 0;
    }

    liga_borda = (uint8_t)(in->liga && !e->liga_ant);
    alterna_borda = (uint8_t)(in->alterna && !e->alterna_ant);
    finaliza_borda = (uint8_t)(in->finaliza && !e->finaliza_ant);

    switch (c->tipo) {

    case DELTA_DIRECT:
        /* acompanha o nivel de LIGA */
        /* DIRECT segue o nivel da fonte, mas com FINALIZA: a regra e "com fonte, o gatilho so
         * desliga; a fonte religa" (achado 1 do simulador do Cronus). FINALIZA desliga, e so a proxima
         * subida da fonte religa. Sem FINALIZA, a flag nunca sobe e o DIRECT segue o nivel como antes. */
        if (liga_borda) e->finalizado = 0;
        if (finaliza_borda) e->finalizado = 1;
        e->ligado = (in->liga && !e->finalizado) ? 1 : 0;
        if (e->ligado) {
            for (i = 0; i < c->n_dest; i++) out->destinos |= 1u << c->dest[i];
            out->intensidade = in->intensidade;             /* DIRECT segue a fonte do quadro */
            out->sinal = in->lado;
        }
        break;

    case DELTA_STATIC:
        if (liga_borda && !e->ligado) { e->ligado = 1; e->intens = in->intensidade; e->lado_trava = in->lado; }
        if (alterna_borda) {
            e->ligado = (uint8_t)!e->ligado;
            if (e->ligado) { e->intens = in->intensidade; e->lado_trava = in->lado; }
        }
        if (finaliza_borda) e->ligado = 0;
        if (e->ligado) {
            for (i = 0; i < c->n_dest; i++) out->destinos |= 1u << c->dest[i];
            /* STATIC trava: uma fonte de botao vale 100 e fica travada; eixo acompanha a zona
             * enquanto esta nela, e segura o ultimo valor fora dela (leitura do manual, 26.1) */
            if (in->intensidade > 0) { e->intens = in->intensidade; e->lado_trava = in->lado; }
            out->intensidade = e->intens;
            out->sinal = e->lado_trava;
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
                e->intens = in->intensidade;   /* TURBO/COMBO: a intensidade e a do inicio */
                e->lado_trava = in->lado;
                e->terminado = 0;
            } else if (alterna_borda) {                /* ALTERNA tambem liga */
                e->ligado = 1;
                e->passo = 0;
                e->em_latencia = 0;
                e->ciclos_feitos = 0;
                e->t_fase = in->t_us;
                e->intens = in->intensidade;   /* TURBO/COMBO: a intensidade e a do inicio */
                e->lado_trava = in->lado;
                e->terminado = 0;
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
            if (e->ligado) {
                uint8_t d = c->dest[e->passo];
                if (d >= 10 && d <= 13) {
                    /* destino de eixo: escreve tambem no silencio (manual do EIXOS, 9.1). A fase
                     * ligada escreve 100 com o sinal do modo; o silencio escreve 0, e so o ABS
                     * escreve -100. Os modos estao na tabela da secao 9.2. */
                    int8_t s_lig = (c->modo == DELTA_MODO_NEG) ? -1
                                 : (c->modo == DELTA_MODO_ABSF) ? in->lado : 1;
                    out->destinos |= 1u << d;
                    if (!e->em_latencia) {
                        out->intensidade = 100;
                        out->sinal = s_lig;
                    } else if (c->modo == DELTA_MODO_ABS) {
                        out->intensidade = 100;             /* ABS silencio: -100 */
                        out->sinal = -1;
                    }
                    /* POS, NEG e ABSF no silencio: 0 (intensidade 0) */
                } else if (!e->em_latencia) {
                    out->destinos |= 1u << c->dest[e->passo];
                    out->intensidade = e->intens;
                    out->sinal = e->lado_trava;
                }
            }
        }
        break;
    }

    /* Manual do EIXOS, 9.2 ("exceção obrigatória"): quando os ciclos terminam, os destinos de eixo
     * repousam em zero de verdade. Decisão do autor: o zero continua escrito enquanto o MOD estiver
     * desligado, para o eixo nao voltar ao cru. Um MOD que nunca iniciou fica em silencio. */
    if (out->terminou) e->terminado = 1;
    if (!e->ligado && e->terminado) {
        for (i = 0; i < c->n_dest; i++) {
            uint8_t d = c->dest[i];
            if (d >= 10 && d <= 13) out->destinos |= 1u << d;   /* intensidade 0, sinal 0 */
        }
    }

    e->liga_ant = in->liga;
    e->alterna_ant = in->alterna;
    e->finaliza_ant = in->finaliza;
}
