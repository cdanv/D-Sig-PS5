/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA ALFA v4. Roda no Linux, sem console.
 * Resultados esperados escritos a mao, a partir das regras do autor (secao 18.8).
 */
#include "../src/alfa.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define B_L1 10
#define B_R1 11
#define B_CROSS 14
#define B_CIRCLE 13
#define B_SQUARE 15

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

static Entrada ent(uint32_t b, uint64_t t_ms) {
    Entrada e;
    memset(&e, 0, sizeof e);
    e.botoes = b;
    e.t_us = t_ms * 1000u;
    return e;
}

static Fonte botao(uint8_t bit) {
    Fonte f;
    memset(&f, 0, sizeof f);
    f.tipo = FONTE_BOTAO;
    f.id = bit;
    return f;
}

static void cfg_vazia(AlfaCfg *c) {
    unsigned a;
    memset(c, 0, sizeof *c);
    c->janela = (uint8_t)idx_ms(50);
    /* DELAY nao escolhido por padrao: as abas tratam como OFF; a SEQUENCIA exige escolha */
    for (a = 0; a < ALFA_ABAS; a++) c->abas[a].delay = ALFA_DELAY_NAO_ESCOLHIDO;
}

/* ---------------------------------------------------------- validacao */
static void testes_validacao(void) {
    AlfaCfg c;
    int ok = 1;

    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 1; c.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    if (alfa_validar(&c) != 0) ok = 0;

    { AlfaCfg x; cfg_vazia(&x); if (alfa_validar(&x) != -1) ok = 0; }            /* sem entradas */
    { AlfaCfg x = c; x.abas[ABA_LIVRE].n = 6; if (alfa_validar(&x) != -1) ok = 0; } /* mais de 5 */
    { AlfaCfg x = c; x.abas[ABA_LIVRE].eou = 2; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_2CLK].n = 2; x.abas[ABA_2CLK].eou = EOU_E;
      x.abas[ABA_2CLK].fontes[0] = botao(B_L1); x.abas[ABA_2CLK].fontes[1] = botao(B_R1);
      if (alfa_validar(&x) != -1) ok = 0; }                                       /* 2CLK nao aceita E */
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_SEQUENCIA].n = 1; x.abas[ABA_SEQUENCIA].fontes[0] = botao(B_L1);
      if (alfa_validar(&x) != -1) ok = 0; }                                       /* sequencia com 1 passo */
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_HOLD_AP].n = 1; x.abas[ABA_HOLD_AP].fontes[0] = botao(B_CROSS);
      x.abas[ABA_HOLD_AP].tempo = (uint8_t)tempo_quantos();
      if (alfa_validar(&x) != -1) ok = 0; }                                       /* tempo fora do catalogo */
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_SEQUENCIA].n = 2; x.abas[ABA_SEQUENCIA].fontes[0] = botao(B_L1);
      x.abas[ABA_SEQUENCIA].fontes[1] = botao(B_L1); if (alfa_validar(&x) != -1) ok = 0; } /* repetida */
    { AlfaCfg x = c; x.janela = (uint8_t)tempo_quantos(); if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x; memset(&x, 0, sizeof x); x.abas[ABA_LIVRE].n = 1;
      x.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO; x.abas[ABA_LIVRE].fontes[0].id = EIXO_LX;
      x.abas[ABA_LIVRE].fontes[0].beta.thr_in = 7; x.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_ABS;
      if (alfa_validar(&x) != -1) ok = 0; }                                       /* piso nao multiplo de 5 */
    { AlfaCfg x; memset(&x, 0, sizeof x); x.abas[ABA_LIVRE].n = 1;
      x.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO; x.abas[ABA_LIVRE].fontes[0].id = EIXO_LX;
      x.abas[ABA_LIVRE].fontes[0].beta.mult = 9; x.abas[ABA_LIVRE].fontes[0].beta.dir = 7;
      if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_LIVRE].n = 1; x.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
      x.abas[ABA_LIVRE].fontes[0].tipo = 9; if (alfa_validar(&x) != -1) ok = 0; }
    /* 6 abas x 5 entradas = 30, o teto total */
    { AlfaCfg x; unsigned a, k; cfg_vazia(&x);
      for (a = 0; a < ALFA_ABAS; a++) {
          x.abas[a].n = 5;
          if (a == ABA_SEQUENCIA) { x.abas[a].eou = EOU_OU; x.abas[a].delay = ALFA_DELAY_OFF; }
          for (k = 0; k < 5; k++) x.abas[a].fontes[k] = botao((uint8_t)k);
      }
      if (alfa_validar(&x) != 0) ok = 0; }
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_SEQUENCIA].n = 2; x.abas[ABA_SEQUENCIA].fontes[0] = botao(B_L1);
      x.abas[ABA_SEQUENCIA].fontes[1] = botao(B_CROSS);
      if (alfa_validar(&x) != -1) ok = 0;                                         /* DELAY nao escolhido */
      x.abas[ABA_SEQUENCIA].delay = ALFA_DELAY_OFF;
      if (alfa_validar(&x) != 0) ok = 0; }                                        /* OFF e escolha valida */
    { AlfaCfg x; cfg_vazia(&x); x.abas[ABA_LIVRE].n = 1; x.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
      x.abas[ABA_LIVRE].delay = (uint8_t)tempo_quantos();
      if (alfa_validar(&x) != -1) ok = 0; }                                       /* DELAY fora do catalogo */
    if (alfa_validar(0) != -1) ok = 0;
    passo("validacao: regras de cada aba, teto de 5 por aba, teto de 30 no total, DELAY", ok, "");
}

