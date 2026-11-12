/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Os DOMINIOS (D1 a D4): qual conjunto de MODs roda agora. Ver docs/ARQUITETURA.md, secao 23.
 * Os comandos sao fixos, como no manual (secao 6). Nada aqui aloca memoria.
 */
#pragma once
#include <stdint.h>
#include "alfa.h"

#define DOMINIO_DUPLO_US 250000u     /* TIME_SHORT_PRESS: janela do duplo clique de dominio */
#define DOMINIO_HOLD_US 500000u      /* TIME_HOLD: segurar L3/R3 para cair em D3/D4 */

typedef struct {
    uint8_t atual;                   /* 0 = nenhum dominio; 1..4 = D1..D4 */
    uint8_t pend[2];                 /* [0] L3, [1] R3: primeiro clique pendente */
    uint64_t t_pend[2];
    uint8_t ant[2];                  /* nivel de L3 e R3 no quadro anterior */
    uint64_t t_press[2];
    uint8_t hold_feito[2];
} Dominio;

typedef struct {
    uint8_t trocou;                  /* 1 se o dominio mudou neste quadro */
} DominioSaida;

void dominio_iniciar(Dominio *d);
void dominio_avaliar(Dominio *d, const Entrada *in, DominioSaida *out);
