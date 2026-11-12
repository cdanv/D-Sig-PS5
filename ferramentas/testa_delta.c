/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO DELTA. Roda no Linux, sem console. Resultados esperados escritos a mao,
 * quadro a quadro, a partir das regras de docs/ARQUITETURA.md, secao 20.
 */
#include "../src/delta.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define D_L1 0
#define D_CROSS 6
#define D_CIRCLE 7
#define D_SQUARE 8
#define BIT(n) (1u << (n))

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

static void cfg_base(DeltaCfg *c, uint8_t tipo) {
    memset(c, 0, sizeof *c);
    c->tipo = tipo;
    c->lat = DELTA_SEM_LAT;
    c->ciclos = DELTA_CICLOS_ATIVO;
}

/* um quadro: liga, alterna, finaliza (pulsos), e o instante em ms */
typedef struct { uint8_t liga, alterna, finaliza; uint64_t t; } Q;

static void quadro(const DeltaCfg *c, DeltaEstado *e, Q q, DeltaSaida *s) {
    DeltaEntrada in;
    in.liga = q.liga; in.alterna = q.alterna; in.finaliza = q.finaliza; in.congelado = 0;
    in.t_us = q.t * 1000u;
    delta_avaliar(c, e, &in, s);
}

/* ------------------------------------------------------------ validacao */
static void testes_validacao(void) {
    DeltaCfg c;
    int ok = 1;

    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS; c.tempo[0] = (uint8_t)idx_ms(100);
    c.lat = (uint8_t)idx_ms(30);
    if (delta_validar(&c) != 0) ok = 0;

    { DeltaCfg x; cfg_base(&x, DELTA_DIRECT); x.n_dest = 1; x.dest[0] = D_CROSS; x.origem_pulso = 1;
      if (delta_validar(&x) != -1) ok = 0; }                             /* DIRECT nao aceita pulso */
    { DeltaCfg x; cfg_base(&x, DELTA_STATIC); x.n_dest = 0;
      if (delta_validar(&x) != -1) ok = 0; }
    { DeltaCfg x; cfg_base(&x, DELTA_STATIC); x.n_dest = 5;
      if (delta_validar(&x) != -1) ok = 0; }                             /* STATIC: ate 4 destinos */
    { DeltaCfg x = c; x.n_dest = 2; if (delta_validar(&x) != -1) ok = 0; } /* TURBO: um passo */
    { DeltaCfg x; cfg_base(&x, DELTA_COMBO); x.n_dest = 11;
      if (delta_validar(&x) != -1) ok = 0; }                             /* COMBO: ate 10 passos */
    { DeltaCfg x = c; x.dest[0] = 18; if (delta_validar(&x) != -1) ok = 0; }
    { DeltaCfg x = c; x.ciclos = 8; if (delta_validar(&x) != -1) ok = 0; }
    { DeltaCfg x = c; x.ciclos = 0; if (delta_validar(&x) != -1) ok = 0; }  /* ciclos: ATIVO ou 1..7 */
    { DeltaCfg x = c; x.lat = (uint8_t)tempo_quantos(); if (delta_validar(&x) != -1) ok = 0; }
    { DeltaCfg x; cfg_base(&x, DELTA_DIRECT); x.n_dest = 1; x.dest[0] = D_CROSS; x.ciclos = 3;
      if (delta_validar(&x) != -1) ok = 0; }                             /* DIRECT nao tem ciclos */
    if (delta_validar(0) != -1) ok = 0;
    passo("validacao: tipos, destinos, passos, ciclos, latencia e origem de DIRECT", ok, "");
}

/* ---------------------------------------------------------- DIRECT */
static void testes_direct(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s; DeltaEntrada in;
    int ok = 1;
    cfg_base(&c, DELTA_DIRECT); c.n_dest = 1; c.dest[0] = D_CROSS;
    delta_iniciar(&e);
    /* nivel: liga de 100 a 199 ms, com quadros de 10 ms. O nivel fica alto varios quadros
     * seguidos: quem so reage a subida erra nos quadros do meio. */
    {
        uint64_t t;
        for (t = 0; t <= 300; t += 10) {
            in.liga = (t >= 100 && t < 200); in.alterna = 0; in.finaliza = 0; in.congelado = 0; in.t_us = t * 1000u;
            delta_avaliar(&c, &e, &in, &s);
            if (((s.destinos & BIT(D_CROSS)) != 0) != (in.liga != 0)) ok = 0;
        }
    }
    passo("DIRECT: o destino acompanha o nivel da origem", ok, "");
}

