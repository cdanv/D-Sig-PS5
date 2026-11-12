/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA INTEGRACAO ALFA -> DELTA: o MOD completo. Roda no Linux, sem console.
 * Cronogramas de botao escritos a mao, e instantes esperados calculados a mao a partir
 * das regras de docs/ARQUITETURA.md, secoes 20 e 21.
 */
#include "../src/modelo.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define B_L1 10
#define B_CROSS 14
#define B_CIRCLE 13
#define B_SQUARE 15
#define B_TRIANGLE 12
/* destinos (indices do Cronus, manual secao 14.2) */
#define D_L1 0
#define D_CROSS 6
#define D_CIRCLE 7

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

static Fonte botao(uint8_t bit) {
    Fonte f;
    memset(&f, 0, sizeof f);
    f.tipo = FONTE_BOTAO; f.id = bit;
    return f;
}

static void mod_vazio(ModCfg *m) {
    unsigned a;
    memset(m, 0, sizeof *m);
    m->alfa.janela = (uint8_t)idx_ms(50);
    for (a = 0; a < ALFA_ABAS; a++) {
        m->alfa.abas[a].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    }
    m->delta.lat = DELTA_SEM_LAT;
    m->delta.ciclos = 0;          /* DIRECT e STATIC nao tem ciclos; os casos de TURBO ligam ATIVO */
}

/* cronograma: botoes em cada instante de 10 ms. `botoes(t)` diz o que esta apertado. */
typedef uint32_t (*Cronograma)(uint64_t t_ms);

static void rodar(const ModCfg *m, Cronograma cr, uint64_t fim_ms, uint64_t t_ms,
                  uint32_t *destinos_em, uint8_t *bloq_em, uint8_t *term_em) {
    ModEstado e;
    ModSaida s;
    uint64_t t;
    modelo_iniciar(&e);
    /* sentinela: se o instante pedido nao for amostrado, a verificacao falha, em vez de
     * reaproveitar o valor da chamada anterior */
    if (destinos_em) *destinos_em = 0xFFFFFFFFu;
    if (bloq_em) *bloq_em = 0xFF;
    if (term_em) *term_em = 0xFF;
    for (t = 0; t <= fim_ms; t += 10) {
        Entrada in;
        memset(&in, 0, sizeof in);
        in.botoes = cr(t);
        in.t_us = t * 1000u;
        modelo_avaliar(m, &e, &in, &s);
        if (t == t_ms) {
            if (destinos_em) *destinos_em = s.destinos;
            if (bloq_em) *bloq_em = s.bloqueado;
            if (term_em) *term_em = s.terminou;
        }
    }
}

/* ---- cronogramas ---- */
/* CROSS segurado em [0,80) e [150,300); SQUARE clicado duas vezes (100 e 120);
 * TRIANGLE segurado em [200,410) */
static uint32_t cr_stat(uint64_t t) {
    uint32_t b = 0;
    if (t < 80 || (t >= 150 && t < 300)) b |= BIT(B_CROSS);
    if (t == 100 || t == 120) b |= BIT(B_SQUARE);
    if (t >= 200 && t < 410) b |= BIT(B_TRIANGLE);
    return b;
}
/* SQUARE duplo em 0/20, e L1 (BLOCK) segurado em [50,80) */
static uint32_t cr_turbo_bloq(uint64_t t) {
    uint32_t b = 0;
    if (t == 0 || t == 20) b |= BIT(B_SQUARE);
    if (t >= 50 && t < 80) b |= BIT(B_L1);
    return b;
}
static uint32_t cr_turbo(uint64_t t) {
    return (t == 0 || t == 20) ? BIT(B_SQUARE) : 0;
}

