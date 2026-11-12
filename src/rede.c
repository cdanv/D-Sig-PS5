/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver rede.h e docs/ARQUITETURA.md, secao 22.
 */
#include "rede.h"
#include <string.h>

int rede_validar(const RedeCfg *r) {
    unsigned i;
    uint32_t todos;
    if (r == 0) return -1;
    if (r->n == 0 || r->n > REDE_MAX) return -1;
    todos = (r->n >= 32) ? 0xFFFFFFFFu : ((1u << r->n) - 1u);
    for (i = 0; i < r->n; i++) {
        const Zeta *z = &r->z[i];
        if (modelo_validar(&r->mod[i]) != 0) return -1;
        if (((z->bloq | z->assoc | z->excl) & ~todos) != 0) return -1;   /* indice fora da rede */
        if (((z->bloq | z->assoc | z->excl) >> i) & 1u) return -1;       /* relacao consigo */
        if (z->excl_inic > 1) return -1;
    }
    return 0;
}

void rede_iniciar(RedeEstado *e) {
    unsigned i;
    memset(e, 0, sizeof *e);
    for (i = 0; i < REDE_MAX; i++) modelo_iniciar(&e->mod[i]);
}

void rede_avaliar(const RedeCfg *r, RedeEstado *e, const Entrada *in, RedeSaida *out) {
    unsigned i;
    uint32_t resp_agora = 0;
    uint32_t resp = e->resp, sobe = e->sobe, cai = e->cai;

    memset(out, 0, sizeof *out);

    for (i = 0; i < r->n; i++) {
        const Zeta *z = &r->z[i];
        uint8_t congela = 0, encerra;

        /* BLOQ: suspende enquanto um alvo responde */
        if (z->bloq & resp) congela = 1;
        /* ASSOC: so responde enquanto um alvo responde; fora disso, congelado */
        if (z->assoc && !(z->assoc & resp)) congela = 1;

        /* EXCL: encerra na subida (INIC) ou na finalizacao (FIM) de um alvo.
         * Disparo unico: fica encerrado ate um MOD ligado por ASSOC subir de novo. */
        if (z->excl) {
            uint32_t disparo = z->excl_inic ? (z->excl & sobe) : (z->excl & cai);
            if (disparo) e->encerrado[i] = 1;
            if (e->encerrado[i] && (z->assoc & sobe)) e->encerrado[i] = 0;  /* rearme */
        }
        encerra = e->encerrado[i];

        modelo_avaliar_ext(&r->mod[i], &e->mod[i], in, congela, encerra, &out->mod[i]);
        if (e->mod[i].delta.ligado && !encerra) resp_agora |= 1u << i;
    }

    /* o que subiu e caiu neste quadro vira o snapshot do proximo */
    e->sobe = resp_agora & ~resp;
    e->cai = resp & ~resp_agora;
    e->resp = resp_agora;
}
