/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA ACUMULACAO: como varios MODs escrevem nos mesmos destinos. Roda no Linux.
 * Regras do manual, secao 26: digital escreve so quando liga (OU); uma saida desligada nao
 * forca zero; o destino vence a supressao, que vem antes.
 */
#include "../src/rede.h"
#include "../src/dominio.h"
#include "../src/pad.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define D_L1 0
#define D_CIRCLE 7
#define D_CROSS 6

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

static unsigned idx_ms(unsigned ms) {
    unsigned i, n = tempo_quantos();
    uint16_t x;
    for (i = 0; i < n; i++) if (tempo_ms(i, &x) == 0 && x == ms) return i;
    return 255u;
}

static void mod_base(ModCfg *m) {
    unsigned a;
    memset(m, 0, sizeof *m);
    m->alfa.janela = (uint8_t)idx_ms(50);
    for (a = 0; a < ALFA_ABAS; a++) m->alfa.abas[a].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    m->delta.lat = DELTA_SEM_LAT;
    m->delta.ciclos = 0;
    m->acao[ABA_LIVRE] = ACAO_LIGA;
}

/* MOD com fonte de botao (LIVRE), resposta DIRECT em um destino. `oculto` suprime o botao. */
static void mod_nivel(ModCfg *m, uint8_t botao, uint8_t dest, uint8_t oculto) {
    mod_base(m);
    m->alfa.abas[ABA_LIVRE].n = 1;
    m->alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_BOTAO;
    m->alfa.abas[ABA_LIVRE].fontes[0].id = botao;
    m->alfa.abas[ABA_LIVRE].oculto = oculto;
    m->alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m->delta.tipo = DELTA_DIRECT;
    m->delta.n_dest = 1;
    m->delta.dest[0] = dest;
}

typedef uint32_t (*Cron)(uint64_t t_ms);

/* botoes de saida da rede no instante t, com o D1 ligado (sem passar pela troca) */
static uint32_t saida_em(const RedeCfg *r, Cron cr, uint64_t fim_ms, uint64_t t_ms) {
    RedeEstado e; RedeSaida s; uint64_t t; uint32_t v = 0xFFFFFFFFu;
    rede_iniciar(&e);
    e.dom.atual = 1;
    for (t = 0; t <= fim_ms; t += 10) {
        Entrada in;
        memset(&in, 0, sizeof in);
        in.botoes = cr(t);
        in.t_us = t * 1000u;
        rede_avaliar(r, &e, &in, &s);
        if (t == t_ms) v = s.botoes;
    }
    return v;
}

/* ------------------------------------------------- OU entre MODs no mesmo destino */
static uint32_t cr_ou(uint64_t t) {
    uint32_t b = 0;
    if (t < 100) b |= BIT(14);                 /* CROSS de 0 a 100 */
    if (t >= 200 && t < 300) b |= BIT(15);               /* SQUARE de 200 a 300 */
    return b;
}
static void testes_ou(void) {
    RedeCfg r; int ok = 1; uint32_t v;
    memset(&r, 0, sizeof r);
    r.n = 2;
    mod_nivel(&r.mod[0], 14, D_CIRCLE, 0);               /* CROSS -> CIRCLE */
    mod_nivel(&r.mod[1], 15, D_CIRCLE, 0);               /* SQUARE -> CIRCLE */
    r.dom[0] = r.dom[1] = 0x1;
    if (rede_validar(&r) != 0) ok = 0;
    v = saida_em(&r, cr_ou, 400, 50);
    if (!(v & DS_BTN_CIRCLE)) ok = 0;                    /* so o MOD 0 ligado */
    v = saida_em(&r, cr_ou, 400, 250);
    if (!(v & DS_BTN_CIRCLE)) ok = 0;                    /* so o MOD 1 ligado */
    v = saida_em(&r, cr_ou, 400, 150);
    if (v & DS_BTN_CIRCLE) ok = 0;                       /* nenhum ligado: CIRCLE nao sai */
    passo("acumulacao: dois MODs no mesmo destino digital funcionam como OU", ok,
          "CIRCLE com CROSS; com SQUARE; sem nenhum");
}

/* ----------------------------------- saida desligada nao forca zero */
/* o jogador aperta CIRCLE de 0 a 100 e o MOD que escreve CIRCLE esta desligado (sem CROSS) */
static uint32_t cr_cru(uint64_t t) {
    return (t < 100) ? BIT(13) : 0;                      /* CIRCLE cru */
}
static void testes_cru(void) {
    RedeCfg r; int ok = 1; uint32_t v;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], 14, D_CIRCLE, 0);
    r.dom[0] = 0x1;
    if (rede_validar(&r) != 0) ok = 0;
    v = saida_em(&r, cr_cru, 200, 50);
    if (!(v & DS_BTN_CIRCLE)) ok = 0;                    /* o sinal cru do jogador passa */
    passo("acumulacao: saida desligada nao forca zero; o sinal cru do jogador passa", ok, "");
}

/* ---------------------------------- destino vence a supressao de outro MOD */
/* MOD 0: CROSS, com fonte L1 OCULTO (suprime L1 e responde CROSS enquanto L1 aperta).
 * MOD 1: SQUARE, escreve L1 como destino. */
static uint32_t cr_supressao(uint64_t t) {
    uint32_t b = 0;
    if (t < 100) b |= DS_BTN_L1;               /* L1 sozinho */
    if (t >= 200 && t < 300) b |= DS_BTN_L1 | BIT(15);   /* L1 e SQUARE */
    return b;
}
static void testes_supressao(void) {
    RedeCfg r; int ok = 1; uint32_t v;
    memset(&r, 0, sizeof r);
    r.n = 2;
    mod_nivel(&r.mod[0], 10, D_CROSS, 1);                /* fonte L1, OCULTO */
    mod_nivel(&r.mod[1], 15, D_L1, 0);                   /* SQUARE escreve L1 */
    r.dom[0] = r.dom[1] = 0x1;
    if (rede_validar(&r) != 0) ok = 0;
    v = saida_em(&r, cr_supressao, 400, 50);
    if (v & DS_BTN_L1) ok = 0;                           /* suprimido: nao vaza */
    if (!(v & DS_BTN_CROSS)) ok = 0;                     /* o MOD 0 responde */
    v = saida_em(&r, cr_supressao, 400, 250);
    if (!(v & DS_BTN_L1)) ok = 0;                        /* o destino vence a supressao */
    passo("acumulacao: o destino vence a supressao de outro MOD", ok,
          "L1 sozinho: suprimido; L1 com SQUARE: o destino de SQUARE passa");
}

/* ---------------------------- MOD fora do dominio nao suprime nada */
static void testes_fora_do_dominio(void) {
    RedeCfg r; int ok = 1; uint32_t v;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], 10, D_CROSS, 1);                /* oculto L1 */
    r.dom[0] = 0x2;                                      /* so em D2; estamos em D1 */
    if (rede_validar(&r) != 0) ok = 0;
    v = saida_em(&r, cr_supressao, 400, 50);
    if (!(v & DS_BTN_L1)) ok = 0;                        /* fora do dominio: L1 passa */
    passo("acumulacao: MOD fora do dominio nao suprime o botao", ok, "");
}

int main(void) {
    testes_ou();
    testes_cru();
    testes_supressao();
    testes_fora_do_dominio();
    if (falhas) { printf("  => BANCADA DA ACUMULACAO FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada da acumulacao aberta: todas as verificacoes passaram\n");
    return 0;
}
