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
/* MOD 0: CROSS -> DIRECT no CIRCLE (o alvo). MOD 1: orfao, sem gatilho que ligue, ASSOC ao MOD 0,
 * em TRIANGLE. CROSS de 100 a 200 e de 300 a 400. Defasagem de um quadro: a adocao liga o orfao
 * no quadro seguinte a subida do alvo (110 e 310). Manual, secao 7. */
static uint32_t cr_orfao(uint64_t t) {
    return ((t >= 100 && t < 200) || (t >= 300 && t < 400)) ? BIT(B_CROSS) : 0;
}
/* o mesmo, com SQUARE de 150 a 160: gatilho do orfao em FINALIZA (manual, secao 7.6) */
static uint32_t cr_orfao_fin(uint64_t t) {
    uint32_t b = cr_orfao(t);
    if (t >= 150 && t < 160) b |= BIT(B_SQUARE);
    return b;
}
static void mod_orfao(ModCfg *m, uint8_t tipo, int com_finaliza) {
    mod_base(m);
    m->delta.tipo = tipo;
    m->delta.n_dest = 1;
    m->delta.dest[0] = D_TRIANGLE;
    if (com_finaliza) {
        m->alfa.abas[ABA_LIVRE].n = 1;
        m->alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_BOTAO;
        m->alfa.abas[ABA_LIVRE].fontes[0].id = B_SQUARE;
        m->alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
        m->acao[ABA_LIVRE] = ACAO_FINALIZA;
    }
}
static void testes_orfao(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_orfao(&r.mod[1], DELTA_DIRECT, 0);
    r.z[1].assoc = BIT(0);
    if (rede_validar(&r) != 0) ok = 0;                                        /* adotado: valido */
    /* sem alvo: aceito, mas inerte. Nunca liga, e o alerta avisa (manual do ZETA, secao 11.1) */
    { RedeCfg x = r; uint32_t al; x.z[1].assoc = 0;
      if (rede_validar(&x) != 0) ok = 0;
      al = rede_alertas(&x, 1);
      if (!(al & ALERTA_INERTE)) ok = 0;
      if (saida_em(&x, cr_orfao, 400, 150, 1) != 0) ok = 0; }
    passo("orfao: adotado pela ASSOC liga; sem alvo, e aceito, inerte, com alerta, e nunca liga", ok, "");

    /* DIRECT: a adocao liga; a queda do alvo so silencia (ASSOC), nao desliga; a nova subida volta */
    ok = 1;
    if (saida_em(&r, cr_orfao, 400, 100, 1) != 0) ok = 0;                 /* alvo ainda nao subiu */
    if (saida_em(&r, cr_orfao, 400, 150, 1) != BIT(D_TRIANGLE)) ok = 0;   /* adotado na subida */
    if (saida_em(&r, cr_orfao, 400, 250, 1) != 0) ok = 0;                 /* alvo caiu: silenciado */
    if (saida_em(&r, cr_orfao, 400, 310, 1) != BIT(D_TRIANGLE)) ok = 0;   /* nova subida */
    passo("orfao DIRECT: adotado na subida; silenciado na queda; volta na nova subida", ok,
          "desligado em 100; ligado em 150; silenciado em 250; ligado em 310");

    /* DIRECT com FINALIZA: o gatilho desliga, e o alvo ainda responde. Nao volta sozinho. */
    ok = 1;
    mod_orfao(&r.mod[1], DELTA_DIRECT, 1);
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_orfao_fin, 400, 120, 1) != BIT(D_TRIANGLE)) ok = 0;  /* adotado */
    if (saida_em(&r, cr_orfao_fin, 400, 160, 1) != 0) ok = 0;                /* FINALIZA desligou */
    if (saida_em(&r, cr_orfao_fin, 400, 180, 1) != 0) ok = 0;                /* alvo ainda responde: nao volta */
    if (saida_em(&r, cr_orfao_fin, 400, 310, 1) != BIT(D_TRIANGLE)) ok = 0;  /* so volta na nova subida */
    passo("orfao com FINALIZA: desligado pelo gatilho; nao volta enquanto o alvo ainda responde", ok,
          "ligado em 120; desligado em 160 e 180; ligado em 310");

    /* STATIC: mesma regra; a trava e preservada na queda e so o FINALIZA a desliga */
    ok = 1;
    mod_orfao(&r.mod[1], DELTA_STATIC, 0);
    if (rede_validar(&r) != 0) ok = 0;
    if (saida_em(&r, cr_orfao, 400, 150, 1) != BIT(D_TRIANGLE)) ok = 0;
    if (saida_em(&r, cr_orfao, 400, 250, 1) != 0) ok = 0;                 /* silenciado, trava preservada */
    if (saida_em(&r, cr_orfao, 400, 310, 1) != BIT(D_TRIANGLE)) ok = 0;
    passo("orfao STATIC: adotado na subida; silenciado na queda, trava preservada", ok,
          "ligado em 150; silenciado em 250; ligado em 310");
}