/* ---------------------------------------------------------- STATIC */
static void testes_static(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s;
    int ok = 1;
    cfg_base(&c, DELTA_STATIC); c.n_dest = 2; c.dest[0] = D_L1; c.dest[1] = D_CROSS;
    delta_iniciar(&e);

    quadro(&c, &e, (Q){1,0,0,0}, &s);                   /* LIGA em 0: trava */
    if (s.destinos != (BIT(D_L1) | BIT(D_CROSS))) ok = 0;
    quadro(&c, &e, (Q){0,0,0,50}, &s);                  /* soltar nao destrava */
    if (s.destinos != (BIT(D_L1) | BIT(D_CROSS))) ok = 0;
    quadro(&c, &e, (Q){1,0,0,80}, &s);                  /* LIGA de novo: ja travado */
    if (s.destinos != (BIT(D_L1) | BIT(D_CROSS))) ok = 0;
    quadro(&c, &e, (Q){0,0,0,90}, &s);                  /* quadro sem pulso: pulsos sao 1 quadro */
    quadro(&c, &e, (Q){0,1,0,100}, &s);                 /* ALTERNA: destrava */
    if (s.destinos != 0) ok = 0;
    quadro(&c, &e, (Q){0,0,0,150}, &s);
    quadro(&c, &e, (Q){0,1,0,200}, &s);                 /* ALTERNA: trava */
    if (s.destinos != (BIT(D_L1) | BIT(D_CROSS))) ok = 0;
    quadro(&c, &e, (Q){0,0,0,250}, &s);
    quadro(&c, &e, (Q){0,0,1,300}, &s);                 /* FINALIZA: destrava */
    if (s.destinos != 0) ok = 0;
    passo("STATIC: LIGA trava, soltar nao destrava, ALTERNA alterna, FINALIZA destrava",
          ok, "dois destinos juntos, na mesma trava");
}

/* ----------------------------------------------------------- TURBO */
static void testes_turbo(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s;
    int ok = 1;
    size_t i;
    /* passo 100 ms, latencia 30 ms, ciclos ATIVO. Ligado em 0, 50, 90; desligado em 100, 120;
     * ligado de novo em 130 e 180; FINALIZA em 200 desliga */
    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS;
    c.tempo[0] = (uint8_t)idx_ms(100); c.lat = (uint8_t)idx_ms(30);
    delta_iniciar(&e);
    {
        struct { Q q; int esperado; } f[] = {
            {{1,0,0,0},1}, {{0,0,0,50},1}, {{0,0,0,90},1}, {{0,0,0,100},0}, {{0,0,0,120},0},
            {{0,0,0,130},1}, {{0,0,0,180},1}, {{0,0,1,200},0}, {{0,0,0,260},0}
        };
        for (i = 0; i < sizeof f / sizeof f[0]; i++) {
            quadro(&c, &e, f[i].q, &s);
            if (((s.destinos & BIT(D_CROSS)) != 0) != (f[i].esperado != 0)) ok = 0;
        }
    }
    passo("TURBO com ciclos ATIVO: liga 100 ms, latencia 30 ms, repete; FINALIZA encerra", ok,
          "ligado em 0, 50, 90, 130, 180; desligado em 100, 120, 200");

    /* ciclos = 2: encerra sozinho depois do segundo ciclo, em 230 ms, sem latencia no fim */
    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS;
    c.tempo[0] = (uint8_t)idx_ms(100); c.lat = (uint8_t)idx_ms(30); c.ciclos = 2;
    delta_iniciar(&e); ok = 1;
    {
        int terminou_em = -1;
        uint64_t t;
        quadro(&c, &e, (Q){1,0,0,0}, &s);
        for (t = 10; t <= 300; t += 10) {
            quadro(&c, &e, (Q){0,0,0,t}, &s);
            if (s.terminou && terminou_em < 0) terminou_em = (int)t;
        }
        if (terminou_em != 230) ok = 0;
        if (e.ligado) ok = 0;
    }
    passo("TURBO com 2 ciclos: encerra sozinho em 230 ms, sem latencia no fim", ok, "");

    /* LIGA com a resposta rodando e ignorado */
    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS;
    c.tempo[0] = (uint8_t)idx_ms(100); c.lat = (uint8_t)idx_ms(30);
    delta_iniciar(&e); ok = 1;
    quadro(&c, &e, (Q){1,0,0,0}, &s);
    quadro(&c, &e, (Q){0,0,0,20}, &s);
    quadro(&c, &e, (Q){1,0,0,50}, &s);                  /* LIGA repetido: nao reinicia */
    quadro(&c, &e, (Q){0,0,0,100}, &s);
    if (s.destinos & BIT(D_CROSS)) ok = 0;              /* em latencia, desligado em 100 */
    quadro(&c, &e, (Q){0,0,0,130}, &s);
    if (!(s.destinos & BIT(D_CROSS))) ok = 0;           /* volta em 130, como se nunca tivesse repetido */
    passo("TURBO: LIGA com a resposta rodando e ignorado", ok, "");
}

