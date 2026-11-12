/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA ZETA: BLOQ, ASSOC e EXCL entre MODs. Roda no Linux, sem console.
 * Cronogramas escritos a mao; instantes esperados calculados a partir de
 * docs/ARQUITETURA.md, secao 22, incluindo a defasagem de um quadro.
 */
#include "../src/rede.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define B_L1 10
#define B_R1 11
#define B_CROSS 14
#define B_SQUARE 15
#define B_TRIANGLE 12
#define D_L1 0
#define D_R1 3
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

/* todos os MODs no dominio D1, e o D1 ligado no inicio (sem passar pela troca, que travaria) */
static void todos_d1(RedeCfg *r) {
    unsigned i;
    for (i = 0; i < r->n; i++) r->dom[i] = 0x1;
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

/* MOD com uma fonte de botao, LIGA, e resposta DIRECT ou STATIC em um destino */
static void mod_resposta(ModCfg *m, uint8_t botao, uint8_t tipo, uint8_t dest) {
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

/* roda a rede com o cronograma e le a saida do MOD `i` no instante t_ms */
static uint32_t saida_em(const RedeCfg *r, Cron cr, uint64_t fim_ms, uint64_t t_ms, unsigned i) {
    RedeEstado e; RedeSaida s; uint64_t t; uint32_t v = 0xFFFFFFFFu;
    rede_iniciar(&e);
    e.dom.atual = 1;                       /* D1 ligado: sem a troca, que travaria os MODs */
    for (t = 0; t <= fim_ms; t += 10) {
        Entrada in;
        memset(&in, 0, sizeof in);
        in.botoes = cr(t);
        in.t_us = t * 1000u;
        rede_avaliar(r, &e, &in, &s);
        if (t == t_ms) v = s.mod[i].destinos;
    }
    return v;
}

/* ------------------------------------------------------------ validacao */
static void testes_validacao(void) {
    RedeCfg r;
    int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    if (rede_validar(&r) != 0) ok = 0;
    { RedeCfg x = r; x.z[0].bloq = BIT(0); if (rede_validar(&x) != -1) ok = 0; }     /* consigo */
    { RedeCfg x = r; x.z[0].assoc = BIT(5); if (rede_validar(&x) != -1) ok = 0; }    /* fora da rede */
    { RedeCfg x = r; x.z[1].excl = BIT(0); x.z[1].excl_inic = 2; if (rede_validar(&x) != -1) ok = 0; }
    { RedeCfg x = r; x.n = 0; if (rede_validar(&x) != -1) ok = 0; }
    passo("validacao: recusa relacao consigo, indice fora da rede e n invalido", ok, "");
}

/* ------------------------------------------------------------- BLOQ */
static uint32_t cr_bloq(uint64_t t) {
    uint32_t b = 0;
    if (t < 100) b |= BIT(B_CROSS);                      /* MOD 0 responde de 0 a 100 */
    if (t < 200) b |= BIT(B_SQUARE);                     /* MOD 1 pressionado de 0 a 200 */
    return b;
}
static void testes_bloq(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    r.z[1].bloq = BIT(0);                                /* MOD 1 e suspenso enquanto MOD 0 responde */
    if (rede_validar(&r) != 0) ok = 0;
    /* MOD 1 responde no inicio (defasagem de um quadro), depois fica suspenso de 20 a 100 */
    if (saida_em(&r, cr_bloq, 300, 50, 1) != 0) ok = 0;
    if (saida_em(&r, cr_bloq, 300, 120, 1) != BIT(D_TRIANGLE)) ok = 0;   /* MOD 0 caiu em 100 */
    if (saida_em(&r, cr_bloq, 300, 150, 1) != BIT(D_TRIANGLE)) ok = 0;
    passo("BLOQ: suspende enquanto o alvo responde, e volta quando o alvo cai", ok,
          "suspenso em 50; de volta em 120 (alvo caiu em 100, com um quadro de defasagem)");
}

/* ------------------------------------------------------------ ASSOC */
static uint32_t cr_assoc(uint64_t t) {
    uint32_t b = 0;
    if (t < 200) b |= BIT(B_SQUARE);                     /* MOD 1 pressionado de 0 a 200 */
    if (t >= 100 && t < 200) b |= BIT(B_CROSS);          /* MOD 0 responde de 100 a 200 */
    return b;
}
static void testes_assoc(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    r.z[1].assoc = BIT(0);                               /* MOD 1 so responde enquanto MOD 0 responde */
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_assoc, 300, 50, 1) != 0) ok = 0;                 /* MOD 0 nao responde: congelado */
    if (saida_em(&r, cr_assoc, 300, 150, 1) != BIT(D_TRIANGLE)) ok = 0;  /* MOD 0 responde desde 100 */
    if (saida_em(&r, cr_assoc, 300, 250, 1) != 0) ok = 0;                /* MOD 0 caiu em 200 */
    passo("ASSOC: so responde enquanto o alvo responder; fora disso, congelado", ok,
          "congelado em 50; responde em 150; congelado em 250");
}