/* ------------------------------------------------------------ validacao */
static void testes_validacao(void) {
    ModCfg m;
    int ok = 1;

    /* MOD valido: STATIC alimentado por LIVRE (LIGA), 2CLK (ALTERNA), HOLD_AP (FINALIZA) e BLOCK */
    mod_vazio(&m);
    m.alfa.abas[ABA_LIVRE].n = 1; m.alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    m.alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_2CLK].n = 1; m.alfa.abas[ABA_2CLK].fontes[0] = botao(B_SQUARE);
    m.alfa.abas[ABA_2CLK].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_HOLD_AP].n = 1; m.alfa.abas[ABA_HOLD_AP].fontes[0] = botao(B_TRIANGLE);
    m.alfa.abas[ABA_HOLD_AP].tempo = (uint8_t)idx_ms(200);
    m.alfa.abas[ABA_HOLD_AP].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_BLOCK].n = 1; m.alfa.abas[ABA_BLOCK].fontes[0] = botao(B_L1);
    m.acao[ABA_LIVRE] = ACAO_LIGA; m.acao[ABA_2CLK] = ACAO_ALTERNA; m.acao[ABA_HOLD_AP] = ACAO_FINALIZA;
    m.delta.tipo = DELTA_STATIC; m.delta.n_dest = 2; m.delta.dest[0] = D_CROSS; m.delta.dest[1] = D_CIRCLE;
    if (modelo_validar(&m) != 0) ok = 0;

    { ModCfg x = m; x.delta.tipo = DELTA_DIRECT; x.delta.n_dest = 1;
      if (modelo_validar(&x) != -1) ok = 0; }                  /* DIRECT com 2CLK: pulso */
    { ModCfg x; mod_vazio(&x); x.alfa.abas[ABA_LIVRE].n = 1; x.alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
      x.alfa.abas[ABA_LIVRE].delay = (uint8_t)idx_ms(300);
      x.delta.tipo = DELTA_DIRECT; x.delta.n_dest = 1; x.delta.dest[0] = D_CROSS;
      if (modelo_validar(&x) != -1) ok = 0; }                  /* DIRECT com DELAY: recusado */
    { ModCfg x; mod_vazio(&x); x.alfa.abas[ABA_LIVRE].n = 1; x.alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
      x.alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
      x.delta.tipo = DELTA_DIRECT; x.delta.n_dest = 1; x.delta.dest[0] = D_CROSS;
      if (modelo_validar(&x) != 0) ok = 0; }                   /* DIRECT com nivel e OFF: aceito */
    { ModCfg x; mod_vazio(&x); x.alfa.abas[ABA_BLOCK].n = 1; x.alfa.abas[ABA_BLOCK].fontes[0] = botao(B_L1);
      x.delta.tipo = DELTA_STATIC; x.delta.n_dest = 1; x.delta.dest[0] = D_CROSS;
      if (modelo_validar(&x) != -1) ok = 0; }                  /* so BLOCK: sem origem */
    { ModCfg x = m; x.acao[ABA_BLOCK] = ACAO_ALTERNA;
      if (modelo_validar(&x) != -1) ok = 0; }                  /* BLOCK so suspende */
    { ModCfg x = m; x.acao[ABA_LIVRE] = 3;
      if (modelo_validar(&x) != -1) ok = 0; }
    passo("validacao: DIRECT so com nivel e sem DELAY; BLOCK so suspende; precisa de origem", ok, "");
}