/* ---------------------------------------------------------------- ALERTAS */
static void testes_alertas(void) {
    RedeCfg r; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    if (rede_alertas(&r, 0) != 0 || rede_alertas(&r, 1) != 0) ok = 0;     /* rede normal: sem alerta */
    { RedeCfg x = r; x.z[1].assoc = BIT(0); x.dom[1] = 0x2;                /* alvo de outro dominio */
      if (!(rede_alertas(&x, 1) & ALERTA_ALVO_OUTRO_DOMINIO)) ok = 0;
      if (rede_alertas(&x, 0) & ALERTA_ALVO_OUTRO_DOMINIO) ok = 0; }
    { RedeCfg x = r; x.z[1].bloq = BIT(0); x.z[1].excl = BIT(0);          /* BLOQ e EXCL no mesmo alvo */
      if (!(rede_alertas(&x, 1) & ALERTA_EXCL_BLOQ_MESMO)) ok = 0; }
    passo("alertas: sem alerta na rede normal; alvo de outro dominio; EXCL e BLOQ no mesmo alvo", ok,
          "rede de exemplo limpa; cada alerta acende so no caso dele");
}

/* ------------------------------------------------------- EIXOS e CADEIA */
static Fonte eixo_fonte(uint8_t id) {
    Fonte f; memset(&f, 0, sizeof f);
    f.tipo = FONTE_EIXO; f.id = id;
    f.beta.mult = 9; f.beta.thr_in = 30; f.beta.thr_out = 0; f.beta.dir = DIR_ABS;
    return f;
}
/* MOD com um eixo como fonte (LIVRE, nivel), resposta DIRECT no destino dado */
static void mod_eixo(ModCfg *m, uint8_t eixo, uint8_t dest) {
    mod_base(m);
    m->alfa.abas[ABA_LIVRE].n = 1;
    m->alfa.abas[ABA_LIVRE].eou = EOU_OU;
    m->alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    m->alfa.abas[ABA_LIVRE].fontes[0] = eixo_fonte(eixo);
    m->delta.tipo = DELTA_DIRECT;
    m->delta.n_dest = 1;
    m->delta.dest[0] = dest;
}
/* um quadro com a rede limpa */
static void um_quadro(const RedeCfg *r, const Entrada *in, RedeSaida *s) {
    RedeEstado e;
    rede_iniciar(&e);
    e.dom.atual = 1;
    rede_avaliar(r, &e, in, s);
}
#define D_LX 10          /* destino 10 = eixo LX (manual, secao 14.2) */
#define EIX_LX 0         /* EIXO_LX em alfa.h */
#define EIX_LY 1
#define EIX_RX 2

static void testes_eixos(void) {
    RedeCfg r; Entrada in; RedeSaida s; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_eixo(&r.mod[0], EIX_RX, D_LX);
    mod_eixo(&r.mod[1], EIX_LY, D_LX);
    if (rede_validar(&r) != 0) ok = 0;

    memset(&in, 0, sizeof in); in.eixo[EIX_RX] = 40; in.eixo[EIX_LY] = 70;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != 70) ok = 0;                          /* dois escrevem LX: vence o de 70 */
    memset(&in, 0, sizeof in); in.eixo[EIX_RX] = 40;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != 40) ok = 0;                          /* so um escreve: 40 */
    memset(&in, 0, sizeof in); in.eixo[EIX_LX] = 25;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != 25) ok = 0;                          /* ninguem escreve: passa o cru, 25 */
    passo("eixos: o maior vence entre os que escrevem LX; sem escrita, passa o cru", ok,
          "70 contra 40 -> 70; so 40 -> 40; sem escrita, 25 cru");
}

