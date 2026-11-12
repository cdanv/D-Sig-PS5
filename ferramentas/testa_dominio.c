/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DOS DOMINIOS. Roda no Linux, sem console. Cronogramas escritos a mao; instantes
 * esperados a partir de docs/ARQUITETURA.md, secao 23 (manual, secoes 6, 25.1 e 40).
 */
#include "../src/rede.h"
#include "../src/dominio.h"
#include "../src/pad.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define B_CROSS 14
#define B_SQUARE 15
#define D_L1 0
#define D_CIRCLE 7
#define D_TRIANGLE 9      /* destino 9 = TRIANGLE (manual, secao 14.2); o botao de entrada e o bit 12 */

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

static void mod_nivel(ModCfg *m, uint8_t botao, uint8_t tipo, uint8_t dest) {
    mod_base(m);
    m->alfa.abas[ABA_LIVRE].n = 1;
    m->alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_BOTAO;
    m->alfa.abas[ABA_LIVRE].fontes[0].id = botao;
    m->alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m->delta.tipo = tipo;
    m->delta.n_dest = 1;
    m->delta.dest[0] = dest;
}

typedef uint32_t (*Cron)(uint64_t t_ms);

/* o que se ve no instante t: destinos de cada MOD, e o dominio ligado */
typedef struct { uint32_t dest[REDE_MAX]; uint8_t atual; } Visto;

static Visto ver_em(const RedeCfg *r, Cron cr, uint64_t fim_ms, uint64_t t_ms) {
    RedeEstado e; RedeSaida s; uint64_t t; Visto v;
    memset(&v, 0, sizeof v);
    rede_iniciar(&e);
    for (t = 0; t <= fim_ms; t += 10) {
        Entrada in;
        memset(&in, 0, sizeof in);
        in.botoes = cr(t);
        in.t_us = t * 1000u;
        rede_avaliar(r, &e, &in, &s);
        if (t == t_ms) {
            unsigned i;
            for (i = 0; i < r->n; i++) v.dest[i] = s.mod[i].destinos;
            v.atual = e.dom.atual;
        }
    }
    return v;
}

/* ------------------------------------------------------ comandos fixos */
/* L1 apertado de 0 a 2000. L3: duplo em 100 e 200 (D1), duplo em 300 e 350 (liga/desliga D1),
 * segurar de 1000 a 1600 (D3). R1 apertado de 400 a 2300. R3: duplo em 500 e 550 (D2),
 * segurar de 1700 a 2300 (D4). */
static uint32_t cr_comandos(uint64_t t) {
    uint32_t b = 0;
    if (t < 2000) b |= DS_BTN_L1;
    if (t >= 400 && t < 2300) b |= DS_BTN_R1;
    if (t == 100 || t == 200 || t == 300 || t == 350) b |= DS_BTN_L3;
    if (t == 500 || t == 550) b |= DS_BTN_R3;
    if (t >= 1000 && t < 1600) b |= DS_BTN_L3;
    if (t >= 1700 && t < 2300) b |= DS_BTN_R3;
    return b;
}
static void testes_comandos(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    r.dom[0] = 0xF;
    if (rede_validar(&r) != 0) ok = 0;
    if (ver_em(&r, cr_comandos, 2400, 50).atual != 0) ok = 0;
    if (ver_em(&r, cr_comandos, 2400, 210).atual != 1) ok = 0;      /* L1 + L3 duplo: D1 */
    if (ver_em(&r, cr_comandos, 2400, 360).atual != 0) ok = 0;      /* de novo: desliga D1 */
    if (ver_em(&r, cr_comandos, 2400, 560).atual != 2) ok = 0;      /* R1 + R3 duplo: D2 */
    if (ver_em(&r, cr_comandos, 2400, 1490).atual != 2) ok = 0;     /* segurar ainda nao passou de 500 */
    if (ver_em(&r, cr_comandos, 2400, 1510).atual != 3) ok = 0;     /* L1 + L3 segurado: D3 */
    if (ver_em(&r, cr_comandos, 2400, 2210).atual != 4) ok = 0;     /* R1 + R3 segurado: D4 */
    passo("comandos fixos: duplo e segurar, com o modificador, para D1 a D4; o mesmo liga e desliga",
          ok, "D1 em 210; desliga em 360; D2 em 560; D3 em 1510; D4 em 2210");
}

/* ------------------------------------------------ MOD so no seu dominio */
/* D1 em 210 (L1 + L3 duplo). CROSS e SQUARE em 300..400: so o MOD de D1 responde.
 * D2 em 650 (R1 + R3 duplo). SQUARE em 700..800: agora so o MOD de D2 responde. */
static uint32_t cr_dominios(uint64_t t) {
    uint32_t b = 0;
    if (t < 2000) b |= DS_BTN_L1;
    if (t >= 500 && t < 2000) b |= DS_BTN_R1;
    if (t == 100 || t == 200) b |= DS_BTN_L3;
    if (t == 600 || t == 650) b |= DS_BTN_R3;
    if (t >= 300 && t < 400) b |= BIT(B_CROSS) | BIT(B_SQUARE);
    if (t >= 700 && t < 800) b |= BIT(B_SQUARE);
    return b;
}
/* L3 duplo em 100 e 200 SEM L1: nao troca o dominio. L3 em 100 e 380 (280 ms, passa de
 * 250): nao troca. */
