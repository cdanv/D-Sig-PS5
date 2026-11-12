/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver perfil.h. Dois buffers, um ativo e um inativo, e um contador de publicacoes.
 *
 * ORDEM DAS OPERACOES. O escritor: (1) grava no buffer inativo, (2) troca o indice
 * ativo, (3) incrementa a versao. O leitor: (a) le a versao, (b) le o indice ativo,
 * (c) copia o buffer, (d) le a versao de novo. Se (a) e (d) diferem, a copia e
 * descartada. Todas as leituras e escritas compartilhadas sao atomicas com ordem
 * sequencial, para o compilador nao reordenar as passagens acima.
 *
 * O PS5 e x86-64. A ordem sequencial de stdatomic.h cobre o caso; o que se confirma
 * no console e a leitura no caminho do quadro, e nao o modelo de memoria.
 */
#include "perfil.h"
#include <stdatomic.h>
#include <string.h>

static PerfilDados g_buf[2];
static atomic_uint g_ativo;          /* indice do buffer que o leitor pode usar */
static atomic_uint g_versao;         /* quantas publicacoes foram concluidas */

static uint32_t soma_de(const uint32_t *campos) {
    uint32_t s = 0;
    unsigned i;
    for (i = 0; i < PERFIL_CAMPOS; i++) s += campos[i];
    return s;
}

int perfil_publicar(const PerfilDados *novo) {
    unsigned inativo = 1u - atomic_load(&g_ativo);
    PerfilDados copia = *novo;
    copia.soma = soma_de(copia.campos);
    copia.versao = atomic_load(&g_versao) + 1u;
    g_buf[inativo] = copia;                          /* (1) escreve no inativo */
    atomic_store(&g_ativo, inativo);                 /* (2) passa a ser o ativo */
    atomic_fetch_add(&g_versao, 1u);                 /* (3) publica a versao */
    return 0;
}

int perfil_ler(PerfilDados *saida) {
    unsigned v1, v2, idx;
    v1 = atomic_load(&g_versao);                     /* (a) */
    idx = atomic_load(&g_ativo);                     /* (b) */
    *saida = g_buf[idx];                             /* (c) copia */
    v2 = atomic_load(&g_versao);                     /* (d) */
    return (v1 == v2) ? 0 : -1;
}

uint32_t perfil_versao_publicada(void) {
    return atomic_load(&g_versao);
}