/* ----------------------------------------------------------- COMBO */
static void testes_combo(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s;
    int ok = 1;
    size_t i;
    /* 3 passos de 50 ms, latencia 10 ms, ciclos = 1. L1 em 0..50; CROSS em 60..110;
     * CIRCLE em 120..170; termina em 170 */
    cfg_base(&c, DELTA_COMBO); c.n_dest = 3;
    c.dest[0] = D_L1; c.dest[1] = D_CROSS; c.dest[2] = D_CIRCLE;
    c.tempo[0] = c.tempo[1] = c.tempo[2] = (uint8_t)idx_ms(50);
    c.lat = (uint8_t)idx_ms(10); c.ciclos = 1;
    if (delta_validar(&c) != 0) ok = 0;
    delta_iniciar(&e);
    {
        /* quadros de 1 ms: a expectativa e a do cronograma, ms a ms, sem tolerancia */
        uint64_t t;
        int terminou_em = -1;
        for (t = 0; t <= 200; t++) {
            uint32_t esperado;
            if (t < 50) esperado = BIT(D_L1);
            else if (t < 60) esperado = 0;               /* latencia 10 ms */
            else if (t < 110) esperado = BIT(D_CROSS);
            else if (t < 120) esperado = 0;
            else if (t < 170) esperado = BIT(D_CIRCLE);
            else esperado = 0;
            quadro(&c, &e, (Q){t == 0, 0, 0, t}, &s);
            if (s.destinos != esperado) ok = 0;
            if (s.terminou && terminou_em < 0) terminou_em = (int)t;
        }
        if (terminou_em != 170) ok = 0;
    }
    passo("COMBO de 3 passos: cada destino no seu tempo, latencia entre eles, termina em 170 ms",
          ok, "L1 0-50, CROSS 60-110, CIRCLE 120-170");

    /* sem deriva: quadros de 7 ms. Cronograma: passo 0 [0,30), lat [30,40), passo 1 [40,70),
     * lat [70,80), passo 2 [80,110). Termina no primeiro quadro >= 110, que e o 112.
     * Se cada fase comecasse no quadro em que a anterior foi detectada, o termino sairia depois. */
    {
        DeltaCfg g; DeltaEstado ge; DeltaSaida gs; uint64_t t; int term = -1;
        cfg_base(&g, DELTA_COMBO); g.n_dest = 3;
        g.dest[0] = D_L1; g.dest[1] = D_CROSS; g.dest[2] = D_CIRCLE;
        g.tempo[0] = g.tempo[1] = g.tempo[2] = (uint8_t)idx_ms(30);
        g.lat = (uint8_t)idx_ms(10); g.ciclos = 1;
        delta_iniciar(&ge);
        for (t = 0; t <= 140; t += 7) {
            quadro(&g, &ge, (Q){t == 0, 0, 0, t}, &gs);
            if (gs.terminou && term < 0) term = (int)t;
        }
        passo("COMBO com quadros de 7 ms: termina no primeiro quadro depois de 110 ms, sem deriva",
              term == 112, "termino em 112 ms");
    }

    /* 10 passos e o maximo */
    {
        int ok10 = 1;
        DeltaCfg x; cfg_base(&x, DELTA_COMBO); x.n_dest = 10; x.ciclos = 1;
        for (i = 0; i < 10; i++) { x.dest[i] = (uint8_t)(i % 18); x.tempo[i] = (uint8_t)idx_ms(50); }
        if (delta_validar(&x) != 0) ok10 = 0;
        x.n_dest = 11;
        if (delta_validar(&x) != -1) ok10 = 0;
        passo("COMBO aceita ate 10 passos, e recusa 11", ok10, "");
    }
}

