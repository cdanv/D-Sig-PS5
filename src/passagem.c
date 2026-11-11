/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ALVO 1 — A PASSAGEM PURA. ALVO 2 — O PRIMEIRO MOD.
 *
 * Sem MOD carregado, este arquivo continua sendo a passagem do Alvo 1: a saida e a
 * entrada, byte por byte. Com MODs carregados, so o campo `buttons` pode mudar, e so
 * quando um MOD dispara. O resto da struct continua passando intacto.
 *
 * POR QUE O ALVO 1 VEM ANTES DE QUALQUER MOD. O D-Sig-PS5 vai rodar DENTRO do processo
 * do jogo. Um defeito aqui faz o jogo travar. Por isso a passagem foi provada antes, e
 * a bancada do Alvo 2 continua conferindo que, sem MOD, nada mudou.
 *
 * O QUE "INVISIVEL" QUER DIZER: sem MOD, todos os campos IGUAIS, inclusive os
 * `reserved` e o enchimento entre campos. A bancada compara a struct INTEIRA com memcmp.
 *
 * ESTE ARQUIVO NAO SABE QUE EXISTE PS5: nenhum cabecalho de SDK, nenhuma chamada de
 * sistema. Por isso o gcc do Linux o compila como esta, e a bancada o linka e roda.
 */
#include "dsig.h"
#include <string.h>

/* Estado do roteador. A contagem serve para a bancada provar que a funcao foi chamada
 * e que os MODs de fato dispararam, em vez de supor. */
static DsigEstado g_estado;

/* Tabela de MODs. Pre-alocada, copiada na carga. Nada disto e tocado por alocacao no
 * caminho do quadro. */
static DsigMod g_mods[DSIG_MAX_MODS];
static size_t  g_n_mods;

void dsig_iniciar(void) {
    memset(&g_estado, 0, sizeof g_estado);
    memset(g_mods, 0, sizeof g_mods);
    g_n_mods = 0;
}

/* Carrega a tabela de MODs. Valida TUDO antes de trocar qualquer coisa: se um MOD
 * for invalido, a tabela anterior continua valendo. Validar aqui, e nao no quadro,
 * mantem o caminho do quadro livre de desvios de erro.
 *
 * Recusa: mais MODs que o teto, ponteiro nulo com n > 0, e `origem` vazia. A origem
 * vazia dispararia em todo quadro — um MOD que nao pede nada e um defeito, nao um
 * recurso. */
int dsig_carregar(const DsigMod *mods, size_t n) {
    size_t i;
    if (n > DSIG_MAX_MODS) return -1;
    if (n > 0 && mods == 0) return -1;
    for (i = 0; i < n; i++) {
        if (mods[i].origem == 0) return -1;
    }
    if (n > 0) memcpy(g_mods, mods, n * sizeof *mods);
    g_n_mods = n;
    return 0;
}

const DsigEstado *dsig_estado(void) {
    return &g_estado;
}

/* O CORACAO.
 *
 * `entrada`  — o pacote como o controle o entregou. Nunca e escrito. Por isso const,
 *              e a bancada confere que nao foi tocado.
 * `saida`    — o pacote que o jogo vai receber.
 *
 * COMO O MOD E APLICADO. Os MODs leem SEMPRE a entrada, nunca a saida ja alterada.
 * Assim, a ordem da tabela nao muda o resultado: um MOD nao enxerga o efeito de outro.
 *   soltar  = uniao das origens que dispararam     -> estes botoes saem
 *   apertar = uniao dos destinos que dispararam    -> estes botoes entram
 *   saida   = (entrada & ~soltar) | apertar
 * Se um bit esta em soltar e em apertar, apertar vence. E a regra, e fica escrita aqui.
 *
 * COMPARADO AO QUE SUBSTITUI. Antes deste alvo, a saida era so `*saida = *entrada`.
 * Esse trecho continua sendo a base; o MOD so mexe em `buttons` depois da copia. */
void dsig_rotear(const ScePadData *entrada, ScePadData *saida) {
    g_estado.quadros++;
    if (entrada == 0 || saida == 0) {        /* o jogo pode chamar com lixo */
        g_estado.recusas++;
        return;
    }
    *saida = *entrada;                        /* a passagem: copia integral */

    if (g_n_mods > 0) {
        const uint32_t lido = entrada->buttons;
        uint32_t soltar = 0;
        uint32_t apertar = 0;
        int disparou = 0;
        size_t i;

        for (i = 0; i < g_n_mods; i++) {
            const uint32_t origem = g_mods[i].origem;
            if ((lido & origem) == origem) {  /* TODOS os botoes da origem */
                soltar  |= origem;
                apertar |= g_mods[i].destino;
                disparou = 1;
            }
        }
        if (disparou) {
            saida->buttons = (lido & ~soltar) | apertar;
            g_estado.disparos++;
        }
    }
    g_estado.passagens++;
}
