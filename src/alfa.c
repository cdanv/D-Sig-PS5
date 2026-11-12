/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver alfa.h. Os tres modos de acionamento e a condicao BETA.
 *
 * REGRAS FIXADAS PELO AUTOR (docs/ARQUITETURA.md, secoes 12 a 14):
 *  - QUALQUER: basta uma fonte acionada. OU entre as fontes.
 *  - SIMULTANEO: todas as fontes acionadas ao mesmo tempo, dentro de uma janela medida
 *    a partir da primeira borda. E entre as fontes.
 *  - SEQUENCIA: as fontes em ordem. Cada passo e uma BORDA de subida da sua fonte.
 *    Espera estourada, passo errado e gatilho durante a sequencia reiniciam.
 *    O passo esperado tem prioridade sobre o gatilho, se forem o mesmo botao.
 *  - OCULTO suprime a fonte do jogo. Vale para o botao da fonte, o tempo todo.
 *
 * DECISAO DE IMPLEMENTACAO, NAO DO AUTOR: dentro de um mesmo quadro, varias bordas
 * sao processadas pela ordem dos indices da sequencia. Um botao apertado a mais de
 * um quadro de distancia e tratado pela ordem temporal dos quadros.
 */
#include "alfa.h"
#include "tempo.h"
#include <string.h>

/* ------------------------------------------------------------------ BETA */

/* Condicao BETA sobre um valor. `dir_vale` = 0 para o gatilho (sem direcao). */
static int beta_conta(const Beta *b, int32_t v, int dir_vale) {
    int32_t pct = (int32_t)(b->mult + 1) * 10;     /* x0.1 a x1.9 */
    int32_t w = v * pct / 100;
    int32_t m;
    int dir_ok = 1, zona, conta;

    if (w > 100) w = 100;
    if (w < -100) w = -100;
    m = w < 0 ? -w : w;

    if (dir_vale) {
        if (b->dir == DIR_NEG && !(w < 0)) dir_ok = 0;
        if (b->dir == DIR_POS && !(w > 0)) dir_ok = 0;
    }

    if (b->thr_out != 0 && b->thr_in > b->thr_out) {
        /* faixa invertida: a fonte conta FORA da faixa, nas duas pontas */
        zona = (m < (int32_t)b->thr_out) || (m > (int32_t)b->thr_in);
    } else {
        zona = (m >= (int32_t)b->thr_in) &&
               (b->thr_out == 0 || m <= (int32_t)b->thr_out);
    }
    conta = dir_ok && (b->inv ? !zona : zona);
    return conta ? 1 : 0;
}

