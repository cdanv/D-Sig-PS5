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
#include <stddef.h>
#include <stdint.h>
#include "pad.h"

/* Teto de MODs na tabela. Fixo e pre-alocado: o roteador roda dentro do jogo e nao
 * aloca memoria no caminho do quadro. O valor 20 herda o teto do Cronus por enquanto;
 * nao ha medida de custo que o justifique ainda. */
#define DSIG_MAX_MODS 20

/* Um MOD de Alvo 2: quando TODOS os botoes de `origem` estao pressionados, eles saem
 * da saida e o `destino` entra no lugar. Os dois sao mascaras DS_BTN_* (pad.h).
 *
 * Um botao so e uma troca simples (L1 entregue como CROSS). Varios botoes em `origem`
 * sao um acorde: so dispara com todos juntos, e entrega um toque no lugar. */
typedef struct {
    uint32_t origem;
    uint32_t destino;
} DsigMod;

typedef struct {
    unsigned long quadros;    /* vezes que o roteador foi chamado */
    unsigned long passagens;  /* vezes que entregou um pacote */
    unsigned long recusas;    /* vezes que recusou (ponteiro invalido) */
    unsigned long disparos;   /* quadros em que pelo menos um MOD disparou */
} DsigEstado;

void              dsig_iniciar(void);
int               dsig_carregar(const DsigMod *mods, size_t n);
void              dsig_rotear(const ScePadData *entrada, ScePadData *saida);
const DsigEstado *dsig_estado(void);
