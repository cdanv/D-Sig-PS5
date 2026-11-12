/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O DELTA do modelo v4: a resposta do MOD. Ver docs/ARQUITETURA.md, secao 20.
 *
 * Um DELTA por MOD. As abas da ALFA o alimentam com LIGA, ALTERNA ou FINALIZA.
 * TURBO e COMBO usam a mesma maquina de execucao: TURBO tem um passo; COMBO tem ate dez.
 *
 * Os destinos sao booleanos nesta fatia. Os 18 destinos sao os do Cronus.
 */
#pragma once
#include <stdint.h>

#define DELTA_DESTINOS 18          /* indices 0..17, como no Cronus (manual, secao 14.2) */
#define DELTA_MAX_DEST 4           /* DIRECT e STATIC: ate quatro destinos */
#define DELTA_MAX_PASSOS 10        /* COMBO: ate dez passos */
#define DELTA_CICLOS_ATIVO 255     /* ciclos sem fim: so FINALIZA encerra */
#define DELTA_SEM_LAT 255          /* sem latencia entre passos */

enum { DELTA_DIRECT = 0, DELTA_STATIC = 1, DELTA_TURBO = 2, DELTA_COMBO = 3 };

typedef struct {
    uint8_t tipo;                  /* DELTA_* */
    uint8_t n_dest;                /* DIRECT/STATIC: 1..4; TURBO: 1; COMBO: 1..10 */
    uint8_t dest[DELTA_MAX_PASSOS];/* indices 0..17. DIRECT/STATIC usam n_dest; TURBO/COMBO, um por passo */
    uint8_t tempo[DELTA_MAX_PASSOS];/* indice do catalogo: duracao de cada passo (TURBO/COMBO) */
    uint8_t lat;                   /* indice do catalogo, ou DELTA_SEM_LAT */
    uint8_t ciclos;                /* 1..7, ou DELTA_CICLOS_ATIVO (TURBO/COMBO) */
    uint8_t origem_pulso;          /* 1 se alguma aba que alimenta este DELTA e pulso */
} DeltaCfg;

typedef struct {
    uint8_t ligado;                /* DIRECT: nivel; STATIC: trava; TURBO/COMBO: rodando */
    uint8_t passo;                 /* TURBO/COMBO: passo corrente */
    uint8_t em_latencia;           /* TURBO/COMBO: 1 entre passos ou ciclos */
    uint8_t ciclos_feitos;         /* TURBO/COMBO: ciclos completos */
    uint8_t liga_ant, alterna_ant, finaliza_ant;  /* bordas do quadro anterior */
    uint64_t t_fase;               /* inicio da fase corrente (passo ou latencia) */
    uint8_t congelado_ant;         /* o BLOCK estava ativo no quadro anterior */
    uint64_t t_pausa;              /* instante em que o congelamento comecou */
} DeltaEstado;

/* entrada do quadro: o que as abas da ALFA produziram */
typedef struct {
    uint8_t liga;                  /* LIGA neste quadro (pulso, ou nivel para DIRECT) */
    uint8_t alterna;               /* ALTERNA neste quadro (pulso) */
    uint8_t finaliza;              /* FINALIZA neste quadro (pulso) */
    uint8_t congelado;             /* BLOCK ativo: o MOD para de responder e os tempos param */
    uint64_t t_us;
} DeltaEntrada;

typedef struct {
    uint32_t destinos;             /* bit i = destino i ligado neste quadro */
    uint8_t terminou;              /* TURBO/COMBO terminaram sozinhos neste quadro */
} DeltaSaida;

/* 0 se valido, -1 se nao. Nao altera nada. */
int delta_validar(const DeltaCfg *c);
void delta_iniciar(DeltaEstado *e);
void delta_avaliar(const DeltaCfg *c, DeltaEstado *e, const DeltaEntrada *in, DeltaSaida *out);
