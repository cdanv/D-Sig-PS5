/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA ALFA DO MODELO v3. Roda no Linux, sem console.
 *
 * Os resultados esperados sao escritos a mao, a partir das regras do autor
 * (docs/ARQUITETURA.md, secoes 12 a 14). Nao vem do codigo de src/alfa.c.
 */
#include "../src/alfa.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
/* bits de pad.h: CROSS=14, CIRCLE=13, SQUARE=15, TRIANGLE=12 */
#define B_CROSS 14
#define B_CIRCLE 13
#define B_SQUARE 15
#define B_TRIANGLE 12

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

/* indice do catalogo para um valor em ms, ou 255 se nao existe */
static unsigned idx_ms(unsigned ms) {
    unsigned i, n = tempo_quantos();
    uint16_t x;
    for (i = 0; i < n; i++) if (tempo_ms(i, &x) == 0 && x == ms) return i;
    return 255u;
}

static Entrada ent(uint32_t botoes, uint64_t t_ms) {
    Entrada e;
    memset(&e, 0, sizeof e);
    e.botoes = botoes;
    e.t_us = t_ms * 1000u;
    return e;
}

static Fonte botao(uint8_t bit, uint8_t oculto) {
    Fonte f;
    memset(&f, 0, sizeof f);
    f.tipo = FONTE_BOTAO;
    f.id = bit;
    f.oculto = oculto;
    return f;
}

static Beta beta_padrao(void) {
    Beta b;
    b.mult = 9; b.thr_in = 0; b.thr_out = 0; b.dir = DIR_ABS; b.inv = 0;
    return b;
}

/* ------------------------------------------------------------------ BETA */
static void testes_beta(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    Entrada in;
    int ok;

    /* eixo LX, zona [20, 60], DIR ABS, x1.0 */
    memset(&c, 0, sizeof c);
    c.n = 1; c.modo = MODO_QUALQUER;
    c.janela = (uint8_t)idx_ms(50);
    c.fontes[0].tipo = FONTE_EIXO; c.fontes[0].id = EIXO_LX;
    c.fontes[0].beta = beta_padrao();
    c.fontes[0].beta.thr_in = 20; c.fontes[0].beta.thr_out = 60;

    ok = (alfa_validar(&c) == 0);
    alfa_iniciar(&e);
    {
        int casos[][2] = {{-30, 1}, {-70, 0}, {-10, 0}, {10, 0}, {50, 1}, {0, 0}};
        size_t k, n = sizeof casos / sizeof casos[0];
        for (k = 0; k < n; k++) {
            memset(&in, 0, sizeof in);
            in.eixo[EIXO_LX] = casos[k][0];
            alfa_iniciar(&e);
            alfa_avaliar(&c, &e, &in, 0, &s);
            if (s.ativa != (uint8_t)casos[k][1]) ok = 0;
        }
    }
    passo("zona BETA: faixa [20,60], DIR ABS", ok, "6 valores escritos a mao");

    /* DIR NEG: so valores negativos contam */
    c.fontes[0].beta.dir = DIR_NEG;
    ok = 1;
    {
        int casos[][2] = {{-30, 1}, {30, 0}};
        size_t k;
        for (k = 0; k < 2; k++) {
            memset(&in, 0, sizeof in);
            in.eixo[EIXO_LX] = casos[k][0];
            alfa_iniciar(&e);
            alfa_avaliar(&c, &e, &in, 0, &s);
            if (s.ativa != (uint8_t)casos[k][1]) ok = 0;
        }
    }
    passo("DIR NEG: positivo nao conta", ok, "");

    /* INV: inverte a zona. Dentro da faixa deixa de contar; fora passa a contar */
    c.fontes[0].beta.dir = DIR_ABS;
    c.fontes[0].beta.inv = 1;
    ok = 1;
    {
        int casos[][2] = {{-50, 0}, {-70, 1}, {-10, 1}};
        size_t k;
        for (k = 0; k < 3; k++) {
            memset(&in, 0, sizeof in);
            in.eixo[EIXO_LX] = casos[k][0];
            alfa_iniciar(&e);
            alfa_avaliar(&c, &e, &in, 0, &s);
            if (s.ativa != (uint8_t)casos[k][1]) ok = 0;
        }
    }
    passo("INV inverte a zona", ok, "");

    /* MULT: x0.1 deixa o valor abaixo do piso; x1.9 leva o valor para dentro */
    c.fontes[0].beta.inv = 0;
    c.fontes[0].beta.mult = 0;                    /* x0.1 */
    ok = 1;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -100;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, 0, &s);
    if (s.ativa != 0) ok = 0;                     /* -100 * 0.1 = -10, abaixo de 20 */
    c.fontes[0].beta.mult = 18;                   /* x1.9 */
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -30;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, 0, &s);
    if (s.ativa != 1) ok = 0;                     /* -30 * 1.9 = -57, dentro de [20,60] */
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -100;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, 0, &s);
    if (s.ativa != 0) ok = 0;                     /* -100 * 1.9 = -190, limitado a -100, fora */
    passo("MULT escala o valor antes da zona, com limite de +-100", ok, "");

    /* gatilho R2: sem direcao, zona [20, 100] */
    memset(&c, 0, sizeof c);
    c.n = 1; c.modo = MODO_QUALQUER;
    c.fontes[0].tipo = FONTE_GATILHO; c.fontes[0].id = GATILHO_R2;
    c.fontes[0].beta = beta_padrao();
    c.fontes[0].beta.thr_in = 20; c.fontes[0].beta.thr_out = 0;
    ok = 1;
    memset(&in, 0, sizeof in); in.gatilho[GATILHO_R2] = 80;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, 0, &s);
    if (s.ativa != 1) ok = 0;
    memset(&in, 0, sizeof in); in.gatilho[GATILHO_R2] = 10;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, 0, &s);
    if (s.ativa != 0) ok = 0;
    passo("gatilho R2: piso de 20 e sem teto", ok, "");
}