/* ----------------------------------------------------------- BETA */
static void testes_beta(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    Entrada in;
    int ok = 1;

    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 1;
    c.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO;
    c.abas[ABA_LIVRE].fontes[0].id = EIXO_LX;
    c.abas[ABA_LIVRE].fontes[0].beta.mult = 9;
    c.abas[ABA_LIVRE].fontes[0].beta.thr_in = 20;
    c.abas[ABA_LIVRE].fontes[0].beta.thr_out = 60;
    c.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_ABS;

    {
        int casos[][2] = {{-30, 1}, {-70, 0}, {-10, 0}, {10, 0}, {50, 1}, {0, 0}};
        size_t k;
        for (k = 0; k < 6; k++) {
            memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = casos[k][0];
            alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
            if (s.ativa[ABA_LIVRE] != (uint8_t)casos[k][1]) ok = 0;
        }
    }
    passo("BETA: zona [20,60], DIR ABS, 6 valores escritos a mao", ok, "");

    c.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_NEG;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 30; alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 0) ok = 0;
    passo("BETA: DIR NEG nao conta positivo", ok, "");

    c.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_ABS;
    c.abas[ABA_LIVRE].fontes[0].beta.inv = 1;
    ok = 1;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -50; alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 0) ok = 0;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -70; alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 1) ok = 0;
    passo("BETA: INV inverte a zona", ok, "");

    c.abas[ABA_LIVRE].fontes[0].beta.inv = 0;
    c.abas[ABA_LIVRE].fontes[0].beta.mult = 18;
    ok = 1;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -30; alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 1) ok = 0;                       /* -57, dentro de [20,60] */
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -100; alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 0) ok = 0;                       /* -190 limitado a -100, fora */
    passo("BETA: MULT escala antes da zona, com limite de +-100", ok, "");
}

