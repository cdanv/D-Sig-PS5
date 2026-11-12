/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A ALFA do modelo v4: seis abas, cada uma com ate cinco entradas.
 * Ver docs/ARQUITETURA.md, secao 18.
 *
 * ABAS: LIVRE, SEQUENCIA, 2CLK, HOLD_AT, HOLD_AP, BLOCK. Em cada aba, o "+" adiciona
 * botoes, gatilhos e eixos. Teto de 5 por aba, 30 no total.
 *
 * FORA DESTA FATIA: ALTERNA, FINALIZA e DELAY nas abas. Dependem de decisao pendente
 * (secao 18.9). O gatilho externo que reiniciava a sequencia tambem fica pendente.
 *
 * Nada aqui aloca memoria: os limites sao constantes.
 */
#pragma once
#include <stdint.h>
#include "pad.h"

#define ALFA_ABAS 6
#define ALFA_POR_ABA 5
#define ALFA_TOTAL (ALFA_ABAS * ALFA_POR_ABA)

enum { ABA_LIVRE = 0, ABA_SEQUENCIA = 1, ABA_2CLK = 2,
       ABA_HOLD_AT = 3, ABA_HOLD_AP = 4, ABA_BLOCK = 5 };

/* tipo de fonte */
enum { FONTE_BOTAO = 0, FONTE_EIXO = 1, FONTE_GATILHO = 2, FONTE_DEDO = 3 };
/* FONTE_DEDO: "ha dedo sobre o touchpad" (FINGER1 ou FINGER2). Vale 0 ou 100. */
/* eixos: analogicos do controle (0..3) e os sensores de movimento (4..9, PS5)
 * ACCX/ACCY/ACCZ: acelerometro; GYROX/GYROY/GYROZ: giroscopio. Todos em -100..+100 apos a
 * normalizacao, que ainda nao foi medida (ver docs/ARQUITETURA.md, secao 25). */
enum { EIXO_LX = 0, EIXO_LY = 1, EIXO_RX = 2, EIXO_RY = 3,
       EIXO_ACCX = 4, EIXO_ACCY = 5, EIXO_ACCZ = 6,
       EIXO_GYROX = 7, EIXO_GYROY = 8, EIXO_GYROZ = 9,
       /* posicao do dedo no touchpad: trava no ultimo toque (ver toque.h) */
       EIXO_F1X = 10, EIXO_F1Y = 11, EIXO_F2X = 12, EIXO_F2Y = 13, EIXO_N = 14 };
enum { GATILHO_L2 = 0, GATILHO_R2 = 1 };
enum { DIR_NEG = 0, DIR_ABS = 1, DIR_POS = 2 };

/* E ou OU. Ignorado onde nao se aplica: 2CLK e sempre OU, SEQUENCIA e sempre ordenada */
enum { EOU_OU = 0, EOU_E = 1 };

/* janela fixa do segundo clique do 2CLK: 300 ms (manual, secao 13.5) */
#define ALFA_JANELA_2CLK_MS 300

/* DELAY OFF: fora da tabela de tempo. A resposta sai no mesmo quadro do acionamento. */
#define ALFA_DELAY_OFF 255

/* DELAY nao escolhido. A SEQUENCIA recusa; as demais abas tratam como OFF. Zero nao e
 * um valor neutro: seria o indice 0 do catalogo (10 ms). Por isso o padrao explicito. */
#define ALFA_DELAY_NAO_ESCOLHIDO 254

typedef struct {
    uint8_t mult;     /* 0..18: x0.1 a x1.9; 9 = x1.0 */
    uint8_t thr_in;   /* 0..100, multiplo de 5 */
    uint8_t thr_out;  /* 0..95, multiplo de 5; 0 = sem teto */
    uint8_t dir;      /* DIR_* (so eixo) */
    uint8_t inv;      /* 1 = inverte a zona */
} Beta;

typedef struct {
    uint8_t tipo;     /* FONTE_* */
    uint8_t id;       /* botao: indice do bit; eixo: EIXO_*; gatilho: GATILHO_* */
    Beta beta;        /* so vale para eixo e gatilho */
} Fonte;

