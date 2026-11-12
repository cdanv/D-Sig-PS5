/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O FORMATO BINARIO DO PERFIL: a configuracao de uma rede de MODs como bytes, para salvar e
 * carregar. Ver docs/ARQUITETURA.md, secao 27.
 *
 * Campo a campo, em ordem little-endian: nao depende do layout de memoria do compilador.
 * Cabecalho: magic "DSG1", versao (1), n de MODs (1), reservado (2). Corpo: um bloco por MOD
 * (ALFA, acoes, DELTA, ZETA, dominio). Rodape: CRC32 de tudo que vem antes.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "rede.h"

#define FORMATO_VERSAO 1
#define FORMATO_CABECALHO 8
#define FORMATO_RODAPE 4
/* bytes por MOD: ALFA com janela (1 + 6 abas x 45 = 271) + acoes (6) + DELTA (25) = 302.
 * A bancada confere contra a escrita real, e a escrita medida deu 644 bytes para 2 MODs. */
#define FORMATO_BLOCO_ALFA_DELTA 302
#define FORMATO_ZETA_DOM 14       /* por MOD, depois do bloco de ALFA/acoes/DELTA: ZETA (4+4+4+1) e dominio (1) */

/* CRC32 padrao (polinomio 0xEDB88320). Exposto para a bancada conferir com o vetor conhecido. */
uint32_t formato_crc32(const uint8_t *dados, size_t n);

/* Grava a rede em `buf`. Recusa rede invalida. *len recebe o tamanho escrito.
 * Devolve 0 se coube, -1 se nao (ou se a rede e invalida). */
int formato_gravar(const RedeCfg *r, uint8_t *buf, size_t cap, size_t *len);

/* Le a rede de `buf`. Recusa: magic errado, versao diferente, CRC errado, tamanho que nao bate,
 * ou rede que nao passa em rede_validar. Em erro, *r nao deve ser usado. */
int formato_ler(const uint8_t *buf, size_t len, RedeCfg *r);
