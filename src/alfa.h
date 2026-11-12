/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A ALFA do modelo de MOD v3: a fonte, os tres modos de acionamento e a condicao BETA.
 * Ver docs/ARQUITETURA.md, secoes 10 a 15.
 *
 * Esta fatia cobre so a ALFA. GAMA, DELTA e ZETA entram em arquivos proprios.
 *
 * ESTADO ENTRE QUADROS. A sequencia, a janela do SIMULTANEO e o nivel anterior de cada
 * fonte vivem em AlfaEstado. Nada aqui aloca memoria; os limites sao constantes.
 *
 * LIMITES. ALFA_MAX_FONTES = 8 e PROVISORIO. O teto real depende do custo por quadro,
 * e ainda nao foi medido.
 */
#pragma once
#include <stdint.h>
#include "pad.h"

#define ALFA_MAX_FONTES 8

/* tipo de fonte */
enum { FONTE_BOTAO = 0, FONTE_EIXO = 1, FONTE_GATILHO = 2 };

/* ids de eixo e gatilho */
enum { EIXO_LX = 0, EIXO_LY = 1, EIXO_RX = 2, EIXO_RY = 3 };
enum { GATILHO_L2 = 0, GATILHO_R2 = 1 };

/* direcao, so para eixo */
enum { DIR_NEG = 0, DIR_ABS = 1, DIR_POS = 2 };

/* modo de acionamento em ALFA */
enum { MODO_QUALQUER = 0, MODO_SIMULTANEO = 1, MODO_SEQUENCIA = 2 };

/* condicao BETA, para eixo e gatilho */
typedef struct {
    uint8_t mult;     /* 0..18: x0.1 a x1.9; 9 = x1.0 (padrao) */
    uint8_t thr_in;   /* 0..100, multiplo de 5: abaixo deste valor a fonte nao conta */
    uint8_t thr_out;  /* 0..95, multiplo de 5; 0 = sem teto */
    uint8_t dir;      /* DIR_NEG, DIR_ABS ou DIR_POS */
    uint8_t inv;      /* 1 = inverte a zona (conta fora da faixa) */
} Beta;

typedef struct {
    uint8_t tipo;     /* FONTE_* */
    uint8_t id;       /* botao: indice do bit em buttons; eixo: EIXO_*; gatilho: GATILHO_* */
    uint8_t oculto;   /* 1 = a fonte e suprimida do jogo */
    Beta beta;        /* so vale para eixo e gatilho */
} Fonte;

/* uma leitura do controle, ja na forma que a ALFA consome */
typedef struct {
    uint32_t botoes;          /* mascara de bits, como em pad.h */
    int32_t eixo[4];          /* LX, LY, RX, RY: -100 a +100 */
    int32_t gatilho[2];       /* L2, R2: 0 a 100 */
    uint64_t t_us;            /* carimbo de tempo, microssegundos */
} Entrada;

typedef struct {
    uint8_t n;                                  /* quantas fontes */
    uint8_t modo;                               /* MODO_* */
    uint8_t janela;                             /* indice do catalogo: janela do SIMULTANEO */
    Fonte fontes[ALFA_MAX_FONTES];
    uint8_t espera[ALFA_MAX_FONTES];            /* indice do catalogo: espera apos o passo k */
} AlfaCfg;

typedef struct {
    uint8_t passo;                              /* proximo passo esperado, 0..n-1 */
    uint64_t t_passo;                           /* quando o ultimo passo foi concluido */
    uint8_t nivel_ant[ALFA_MAX_FONTES];         /* nivel de cada fonte no quadro anterior */
    uint8_t janela_aberta;                      /* SIMULTANEO */
    uint64_t t_janela;                          /* inicio da janela do SIMULTANEO */
    uint32_t ja_mask;                           /* SIMULTANEO: fontes que tiveram borda na janela */
} AlfaEstado;

typedef struct {
    uint8_t ativa;                              /* a ALFA esta acionada neste quadro */
    uint8_t concluida;                          /* SEQUENCIA: a sequencia terminou neste quadro */
    uint8_t reiniciou;                          /* SEQUENCIA: a sequencia foi reiniciada agora */
    uint32_t ocultar_botoes;                    /* mascara de bits a suprimir do jogo */
    uint8_t ocultar_analog;                     /* bits 0..3 eixos, 4 L2, 5 R2 */
} AlfaSaida;

/* 0 se a configuracao e valida, -1 se nao. Nao altera nada. */
int alfa_validar(const AlfaCfg *c);

void alfa_iniciar(AlfaEstado *e);

/* Avalia um quadro. `reinicio` = 1 quando o gatilho da GAMA foi acionado neste quadro
 * (a GAMA e quem o detecta). Escreve em *out. */
void alfa_avaliar(const AlfaCfg *c, AlfaEstado *e, const Entrada *in, int reinicio,
                  AlfaSaida *out);
