/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A interface do roteador. E tudo o que a ponte (o lado que fala com a libScePad)
 * precisa saber do D-Sig, e tudo o que a bancada precisa para testar sem PS5.
 *
 * Manter esta fronteira estreita e o que permite a bancada existir: a bancada linka
 * o roteador e NAO linka a ponte. Se o roteador conhecesse a libScePad, so daria para
 * testar com um PS5 na mesa — que foi a situacao do Cronus durante meses, e custou
 * caro.
 */
#pragma once
#include "pad.h"

typedef struct {
    unsigned long quadros;    /* vezes que o roteador foi chamado */
    unsigned long passagens;  /* vezes que entregou um pacote */
    unsigned long recusas;    /* vezes que recusou (ponteiro invalido) */
} DsigEstado;

void              dsig_iniciar(void);
void              dsig_rotear(const ScePadData *entrada, ScePadData *saida);
const DsigEstado *dsig_estado(void);