static int nivel_fonte(const Fonte *f, const Entrada *in) {
    switch (f->tipo) {
    case FONTE_BOTAO:
        return (int)((in->botoes >> f->id) & 1u);
    case FONTE_EIXO:
        return beta_conta(&f->beta, in->eixo[f->id], 1);
    case FONTE_GATILHO:
        return beta_conta(&f->beta, in->gatilho[f->id], 0);
    default:
        return 0;
    }
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

int alfa_validar(const AlfaCfg *c) {
    unsigned i, j;
    if (c == 0) return -1;
    if (c->n == 0 || c->n > ALFA_MAX_FONTES) return -1;
    if (c->modo > MODO_SEQUENCIA) return -1;
    if (c->modo != MODO_QUALQUER && c->n < 2) return -1;   /* com uma fonte, e igual a QUALQUER */
    if (c->janela >= tempo_quantos()) return -1;

    for (i = 0; i < c->n; i++) {
        const Fonte *f = &c->fontes[i];
        if (f->tipo == FONTE_BOTAO) {
            if (f->id >= 32) return -1;
        } else if (f->tipo == FONTE_EIXO) {
            if (f->id > EIXO_RY) return -1;
            if (!beta_valida(&f->beta)) return -1;
        } else if (f->tipo == FONTE_GATILHO) {
            if (f->id > GATILHO_R2) return -1;
            if (!beta_valida(&f->beta)) return -1;
        } else {
            return -1;
        }
        if (f->oculto > 1) return -1;

        if (c->modo == MODO_SEQUENCIA && i + 1 < c->n && c->espera[i] >= tempo_quantos())
            return -1;

        /* fonte repetida nao faz sentido em SIMULTANEO nem em SEQUENCIA */
        if (c->modo != MODO_QUALQUER) {
            for (j = 0; j < i; j++) {
                const Fonte *g = &c->fontes[j];
                if (g->tipo == f->tipo && g->id == f->id) return -1;
            }
        }
    }
    return 0;
}

/* ----------------------------------------------------------- execucao */

void alfa_iniciar(AlfaEstado *e) {
    memset(e, 0, sizeof *e);
}

static uint64_t ms_para_us(uint16_t ms) {
    return (uint64_t)ms * 1000u;
}

void alfa_avaliar(const AlfaCfg *c, AlfaEstado *e, const Entrada *in, int reinicio,
                  AlfaSaida *out) {
    uint8_t nivel[ALFA_MAX_FONTES];
    uint8_t borda[ALFA_MAX_FONTES];
    unsigned i;

    memset(out, 0, sizeof *out);

    /* 1. nivel e borda de cada fonte, e a supressao das OCULTAS */
    for (i = 0; i < c->n; i++) {
        const Fonte *f = &c->fontes[i];
        nivel[i] = (uint8_t)nivel_fonte(f, in);
        borda[i] = (uint8_t)(nivel[i] && !e->nivel_ant[i]);
        e->nivel_ant[i] = nivel[i];
        if (f->oculto) {
            if (f->tipo == FONTE_BOTAO) out->ocultar_botoes |= 1u << f->id;
            else if (f->tipo == FONTE_EIXO) out->ocultar_analog |= (uint8_t)(1u << f->id);
            else out->ocultar_analog |= (uint8_t)(1u << (4 + f->id));
        }
    }

    /* 2. QUALQUER: OU entre as fontes */
    if (c->modo == MODO_QUALQUER) {
        for (i = 0; i < c->n; i++) if (nivel[i]) out->ativa = 1;
        return;
    }

    /* 3. SIMULTANEO: todas juntas, dentro de uma janela */
    if (c->modo == MODO_SIMULTANEO) {
        uint32_t todas = (c->n >= 32) ? 0xFFFFFFFFu : ((1u << c->n) - 1u);
        uint32_t nivel_mask = 0, borda_mask = 0;
        uint16_t janela_ms = 0;
        uint64_t janela_us;

        tempo_ms(c->janela, &janela_ms);
        janela_us = ms_para_us(janela_ms);

        for (i = 0; i < c->n; i++) {
            if (nivel[i]) nivel_mask |= 1u << i;
            if (borda[i]) borda_mask |= 1u << i;
        }

        /* a janela expira, a nao ser que o conjunto ja esteja completo */
        if (e->janela_aberta && e->ja_mask != todas &&
            in->t_us - e->t_janela > janela_us) {
            e->janela_aberta = 0;
            e->ja_mask = 0;
        }
        /* a primeira borda abre a janela */
        if (!e->janela_aberta && borda_mask) {
            e->janela_aberta = 1;
            e->t_janela = in->t_us;
            e->ja_mask = 0;
        }
        if (e->janela_aberta) e->ja_mask |= borda_mask;

        /* soltar uma fonte que ja entrou desmonta o conjunto */
        if (e->janela_aberta && (e->ja_mask & ~nivel_mask)) {
            e->janela_aberta = 0;
            e->ja_mask = 0;
        }

        if (e->janela_aberta && e->ja_mask == todas && nivel_mask == todas)
            out->ativa = 1;
        return;
    }

    /* 4. SEQUENCIA */
    /* 4a. a espera entre passos estourou: reinicia */
    if (e->passo > 0) {
        uint16_t ms = 0;
        tempo_ms(c->espera[e->passo - 1], &ms);
        if (in->t_us - e->t_passo > ms_para_us(ms)) {
            e->passo = 0;
            out->reiniciou = 1;
        }
    }

    /* 4b. gatilho durante a sequencia reinicia, salvo se o passo esperado tem borda
     *     neste quadro (prioridade ao passo esperado, decisao do autor) */
    if (reinicio && e->passo > 0) {
        int prioridade = (e->passo < c->n && borda[e->passo]);
        if (!prioridade) {
            e->passo = 0;
            out->reiniciou = 1;
        }
    }

    /* 4c. bordas das fontes, pela ordem dos indices */
    for (i = 0; i < c->n; i++) {
        if (!borda[i]) continue;
        if (e->passo == i) {
            e->passo++;
            e->t_passo = in->t_us;
            if (e->passo == c->n) {
                out->concluida = 1;
                out->ativa = 1;
                e->passo = 0;
            }
        } else if (e->passo > 0) {
            /* passo errado com sequencia em andamento: reinicia */
            e->passo = 0;
            out->reiniciou = 1;
            break;
        }
        /* passo errado sem sequencia em andamento: ignorado */
    }
}