/* ------------------------------------------------------ LIVRE: OU e E */
static void testes_livre(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    unsigned m;
    int ok = 1;

    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 3; c.abas[ABA_LIVRE].eou = EOU_OU;
    c.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    c.abas[ABA_LIVRE].fontes[1] = botao(B_SQUARE);
    c.abas[ABA_LIVRE].fontes[2] = botao(B_CIRCLE);
    for (m = 0; m < 8; m++) {
        uint32_t b = 0;
        if (m & 1) b |= BIT(B_CROSS);
        if (m & 2) b |= BIT(B_SQUARE);
        if (m & 4) b |= BIT(B_CIRCLE);
        alfa_iniciar(&e);
        alfa_avaliar(&c, &e, &(Entrada){.botoes = b, .t_us = 1000}, &s);
        if (s.ativa[ABA_LIVRE] != (uint8_t)(m != 0)) ok = 0;
    }
    passo("LIVRE em OU: basta uma, nas 8 combinacoes", ok, "");

    c.abas[ABA_LIVRE].eou = EOU_E;
    alfa_iniciar(&e); ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_SQUARE), .t_us = 10000}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_SQUARE)|BIT(B_CIRCLE), .t_us = 20000}, &s);
    if (!s.ativa[ABA_LIVRE]) ok = 0;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_SQUARE)|BIT(B_CIRCLE), .t_us = 30000}, &s);
    if (s.ativa[ABA_LIVRE]) ok = 0;
    passo("LIVRE em E: tres botoes dentro de 50 ms ativam; soltar um desliga", ok, "");

    alfa_iniciar(&e); ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_SQUARE)|BIT(B_CIRCLE), .t_us = 80000}, &s);
    if (s.ativa[ABA_LIVRE]) ok = 0;
    passo("LIVRE em E: fora da janela de 50 ms, nao ativa", ok, "");
}

/* ----------------------------------------------------- SEQUENCIA */
typedef struct { uint32_t b; uint64_t t; } Quadro;

static void rodar(const AlfaCfg *c, AlfaEstado *e, const Quadro *q, size_t n,
                  int *concluiu, size_t *fc, int *reiniciou, size_t *fr) {
    size_t i;
    AlfaSaida s;
    *concluiu = 0; *reiniciou = 0; *fc = (size_t)-1; *fr = (size_t)-1;
    for (i = 0; i < n; i++) {
        Entrada in = ent(q[i].b, q[i].t);
        alfa_avaliar(c, e, &in, &s);
        if (s.ativa[ABA_SEQUENCIA]) { *fc = i; *concluiu = 1; }
        if (s.reiniciou[ABA_SEQUENCIA]) { if (!*reiniciou) *fr = i; *reiniciou = 1; }
    }
}

static void testes_sequencia(void) {
    AlfaCfg c;
    AlfaEstado e;
    int concluiu, reiniciou;
    size_t fc, fr;

    cfg_vazia(&c);
    c.abas[ABA_SEQUENCIA].n = 3;
    c.abas[ABA_SEQUENCIA].fontes[0] = botao(B_CROSS);
    c.abas[ABA_SEQUENCIA].fontes[1] = botao(B_CIRCLE);
    c.abas[ABA_SEQUENCIA].fontes[2] = botao(B_SQUARE);
    c.abas[ABA_SEQUENCIA].espera[0] = (uint8_t)idx_ms(200);
    c.abas[ABA_SEQUENCIA].espera[1] = (uint8_t)idx_ms(200);
    c.abas[ABA_SEQUENCIA].delay = ALFA_DELAY_OFF;
    if (alfa_validar(&c) != 0) { passo("SEQUENCIA: configuracao valida", 0, ""); return; }

    {
        Quadro q[] = {{0,0},{BIT(B_CROSS),100},{0,150},{BIT(B_CIRCLE),200},{0,250},{BIT(B_SQUARE),300}};
        alfa_iniciar(&e); rodar(&c, &e, q, 6, &concluiu, &fc, &reiniciou, &fr);
        passo("SEQUENCIA: ordem certa dentro dos prazos conclui", concluiu && fc == 5 && !reiniciou, "");
    }
    {
        Quadro q[] = {{BIT(B_CROSS),100},{0,150},{BIT(B_CIRCLE),400},{0,450},{BIT(B_SQUARE),500}};
        alfa_iniciar(&e); rodar(&c, &e, q, 5, &concluiu, &fc, &reiniciou, &fr);
        passo("SEQUENCIA: espera estourada reinicia e nao conclui", reiniciou && fr == 2 && !concluiu, "");
    }
    {
        Quadro q[] = {{BIT(B_CROSS),100},{0,150},{BIT(B_SQUARE),200},{0,250},
                      {BIT(B_CIRCLE),300},{0,350},{BIT(B_SQUARE),400}};
        alfa_iniciar(&e); rodar(&c, &e, q, 7, &concluiu, &fc, &reiniciou, &fr);
        passo("SEQUENCIA: passo errado reinicia", reiniciou && fr == 2 && !concluiu, "");
    }
    {
        Quadro q[] = {{BIT(B_CROSS),100},{0,150},{BIT(B_CROSS),120},{0,130}};
        alfa_iniciar(&e); rodar(&c, &e, q, 4, &concluiu, &fc, &reiniciou, &fr);
        passo("SEQUENCIA: segurar um botao conta uma vez, pela borda", !concluiu, "");
    }
    {
        Quadro q[] = {{BIT(B_CROSS),100},{0,150},{BIT(B_CIRCLE),200},{0,250},{BIT(B_SQUARE),300},
                      {0,350},{BIT(B_CROSS),400},{0,450},{BIT(B_CIRCLE),500},{0,550},{BIT(B_SQUARE),600}};
        alfa_iniciar(&e); rodar(&c, &e, q, 11, &concluiu, &fc, &reiniciou, &fr);
        passo("SEQUENCIA: apos concluir, a proxima comeca do passo 1", concluiu && fc == 10, "");
    }
}

