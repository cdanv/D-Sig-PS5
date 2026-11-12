/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A TABELA UNICA DE TEMPO do D-Sig-PS5.
 *
 * Todo item que tem ajuste de tempo guarda um INDICE nesta lista, nunca um numero
 * livre. O padrao segue o do HOLD do Cronus: o usuario escolhe um valor da lista.
 * Ver docs/ARQUITETURA.md, secoes 11 e 14.1.
 *
 * ORIGEM DOS VALORES. A lista e a uniao das tabelas de tempo do manual do D-Sig do
 * Cronus 1.0.17: o HOLD (63 valores, secao 13.3), e o que o AFTER, o DELAY, o passo e a
 * latencia do TURBO usam (secoes 14.5, 17 e 21). Os unicos valores que nao estao no
 * HOLD sao 10, 20, 30 e 40 ms, vindos da latencia do TURBO. A bancada
 * ferramentas/testa_tempo.c confere a lista contra esses valores.
 *
 * A lista esta em ordem crescente. O indice 0 e 10 ms, o ultimo e 4000 ms.
 */
#include "tempo.h"

#define TEMPO_N 67

static const uint16_t g_tempos[TEMPO_N] = {
      10,   20,   30,   40,   50,   60,   70,   80,
      90,  100,  110,  120,  130,  140,  150,  160,
     170,  180,  190,  200,  210,  220,  230,  240,
     250,  260,  270,  280,  290,  300,  350,  400,
     450,  500,  550,  600,  650,  700,  750,  800,
     850,  900,  950, 1000, 1100, 1200, 1300, 1400,
    1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200,
    2300, 2400, 2500, 2600, 2800, 3000, 3200, 3400,
    3600, 3800, 4000,
};

unsigned tempo_quantos(void) {
    return TEMPO_N;
}

int tempo_ms(unsigned indice, uint16_t *ms) {
    if (ms == 0 || indice >= TEMPO_N) return -1;   /* indice fora da lista: recusa */
    *ms = g_tempos[indice];
    return 0;
}
