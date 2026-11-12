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
#include "dominio.h"

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
    uint8_t dom[REDE_MAX];   /* PL_HAB: bit d-1 = o MOD roda no dominio Dd (d = 1..4). 0 = desativado */
} RedeCfg;

typedef struct {
    ModEstado mod[REDE_MAX];
    uint8_t encerrado[REDE_MAX];
    uint32_t resp;          /* MODs com resposta de pe, no quadro anterior */
    uint32_t sobe;          /* MODs cuja resposta subiu no quadro anterior */
    uint32_t cai;           /* MODs cuja resposta caiu no quadro anterior */
    uint8_t vale[REDE_MAX]; /* a intensidade que cada MOD oferece a um orfao (cadeia), quadro anterior */
    uint8_t prof[REDE_MAX]; /* a profundidade dessa intensidade: 0 = fonte propria, 1..4 = por orfaos */
    int8_t lado[REDE_MAX];  /* o sinal dessa intensidade, com o mesmo criterio */
    Dominio dom;            /* o dominio ligado */
    uint32_t trava;         /* MODs travados depois de uma troca de dominio */
} RedeEstado;

typedef struct {
    ModSaida mod[REDE_MAX];
    uint32_t botoes;        /* a saida do controle: a entrada, sem os suprimidos, mais os destinos */
    uint8_t ocultar_toque;  /* 1 = o toque do touchpad sai suprimido do jogo (manual, secao 26) */
    int32_t eixo[4];        /* LX, LY, RX, RY: 0..100 onde um MOD escreveu; senao, o valor cru (manual, 26.1) */
    int32_t gatilho[2];     /* L2, R2: 0..100 onde um MOD escreveu (destinos 1 e 4); senao, o valor cru */
} RedeSaida;

/* 0 se valido, -1 se nao. Recusa so o que e MALFORMADO: MOD invalido, relacao com indice fora de
 * 0..n-1, relacao de um MOD consigo mesmo, e mais de REDE_MAX. O que e semantico (inerte, alvo de
 * outro dominio) nao e recusado: vira alerta, em rede_alertas (manual do ZETA, secao 11). */
int rede_validar(const RedeCfg *r);

/* Alertas de um MOD. Um MOD com alerta roda normalmente; o alerta so avisa. Mascara de bits. */
#define ALERTA_INERTE              1u   /* sem gatilho que ligue e sem ASSOC: nunca liga (vermelho) */
#define ALERTA_ALVO_OUTRO_DOMINIO  2u   /* uma relacao aponta para um MOD sem dominio em comum (vermelho) */
#define ALERTA_EXCL_BLOQ_MESMO     4u   /* o mesmo alvo em BLOQ e EXCL: o EXCL sempre vence (observacao) */
uint32_t rede_alertas(const RedeCfg *r, unsigned i);
void rede_iniciar(RedeEstado *e);
void rede_avaliar(const RedeCfg *r, RedeEstado *e, const Entrada *in, RedeSaida *out);