/* ----------------------------------------------------------- QUALQUER */
static void testes_qualquer(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok = 1;
    unsigned mascara;
    memset(&c, 0, sizeof c);
    c.n = 3; c.modo = MODO_QUALQUER;
    c.fontes[0] = botao(B_CROSS, 0);
    c.fontes[1] = botao(B_SQUARE, 0);
    c.fontes[2] = botao(B_TRIANGLE, 0);
    c.janela = (uint8_t)idx_ms(50);
    if (alfa_validar(&c) != 0) ok = 0;

    /* as 8 combinacoes: ativa se, e so se, alguma fonte esta apertada */
    for (mascara = 0; mascara < 8; mascara++) {
        uint32_t b = 0;
        int esperado;
        if (mascara & 1) b |= BIT(B_CROSS);
        if (mascara & 2) b |= BIT(B_SQUARE);
        if (mascara & 4) b |= BIT(B_TRIANGLE);
        esperado = (mascara != 0);
        alfa_iniciar(&e);
        alfa_avaliar(&c, &e, &(Entrada){0}, 0, &s);      /* quadro vazio antes */
        alfa_iniciar(&e);
        alfa_avaliar(&c, &e, &(Entrada){.botoes = b, .t_us = 1000}, 0, &s);
        if (s.ativa != (uint8_t)esperado) ok = 0;
    }
    passo("QUALQUER: basta uma, em todas as 8 combinacoes", ok, "");
}

/* ---------------------------------------------------------- SIMULTANEO */
static void testes_simultaneo(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    int ok;

    memset(&c, 0, sizeof c);
    c.n = 3; c.modo = MODO_SIMULTANEO;
    c.fontes[0] = botao(B_CROSS, 0);
    c.fontes[1] = botao(B_CIRCLE, 0);
    c.fontes[2] = botao(B_SQUARE, 0);
    c.janela = (uint8_t)idx_ms(50);
    if (alfa_validar(&c) != 0) { passo("SIMULTANEO: configuracao valida", 0, ""); return; }

    /* tres botoes em 20 ms, dentro da janela de 50: ativa quando as tres estao juntas */
    alfa_iniciar(&e);
    ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, 0, &s);
    if (s.ativa) ok = 0;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE), .t_us = 10000}, 0, &s);
    if (s.ativa) ok = 0;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 20000}, 0, &s);
    if (!s.ativa) ok = 0;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 30000}, 0, &s);
    if (!s.ativa) ok = 0;                          /* segurando, continua ativa */
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 40000}, 0, &s);
    if (s.ativa) ok = 0;                           /* soltou uma: desmonta */
    passo("SIMULTANEO: tres botoes em 20 ms, dentro da janela, ativa e segura", ok, "");

    /* o mesmo conjunto espalhado em 80 ms, com janela de 50: nao ativa */
    alfa_iniciar(&e);
    ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE), .t_us = 40000}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 80000}, 0, &s);
    if (s.ativa) ok = 0;
    passo("SIMULTANEO: fora da janela de 50 ms, nao ativa", ok, "");

    /* so dois dos tres: nao ativa */
    alfa_iniciar(&e);
    ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE), .t_us = 0}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE), .t_us = 10000}, 0, &s);
    if (s.ativa) ok = 0;
    passo("SIMULTANEO: dois de tres nao ativa", ok, "");

    /* soltar uma fonte desmonta a janela: a CROSS de t=0 nao conta mais.
     * A janela nova abre em t=10 (CIRCLE). Com CROSS solta em t=5, o conjunto so fecha
     * quando CROSS entra de novo, dentro da janela de 50 ms aberta em t=10. */
    alfa_iniciar(&e);
    ok = 1;
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS), .t_us = 0}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = 0, .t_us = 5000}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CIRCLE), .t_us = 10000}, 0, &s);
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 30000}, 0, &s);
    if (s.ativa) ok = 0;                           /* ainda falta CROSS */
    alfa_avaliar(&c, &e, &(Entrada){.botoes = BIT(B_CROSS)|BIT(B_CIRCLE)|BIT(B_SQUARE), .t_us = 45000}, 0, &s);
    if (!s.ativa) ok = 0;                          /* CROSS entrou dentro da janela de t=10 */
    passo("SIMULTANEO: soltar uma fonte desmonta; a janela nova conta so o que entrou nela", ok, "");
}