/* -------------------------------------------------------- EXCL INIC */
/* MOD 0: STATIC em L1, travado pelo SQUARE. MOD 1: DIRECT no CROSS (alvo do EXCL).
 * SQUARE em 0..10 (trava); CROSS de 100 a 200 (alvo sobe em 100); SQUARE de novo em 150. */
static uint32_t cr_excl(uint64_t t) {
    uint32_t b = 0;
    if (t < 10) b |= BIT(B_SQUARE);
    if (t >= 100 && t < 200) b |= BIT(B_CROSS);
    if (t >= 150 && t < 160) b |= BIT(B_SQUARE);
    return b;
}
static void testes_excl_inic(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_SQUARE, DELTA_STATIC, D_L1);
    mod_resposta(&r.mod[1], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    r.z[0].excl = BIT(1);
    r.z[0].excl_inic = 1;
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_excl, 400, 50, 0) != BIT(D_L1)) ok = 0;   /* travado */
    if (saida_em(&r, cr_excl, 400, 130, 0) != 0) ok = 0;          /* alvo subiu em 100: encerrado */
    if (saida_em(&r, cr_excl, 400, 180, 0) != 0) ok = 0;          /* travar de novo nao funciona */
    if (saida_em(&r, cr_excl, 400, 380, 0) != 0) ok = 0;          /* disparo unico: sem rearme */
    passo("EXCL INIC: encerra quando o alvo sobe; disparo unico, sem rearme por si so", ok,
          "travado em 50; encerrado em 130, 180 e 380");
}

/* -------------------------------------------------------- EXCL FIM */
/* MOD 0 travado pelo SQUARE em 0..10. Alvo (CROSS) de 100 a 200. FIM: encerra quando o alvo cai. */
static uint32_t cr_excl_fim(uint64_t t) {
    uint32_t b = 0;
    if (t < 10) b |= BIT(B_SQUARE);
    if (t >= 100 && t < 200) b |= BIT(B_CROSS);
    return b;
}
static void testes_excl_fim(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_SQUARE, DELTA_STATIC, D_L1);
    mod_resposta(&r.mod[1], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    r.z[0].excl = BIT(1);
    r.z[0].excl_inic = 0;                                 /* FIM */
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_excl_fim, 400, 150, 0) != BIT(D_L1)) ok = 0;  /* alvo sobe: nada */
    if (saida_em(&r, cr_excl_fim, 400, 230, 0) != 0) ok = 0;          /* alvo caiu em 200: encerrado */
    passo("EXCL FIM: nao encerra quando o alvo sobe; encerra quando ele termina", ok,
          "travado em 150; encerrado em 230");
}

/* ------------------------------------------------------ rearme por ASSOC */
/* MOD 0 (X): STATIC em L1, travado pelo SQUARE; EXCL = MOD 1 (INIC); ASSOC = MOD 2 | MOD 3.
 * MOD 2 (Z): DIRECT no TRIANGLE, responde de 0 a 300 (mantem X fora da congelacao).
 * MOD 3 (W): DIRECT no R1, pressionado de 260 a 290 (sobe em 260: rearme).
 * SQUARE: 20..30 (trava), 160..170 (encerrado: nao trava), 280..290 (trava de novo).
 * CROSS (MOD 1): 100..150 (sobe em 100: encerra X), 200..250 (sobe de novo: nao rearma). */