static void testes_cadeia(void) {
    RedeCfg r; RedeEstado e; RedeSaida s; Entrada in; int ok = 1;
    uint64_t f;
    unsigned k;
    memset(&r, 0, sizeof r);
    r.n = 6;
    todos_d1(&r);
    mod_eixo(&r.mod[0], EIX_RX, D_CIRCLE);            /* MOD 0: a fonte, com intensidade 60 */
    for (k = 1; k < 6; k++) {                         /* MODs 1 a 5: orfaos, em cadeia, ASSOC ao anterior */
        mod_orfao(&r.mod[k], DELTA_DIRECT, 0);
        r.mod[k].delta.dest[0] = D_TRIANGLE;
        r.z[k].assoc = BIT(k - 1);
    }
    if (rede_validar(&r) != 0) ok = 0;

    rede_iniciar(&e);
    e.dom.atual = 1;
    /* RX = 60 o tempo todo: a fonte fica na zona. Quadros de 10 ms; a cadeia desce um nivel por quadro */
    for (f = 0; f <= 12; f++) {
        memset(&in, 0, sizeof in); in.eixo[EIX_RX] = 60; in.t_us = f * 10000u;
        rede_avaliar(&r, &e, &in, &s);
    }
    /* orfao 4 esta no nivel 4 da cadeia: herda 60 e responde */
    if (s.mod[4].intensidade != 60 || !(s.mod[4].destinos & BIT(D_TRIANGLE))) ok = 0;
    /* orfao 5 esta no nivel 5: passa do limite de 4 niveis, o valor e zero, e ele se encerra */
    if (s.mod[5].intensidade != 0 || (s.mod[5].destinos & BIT(D_TRIANGLE))) ok = 0;
    passo("cadeia de orfaos: herda ate 4 niveis; no 5o nivel, zero, e autoencerra", ok,
          "orfao 4 com 60 e ligado; orfao 5 com 0 e desligado");

    /* a fonte sai da zona: a cadeia cai a zero, e os orfaos se encerram */
    ok = 1;
    for (f = 13; f <= 25; f++) {
        memset(&in, 0, sizeof in); in.eixo[EIX_RX] = 0; in.t_us = f * 10000u;
        rede_avaliar(&r, &e, &in, &s);
    }
    if (s.mod[1].intensidade != 0 || (s.mod[1].destinos & BIT(D_TRIANGLE))) ok = 0;
    passo("cadeia sem fonte: sem valor, o orfao se encerra", ok, "fonte fora da zona: orfao 1 com 0 e desligado");
}

/* ---------------------------------------------------- toque oculto na rede */
static void testes_toque_rede(void) {
    RedeCfg r; Entrada in; RedeSaida s; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_resposta(&r.mod[0], B_CROSS, DELTA_DIRECT, D_CIRCLE);
    mod_resposta(&r.mod[1], B_SQUARE, DELTA_DIRECT, D_TRIANGLE);
    r.mod[1].alfa.abas[ABA_LIVRE].n = 1;
    r.mod[1].alfa.abas[ABA_LIVRE].oculto = 1;
    r.mod[1].alfa.abas[ABA_LIVRE].fontes[0].tipo = FONTE_DEDO;
    r.mod[1].alfa.abas[ABA_LIVRE].fontes[0].id = 0;
    if (rede_validar(&r) != 0) ok = 0;
    memset(&in, 0, sizeof in); in.dedo[0] = 1;
    um_quadro(&r, &in, &s);
    if (!s.ocultar_toque) ok = 0;                          /* um MOD oculta o toque: a rede o suprime */
    r.mod[1].alfa.abas[ABA_LIVRE].oculto = 0;
    um_quadro(&r, &in, &s);
    if (s.ocultar_toque) ok = 0;
    passo("toque na rede: um MOD com dedo oculto suprime o toque da saida; sem ocultar, passa", ok, "");
}

/* ------------------------------------------------------ eixos com sinal */
/* O acumulador guarda o sinal, e o maior em modulo vence (manual, 10.1). Empate em modulo: o
 * primeiro avaliado (o de numero menor). */
