/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver alfa.h. Regras de cada aba: docs/ARQUITETURA.md, secao 18.8.
 *
 * DECISOES DE IMPLEMENTACAO, NAO DO AUTOR (marcadas para confirmacao):
 *  - SEQUENCIA: o passo e a BORDA de subida da fonte. Segurar um botao conta uma vez.
 *  - Dentro de um mesmo quadro, bordas sao processadas pela ordem das entradas.
 *  - HOLD_AT e HOLD_AP: o tempo de segurar vem do catalogo da propria aba.
 *  - BLOCK com E: o bloqueio vale enquanto todas as entradas estiverem apertadas,
 *    dentro da janela de simultaneidade.
 */
#include "alfa.h"
#include "tempo.h"
#include <string.h>

#define G(a, k) ((unsigned)(a) * ALFA_POR_ABA + (unsigned)(k))

/* ------------------------------------------------------------------ BETA */

/* valor do eixo ou gatilho depois do MULT, limitado a -100..+100. E a intensidade (manual, secao 26.1) */
static int32_t beta_valor(const Beta *b, int32_t v) {
    int32_t pct = (int32_t)(b->mult + 1) * 10;
    int32_t w = v * pct / 100;
    if (w > 100) w = 100;
    if (w < -100) w = -100;
    return w;
}

static int beta_conta(const Beta *b, int32_t v, int dir_vale) {
    int32_t w = beta_valor(b, v);
    int32_t m;
    int dir_ok = 1, zona, conta;

    m = w < 0 ? -w : w;

    if (dir_vale) {
        if (b->dir == DIR_NEG && !(w < 0)) dir_ok = 0;
        if (b->dir == DIR_POS && !(w > 0)) dir_ok = 0;
    }
    if (b->thr_out != 0 && b->thr_in > b->thr_out) {
        zona = (m < (int32_t)b->thr_out) || (m > (int32_t)b->thr_in);
    } else {
        zona = (m >= (int32_t)b->thr_in) &&
               (b->thr_out == 0 || m <= (int32_t)b->thr_out);
    }
    conta = dir_ok && (b->inv ? !zona : zona);
    return conta ? 1 : 0;
}

/* intensidade da fonte neste quadro, 0..100. Botao e toque: 100 quando ha nivel. Eixo: o modulo do
 * valor depois do MULT, so quando esta na zona. Gatilho: o valor depois do MULT, so na zona. */
static int intensidade_fonte(const Fonte *f, const Entrada *in) {
    int32_t w;
    switch (f->tipo) {
    case FONTE_BOTAO:   return ((in->botoes >> f->id) & 1u) ? 100 : 0;
    case FONTE_DEDO:    return in->dedo[f->id] ? 100 : 0;
    case FONTE_EIXO:
        if (!beta_conta(&f->beta, in->eixo[f->id], 1)) return 0;
        w = beta_valor(&f->beta, in->eixo[f->id]);
        return w < 0 ? -w : w;
    case FONTE_GATILHO:
        if (!beta_conta(&f->beta, in->gatilho[f->id], 0)) return 0;
        return beta_valor(&f->beta, in->gatilho[f->id]);
    default:            return 0;
    }
}

static int nivel_fonte(const Fonte *f, const Entrada *in) {
    switch (f->tipo) {
    case FONTE_BOTAO:   return (int)((in->botoes >> f->id) & 1u);
    case FONTE_EIXO:    return beta_conta(&f->beta, in->eixo[f->id], 1);
    case FONTE_GATILHO: return beta_conta(&f->beta, in->gatilho[f->id], 0);
    case FONTE_DEDO:    return in->dedo[f->id] ? 1 : 0;
    default:            return 0;
    }
}