static uint32_t cr_rearme(uint64_t t) {
    uint32_t b = 0;
    if (t < 300) b |= BIT(B_TRIANGLE);
    if (t >= 20 && t < 30) b |= BIT(B_SQUARE);
    if (t >= 160 && t < 170) b |= BIT(B_SQUARE);
    if (t >= 280 && t < 290) b |= BIT(B_SQUARE);
    if (t >= 100 && t < 150) b |= BIT(B_CROSS);
    if (t >= 200 && t < 250) b |= BIT(B_CROSS);
    if (t >= 260 && t < 290) b |= BIT(B_R1);
    return b;
}
static void testes_rearme(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 4;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_SQUARE, DELTA_STATIC, D_L1);
    mod_resposta(&r.mod[1], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[2], B_TRIANGLE, DELTA_DIRECT, D_TRIANGLE);
    mod_resposta(&r.mod[3], B_R1, DELTA_DIRECT, D_R1);
    r.z[0].excl = BIT(1);
    r.z[0].excl_inic = 1;
    r.z[0].assoc = BIT(2) | BIT(3);
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_rearme, 400, 50, 0) != BIT(D_L1)) ok = 0;   /* travado */
    if (saida_em(&r, cr_rearme, 400, 130, 0) != 0) ok = 0;          /* encerrado em 110 */
    if (saida_em(&r, cr_rearme, 400, 240, 0) != 0) ok = 0;          /* o alvo subiu de novo: nao rearma */
    if (saida_em(&r, cr_rearme, 400, 270, 0) != 0) ok = 0;         /* rearmado em 270, ainda sem trava */
    if (saida_em(&r, cr_rearme, 400, 290, 0) != BIT(D_L1)) ok = 0;  /* trava em 280 */
    passo("EXCL: rearme so pela subida de um MOD ligado por ASSOC", ok,
          "encerrado em 130 e 240; rearmado em 270 (um quadro depois de W subir); travado em 280");
}

/* ----------------------------------------------------------------- ORFAO */
/* MOD 0: CROSS -> DIRECT no CIRCLE (o alvo). MOD 1: sem fonte, ASSOC ao MOD 0, em TRIANGLE.
 * CROSS de 100 a 200. A defasagem de um quadro: o orfao liga no quadro seguinte a subida do
 * alvo (110), e desliga no quadro seguinte a queda (210). */
static uint32_t cr_orfao(uint64_t t) {
    return (t >= 100 && t < 200) ? BIT(B_CROSS) : 0;
}
static void mod_orfao(ModCfg *m, uint8_t tipo) {
    mod_base(m);
    m->alfa.abas[ABA_LIVRE].n = 0;                       /* sem fonte */
    m->delta.tipo = tipo;
    m->delta.n_dest = 1;
    m->delta.dest[0] = D_TRIANGLE;
}
static void testes_orfao(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_orfao(&r.mod[1], DELTA_DIRECT);
    r.z[1].assoc = BIT(0);
    if (rede_validar(&r) != 0) ok = 0;                                     /* orfao adotado: valido */
    { RedeCfg x = r; x.z[1].assoc = 0; if (rede_validar(&x) != -1) ok = 0; }  /* sem alvo: sem resposta */
    passo("orfao: valido so com ASSOC; sem alvo, recusado", ok, "");

    ok = 1;
    if (saida_em(&r, cr_orfao, 300, 100, 1) != 0) ok = 0;                 /* alvo ainda nao subiu */
    if (saida_em(&r, cr_orfao, 300, 150, 1) != BIT(D_TRIANGLE)) ok = 0;   /* sobe com o alvo */
    if (saida_em(&r, cr_orfao, 300, 210, 1) != 0) ok = 0;                 /* desce com o alvo */
    passo("orfao DIRECT: sobe junto com o alvo e desce junto", ok,
          "desligado em 100; ligado em 150; desligado em 210");

    /* STATIC: a subida comeca a resposta, e so a queda do alvo (FINALIZA) a encerra */
    ok = 1;
    r.mod[1].delta.tipo = DELTA_STATIC;
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_orfao, 300, 150, 1) != BIT(D_TRIANGLE)) ok = 0;
    if (saida_em(&r, cr_orfao, 300, 250, 1) != 0) ok = 0;
    passo("orfao STATIC: liga na subida do alvo e encerra na queda", ok,
          "ligado em 150; desligado em 250");
}

int main(void) {
    testes_orfao();
    testes_validacao();
    testes_bloq();
    testes_assoc();
    testes_excl_inic();
    testes_excl_fim();
    testes_rearme();
    if (falhas) { printf("  => BANCADA DA ZETA FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada da ZETA aberta: todas as verificacoes passaram\n");
    return 0;
}
