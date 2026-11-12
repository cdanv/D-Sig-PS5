/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver formato.h e docs/ARQUITETURA.md, secao 27.
 */
#include "formato.h"
#include "alfa.h"
#include <string.h>

static const uint8_t MAGIC[4] = { 'D', 'S', 'G', '1' };

uint32_t formato_crc32(const uint8_t *dados, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    unsigned k;
    for (i = 0; i < n; i++) {
        crc ^= dados[i];
        for (k = 0; k < 8; k++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return ~crc;
}

/* ----------------------------------------------------- escrita e leitura de inteiros */
static void po8(uint8_t **p, uint8_t v) { *(*p)++ = v; }
static void po32(uint8_t **p, uint32_t v) {
    (*p)[0] = (uint8_t)v; (*p)[1] = (uint8_t)(v >> 8);
    (*p)[2] = (uint8_t)(v >> 16); (*p)[3] = (uint8_t)(v >> 24);
    *p += 4;
}
static uint8_t gi8(const uint8_t **p) { return *(*p)++; }
static uint32_t gi32(const uint8_t **p) {
    uint32_t v = (uint32_t)(*p)[0] | ((uint32_t)(*p)[1] << 8) |
                 ((uint32_t)(*p)[2] << 16) | ((uint32_t)(*p)[3] << 24);
    *p += 4;
    return v;
}

static void po_beta(uint8_t **p, const Beta *b) {
    po8(p, b->mult); po8(p, b->thr_in); po8(p, b->thr_out); po8(p, b->dir); po8(p, b->inv);
}
static void go_beta(const uint8_t **p, Beta *b) {
    b->mult = gi8(p); b->thr_in = gi8(p); b->thr_out = gi8(p); b->dir = gi8(p); b->inv = gi8(p);
}

static void po_mod(uint8_t **p, const ModCfg *m) {
    unsigned a, k, j;
    po8(p, m->alfa.janela);
    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &m->alfa.abas[a];
        po8(p, ab->n); po8(p, ab->eou); po8(p, ab->oculto); po8(p, ab->tempo); po8(p, ab->delay);
        for (k = 0; k < ALFA_POR_ABA; k++) po8(p, ab->espera[k]);
        for (k = 0; k < ALFA_POR_ABA; k++) {
            po8(p, ab->fontes[k].tipo);
            po8(p, ab->fontes[k].id);
            po_beta(p, &ab->fontes[k].beta);
        }
    }
    for (a = 0; a < ALFA_ABAS; a++) po8(p, m->acao[a]);
    po8(p, m->delta.tipo); po8(p, m->delta.n_dest);
    for (j = 0; j < DELTA_MAX_PASSOS; j++) po8(p, m->delta.dest[j]);
    for (j = 0; j < DELTA_MAX_PASSOS; j++) po8(p, m->delta.tempo[j]);
    po8(p, m->delta.lat); po8(p, m->delta.ciclos); po8(p, m->delta.origem_pulso);
    po8(p, m->delta.modo);
}

static void go_mod(const uint8_t **p, ModCfg *m) {
    unsigned a, k, j;
    m->alfa.janela = gi8(p);
    for (a = 0; a < ALFA_ABAS; a++) {
        Aba *ab = &m->alfa.abas[a];
        ab->n = gi8(p); ab->eou = gi8(p); ab->oculto = gi8(p); ab->tempo = gi8(p); ab->delay = gi8(p);
        for (k = 0; k < ALFA_POR_ABA; k++) ab->espera[k] = gi8(p);
        for (k = 0; k < ALFA_POR_ABA; k++) {
            ab->fontes[k].tipo = gi8(p);
            ab->fontes[k].id = gi8(p);
            go_beta(p, &ab->fontes[k].beta);
        }
    }
    for (a = 0; a < ALFA_ABAS; a++) m->acao[a] = gi8(p);
    m->delta.tipo = gi8(p); m->delta.n_dest = gi8(p);
    for (j = 0; j < DELTA_MAX_PASSOS; j++) m->delta.dest[j] = gi8(p);
    for (j = 0; j < DELTA_MAX_PASSOS; j++) m->delta.tempo[j] = gi8(p);
    m->delta.lat = gi8(p); m->delta.ciclos = gi8(p); m->delta.origem_pulso = gi8(p);
    m->delta.modo = gi8(p);
}

int formato_gravar(const RedeCfg *r, uint8_t *buf, size_t cap, size_t *len) {
    uint8_t *p = buf;
    unsigned i;
    size_t total;
    if (r == 0 || buf == 0 || len == 0) return -1;
    if (rede_validar(r) != 0) return -1;
    /* por MOD: o bloco da ALFA/acoes/DELTA, mais ZETA (4+4+4+1) e dominio (1) */
    total = FORMATO_CABECALHO + (size_t)r->n * (FORMATO_BLOCO_ALFA_DELTA + FORMATO_ZETA_DOM) + FORMATO_RODAPE;
    if (total > cap) return -1;

    memcpy(p, MAGIC, 4); p += 4;
    po8(&p, FORMATO_VERSAO);
    po8(&p, r->n);
    po8(&p, 0); po8(&p, 0);                              /* reservado */
    for (i = 0; i < r->n; i++) {
        po_mod(&p, &r->mod[i]);
        po32(&p, r->z[i].bloq);
        po32(&p, r->z[i].assoc);
        po32(&p, r->z[i].excl);
        po8(&p, r->z[i].excl_inic);
        po8(&p, r->dom[i]);
    }
    *len = (size_t)(p - buf) + FORMATO_RODAPE;
    po32(&p, formato_crc32(buf, (size_t)(p - buf)));
    return 0;
}

int formato_ler(const uint8_t *buf, size_t len, RedeCfg *r) {
    const uint8_t *p = buf;
    unsigned i, n;
    uint32_t crc_lido, crc_calc;
    size_t corpo;
    if (buf == 0 || r == 0) return -1;
    if (len < FORMATO_CABECALHO + FORMATO_RODAPE) return -1;

    corpo = len - FORMATO_RODAPE;
    crc_calc = formato_crc32(buf, corpo);
    crc_lido = (uint32_t)buf[corpo] | ((uint32_t)buf[corpo + 1] << 8) |
               ((uint32_t)buf[corpo + 2] << 16) | ((uint32_t)buf[corpo + 3] << 24);
    if (crc_lido != crc_calc) return -1;

    if (memcmp(p, MAGIC, 4) != 0) return -1;
    p += 4;
    if (gi8(&p) != FORMATO_VERSAO) return -1;
    n = gi8(&p);
    if (n == 0 || n > REDE_MAX) return -1;
    p += 2;                                              /* reservado */

    memset(r, 0, sizeof *r);
    r->n = (uint8_t)n;
    for (i = 0; i < n; i++) {
        if ((size_t)(p - buf) + FORMATO_BLOCO_ALFA_DELTA + FORMATO_ZETA_DOM > corpo) return -1;
        go_mod(&p, &r->mod[i]);
        r->z[i].bloq = gi32(&p);
        r->z[i].assoc = gi32(&p);
        r->z[i].excl = gi32(&p);
        r->z[i].excl_inic = gi8(&p);
        r->dom[i] = gi8(&p);
    }
    if ((size_t)(p - buf) != corpo) return -1;           /* sobra ou falta byte */
    return rede_validar(r) == 0 ? 0 : -1;
}