/* ------------------------------------------------------------- 2CLK */
static void testes_2clk(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok = 1, pulsos;
    size_t i;

    cfg_vazia(&c);
    c.abas[ABA_2CLK].n = 2; c.abas[ABA_2CLK].eou = EOU_OU;
    c.abas[ABA_2CLK].fontes[0] = botao(B_L1);
    c.abas[ABA_2CLK].fontes[1] = botao(B_R1);

    /* o exemplo do autor: L1 e R1, cada um com o seu duplo clique, contam pela acao dupla
     * de cada botao. L1 em 0 e 200 ms; R1 em 100 e 300 ms: dois pulsos, cada um do seu botao. */
    {
        Quadro q[] = {{BIT(B_L1),0},{0,30},{BIT(B_R1),100},{0,130},{BIT(B_L1),200},{0,230},{BIT(B_R1),300},{0,330}};
        /* confere os INSTANTES dos pulsos, nao so a contagem: esperados em 200 ms e 300 ms */
        uint64_t instantes[8]; int n_p = 0;
        alfa_iniciar(&e);
        for (i = 0; i < 8; i++) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = q[i].b, .t_us = q[i].t * 1000u}, &s);
            if (s.ativa[ABA_2CLK]) { if (n_p < 8) instantes[n_p] = q[i].t; n_p++; }
        }
        pulsos = (n_p == 2 && instantes[0] == 200 && instantes[1] == 300);
        passo("2CLK em OU: L1 e R1, cada um com duplo clique, contam pela acao de cada botao",
              pulsos, "pulsos exatos em 200 ms (L1) e 300 ms (R1)");
    }
    /* segundo clique fora de 300 ms nao conta */
    {
        ok = 1; pulsos = 0;
        alfa_iniciar(&e);
        alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_L1), .t_us = 0}, &s);
        alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 50000}, &s);
        alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_L1), .t_us = 400000}, &s);
        if (s.ativa[ABA_2CLK]) ok = 0;
        passo("2CLK: segundo clique depois de 300 ms nao conta", ok, "");
    }
}