/* ------------------------------------------- STATIC com LIGA, ALTERNA, FINALIZA */
static void testes_static(void) {
    ModCfg m;
    int ok = 1;
    uint32_t d = 0;

    mod_vazio(&m);
    m.alfa.abas[ABA_LIVRE].n = 1; m.alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    m.alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_2CLK].n = 1; m.alfa.abas[ABA_2CLK].fontes[0] = botao(B_SQUARE);
    m.alfa.abas[ABA_2CLK].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_HOLD_AP].n = 1; m.alfa.abas[ABA_HOLD_AP].fontes[0] = botao(B_TRIANGLE);
    m.alfa.abas[ABA_HOLD_AP].tempo = (uint8_t)idx_ms(200);
    m.alfa.abas[ABA_HOLD_AP].delay = ALFA_DELAY_OFF;
    m.acao[ABA_LIVRE] = ACAO_LIGA; m.acao[ABA_2CLK] = ACAO_ALTERNA; m.acao[ABA_HOLD_AP] = ACAO_FINALIZA;
    m.delta.tipo = DELTA_STATIC; m.delta.n_dest = 2; m.delta.dest[0] = D_CROSS; m.delta.dest[1] = D_CIRCLE;
    if (modelo_validar(&m) != 0) ok = 0;

    /* CROSS segura em 0..80: liga a trava. Soltar nao destrava. SQUARE clicado duas vezes
     * (100 e 120) alterna: desliga em 120. CROSS de novo em 150: liga. TRIANGLE segurado de
     * 200 a 410: a HOLD_AP dispara em 400 e FINALIZA: desliga. */
    rodar(&m, cr_stat, 500, 50, &d, 0, 0);
    if (d != (BIT(D_CROSS) | BIT(D_CIRCLE))) ok = 0;
    rodar(&m, cr_stat, 500, 90, &d, 0, 0);
    if (d != (BIT(D_CROSS) | BIT(D_CIRCLE))) ok = 0;
    rodar(&m, cr_stat, 500, 110, &d, 0, 0);
    if (d != (BIT(D_CROSS) | BIT(D_CIRCLE))) ok = 0;
    rodar(&m, cr_stat, 500, 130, &d, 0, 0);
    if (d != 0) ok = 0;                                         /* ALTERNA desligou em 120 */
    rodar(&m, cr_stat, 500, 160, &d, 0, 0);
    if (d != (BIT(D_CROSS) | BIT(D_CIRCLE))) ok = 0;            /* CROSS de novo: liga */
    rodar(&m, cr_stat, 500, 390, &d, 0, 0);
    if (d != (BIT(D_CROSS) | BIT(D_CIRCLE))) ok = 0;
    rodar(&m, cr_stat, 500, 410, &d, 0, 0);
    if (d != 0) ok = 0;                                         /* FINALIZA da HOLD_AP em 400 */
    passo("MOD: LIGA trava, ALTERNA alterna, FINALIZA de outra aba encerra (STATIC)", ok,
          "trava em 50, 90, 110; off em 130 (ALTERNA em 120); on em 160 e 390; off em 410");
}

/* ------------------------------------------------ TURBO e BLOCK congela */
static void testes_turbo(void) {
    ModCfg m;
    int ok = 1;
    uint32_t d; uint8_t bloq;

    mod_vazio(&m);
    m.alfa.abas[ABA_2CLK].n = 1; m.alfa.abas[ABA_2CLK].fontes[0] = botao(B_SQUARE);
    m.alfa.abas[ABA_2CLK].delay = ALFA_DELAY_OFF;
    m.acao[ABA_2CLK] = ACAO_LIGA;
    m.delta.tipo = DELTA_TURBO; m.delta.n_dest = 1; m.delta.dest[0] = D_CROSS;
    m.delta.tempo[0] = (uint8_t)idx_ms(100); m.delta.lat = (uint8_t)idx_ms(30);
    m.delta.ciclos = DELTA_CICLOS_ATIVO;
    if (modelo_validar(&m) != 0) ok = 0;

    /* duplo clique em 0 e 20 liga o TURBO: 100 ms ligado (20..119), 30 ms de latencia
     * (120..149), liga de novo em 150 */
    rodar(&m, cr_turbo, 500, 60, &d, 0, 0);
    if (!(d & BIT(D_CROSS))) ok = 0;
    rodar(&m, cr_turbo, 500, 130, &d, 0, 0);
    if (d & BIT(D_CROSS)) ok = 0;
    rodar(&m, cr_turbo, 500, 160, &d, 0, 0);
    if (!(d & BIT(D_CROSS))) ok = 0;
    passo("MOD: duplo clique em 2CLK liga o TURBO; 100 ms ligado, 30 de latencia, repete", ok, "");

    /* BLOCK (L1) suspende entre 50 e 80: a resposta congela e retoma de onde parou.
     * Antes da pausa: 30 ms do passo. Passo restante: 70 ms, de 80 a 150. */
    ok = 1;
    mod_vazio(&m);
    m.alfa.abas[ABA_2CLK].n = 1; m.alfa.abas[ABA_2CLK].fontes[0] = botao(B_SQUARE);
    m.alfa.abas[ABA_2CLK].delay = ALFA_DELAY_OFF;
    m.acao[ABA_2CLK] = ACAO_LIGA;
    m.alfa.abas[ABA_BLOCK].n = 1; m.alfa.abas[ABA_BLOCK].fontes[0] = botao(B_L1);
    m.delta.tipo = DELTA_TURBO; m.delta.n_dest = 1; m.delta.dest[0] = D_CROSS;
    m.delta.tempo[0] = (uint8_t)idx_ms(100); m.delta.lat = (uint8_t)idx_ms(30);
    m.delta.ciclos = DELTA_CICLOS_ATIVO;
    if (modelo_validar(&m) != 0) ok = 0;

    rodar(&m, cr_turbo_bloq, 500, 60, &d, &bloq, 0);
    if (d != 0 || !bloq) ok = 0;                               /* congelado: nao responde */
    rodar(&m, cr_turbo_bloq, 500, 100, &d, &bloq, 0);
    if (!(d & BIT(D_CROSS)) || bloq) ok = 0;                   /* solto em 80: retoma */
    rodar(&m, cr_turbo_bloq, 500, 140, &d, &bloq, 0);
    if (!(d & BIT(D_CROSS))) ok = 0;                           /* ainda no passo restante, ate 150 */
    rodar(&m, cr_turbo_bloq, 500, 160, &d, &bloq, 0);
    if (d & BIT(D_CROSS)) ok = 0;                              /* latencia, de 150 a 180 */
    passo("MOD: BLOCK congela o TURBO e retoma de onde parou", ok,
          "congelado de 50 a 80; passo restante de 70 ms: de 80 a 150; latencia de 150 a 180");
}