/* ------------------------------------------------ FINALIZA e ciclos */
static void testes_finaliza(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s;
    int ok = 1;
    /* COMBO com ciclos ATIVO nao termina sozinho: so FINALIZA encerra */
    cfg_base(&c, DELTA_COMBO); c.n_dest = 1; c.dest[0] = D_CROSS;
    c.tempo[0] = (uint8_t)idx_ms(50); c.ciclos = DELTA_CICLOS_ATIVO;
    delta_iniciar(&e);
    quadro(&c, &e, (Q){1,0,0,0}, &s);
    {
        uint64_t t;
        for (t = 10; t <= 1000; t += 10) {
            quadro(&c, &e, (Q){0,0,0,t}, &s);
            if (s.terminou) ok = 0;
        }
    }
    if (!e.ligado) ok = 0;                               /* ainda rodando, sem FINALIZA */
    quadro(&c, &e, (Q){0,0,1,1010}, &s);
    if (e.ligado) ok = 0;
    passo("COMBO com ciclos ATIVO: nao termina sozinho; FINALIZA encerra", ok, "");
}

/* ------------------------------------------- congelamento (BLOCK) no DELTA */
static void testes_congelado(void) {
    DeltaCfg c; DeltaEstado e; DeltaSaida s; DeltaEntrada in;
    int ok = 1;
    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS;
    c.tempo[0] = (uint8_t)idx_ms(100); c.lat = (uint8_t)idx_ms(30); c.ciclos = DELTA_CICLOS_ATIVO;
    delta_iniciar(&e);
    /* liga em 0; congela de 50 a 80: nenhuma saida durante o congelamento, e a fase para */
    memset(&in, 0, sizeof in); in.liga = 1; in.t_us = 0; delta_avaliar(&c, &e, &in, &s);
    if (!(s.destinos & BIT(D_CROSS))) ok = 0;
    memset(&in, 0, sizeof in); in.congelado = 1; in.t_us = 50000; delta_avaliar(&c, &e, &in, &s);
    if (s.destinos != 0) ok = 0;
    memset(&in, 0, sizeof in); in.congelado = 1; in.t_us = 70000; delta_avaliar(&c, &e, &in, &s);
    if (s.destinos != 0) ok = 0;
    /* durante o congelamento, um LIGA nao reinicia nem dispara nada */
    memset(&in, 0, sizeof in); in.congelado = 1; in.liga = 1; in.t_us = 75000; delta_avaliar(&c, &e, &in, &s);
    if (s.destinos != 0) ok = 0;
    /* o passo comecou em 0 e foi congelado em 50 (50 ms consumidos). Retoma em 80: restam
     * 50 ms, entao o passo acaba em 130 e a latencia vai de 130 a 160. */
    memset(&in, 0, sizeof in); in.t_us = 80000; delta_avaliar(&c, &e, &in, &s);
    if (!(s.destinos & BIT(D_CROSS))) ok = 0;
    memset(&in, 0, sizeof in); in.t_us = 120000; delta_avaliar(&c, &e, &in, &s);
    if (!(s.destinos & BIT(D_CROSS))) ok = 0;
    memset(&in, 0, sizeof in); in.t_us = 130000; delta_avaliar(&c, &e, &in, &s);
    if (s.destinos & BIT(D_CROSS)) ok = 0;                 /* latencia de 130 a 160 */
    passo("DELTA congelado: zera a saida, nao aceita eventos, e retoma com o tempo que faltava", ok, "passo acaba em 130 ms, depois de retomar em 80");
}

