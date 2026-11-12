/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A REDE de MODs com a ZETA: BLOQ, ASSOC e EXCL entre os MODs. Ver docs/ARQUITETURA.md, secao 22.
 *
 * Cada relacao e uma mascara de bits sobre os indices dos MODs (bit i = MOD i).
 * As relacoes olham o estado do QUADRO ANTERIOR (defasagem de um quadro, manual secao 24.1),
 * assim o resultado nao depende da ordem dos MODs.
 */
#pragma once
#include <stdint.h>
#include "modelo.h"

#define REDE_MAX 20

typedef struct {
    uint32_t bloq;          /* BLOQ: MODs que suspendem este MOD enquanto respondem */
    uint32_t assoc;         /* ASSOC: este MOD so responde enquanto um destes responder */
    uint32_t excl;          /* EXCL: MODs que encerram este MOD */
    uint8_t excl_inic;      /* 1 = EXCL dispara na subida do alvo (INIC); 0 = na finalizacao (FIM) */
} Zeta;

typedef struct {
    uint8_t n;
    ModCfg mod[REDE_MAX];
    Zeta z[REDE_MAX];
} RedeCfg;

typedef struct {
    ModEstado mod[REDE_MAX];
    uint8_t encerrado[REDE_MAX];
    uint32_t resp;          /* MODs com resposta de pe, no quadro anterior */
    uint32_t sobe;          /* MODs cuja resposta subiu no quadro anterior */
    uint32_t cai;           /* MODs cuja resposta caiu no quadro anterior */
} RedeEstado;

typedef struct {
    ModSaida mod[REDE_MAX];
} RedeSaida;

/* 0 se valido, -1 se nao. Recusa: MOD invalido, relacao com indice fora de 0..n-1,
 * relacao de um MOD consigo mesmo, e mais de REDE_MAX. */
int rede_validar(const RedeCfg *r);
void rede_iniciar(RedeEstado *e);
void rede_avaliar(const RedeCfg *r, RedeEstado *e, const Entrada *in, RedeSaida *out);
