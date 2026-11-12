/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O TOQUE DO TOUCHPAD: converte as coordenadas dos dois dedos para a entrada da ALFA.
 * Ver docs/ARQUITETURA.md, secao 26.
 *
 * Regras (confirmadas no monitor do Zen Studio, secao 24.2):
 *  - FINGER1 e FINGER2 (botao): 1 quando ha dedo sobre o touchpad, 0 quando nao ha.
 *  - FINGERnX e FINGERnY (eixos): acompanham a posicao do dedo, em -100..+100, e TRAVAM no
 *    ultimo toque: ao soltar, o valor fica.
 *
 * A FAIXA DAS COORDENADAS (x_max, y_max) NAO ESTA VERIFICADA. Entra como calibracao, e fica
 * para medir no console. Nada aqui afirma o valor real do PS5.
 */
#pragma once
#include <stdint.h>
#include "alfa.h"

typedef struct {
    uint16_t x_max;     /* coordenada X que vale +100; a origem (0) vale -100 */
    uint16_t y_max;     /* idem para Y */
} ToqueCal;

typedef struct {
    uint8_t presente[2];        /* FINGER1, FINGER2 */
    uint16_t x[2], y[2];        /* coordenadas brutas do dispositivo */
} ToqueRaw;

typedef struct {
    int32_t pos[4];             /* F1X, F1Y, F2X, F2Y: a ultima posicao (trava) */
} ToqueEstado;

/* 0 se a calibracao e valida, -1 se nao (faixa zero). */
int toque_validar(const ToqueCal *c);
void toque_iniciar(ToqueEstado *e);
/* Escreve em `out` os dedos (dedo[]) e os eixos F1X..F2Y (eixo[]). Nao mexe nos outros campos. */
void toque_aplicar(const ToqueCal *c, ToqueEstado *e, const ToqueRaw *raw, Entrada *out);