/* ------------------------------------------------ OCULTO e DELAY na integracao */
static void testes_oculto_delay(void) {
    ModCfg m;
    ModEstado e;
    ModSaida s;
    Entrada in;
    int ok = 1;
    uint64_t t;

    /* OCULTO da aba chega ao jogo */
    mod_vazio(&m);
    m.alfa.abas[ABA_LIVRE].n = 1; m.alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    m.alfa.abas[ABA_LIVRE].oculto = 1; m.alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m.acao[ABA_LIVRE] = ACAO_LIGA;
    m.delta.tipo = DELTA_STATIC; m.delta.n_dest = 1; m.delta.dest[0] = D_CROSS;
    modelo_iniciar(&e);
    memset(&in, 0, sizeof in); in.botoes = BIT(B_CROSS); in.t_us = 0;
    modelo_avaliar(&m, &e, &in, &s);
    if (!(s.ocultar_botoes & BIT(B_CROSS))) ok = 0;
    passo("MOD: a aba OCULTO suprime o botao do jogo, e a resposta ainda acontece", ok && (s.destinos & BIT(D_CROSS)), "");

    /* DELAY de 200 ms numa aba 2CLK com LIGA: a trava de STATIC sai 200 ms depois da
     * descida do pulso. Duplo clique em 0 e 20 gera pulso em 20 e descida em 30:
     * dispara em 230 ms. */
    ok = 1;
    mod_vazio(&m);
    m.alfa.abas[ABA_2CLK].n = 1; m.alfa.abas[ABA_2CLK].fontes[0] = botao(B_SQUARE);
    m.alfa.abas[ABA_2CLK].delay = (uint8_t)idx_ms(200);
    m.acao[ABA_2CLK] = ACAO_LIGA;
    m.delta.tipo = DELTA_STATIC; m.delta.n_dest = 1; m.delta.dest[0] = D_CROSS;
    if (modelo_validar(&m) != 0) ok = 0;
    modelo_iniciar(&e);
    {
        uint32_t quando_on = 9999;
        for (t = 0; t <= 300; t += 10) {
            memset(&in, 0, sizeof in);
            in.botoes = (t == 0 || t == 20) ? BIT(B_SQUARE) : 0;
            in.t_us = t * 1000u;
            modelo_avaliar(&m, &e, &in, &s);
            if ((s.destinos & BIT(D_CROSS)) && quando_on == 9999) quando_on = (uint32_t)t;
        }
        if (quando_on != 230) ok = 0;
    }
    passo("MOD: DELAY de 2CLK atrasa a resposta da STATIC em 200 ms, a partir da descida", ok,
          "trava em 230 ms");
}