static void testes_eixos_sinal(void) {
    RedeCfg r; Entrada in; RedeSaida s; int ok = 1;
    memset(&r, 0, sizeof r);
    r.n = 2;
    todos_d1(&r);
    mod_eixo(&r.mod[0], EIX_RX, D_LX);
    mod_eixo(&r.mod[1], EIX_LY, D_LX);
    memset(&in, 0, sizeof in); in.eixo[EIX_RX] = -70; in.eixo[EIX_LY] = 40;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != -70) ok = 0;                     /* -70 contra +40: vence o -70 */
    memset(&in, 0, sizeof in); in.eixo[EIX_RX] = 70; in.eixo[EIX_LY] = -70;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != 70) ok = 0;                      /* +70 contra -70: empate, fica o primeiro (+70) */
    memset(&in, 0, sizeof in); in.eixo[EIX_RX] = -70; in.eixo[EIX_LY] = 90;
    um_quadro(&r, &in, &s);
    if (s.eixo[0] != 90) ok = 0;                      /* +90 contra -70: o maior em modulo, +90 */
    passo("eixos com sinal: -70 contra +40 e -70; empate fica o primeiro; +90 contra -70 vai a +90", ok,
          "manual, 10.1 e 6.1");
}

/* ------------------------------------------------ orfao e sinal (achado 2) */
/* Fonte de eixo com LX = -80 (sinal -1, intensidade 80); orfao no RY (13). Medido no simulador
 * do Cronus (achado 2): DIRECT orfao preserva o sinal e a intensidade; STATIC orfao vale 100, +. */
static void orfao_no_ry(RedeCfg *r, uint8_t tipo) {
    memset(r, 0, sizeof *r);
    r->n = 2;
    todos_d1(r);
    mod_eixo(&r->mod[0], EIX_LX, D_LX);
    mod_orfao(&r->mod[1], tipo, 0);
    r->mod[1].delta.dest[0] = 13;                      /* RY */
    r->z[1].assoc = BIT(0);
}
static void testes_orfao_sinal(void) {
    RedeCfg r; RedeEstado e; RedeSaida s; Entrada in; int ok = 1; uint64_t f;
    orfao_no_ry(&r, DELTA_DIRECT);
    rede_iniciar(&e); e.dom.atual = 1;
    for (f = 0; f <= 3; f++) {
        memset(&in, 0, sizeof in); in.eixo[EIX_LX] = -80; in.t_us = f * 10000u;
        rede_avaliar(&r, &e, &in, &s);
    }
    if (s.mod[1].intensidade != 80 || s.mod[1].sinal != -1 || s.eixo[3] != -80) ok = 0;
    passo("orfao DIRECT: preserva o sinal e a intensidade do alvo (RY = -80)", ok,
          "fonte LX -80 -> orfao RY -80");

    ok = 1;
    orfao_no_ry(&r, DELTA_STATIC);
    rede_iniciar(&e); e.dom.atual = 1;
    for (f = 0; f <= 3; f++) {
        memset(&in, 0, sizeof in); in.eixo[EIX_LX] = -80; in.t_us = f * 10000u;
        rede_avaliar(&r, &e, &in, &s);
    }
    if (s.mod[1].intensidade != 100 || s.mod[1].sinal != 1 || s.eixo[3] != 100) ok = 0;
    passo("orfao STATIC: escreve 100 fixo, +, sem herdar a intensidade (RY = 100)", ok,
          "fonte LX -80 -> orfao STATIC RY 100");

    /* TURBO em eixo: escreve 100, +, sem herdar (achado 2) */
    ok = 1;
    orfao_no_ry(&r, DELTA_TURBO);
    r.mod[1].delta.tempo[0] = idx_ms(100);
    r.mod[1].delta.ciclos = DELTA_CICLOS_ATIVO;
    rede_iniciar(&e); e.dom.atual = 1;
    for (f = 0; f <= 3; f++) {
        memset(&in, 0, sizeof in); in.eixo[EIX_LX] = -80; in.t_us = f * 10000u;
        rede_avaliar(&r, &e, &in, &s);
    }
    if (s.mod[1].intensidade != 100 || s.mod[1].sinal != 1 || s.eixo[3] != 100) ok = 0;
    passo("orfao TURBO em eixo: escreve 100, +, sem herdar (RY = 100)", ok,
          "fonte LX -80 -> orfao TURBO RY 100 na fase ligada");
}

int main(void) {
    testes_orfao_sinal();
    testes_eixos_sinal();
    testes_toque_rede();
    testes_eixos();
    testes_cadeia();
    testes_alertas();
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