/* ---------------------------------------------------------- HOLD */
static void testes_hold(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok = 1, pulsos;

    /* HOLD_AP: pulso quando a pressao passa de 200 ms, ainda apertado, uma vez por pressao */
    cfg_vazia(&c);
    c.abas[ABA_HOLD_AP].n = 1; c.abas[ABA_HOLD_AP].fontes[0] = botao(B_CROSS);
    c.abas[ABA_HOLD_AP].tempo = (uint8_t)idx_ms(200);
    alfa_iniciar(&e); pulsos = 0;
    {
        uint64_t t;
        for (t = 0; t <= 400; t += 50) {
            uint32_t b = (t <= 400) ? BIT(B_CROSS) : 0;
            alfa_avaliar(&c, &e, &(Entrada){.botoes = b, .t_us = t * 1000u}, &s);
            if (s.ativa[ABA_HOLD_AP]) { pulsos++; if (t != 200) ok = 0; }
        }
    }
    passo("HOLD_AP: pulsa uma vez, em 200 ms, enquanto segura", ok && pulsos == 1, "");

    /* HOLD_AT: pulso na soltura, so se a pressao nao passou de 200 ms (toque curto) */
    cfg_vazia(&c);
    c.abas[ABA_HOLD_AT].n = 1; c.abas[ABA_HOLD_AT].fontes[0] = botao(B_CROSS);
    c.abas[ABA_HOLD_AT].tempo = (uint8_t)idx_ms(200);
    ok = 1; alfa_iniciar(&e);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 150000}, &s);
    if (!s.ativa[ABA_HOLD_AT]) ok = 0;                           /* toque curto: pulso na soltura */
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 300000}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 600000}, &s);
    if (s.ativa[ABA_HOLD_AT]) ok = 0;                            /* segurou alem do tempo: nada */
    passo("HOLD_AT: toque curto dispara na soltura; segurar alem do tempo nao", ok, "");
}

/* ---------------------------------------------------------- BLOCK */
static void testes_block(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok = 1;

    cfg_vazia(&c);
    c.abas[ABA_BLOCK].n = 2; c.abas[ABA_BLOCK].eou = EOU_OU;
    c.abas[ABA_BLOCK].fontes[0] = botao(B_CROSS);
    c.abas[ABA_BLOCK].fontes[1] = botao(B_SQUARE);
    alfa_iniciar(&e);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_SQUARE), .t_us = 0}, &s);
    if (!s.ativa[ABA_BLOCK]) ok = 0;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 10000}, &s);
    if (s.ativa[ABA_BLOCK]) ok = 0;
    passo("BLOCK em OU: suspende enquanto qualquer um estiver apertado", ok, "");
}

/* ------------------------------------------------------ OCULTO */
static void testes_oculto(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;

    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 1; c.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    c.abas[ABA_LIVRE].oculto = 1;
    alfa_iniciar(&e);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 0}, &s);
    passo("OCULTO: a aba suprime seus botoes o tempo todo",
          (s.ocultar_botoes & BIT(B_CROSS)) != 0, "");
}

