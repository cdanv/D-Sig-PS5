/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A tabela unica de tempo. Ver tempo.c.
 */
#pragma once
#include <stdint.h>

/* Quantos valores a lista tem. */
unsigned tempo_quantos(void);

/* Escreve em *ms o valor do indice. Devolve 0 se o indice existe, -1 se nao existe
 * (e nao escreve nada). */
int tempo_ms(unsigned indice, uint16_t *ms);