/* MOD travado por dominio: so suprime. Um dedo em aba oculta continua suprimindo o toque. */
static void testes_so_ocultar_toque(void) {
    ModCfg m; ModSaida o; int ok = 1;
    memset(&m, 0, sizeof m);
    m.alfa.janela = 0;
    m.alfa.abas[ABA_LIVRE].n = 1;
    m.alfa.abas[ABA_LIVRE].oculto = 1;
    m.alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_DEDO;
    m.alfa.abas[ABA_LIVRE].fontes[0].id = 0;
    modelo_so_ocultar(&m, &o);
    if (o.ocultar_toque != 1) ok = 0;
    m.alfa.abas[ABA_LIVRE].oculto = 0;
    modelo_so_ocultar(&m, &o);
    if (o.ocultar_toque != 0) ok = 0;
    passo("travado: so-ocultar suprime o toque da aba oculta, e so ela", ok, "");
}

/* ------------------------------------------------- STATIC de eixo (manual, 8.1) */
/* STATIC em eixo nao trava: responde enquanto a fonte esta na zona, escreve 100 x MULT, com o lado
 * atual. Fora da zona fica calado. */
static void testes_static_eixo(void) {
    ModCfg m; ModEstado e; ModSaida o; Entrada in; int ok = 1;
    unsigned t;
    memset(&m, 0, sizeof m);
    m.alfa.janela = 0;
    for (t = 0; t < ALFA_ABAS; t++) m.alfa.abas[t].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    m.alfa.abas[ABA_LIVRE].n = 1;
    m.alfa.abas[ABA_LIVRE].eou = EOU_OU;
    m.alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m.alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_EIXO;
    m.alfa.abas[ABA_LIVRE].fontes[0].id = EIXO_LX;
    m.alfa.abas[ABA_LIVRE].fontes[0].beta.mult = 9;      /* x1.0: teto 100 */
    m.alfa.abas[ABA_LIVRE].fontes[0].beta.thr_in = 30;
    m.alfa.abas[ABA_LIVRE].fontes[0].beta.dir = DIR_ABS;
    m.acao[ABA_LIVRE] = ACAO_LIGA;
    m.delta.tipo = DELTA_STATIC; m.delta.n_dest = 1; m.delta.dest[0] = 0; m.delta.lat = DELTA_SEM_LAT;
    modelo_iniciar(&e);

    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -60; in.t_us = 0;          /* na zona, lado -1 */
    modelo_avaliar(&m, &e, &in, &o);
    if (!o.destinos || o.intensidade != 100 || o.sinal != -1) ok = 0;
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 10; in.t_us = 10000;       /* fora da zona: calado */
    modelo_avaliar(&m, &e, &in, &o);
    if (o.destinos != 0) ok = 0;                                              /* nao trava */
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = 50; in.t_us = 20000;       /* na zona de novo, lado +1 */
    modelo_avaliar(&m, &e, &in, &o);
    if (!o.destinos || o.intensidade != 100 || o.sinal != 1) ok = 0;
    m.alfa.abas[ABA_LIVRE].fontes[0].beta.mult = 4;                          /* x0.5: teto 50 */
    modelo_iniciar(&e);
    memset(&in, 0, sizeof in); in.eixo[EIXO_LX] = -60; in.t_us = 30000;
    modelo_avaliar(&m, &e, &in, &o);
    if (o.intensidade != 50) ok = 0;                                          /* 100 x MULT */
    passo("STATIC de eixo: nao trava; fora da zona fica calado; escreve 100 x MULT com o lado", ok,
          "lado -1 com 100; fora da zona 0; lado +1 com 100; MULT x0.5 com 50");
}

int main(void) {
    testes_static_eixo();
    testes_so_ocultar_toque();
    testes_validacao();
    testes_static();
    testes_turbo();
    testes_oculto_delay();
    if (falhas) { printf("  => BANCADA DO MOD COMPLETO FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do MOD completo aberta: todas as verificacoes passaram\n");
    return 0;
}