/* ------------------------------------------------------------ DELAY */
static void testes_delay(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok;
    uint64_t t;

    /* LIVRE com OFF: o disparo e o proprio nivel */
    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 1; c.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    c.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    ok = 1; alfa_iniciar(&e);
    for (t = 0; t <= 200; t += 100) {
        uint32_t b = (t < 200) ? BIT(B_CROSS) : 0;
        alfa_avaliar(&c, &e, &(Entrada){.botoes = b, .t_us = t * 1000u}, &s);
        if (s.disparo[ABA_LIVRE] != s.ativa[ABA_LIVRE]) ok = 0;
    }
    passo("DELAY OFF: o disparo e o proprio nivel, no mesmo quadro", ok, "");

    /* LIVRE com 300 ms: conta do acionamento; segurando, dispara uma vez em 300 ms */
    c.abas[ABA_LIVRE].delay = (uint8_t)idx_ms(300);
    ok = 1; alfa_iniciar(&e);
    {
        int n = 0; uint64_t quando = 0;
        for (t = 0; t <= 400; t += 100) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = t * 1000u}, &s);
            if (s.disparo[ABA_LIVRE]) { n++; quando = t; }
        }
        if (n != 1 || quando != 300) ok = 0;
    }
    passo("DELAY em LIVRE: segurando, dispara uma vez, em 300 ms", ok, "");

    /* LIVRE com 300 ms: soltar antes do fim zera; nada dispara */
    ok = 1; alfa_iniciar(&e);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 100000}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 150000}, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 400000}, &s);
    if (s.disparo[ABA_LIVRE]) ok = 0;
    passo("DELAY em LIVRE: soltar antes do fim zera a contagem", ok, "");

    /* 2CLK com 200 ms: conta depois que o pulso desce. Pulso em 50 ms, descida em 80 ms,
     * disparo em 280 ms */
    cfg_vazia(&c);
    c.abas[ABA_2CLK].n = 1; c.abas[ABA_2CLK].fontes[0] = botao(B_CROSS);
    c.abas[ABA_2CLK].delay = (uint8_t)idx_ms(200);
    ok = 1; alfa_iniciar(&e);
    {
        Quadro q[] = {{BIT(B_CROSS),0},{0,20},{BIT(B_CROSS),50},{0,80},{0,200},{0,280},{0,300}};
        size_t i; int n = 0; uint64_t quando = 0;
        for (i = 0; i < 7; i++) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = q[i].b, .t_us = q[i].t * 1000u}, &s);
            if (s.disparo[ABA_2CLK]) { n++; quando = q[i].t; }
        }
        if (n != 1 || quando != 280) ok = 0;
    }
    passo("DELAY em 2CLK: conta depois que o pulso desce; dispara em 280 ms", ok, "");

    /* 2CLK: um novo pulso durante o atraso zera a contagem */
    ok = 1; alfa_iniciar(&e);
    {
        Quadro q[] = {{BIT(B_CROSS),0},{0,20},{BIT(B_CROSS),50},{0,80},
                      {BIT(B_CROSS),150},{0,160},{BIT(B_CROSS),180},{0,210},{0,300},{0,410},{0,420}};
        size_t i; int n = 0; uint64_t quando = 0;
        for (i = 0; i < 11; i++) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = q[i].b, .t_us = q[i].t * 1000u}, &s);
            if (s.disparo[ABA_2CLK]) { n++; quando = q[i].t; }
        }
        if (n != 1 || quando != 410) ok = 0;
    }
    passo("DELAY em 2CLK: novo pulso durante o atraso zera; so dispara 200 ms depois do novo pulso",
          ok, "disparo esperado em 410 ms, nao em 280 ms");

    /* 2CLK: o atraso antigo expira ANTES do novo pulso descer. So o zeramento impede o
     * disparo no instante 280 ms; o correto e so disparar 200 ms depois do novo pulso. */
    cfg_vazia(&c);
    c.abas[ABA_2CLK].n = 1; c.abas[ABA_2CLK].fontes[0] = botao(B_CROSS);
    c.abas[ABA_2CLK].delay = (uint8_t)idx_ms(200);
    ok = 1; alfa_iniciar(&e);
    {
        /* o novo pulso sobe em 280 ms, exatamente quando o atraso antigo venceria */
        Quadro q[] = {{BIT(B_CROSS),0},{0,20},{BIT(B_CROSS),50},{0,80},
                      {BIT(B_CROSS),260},{0,270},{BIT(B_CROSS),280},{0,300},{0,480},{0,500}};
        size_t i; int n = 0; uint64_t quando = 0;
        for (i = 0; i < 10; i++) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = q[i].b, .t_us = q[i].t * 1000u}, &s);
            if (s.disparo[ABA_2CLK]) { n++; quando = q[i].t; }
        }
        if (n != 1 || quando != 500) ok = 0;
    }
    passo("DELAY em 2CLK: novo pulso no instante do vencimento zera; dispara em 500 ms",
          ok, "o zeramento e o que impede o disparo em 280 ms");

    /* SEQUENCIA sem DELAY escolhido e recusada; com OFF, a resposta sai na conclusao */
    cfg_vazia(&c);
    c.abas[ABA_SEQUENCIA].n = 2;
    c.abas[ABA_SEQUENCIA].fontes[0] = botao(B_CROSS);
    c.abas[ABA_SEQUENCIA].fontes[1] = botao(B_CIRCLE);
    c.abas[ABA_SEQUENCIA].espera[0] = (uint8_t)idx_ms(200);
    ok = (alfa_validar(&c) == -1);
    c.abas[ABA_SEQUENCIA].delay = ALFA_DELAY_OFF;
    ok = ok && (alfa_validar(&c) == 0);
    alfa_iniciar(&e);
    {
        Quadro q[] = {{BIT(B_CROSS),100},{0,150},{BIT(B_CIRCLE),200},{0,250}};
        size_t i; int n = 0;
        for (i = 0; i < 4; i++) {
            alfa_avaliar(&c, &e, &(Entrada){.botoes = q[i].b, .t_us = q[i].t * 1000u}, &s);
            if (s.disparo[ABA_SEQUENCIA]) n++;
        }
        if (n != 1) ok = 0;
    }
    passo("DELAY na SEQUENCIA: obrigatorio escolher; OFF dispara na conclusao", ok, "");
}

