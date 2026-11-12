/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O MOD completo do modelo v4: ALFA (fontes e abas) ligada ao DELTA (a resposta).
 * Ver docs/ARQUITETURA.md, secao 21.
 */
#pragma once
#include <stdint.h>
#include "alfa.h"
#include "delta.h"

/* acao de cada aba sobre o DELTA do MOD */
enum { ACAO_LIGA = 0, ACAO_ALTERNA = 1, ACAO_FINALIZA = 2 };

typedef struct {
    AlfaCfg alfa;
    uint8_t acao[ALFA_ABAS];       /* ACAO_*; ignorada em BLOCK */
    DeltaCfg delta;                /* origem_pulso e calculado; o valor de entrada e ignorado */
} ModCfg;

typedef struct {
    AlfaEstado alfa;
    DeltaEstado delta;
} ModEstado;

typedef struct {
    uint32_t destinos;             /* bit i = destino i ligado neste quadro */
    uint8_t terminou;              /* TURBO/COMBO terminaram sozinhos */
    uint8_t bloqueado;             /* BLOCK ativo: o MOD nao responde */
    uint32_t ocultar_botoes;       /* da ALFA: botoes suprimidos do jogo */
    uint16_t ocultar_analog;
} ModSaida;

/* 0 se o MOD e valido, -1 se nao */
int modelo_validar(const ModCfg *m);
void modelo_iniciar(ModEstado *e);
void modelo_avaliar(const ModCfg *m, ModEstado *e, const Entrada *in, ModSaida *out);

/* ORFAO (manual, secao 7): MOD sem nenhum gatilho que ligue (LIGA ou ALTERNA). So FINALIZA nao
 * liga. 1 se o MOD nao liga sozinho, 0 se liga. Quem o liga e a adocao, pela ASSOC da ZETA. */
int modelo_sem_ligar(const ModCfg *m);
/* Valida um MOD que depende da adocao. A ASSOC que o adota e conferida na rede. 0 se valido. */
int modelo_validar_adotado(const ModCfg *m);
/* Igual a modelo_avaliar_ext, mas com a adocao: `adota` = 1 injeta a borda de subida de um alvo
 * da ASSOC. Para um MOD com gatilho que liga, `adota` nao muda nada e pode ser 0. */
void modelo_avaliar_adotado(const ModCfg *m, ModEstado *e, const Entrada *in, uint8_t adota,
                            uint8_t congela, uint8_t encerra, ModSaida *out);

/* MOD travado por dominio: nao detecta nem responde, mas as abas OCULTO continuam
 * suprimindo os seus botoes (manual, secao 25.1). */
void modelo_so_ocultar(const ModCfg *m, ModSaida *out);

/* Igual a modelo_avaliar, mas com as relacoes da ZETA: `congela` suspende o MOD (BLOQ, ASSOC);
 * `encerra` desliga o MOD (EXCL), e ele fica sem responder. A ALFA continua avancando. */
void modelo_avaliar_ext(const ModCfg *m, ModEstado *e, const Entrada *in,
                        uint8_t congela, uint8_t encerra, ModSaida *out);