/* leitura do controle, ja na forma que a ALFA consome */
typedef struct {
    uint32_t botoes;          /* mascara de bits, como em pad.h */
    int32_t eixo[EIXO_N];     /* EIXO_*: -100 a +100 */
    uint8_t dedo[2];          /* FINGER1, FINGER2: 1 se ha dedo sobre o touchpad */
    int32_t gatilho[2];       /* L2, R2: 0 a 100 */
    uint64_t t_us;            /* carimbo de tempo, microssegundos */
} Entrada;

typedef struct {
    uint8_t n;                          /* entradas usadas, 0..5 */
    uint8_t eou;                        /* EOU_* */
    uint8_t oculto;                     /* 1 = as fontes da aba sao suprimidas do jogo */
    uint8_t tempo;                      /* indice do catalogo: HOLD_AT e HOLD_AP */
    uint8_t delay;                      /* indice do catalogo, ou ALFA_DELAY_OFF */
    uint8_t espera[ALFA_POR_ABA];       /* indice do catalogo: SEQUENCIA, espera apos o passo k */
    Fonte fontes[ALFA_POR_ABA];
} Aba;

typedef struct {
    Aba abas[ALFA_ABAS];
    uint8_t janela;                     /* indice do catalogo: janela do E */
} AlfaCfg;

typedef struct {
    uint8_t nivel_ant[ALFA_TOTAL];      /* nivel de cada entrada no quadro anterior */
    uint64_t t_borda[ALFA_TOTAL];       /* ultima borda de subida de cada entrada */
    uint8_t disparou[ALFA_TOTAL];       /* HOLD_AP: ja disparou nesta pressao */
    uint8_t clk_pend[ALFA_TOTAL];       /* 2CLK: primeiro clique pendente */
    uint64_t clk_t1[ALFA_TOTAL];        /* 2CLK: instante do primeiro clique */
    uint8_t seq_passo[ALFA_ABAS];       /* SEQUENCIA: proximo passo, por aba (uso interno) */
    uint64_t seq_t[ALFA_ABAS];          /* SEQUENCIA: instante do ultimo passo */
    uint8_t win_aberta[ALFA_ABAS];      /* janela do E, por aba */
    uint64_t win_t[ALFA_ABAS];
    uint32_t win_ja[ALFA_ABAS];
    uint8_t ant_ativa[ALFA_ABAS];       /* ativa de cada aba no quadro anterior */
    uint8_t dl_estado[ALFA_ABAS];       /* DELAY: 0 ocioso, 1 contando, 2 ja disparou */
    uint64_t dl_t[ALFA_ABAS];           /* DELAY: instante de inicio da contagem */
} AlfaEstado;

typedef struct {
    uint8_t ativa[ALFA_ABAS];           /* por aba: nivel (LIVRE, BLOCK) ou pulso (demais) */
    uint8_t disparo[ALFA_ABAS];         /* por aba, depois do DELAY: pulso que vai ao DELTA */
    uint8_t reiniciou[ALFA_ABAS];       /* SEQUENCIA: reiniciou neste quadro */
    uint8_t intens[ALFA_ABAS];          /* 0..100: a maior intensidade das fontes com nivel na aba (manual, 26.1) */
    uint32_t ocultar_botoes;
    uint16_t ocultar_analog;            /* bits 0..13 eixos (EIXO_*), 14 L2, 15 R2 */
} AlfaSaida;

/* 1 se nenhuma fonte esta acionada neste quadro (repouso). Usado para destravar um MOD
 * depois de uma troca de dominio (manual, secao 25.1). Nao altera nada. */
int alfa_em_repouso(const AlfaCfg *c, const Entrada *in);

/* 0 se valido, -1 se nao. Nao altera nada. */
int alfa_validar(const AlfaCfg *c);
void alfa_iniciar(AlfaEstado *e);
void alfa_avaliar(const AlfaCfg *c, AlfaEstado *e, const Entrada *in, AlfaSaida *out);