static uint32_t cr_sem_mod(uint64_t t) {
    if (t == 100 || t == 200) return DS_BTN_L3;
    return 0;
}
/* L1 apertado so ate 150. L3 em 100 (com L1) e 200 (sem L1): o segundo clique nao conta */
static uint32_t cr_modificador_solto(uint64_t t) {
    uint32_t b = 0;
    if (t < 150) b |= DS_BTN_L1;
    if (t == 100 || t == 200) b |= DS_BTN_L3;
    return b;
}
static uint32_t cr_janela(uint64_t t) {
    uint32_t b = DS_BTN_L1;                 /* L1 apertado o tempo todo */
    if (t == 100 || t == 380) b |= DS_BTN_L3;
    return b;
}
static void testes_janela_e_modificador(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    r.dom[0] = 0xF;
    if (ver_em(&r, cr_sem_mod, 400, 210).atual != 0) ok = 0;
    passo("duplo clique de dominio exige o modificador apertado", ok, "L3 duplo sem L1: nada");

    ok = 1;
    if (ver_em(&r, cr_modificador_solto, 400, 210).atual != 0) ok = 0;
    passo("duplo clique de dominio: o modificador precisa estar apertado no segundo clique",
          ok, "L1 solto antes do segundo clique: nada");

    ok = 1;
    if (ver_em(&r, cr_janela, 500, 400).atual != 0) ok = 0;
    passo("duplo clique de dominio: 280 ms entre cliques passa da janela de 250 ms", ok, "nao troca");
}

static void testes_dominios(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    mod_nivel(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_nivel(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    r.dom[0] = 0x1;                       /* MOD 0 so no D1 */
    r.dom[1] = 0x2;                       /* MOD 1 so no D2 */
    if (rede_validar(&r) != 0) ok = 0;
    {
        Visto v = ver_em(&r, cr_dominios, 900, 350);
        if (!(v.dest[0] & BIT(D_CIRCLE)) || (v.dest[1] & BIT(D_TRIANGLE))) ok = 0;
    }
    {
        Visto v = ver_em(&r, cr_dominios, 900, 750);
        if ((v.dest[0] & BIT(D_CIRCLE)) || !(v.dest[1] & BIT(D_TRIANGLE))) ok = 0;
    }
    passo("dominio: so os MODs do dominio ligado respondem", ok,
          "em D1 (350 ms): so o MOD 0; em D2 (750 ms): so o MOD 1");
}

/* ---------------------------------------- troca: trava e reinicio de estado */
/* MOD 0 (STATIC em L1, pelo SQUARE) esta em D1 e em D2. SQUARE em 300..310 trava no D1.
 * D2 em 650. SQUARE em 640..700: estava apertado na troca, fica travado ate soltar em 700.
 * SQUARE em 720..730 trava de novo. */
static uint32_t cr_troca(uint64_t t) {
    uint32_t b = 0;
    if (t < 2000) b |= DS_BTN_L1;
    if (t >= 500 && t < 2000) b |= DS_BTN_R1;
    if (t == 100 || t == 200) b |= DS_BTN_L3;
    if (t == 600 || t == 650) b |= DS_BTN_R3;
    if (t >= 300 && t < 310) b |= BIT(B_SQUARE);
    if (t >= 640 && t < 700) b |= BIT(B_SQUARE);
    if (t >= 720 && t < 730) b |= BIT(B_SQUARE);
    return b;
}
static void testes_troca(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], B_SQUARE, DELTA_STATIC, D_L1);
    r.dom[0] = 0x3;                        /* D1 e D2 */
    if (rede_validar(&r) != 0) ok = 0;
    if (!(ver_em(&r, cr_troca, 1000, 320).dest[0] & BIT(D_L1))) ok = 0;   /* travado em D1 */
    if (ver_em(&r, cr_troca, 1000, 680).dest[0] != 0) ok = 0;             /* troca reinicia; travado */
    if (ver_em(&r, cr_troca, 1000, 710).dest[0] != 0) ok = 0;            /* solto em 700: zerado, sem lembrar D1 */
    if (!(ver_em(&r, cr_troca, 1000, 740).dest[0] & BIT(D_L1))) ok = 0;   /* volta a travar em D2 */
    passo("troca de dominio: reinicia o estado (STATIC cai) e trava ate a fonte repousar", ok,
          "travado em D1 em 320; zerado e travado em 680; zerado em 710 (sem lembrar D1); trava de novo em 740");
}

/* ---------------------------------------------------- sem dominio */
static void testes_sem_dominio(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 1;
    mod_nivel(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    r.dom[0] = 0;                          /* sem dominio: nunca roda */
    if (rede_validar(&r) != 0) ok = 0;
    if (ver_em(&r, cr_dominios, 900, 350).dest[0] != 0) ok = 0;
    passo("MOD sem dominio nunca roda", ok, "CROSS apertado com D1 ligado; nada sai");

    ok = 1;
    r.dom[0] = 0x10;                       /* fora de D1..D4 */
    if (rede_validar(&r) != -1) ok = 0;
    passo("validacao: recusa mascara fora de D1..D4", ok, "");
}

int main(void) {
    testes_comandos();
    testes_janela_e_modificador();
    testes_dominios();
    testes_troca();
    testes_sem_dominio();
    if (falhas) { printf("  => BANCADA DOS DOMINIOS FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada dos dominios aberta: todas as verificacoes passaram\n");
    return 0;
}