/* ---------------------------------------------- sensores de movimento (PS5) */
static Beta beta_padrao(void) {
    Beta b;
    b.mult = 9; b.thr_in = 0; b.thr_out = 0; b.dir = DIR_ABS; b.inv = 0;
    return b;
}

static void testes_sensores(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    Entrada in;
    int ok = 1;

    /* giroscopio X como fonte: zona [20,60], DIR ABS, x1.0 */
    memset(&c, 0, sizeof c);
    c.janela = (uint8_t)idx_ms(50);
    c.abas[ABA_LIVRE].n = 1;
    c.abas[ABA_LIVRE].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    c.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO;
    c.abas[ABA_LIVRE].fontes[0].id = EIXO_GYROX;
    c.abas[ABA_LIVRE].fontes[0].beta = beta_padrao();
    c.abas[ABA_LIVRE].fontes[0].beta.thr_in = 20;
    c.abas[ABA_LIVRE].fontes[0].beta.thr_out = 60;
    if (alfa_validar(&c) != 0) ok = 0;
    memset(&in, 0, sizeof in); in.eixo[EIXO_GYROX] = -30;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 1) ok = 0;
    memset(&in, 0, sizeof in); in.eixo[EIXO_GYROX] = 0;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 0) ok = 0;
    passo("sensor de giroscopio como fonte de eixo, com BETA", ok, "GYROX -30 ativa; 0 nao");

    /* validacao: dez eixos; o id 10 nao existe */
    ok = 1;
    { AlfaCfg x = c; x.abas[ABA_LIVRE].fontes[0].id = EIXO_GYROZ;
      if (alfa_validar(&x) != 0) ok = 0; }
    { AlfaCfg x = c; x.abas[ABA_LIVRE].fontes[0].id = EIXO_N;
      if (alfa_validar(&x) != -1) ok = 0; }
    passo("validacao: aceita os dez eixos e recusa o id 10", ok, "");

    /* ocultacao: ACCX (bit 4) e L2 (bit 10) nao colidem */
    ok = 1;
    {
        AlfaCfg o; AlfaSaida so;
        memset(&o, 0, sizeof o);
        o.janela = (uint8_t)idx_ms(50);
        o.abas[ABA_LIVRE].n = 1;
        o.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
        o.abas[ABA_LIVRE].oculto = 1;
        o.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO;
        o.abas[ABA_LIVRE].fontes[0].id = EIXO_ACCX;
        o.abas[ABA_LIVRE].fontes[0].beta = beta_padrao();
        if (alfa_validar(&o) != 0) ok = 0;
        alfa_iniciar(&e); memset(&in, 0, sizeof in); alfa_avaliar(&o, &e, &in, &so);
        if (so.ocultar_analog != (uint16_t)(1u << EIXO_ACCX)) ok = 0;
        o.abas[ABA_LIVRE].fontes[0].tipo = FONTE_GATILHO;
        o.abas[ABA_LIVRE].fontes[0].id = GATILHO_L2;
        alfa_iniciar(&e); alfa_avaliar(&o, &e, &in, &so);
        if (so.ocultar_analog != (uint16_t)(1u << (EIXO_N + GATILHO_L2))) ok = 0;
    }
    passo("ocultacao de sensores e gatilhos: bits separados (ACCX no 4, L2 no 10)", ok, "");
}