/* ----------------------------------------------------------- SEQUENCIA */
/* A sequencia CROSS -> CIRCLE -> SQUARE, com espera de 200 ms apos cada um dos dois
 * primeiros passos. Os quadros sao: (botoes, tempo em ms, gatilho acionado). */
typedef struct { uint32_t b; uint64_t t; int r; } Quadro;

static void rodar(const AlfaCfg *c, AlfaEstado *e, const Quadro *q, size_t n,
                  int *concluiu, int *reiniciou, size_t *frame_concluiu,
                  size_t *frame_reiniciou, uint32_t *oculto_em_algum) {
    size_t i;
    AlfaSaida s;
    *concluiu = 0; *reiniciou = 0;
    *frame_concluiu = (size_t)-1; *frame_reiniciou = (size_t)-1;
    *oculto_em_algum = 0;
    for (i = 0; i < n; i++) {
        Entrada in = ent(q[i].b, q[i].t);
        alfa_avaliar(c, e, &in, q[i].r, &s);
        *oculto_em_algum |= s.ocultar_botoes;
        if (s.concluida) { *frame_concluiu = i; *concluiu = 1; }   /* a ULTIMA conclusao */
        if (s.reiniciou) { if (!*reiniciou) *frame_reiniciou = i; *reiniciou = 1; }
    }
}

static void montar_sequencia(AlfaCfg *c, uint8_t oculto) {
    memset(c, 0, sizeof *c);
    c->n = 3; c->modo = MODO_SEQUENCIA;
    c->fontes[0] = botao(B_CROSS, oculto);
    c->fontes[1] = botao(B_CIRCLE, oculto);
    c->fontes[2] = botao(B_SQUARE, oculto);
    c->espera[0] = (uint8_t)idx_ms(200);
    c->espera[1] = (uint8_t)idx_ms(200);
    c->janela = (uint8_t)idx_ms(50);
}