int alfa_em_repouso(const AlfaCfg *c, const Entrada *in) {
    unsigned a, k;
    for (a = 0; a < ALFA_ABAS; a++) {
        for (k = 0; k < c->abas[a].n; k++) {
            if (nivel_fonte(&c->abas[a].fontes[k], in)) return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------ validacao */

static int beta_valida(const Beta *b) {
    if (b->mult > 18) return 0;
    if (b->thr_in > 100 || b->thr_in % 5 != 0) return 0;
    if (b->thr_out > 95 || (b->thr_out != 0 && b->thr_out % 5 != 0)) return 0;
    if (b->dir > DIR_POS) return 0;
    if (b->inv > 1) return 0;
    return 1;
}

static int fonte_valida(const Fonte *f) {
    if (f->tipo == FONTE_BOTAO) return f->id < 32;
    if (f->tipo == FONTE_EIXO) return f->id < EIXO_N && beta_valida(&f->beta);
    if (f->tipo == FONTE_GATILHO) return f->id <= GATILHO_R2 && beta_valida(&f->beta);
    if (f->tipo == FONTE_DEDO) return f->id <= 1;
    return 0;
}

static int aba_valida(const Aba *ab, unsigned a) {
    unsigned i, j;
    if (ab->n == 0) return 1;                       /* aba vazia e permitida */
    if (ab->n > ALFA_POR_ABA) return 0;
    if (ab->eou > EOU_E) return 0;
    if (ab->oculto > 1) return 0;

    if (a == ABA_2CLK && ab->eou != EOU_OU) return 0;         /* 2CLK e sempre OU */
    if (a == ABA_SEQUENCIA && ab->n < 2) return 0;            /* sequencia tem pelo menos 2 passos */
    if (a != ABA_SEQUENCIA && ab->eou == EOU_E && ab->n < 2) return 0;

    if ((a == ABA_HOLD_AT || a == ABA_HOLD_AP) && ab->tempo >= tempo_quantos()) return 0;

    /* DELAY: OFF ou um indice do catalogo. Na SEQUENCIA, o DELAY e obrigatorio (escolha
     * explicita; OFF e uma escolha valida, mas precisa ser dita). */
    if (ab->delay == ALFA_DELAY_NAO_ESCOLHIDO) {
        if (a == ABA_SEQUENCIA) return 0;                   /* obrigatorio: precisa ser escolhido */
    } else if (ab->delay != ALFA_DELAY_OFF && ab->delay >= tempo_quantos()) {
        return 0;
    }

    for (i = 0; i < ab->n; i++) {
        if (!fonte_valida(&ab->fontes[i])) return 0;
        if (a == ABA_SEQUENCIA && i + 1 < ab->n && ab->espera[i] >= tempo_quantos()) return 0;
        /* repetir fonte e sem sentido em SEQUENCIA e em E */
        if (a == ABA_SEQUENCIA || ab->eou == EOU_E) {
            for (j = 0; j < i; j++) {
                const Fonte *g = &ab->fontes[j], *f = &ab->fontes[i];
                if (g->tipo == f->tipo && g->id == f->id) return 0;
            }
        }
    }
    return 1;
}

int alfa_validar(const AlfaCfg *c) {
    unsigned a, total = 0;
    if (c == 0) return -1;
    if (c->janela >= tempo_quantos()) return -1;
    for (a = 0; a < ALFA_ABAS; a++) {
        if (!aba_valida(&c->abas[a], a)) return -1;
        total += c->abas[a].n;
    }
    if (total == 0) return -1;
    return 0;
}

/* ----------------------------------------------------------- execucao */

void alfa_iniciar(AlfaEstado *e) {
    memset(e, 0, sizeof *e);
}

static uint64_t ms_us(uint16_t ms) { return (uint64_t)ms * 1000u; }

/* Janela do E. Devolve 1 quando o conjunto fecha neste quadro (ou, se `exige_nivel`,
 * enquanto ele continua fechado). `ev` sao as entradas com evento neste quadro;
 * `nivel_m` sao as entradas apertadas agora. Com `pulso`, o conjunto e zerado ao fechar. */
static int janela_E(AlfaEstado *e, unsigned a, uint32_t ev, uint32_t nivel_m,
                    int exige_nivel, int pulso, uint64_t now, uint64_t janela_us,
                    uint32_t todas) {
    int fecha = 0;
    if (e->win_aberta[a] && e->win_ja[a] != todas && now - e->win_t[a] > janela_us) {
        e->win_aberta[a] = 0;
        e->win_ja[a] = 0;
    }
    if (!e->win_aberta[a] && ev) {
        e->win_aberta[a] = 1;
        e->win_t[a] = now;
        e->win_ja[a] = 0;
    }
    if (e->win_aberta[a]) e->win_ja[a] |= ev;
    if (e->win_aberta[a] && exige_nivel && (e->win_ja[a] & ~nivel_m)) {
        e->win_aberta[a] = 0;
        e->win_ja[a] = 0;
    }
    if (e->win_aberta[a] && e->win_ja[a] == todas &&
        (!exige_nivel || nivel_m == todas)) {
        fecha = 1;
    }
    if (fecha && pulso) {
        e->win_aberta[a] = 0;
        e->win_ja[a] = 0;
    }
    return fecha;
}

void alfa_avaliar(const AlfaCfg *c, AlfaEstado *e, const Entrada *in, AlfaSaida *out) {
    uint8_t nivel[ALFA_TOTAL], borda[ALFA_TOTAL], solta[ALFA_TOTAL];
    uint32_t nivel_m[ALFA_ABAS], borda_m[ALFA_ABAS], solta_m[ALFA_ABAS];
    unsigned a, k;
    uint16_t janela_ms = 0;
    uint64_t janela_us;

    memset(out, 0, sizeof *out);
    tempo_ms(c->janela, &janela_ms);
    janela_us = ms_us(janela_ms);

    /* 1. nivel, borda e soltura de cada entrada; supressao das abas OCULTAS */
    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &c->abas[a];
        nivel_m[a] = borda_m[a] = solta_m[a] = 0;
        for (k = 0; k < ab->n; k++) {
            unsigned g = G(a, k);
            const Fonte *f = &ab->fontes[k];
            nivel[g] = (uint8_t)nivel_fonte(f, in);
            borda[g] = (uint8_t)(nivel[g] && !e->nivel_ant[g]);
            solta[g] = (uint8_t)(!nivel[g] && e->nivel_ant[g]);
            e->nivel_ant[g] = nivel[g];
            if (borda[g]) e->t_borda[g] = in->t_us;
            if (nivel[g]) {
                int v = intensidade_fonte(f, in);
                nivel_m[a] |= 1u << k;
                if (v > out->intens[a]) out->intens[a] = (uint8_t)v;
            }
            if (borda[g]) borda_m[a] |= 1u << k;
            if (solta[g]) solta_m[a] |= 1u << k;
            if (ab->oculto) {
                if (f->tipo == FONTE_BOTAO) out->ocultar_botoes |= 1u << f->id;
                else if (f->tipo == FONTE_EIXO) out->ocultar_analog |= (uint16_t)(1u << f->id);
                else if (f->tipo == FONTE_GATILHO) out->ocultar_analog |= (uint16_t)(1u << (EIXO_N + f->id));  /* L2, R2 */
                /* FONTE_DEDO: ocultar o toque ainda nao e suportado nesta fatia */
            }
        }
    }

    /* 2. cada aba */
    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &c->abas[a];
        uint32_t todas;
        if (ab->n == 0) continue;
        todas = (1u << ab->n) - 1u;

        switch (a) {

        case ABA_LIVRE:
            if (ab->eou == EOU_OU) {
                out->ativa[a] = (uint8_t)(nivel_m[a] != 0);
            } else {
                out->ativa[a] = (uint8_t)janela_E(e, a, borda_m[a], nivel_m[a], 1, 0,
                                                  in->t_us, janela_us, todas);
            }
            break;

        case ABA_SEQUENCIA: {
            /* a espera entre passos estourou: reinicia */
            if (e->seq_passo[a] > 0) {
                uint16_t ms = 0;
                tempo_ms(ab->espera[e->seq_passo[a] - 1], &ms);
                if (in->t_us - e->seq_t[a] > ms_us(ms)) {
                    e->seq_passo[a] = 0;
                    out->reiniciou[a] = 1;
                }
            }
            /* bordas, pela ordem das entradas */
            for (k = 0; k < ab->n; k++) {
                if (!(borda_m[a] & (1u << k))) continue;
                if (e->seq_passo[a] == k) {
                    e->seq_passo[a]++;
                    e->seq_t[a] = in->t_us;
                    if (e->seq_passo[a] == ab->n) {
                        out->ativa[a] = 1;               /* pulso ao concluir */
                        e->seq_passo[a] = 0;
                    }
                } else if (e->seq_passo[a] > 0) {
                    e->seq_passo[a] = 0;                 /* passo errado reinicia */
                    out->reiniciou[a] = 1;
                    break;
                }
            }
            break;
        }

        case ABA_2CLK:
            for (k = 0; k < ab->n; k++) {
                unsigned g = G(a, k);
                if (e->clk_pend[g] && in->t_us - e->clk_t1[g] > ms_us(ALFA_JANELA_2CLK_MS))
                    e->clk_pend[g] = 0;
                if (!(borda_m[a] & (1u << k))) continue;
                if (e->clk_pend[g]) {
                    out->ativa[a] = 1;                   /* segundo clique: pulso */
                    e->clk_pend[g] = 0;
                } else {
                    e->clk_pend[g] = 1;
                    e->clk_t1[g] = in->t_us;
                }
            }
            break;

        case ABA_HOLD_AP: {
            uint32_t ev = 0;
            uint64_t tempo_us = 0;
            uint16_t ms = 0;
            tempo_ms(ab->tempo, &ms);
            tempo_us = ms_us(ms);
            for (k = 0; k < ab->n; k++) {
                unsigned g = G(a, k);
                if (!nivel[g]) { e->disparou[g] = 0; continue; }
                if (!e->disparou[g] && in->t_us - e->t_borda[g] >= tempo_us) {
                    e->disparou[g] = 1;
                    ev |= 1u << k;
                }
            }
            if (ab->eou == EOU_OU) {
                out->ativa[a] = (uint8_t)(ev != 0);
            } else {
                out->ativa[a] = (uint8_t)janela_E(e, a, ev, nivel_m[a], 1, 1,
                                                  in->t_us, janela_us, todas);
            }
            break;
        }

        case ABA_HOLD_AT: {
            /* ATE: vale na soltura, e so se a pressao nao passou do tempo (toque curto) */
            uint32_t ev = 0;
            uint64_t tempo_us = 0;
            uint16_t ms = 0;
            tempo_ms(ab->tempo, &ms);
            tempo_us = ms_us(ms);
            for (k = 0; k < ab->n; k++) {
                unsigned g = G(a, k);
                if (solta[g] && in->t_us - e->t_borda[g] < tempo_us) ev |= 1u << k;
            }
            if (ab->eou == EOU_OU) {
                out->ativa[a] = (uint8_t)(ev != 0);
            } else {
                out->ativa[a] = (uint8_t)janela_E(e, a, ev, nivel_m[a], 0, 1,
                                                  in->t_us, janela_us, todas);
            }
            break;
        }

        case ABA_BLOCK:
            if (ab->eou == EOU_OU) {
                out->ativa[a] = (uint8_t)(nivel_m[a] != 0);
            } else {
                out->ativa[a] = (uint8_t)janela_E(e, a, borda_m[a], nivel_m[a], 1, 0,
                                                  in->t_us, janela_us, todas);
            }
            break;

        default:
            break;
        }

        /* 3. DELAY: a resposta da aba sai depois do atraso escolhido.
         *    - regra geral (pulso): conta depois que o pulso desce; novo pulso zera.
         *    - excecao (LIVRE e BLOCK, nivel): conta do acionamento; zera se cair antes.
         *    - OFF: a resposta e a propria ativa, sem atraso. */
        {
            uint8_t ativa = out->ativa[a];
            if (ab->delay == ALFA_DELAY_OFF || ab->delay == ALFA_DELAY_NAO_ESCOLHIDO) {
                out->disparo[a] = ativa;
            } else {
                uint16_t ms = 0;
                uint64_t dl_us;
                tempo_ms(ab->delay, &ms);
                dl_us = ms_us(ms);
                if (a == ABA_LIVRE || a == ABA_BLOCK) {
                    if (!ativa) {
                        e->dl_estado[a] = 0;                       /* soltou: zera */
                    } else if (e->dl_estado[a] == 0) {
                        e->dl_estado[a] = 1;                       /* subida: conta */
                        e->dl_t[a] = in->t_us;
                    }
                    if (e->dl_estado[a] == 1 && in->t_us - e->dl_t[a] >= dl_us) {
                        out->disparo[a] = 1;
                        e->dl_estado[a] = 2;                       /* um disparo por acionamento */
                    }
                } else {
                    if (ativa) {
                        e->dl_estado[a] = 0;                       /* pulso novo zera a contagem */
                    } else if (e->ant_ativa[a]) {
                        e->dl_estado[a] = 1;                       /* o pulso desceu: conta */
                        e->dl_t[a] = in->t_us;
                    }
                    if (e->dl_estado[a] == 1 && in->t_us - e->dl_t[a] >= dl_us) {
                        out->disparo[a] = 1;
                        e->dl_estado[a] = 0;
                    }
                }
            }
            e->ant_ativa[a] = ativa;
        }
    }
}
