/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver rede.h e docs/ARQUITETURA.md, secao 22.
 */
#include "rede.h"
#include "alfa.h"
#include "pad.h"
#include <string.h>

/* Botao de cada destino digital (manual, secao 14.2). Os destinos 10 a 13 sao eixos e ainda
 * nao viram botao: esta fatia nao tem intensidade de eixo. Valor 0 = sem botao. */
static const uint32_t g_botao_do_destino[DELTA_DESTINOS] = {
    DS_BTN_L1, DS_BTN_L2, DS_BTN_L3, DS_BTN_R1, DS_BTN_R2, DS_BTN_R3,
    DS_BTN_CROSS, DS_BTN_CIRCLE, DS_BTN_SQUARE, DS_BTN_TRIANGLE,
    0, 0, 0, 0,
    DS_BTN_UP, DS_BTN_DOWN, DS_BTN_LEFT, DS_BTN_RIGHT
};

static uint32_t botoes_dos_destinos(uint32_t destinos) {
    uint32_t b = 0;
    unsigned i;
    for (i = 0; i < DELTA_DESTINOS; i++) {
        if (((destinos >> i) & 1u) && g_botao_do_destino[i]) b |= g_botao_do_destino[i];
    }
    return b;
}

int rede_validar(const RedeCfg *r) {
    unsigned i;
    uint32_t todos;
    if (r == 0) return -1;
    if (r->n == 0 || r->n > REDE_MAX) return -1;
    todos = (r->n >= 32) ? 0xFFFFFFFFu : ((1u << r->n) - 1u);
    for (i = 0; i < r->n; i++) {
        const Zeta *z = &r->z[i];
        if (modelo_sem_ligar(&r->mod[i])) {
            /* sem gatilho que ligue: valido se o DELTA e valido. Sem ASSOC, e inerte: o alerta avisa */
            if (modelo_validar_adotado(&r->mod[i]) != 0) return -1;
        } else if (modelo_validar(&r->mod[i]) != 0) {
            return -1;
        }
        if (((z->bloq | z->assoc | z->excl) & ~todos) != 0) return -1;   /* indice fora da rede */
        if (((z->bloq | z->assoc | z->excl) >> i) & 1u) return -1;       /* relacao consigo */
        if (z->excl_inic > 1) return -1;
        if (r->dom[i] > 0x0Fu) return -1;                                 /* so D1..D4 */
    }
    return 0;
}

uint32_t rede_alertas(const RedeCfg *r, unsigned i) {
    const Zeta *z = &r->z[i];
    uint32_t rel = z->bloq | z->assoc | z->excl, a = 0;
    unsigned j;
    if (modelo_sem_ligar(&r->mod[i]) && z->assoc == 0) a |= ALERTA_INERTE;
    for (j = 0; j < r->n; j++) {
        if (!((rel >> j) & 1u)) continue;
        if (!(r->dom[i] & r->dom[j])) a |= ALERTA_ALVO_OUTRO_DOMINIO;
    }
    if (z->bloq & z->excl) a |= ALERTA_EXCL_BLOQ_MESMO;
    return a;
}

void rede_iniciar(RedeEstado *e) {
    unsigned i;
    memset(e, 0, sizeof *e);
    dominio_iniciar(&e->dom);
    for (i = 0; i < REDE_MAX; i++) modelo_iniciar(&e->mod[i]);
}

void rede_avaliar(const RedeCfg *r, RedeEstado *e, const Entrada *in, RedeSaida *out) {
    unsigned i;
    DominioSaida dso;
    uint8_t atual;
    uint32_t resp_agora = 0;
    uint32_t resp = e->resp, sobe = e->sobe, cai = e->cai;

    memset(out, 0, sizeof *out);

    /* dominio: a troca reinicia o estado de todos os MODs e os trava ate a fonte repousar */
    dominio_avaliar(&e->dom, in, &dso);
    atual = e->dom.atual;
    if (dso.trocou) {
        for (i = 0; i < r->n; i++) {
            modelo_iniciar(&e->mod[i]);
            e->encerrado[i] = 0;
        }
        e->trava = (r->n >= 32) ? 0xFFFFFFFFu : ((1u << r->n) - 1u);
        resp = sobe = cai = 0;
    }

    for (i = 0; i < r->n; i++) {
        const Zeta *z = &r->z[i];
        uint8_t congela = 0, encerra;

        /* fora do dominio atual, o MOD nao roda e nao suprime nada */
        if (!atual || !((r->dom[i] >> (atual - 1)) & 1u)) continue;

        /* travado: so suprime, ate a fonte repousar */
        if (e->trava & (1u << i)) {
            if (alfa_em_repouso(&r->mod[i].alfa, in)) {
                e->trava &= ~(1u << i);
            } else {
                modelo_so_ocultar(&r->mod[i], &out->mod[i]);
                continue;
            }
        }

        /* BLOQ: suspende enquanto um alvo responde */
        if (z->bloq & resp) congela = 1;
        /* ASSOC: so responde enquanto um alvo responde; fora disso, congelado (preserva o estado) */
        if (z->assoc && !(z->assoc & resp)) congela = 1;

        /* EXCL: encerra na subida (INIC) ou na finalizacao (FIM) de um alvo.
         * Disparo unico: fica encerrado ate um MOD ligado por ASSOC subir de novo. */
        if (z->excl) {
            uint32_t disparo = z->excl_inic ? (z->excl & sobe) : (z->excl & cai);
            if (disparo) e->encerrado[i] = 1;
            if (e->encerrado[i] && (z->assoc & sobe)) e->encerrado[i] = 0;  /* rearme */
        }
        encerra = e->encerrado[i];

        if (modelo_sem_ligar(&r->mod[i])) {
            /* ORFAO (manual, secao 7.2): a borda de subida de um alvo da ASSOC liga o MOD. A queda
             * do alvo nao o desliga: ele so fica silenciado pela ASSOC, com o estado preservado. */
            uint8_t adota = (z->assoc & sobe) ? 1 : 0;
            modelo_avaliar_adotado(&r->mod[i], &e->mod[i], in, adota, congela, encerra, &out->mod[i]);
        } else {
            modelo_avaliar_ext(&r->mod[i], &e->mod[i], in, congela, encerra, &out->mod[i]);
        }
        if (e->mod[i].delta.ligado && !encerra) resp_agora |= 1u << i;
    }

    /* o que subiu e caiu neste quadro vira o snapshot do proximo */
    e->sobe = resp_agora & ~resp;
    e->cai = resp & ~resp_agora;
    e->resp = resp_agora;

    /* acumulacao (manual, secao 26): cada MOD escreve no seu acumulador, e tudo e aplicado no
     * fim. Digital escreve so quando liga, por OU; uma saida desligada nao forca zero, e o
     * sinal cru do jogador passa. O destino vence a supressao: a supressao vem antes. */
    {
        uint32_t suprimido = 0, acumulado = 0;
        for (i = 0; i < r->n; i++) {
            suprimido |= out->mod[i].ocultar_botoes;
            acumulado |= botoes_dos_destinos(out->mod[i].destinos);
        }
        out->botoes = (in->botoes & ~suprimido) | acumulado;
    }
}