static void testes_sequencia(void) {
    AlfaCfg c;
    AlfaEstado e;
    int concluiu, reiniciou, ok;
    size_t fc, fr;
    uint32_t oc;

    montar_sequencia(&c, 0);
    if (alfa_validar(&c) != 0) { passo("SEQUENCIA: configuracao valida", 0, ""); return; }

    /* sucesso dentro dos prazos */
    {
        Quadro q[] = {
            {0, 0, 0}, {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {BIT(B_CIRCLE), 200, 0}, {0, 250, 0}, {BIT(B_SQUARE), 300, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 6, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: ordem certa dentro dos prazos conclui, no ultimo passo",
              concluiu && fc == 5 && !reiniciou, "");
    }

    /* espera estourada: 300 ms apos o primeiro passo, com prazo de 200 */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {BIT(B_CIRCLE), 400, 0}, {0, 450, 0}, {BIT(B_SQUARE), 500, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 5, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: espera estourada reinicia e nao conclui",
              reiniciou && fr == 2 && !concluiu, "");
    }

    /* passo errado: SQUARE quando o esperado era CIRCLE */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {BIT(B_SQUARE), 200, 0}, {0, 250, 0},
            {BIT(B_CIRCLE), 300, 0}, {0, 350, 0}, {BIT(B_SQUARE), 400, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 7, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: passo errado reinicia; a sequencia recomecada conclui",
              reiniciou && fr == 2 && !concluiu, "");
    }

    /* gatilho durante a sequencia, com o passo esperado de outro botao */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {0, 180, 1},                           /* gatilho no meio: reinicia */
            {BIT(B_CIRCLE), 200, 0}, {0, 250, 0},  /* ignorado: passo 0 */
            {BIT(B_CROSS), 300, 0}, {0, 350, 0},
            {BIT(B_CIRCLE), 400, 0}, {0, 450, 0},
            {BIT(B_SQUARE), 500, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 10, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: gatilho no meio reinicia, e a nova sequencia conclui",
              reiniciou && fr == 2 && concluiu && fc == 9, "");
    }

    /* prioridade ao passo esperado: o gatilho e o mesmo botao do passo esperado */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {BIT(B_CIRCLE), 200, 1},               /* gatilho e CIRCLE, que e o passo esperado */
            {0, 250, 0},
            {BIT(B_SQUARE), 300, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 5, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: passo esperado tem prioridade sobre o gatilho de mesmo botao",
              !reiniciou && concluiu && fc == 4, "");
    }

    /* OCULTO: os botoes dos passos somem do jogo durante toda a sequencia */
    {
        AlfaCfg co;
        Quadro q[] = {{0, 0, 0}, {BIT(B_CROSS), 100, 0}, {0, 150, 0}};
        montar_sequencia(&co, 1);
        alfa_iniciar(&e);
        rodar(&co, &e, q, 3, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: fontes OCULTO somem do jogo",
              (oc & (BIT(B_CROSS) | BIT(B_CIRCLE) | BIT(B_SQUARE))) ==
              (BIT(B_CROSS) | BIT(B_CIRCLE) | BIT(B_SQUARE)), "");
    }

    /* segurar um botao nao repete o passo: o passo e a BORDA de subida */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {BIT(B_CROSS), 120, 0}, {BIT(B_CROSS), 140, 0},
            {0, 150, 0},
            {BIT(B_CIRCLE), 200, 0}, {0, 250, 0}, {BIT(B_SQUARE), 300, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 7, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: segurar um botao conta uma vez, pela borda",
              concluiu && fc == 6 && !reiniciou, "");
    }

    /* apos concluir, uma nova sequencia comeca do zero */
    {
        Quadro q[] = {
            {BIT(B_CROSS), 100, 0}, {0, 150, 0},
            {BIT(B_CIRCLE), 200, 0}, {0, 250, 0},
            {BIT(B_SQUARE), 300, 0}, {0, 350, 0},
            {BIT(B_CROSS), 400, 0}, {0, 450, 0},
            {BIT(B_CIRCLE), 500, 0}, {0, 550, 0},
            {BIT(B_SQUARE), 600, 0},
        };
        alfa_iniciar(&e);
        rodar(&c, &e, q, 11, &concluiu, &reiniciou, &fc, &fr, &oc);
        passo("SEQUENCIA: apos concluir, a proxima comeca do passo 1", concluiu && fc == 10, "");
    }
    (void)ok;
}

/* ------------------------------------------------------------ validacao */
static void testes_validacao(void) {
    AlfaCfg c;
    int ok = 1;

    montar_sequencia(&c, 0);
    if (alfa_validar(&c) != 0) ok = 0;

    { AlfaCfg x = c; x.n = 0; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.n = 9; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.modo = 3; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.janela = (uint8_t)tempo_quantos(); if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.espera[0] = (uint8_t)tempo_quantos(); if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.fontes[1] = x.fontes[0]; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; x.n = 1; x.modo = MODO_SIMULTANEO; if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x; memset(&x, 0, sizeof x); x.n = 1; x.modo = MODO_QUALQUER;
      x.fontes[0].tipo = FONTE_EIXO; x.fontes[0].id = EIXO_LX; x.fontes[0].beta = beta_padrao();
      x.fontes[0].beta.thr_in = 7;                       /* nao e multiplo de 5 */
      if (alfa_validar(&x) != -1) ok = 0;
      x.fontes[0].beta.thr_in = 20; x.fontes[0].beta.mult = 19;   /* fora de 0..18 */
      if (alfa_validar(&x) != -1) ok = 0; }
    { AlfaCfg x = c; if (alfa_validar(0) != -1) ok = 0; (void)x; }
    passo("validacao recusa cada limite fora da faixa, e aceita a configuracao boa", ok, "");
}

int main(void) {
    testes_validacao();
    testes_beta();
    testes_qualquer();
    testes_simultaneo();
    testes_sequencia();

    if (falhas) { printf("  => BANCADA DA ALFA FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada da ALFA aberta: todas as verificacoes passaram\n");
    return 0;
}