/* ------------------------------------------------------------ intensidade */
/* DIRECT segue a intensidade do quadro; STATIC trava (botao vale 100, eixo segue a zona e segura
 * fora dela); TURBO captura a intensidade do inicio. Leitura do manual, secao 26.1 e 7.4. */
static void testes_intensidade(void) {
    DeltaCfg c; DeltaEstado e; DeltaEntrada in; DeltaSaida out; int ok = 1;
    cfg_base(&c, DELTA_DIRECT); c.n_dest = 1; c.dest[0] = D_CIRCLE; c.origem_pulso = 0;
    delta_iniciar(&e);
    memset(&in, 0, sizeof in); in.liga = 1; in.intensidade = 60; in.t_us = 0;
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 60) ok = 0;
    memset(&in, 0, sizeof in); in.liga = 1; in.intensidade = 30; in.t_us = 10000;
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 30) ok = 0;                      /* segue o quadro */
    memset(&in, 0, sizeof in); in.t_us = 20000;
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 0) ok = 0;                       /* desligado: 0 */
    passo("intensidade DIRECT: segue a fonte do quadro; desligado vale 0", ok, "60, 30, depois 0");

    ok = 1;
    cfg_base(&c, DELTA_STATIC); c.n_dest = 1; c.dest[0] = D_L1; c.origem_pulso = 1;
    delta_iniciar(&e);
    memset(&in, 0, sizeof in); in.liga = 1; in.intensidade = 100; in.t_us = 0;   /* botao: 100 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 100) ok = 0;
    memset(&in, 0, sizeof in); in.t_us = 10000;            /* soltou: a trava segura 100 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 100 || !out.destinos) ok = 0;
    memset(&in, 0, sizeof in); in.intensidade = 40; in.t_us = 20000;   /* eixo na zona com 40 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 40) ok = 0;
    memset(&in, 0, sizeof in); in.t_us = 30000;            /* fora da zona: segura 40 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 40) ok = 0;
    passo("intensidade STATIC: botao trava em 100; eixo segue a zona e segura fora dela", ok,
          "100, 100 solto, 40 na zona, 40 fora dela");

    ok = 1;
    cfg_base(&c, DELTA_TURBO); c.n_dest = 1; c.dest[0] = D_CROSS; c.tempo[0] = idx_ms(100); c.origem_pulso = 1;
    delta_iniciar(&e);
    memset(&in, 0, sizeof in); in.liga = 1; in.intensidade = 50; in.t_us = 0;   /* inicio: 50 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 50 || !out.destinos) ok = 0;
    memset(&in, 0, sizeof in); in.intensidade = 90; in.t_us = 200000;   /* meio do ciclo: ainda 50 */
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 50) ok = 0;
    passo("intensidade TURBO: captura a intensidade do inicio, nao a do quadro", ok,
          "inicio 50; no meio do ciclo, com a fonte em 90, continua 50");
}

/* ------------------------------------------------------------- sinal */
static void testes_sinal(void) {
    DeltaCfg c; DeltaEstado e; DeltaEntrada in; DeltaSaida out; int ok = 1;
    cfg_base(&c, DELTA_DIRECT); c.n_dest = 1; c.dest[0] = D_CIRCLE; c.origem_pulso = 0;
    delta_iniciar(&e);
    memset(&in, 0, sizeof in); in.liga = 1; in.intensidade = 60; in.lado = -1; in.t_us = 0;
    delta_avaliar(&c, &e, &in, &out);
    if (out.intensidade != 60 || out.sinal != -1) ok = 0;   /* DIRECT leva o lado da fonte */
    passo("sinal DIRECT: leva o lado da fonte junto com a intensidade", ok, "lado -1, intensidade 60");
}

int main(void) {
    testes_sinal();
    testes_intensidade();
    testes_validacao();
    testes_direct();
    testes_static();
    testes_turbo();
    testes_combo();
    testes_finaliza();
    testes_congelado();
    if (falhas) { printf("  => BANCADA DO DELTA FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do DELTA aberta: todas as verificacoes passaram\n");
    return 0;
}