/* ------------------------------------------------------------ intensidade */
/* A intensidade de uma aba e a maior das fontes com nivel nela (manual, secao 26.1). Eixo: o modulo
 * do valor depois do MULT, so na zona. Botao: 100. Os valores abaixo sao escritos a mao. */
static void testes_intensidade(void) {
    AlfaCfg c; AlfaEstado e; AlfaSaida o; Entrada in; int ok = 1;
    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 2;
    c.abas[ABA_LIVRE].eou = EOU_OU;
    c.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    c.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO;
    c.abas[ABA_LIVRE].fontes[0].id = EIXO_LX;
    c.abas[ABA_LIVRE].fontes[0].beta.mult = 9;      /* x1.0 */
    c.abas[ABA_LIVRE].fontes[0].beta.thr_in = 50;   /* zona: modulo >= 50 */
    c.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_ABS;
    c.abas[ABA_LIVRE].fontes[1] = botao(B_CROSS);
    alfa_iniciar(&e);

    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 70; in.t_us = 0;
    alfa_avaliar(&c, &e, &in, &o);
    if (!o.ativa[ABA_LIVRE] || o.intens[ABA_LIVRE] != 70) ok = 0;         /* eixo 70 -> intensidade 70 */

    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -80; in.t_us = 10000;
    alfa_avaliar(&c, &e, &in, &o);
    if (!o.ativa[ABA_LIVRE] || o.intens[ABA_LIVRE] != 80) ok = 0;         /* sinal negativo: modulo 80 */

    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 30; in.t_us = 20000;
    alfa_avaliar(&c, &e, &in, &o);
    if (o.ativa[ABA_LIVRE] || o.intens[ABA_LIVRE] != 0) ok = 0;           /* fora da zona: nada */

    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 60; in.botoes = BIT(B_CROSS); in.t_us = 30000;
    alfa_avaliar(&c, &e, &in, &o);
    if (o.intens[ABA_LIVRE] != 100) ok = 0;                               /* botao vale 100: o maior */

    passo("intensidade: eixo pelo modulo na zona; botao vale 100; o maior vence", ok,
          "70 -> 70; -80 -> 80; 30 fora da zona -> 0; 60 com botao -> 100");
}

/* ------------------------------------------------------- toque oculto */
/* Uma aba oculta com FONTE_DEDO suprime o toque do touchpad do jogo; sem ocultar, nao. */
static void testes_toque_oculto(void) {
    AlfaCfg c; AlfaEstado e; AlfaSaida o; Entrada in; int ok = 1;
    cfg_vazia(&c);
    c.abas[ABA_LIVRE].n = 1;
    c.abas[ABA_LIVRE].eou = EOU_OU;
    c.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    c.abas[ABA_LIVRE].oculto = 1;
    c.abas[ABA_LIVRE].fontes[0].tipo = FONTE_DEDO;
    c.abas[ABA_LIVRE].fontes[0].id = 0;
    alfa_iniciar(&e);
    memset(&in, 0, sizeof in); in.dedo[0] = 1; in.t_us = 0;
    alfa_avaliar(&c, &e, &in, &o);
    if (!o.ocultar_toque) ok = 0;                          /* oculta: o toque sai do jogo */
    c.abas[ABA_LIVRE].oculto = 0;
    alfa_iniciar(&e);
    alfa_avaliar(&c, &e, &in, &o);
    if (o.ocultar_toque) ok = 0;                           /* sem ocultar: o toque passa */
    passo("toque oculto: aba oculta com dedo suprime o toque; sem ocultar, nao", ok, "");
}

int main(void) {
    testes_toque_oculto();
    testes_intensidade();
    testes_sensores();
    testes_delay();
    testes_validacao();
    testes_beta();
    testes_livre();
    testes_sequencia();
    testes_2clk();
    testes_hold();
    testes_block();
    testes_oculto();

    if (falhas) { printf("  => BANCADA DA ALFA FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada da ALFA aberta: todas as verificacoes passaram\n");
    return 0;
}
