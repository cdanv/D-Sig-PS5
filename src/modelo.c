/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ver modelo.h e docs/ARQUITETURA.md, secao 21.
 *
 * Abas de nivel (LIVRE): alimentam o DELTA com o seu nivel quando o DELAY e OFF.
 * Abas de pulso (SEQUENCIA, 2CLK, HOLD_AT, HOLD_AP): alimentam o DELTA com o pulso.
 * BLOCK: nao alimenta o DELTA. Suspende o MOD.
 */
#include "modelo.h"
#include <string.h>

static int aba_alimenta(unsigned a) {
    return a != ABA_BLOCK;
}

static int aba_e_nivel(unsigned a) {
    return a == ABA_LIVRE || a == ABA_BLOCK;
}

int modelo_validar(const ModCfg *m) {
    unsigned a, alimentam = 0;
    uint8_t origem_pulso = 0;
    DeltaCfg d;

    if (m == 0) return -1;
    if (alfa_validar(&m->alfa) != 0) return -1;

    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &m->alfa.abas[a];
        if (ab->n == 0) continue;
        if (m->acao[a] > ACAO_FINALIZA) return -1;

        if (!aba_alimenta(a)) {
            /* BLOCK: suspende, nao tem acao sobre o DELTA */
            if (m->acao[a] != ACAO_LIGA) return -1;
            continue;
        }
        alimentam++;
        if (!aba_e_nivel(a)) origem_pulso = 1;

        if (m->delta.tipo == DELTA_DIRECT) {
            if (!aba_e_nivel(a)) return -1;                     /* DIRECT: so nivel */
            if (ab->delay != ALFA_DELAY_OFF && ab->delay != ALFA_DELAY_NAO_ESCOLHIDO) return -1;
        }
    }
    if (alimentam == 0) return -1;                              /* sem origem, sem resposta */

    d = m->delta;
    d.origem_pulso = origem_pulso;
    return delta_validar(&d);
}

void modelo_iniciar(ModEstado *e) {
    alfa_iniciar(&e->alfa);
    delta_iniciar(&e->delta);
    e->gz_ant = 0;
    e->gz_destravado = 0;
}

/* STATIC com fonte de gatilho (manual do EIXOS, 8.3): so LIVRE, so LIGA, so gatilhos, nivel (sem DELAY).
 * O teto e a memoria "ja destravou" valem para este caso. */
static int gatilho_static(const ModCfg *m) {
    unsigned a, k;
    const Aba *l = &m->alfa.abas[ABA_LIVRE];
    if (m->delta.tipo != DELTA_STATIC) return 0;
    if (l->n == 0 || m->acao[ABA_LIVRE] != ACAO_LIGA) return 0;
    if (l->delay != ALFA_DELAY_OFF && l->delay != ALFA_DELAY_NAO_ESCOLHIDO) return 0;
    for (k = 0; k < l->n; k++) if (l->fontes[k].tipo != FONTE_GATILHO) return 0;
    for (a = 1; a < ALFA_ABAS; a++) {
        if (a == ABA_BLOCK) continue;
        if (m->alfa.abas[a].n != 0) return 0;
    }
    return 1;
}

int modelo_sem_ligar(const ModCfg *m) {
    unsigned a;
    for (a = 0; a < ALFA_ABAS; a++) {
        if (!aba_alimenta(a)) continue;                         /* BLOCK nao liga */
        if (m->alfa.abas[a].n == 0) continue;
        if (m->acao[a] == ACAO_LIGA || m->acao[a] == ACAO_ALTERNA) return 0;
    }
    return 1;
}

int modelo_validar_adotado(const ModCfg *m) {
    unsigned a, com_abas = 0;
    DeltaCfg d;

    if (m == 0) return -1;
    if (!modelo_sem_ligar(m)) return -1;
    for (a = 0; a < ALFA_ABAS; a++) if (m->alfa.abas[a].n != 0) com_abas = 1;
    /* com abas (so FINALIZA, que e gatilho legitimo do orfao), vale a validacao normal */
    if (com_abas) return modelo_validar(m);
    /* sem nenhuma aba: a ALFA e vazia por definicao (alfa_validar recusa isso). So o DELTA conta.
     * O orfao recebe pulso da adocao, exceto DIRECT, que vira trinco (ver avaliar_nucleo). */
    d = m->delta;
    d.origem_pulso = (uint8_t)(d.tipo != DELTA_DIRECT);
    return delta_validar(&d);
}

/* Nucleo comum de modelo_avaliar_ext e modelo_avaliar_adotado. `adota` e a borda que liga o
 * orfao (manual, secao 7.2); para um MOD com fonte, e sempre 0. */
static void avaliar_nucleo(const ModCfg *m, ModEstado *e, const Entrada *in, uint8_t adota,
                           uint8_t intens_alvo, int8_t lado_alvo, uint8_t congela, uint8_t encerra,
                           ModSaida *out) {
    AlfaSaida as;
    DeltaEntrada de;
    DeltaSaida ds;
    unsigned a;
    uint8_t bloqueio = 0, fonte_int = 0, teto_win = 0;
    int8_t fonte_lado = 0;
    unsigned k;
    int puro = 1, tem_fonte = 0;
    DeltaCfg cfg;

    memset(out, 0, sizeof *out);
    memset(&de, 0, sizeof de);
    de.lado = 1;                                        /* botao e orfao: o lado e sempre +1 */

    alfa_avaliar(&m->alfa, &e->alfa, in, &as);
    out->ocultar_botoes = as.ocultar_botoes;
    out->ocultar_analog = as.ocultar_analog;
    out->ocultar_toque = as.ocultar_toque;

    for (a = 0; a < ALFA_ABAS; a++) {
        if (m->alfa.abas[a].n == 0) continue;
        if (!aba_alimenta(a)) {
            if (as.ativa[a]) bloqueio = 1;                      /* BLOCK */
            continue;
        }
        if (as.intens[a] > fonte_int) { fonte_int = as.intens[a]; fonte_lado = as.lado[a]; }
        /* STATIC de eixo puro: so LIVRE, so LIGA, so fontes de eixo, e nivel (sem DELAY) */
        tem_fonte = 1;
        if (a != ABA_LIVRE || m->acao[a] != ACAO_LIGA) puro = 0;
        if (m->alfa.abas[a].delay != ALFA_DELAY_OFF && m->alfa.abas[a].delay != ALFA_DELAY_NAO_ESCOLHIDO) puro = 0;
        for (k = 0; k < m->alfa.abas[a].n; k++) if (m->alfa.abas[a].fontes[k].tipo != FONTE_EIXO) puro = 0;
        /* o disparo de cada aba (com DELAY) alimenta o DELTA pela sua acao */
        switch (m->acao[a]) {
        case ACAO_LIGA:
            de.liga |= as.disparo[a];
            if (as.disparo[a] && as.intens[a] > de.intensidade) { de.intensidade = as.intens[a]; de.lado = as.lado[a]; teto_win = as.teto[a]; }
            break;
        case ACAO_ALTERNA:
            de.alterna |= as.disparo[a];
            if (as.disparo[a] && as.intens[a] > de.intensidade) { de.intensidade = as.intens[a]; de.lado = as.lado[a]; teto_win = as.teto[a]; }
            break;
        case ACAO_FINALIZA: de.finaliza |= as.disparo[a]; break;
        default:            break;
        }
    }
    out->fonte_intens = modelo_sem_ligar(m) ? 0 : fonte_int;
    out->fonte_lado = modelo_sem_ligar(m) ? 0 : fonte_lado;

    out->bloqueado = (uint8_t)(bloqueio || congela);

    if (encerra) {
        /* EXCL: o MOD se encerra. Zera o DELTA; a saida fica desligada */
        delta_iniciar(&e->delta);
        out->destinos = 0;
        out->terminou = 0;
        return;
    }

    de.congelado = (uint8_t)(bloqueio || congela);
    de.t_us = in->t_us;
    if (adota) de.liga = 1;
    if (modelo_sem_ligar(m)) {
        /* O orfao herda do alvo conforme o tipo (medido no simulador do Cronus, achado 2):
         *  DIRECT: a intensidade e o SINAL do alvo; STATIC: 100 fixo, +, sem herdar;
         *  TURBO em eixo: 100, +, sem herdar; TURBO em digital: a intensidade do alvo, sinal descartado.
         * COMBO usa o primeiro destino para decidir (limite: COMBO com destinos mistos). */
        int eixo_dest = (m->delta.dest[0] >= 10 && m->delta.dest[0] <= 13);
        if (m->delta.tipo == DELTA_STATIC || (m->delta.tipo != DELTA_DIRECT && eixo_dest)) {
            de.intensidade = 100;
            de.lado = 1;
        } else {
            de.intensidade = intens_alvo;
            de.lado = lado_alvo;
        }
    }

    if (modelo_sem_ligar(m) && m->delta.tipo == DELTA_DIRECT) {
        /* DIRECT sem gatilho que ligue nao segue nivel nenhum: o estado e um trinco. Sobe com a
         * adocao e cai com a borda de FINALIZA. Nao cai com o alvo (manual, secao 7.3 e 12.1). */
        uint8_t fim = (uint8_t)(de.finaliza && !e->delta.finaliza_ant);
        de.liga = (uint8_t)((adota || (e->delta.ligado && !fim)) ? 1 : 0);
    }

    if (gatilho_static(m) && !encerra) {
        /* manual do EIXOS, 8.3: dentro da zona (>= THR_IN e <= THR_OUT) trava; alem do THR_OUT
         * destrava; de volta ao repouso (< THR_IN) pode travar de novo. A memoria "ja destravou"
         * impede que a descida pela zona religue o MOD. O sinal e sempre + e o valor 100 x MULT. */
        uint8_t b = as.bruto[ABA_LIVRE], t_in = as.bruto_in[ABA_LIVRE], t_out = as.bruto_out[ABA_LIVRE];
        uint8_t zona = (b > 0 && b >= t_in && (t_out == 0 || b <= t_out)) ? 1 : 0;
        uint8_t repouso = (b == 0 || b < t_in) ? 1 : 0;
        uint8_t passou_teto = (t_out > 0 && b > t_out) ? 1 : 0;
        uint8_t liga = 0, fim = 0;
        if (passou_teto) {
            fim = 1;
            e->gz_destravado = 1;
        } else if (zona && !e->gz_ant && !e->gz_destravado) {
            liga = 1;
        }
        if (repouso) e->gz_destravado = 0;
        e->gz_ant = zona;
        de.liga = liga;
        de.finaliza = fim;
        de.intensidade = as.bruto_teto[ABA_LIVRE];
        de.lado = 1;
    }
    cfg = m->delta;
    if (puro && tem_fonte && m->delta.tipo == DELTA_STATIC && !modelo_sem_ligar(m)) {
        /* manual, 8.1: STATIC em eixo nao trava. Segue a zona, escreve 100 x MULT com o lado atual */
        cfg.tipo = DELTA_DIRECT;
        if (de.intensidade > 0) de.intensidade = teto_win;
    }
    delta_avaliar(&cfg, &e->delta, &de, &ds);

    out->destinos = out->bloqueado ? 0 : ds.destinos;
    out->terminou = ds.terminou;
    out->intensidade = out->bloqueado ? 0 : ds.intensidade;
    out->sinal = out->bloqueado ? 0 : ds.sinal;
}

void modelo_avaliar_ext(const ModCfg *m, ModEstado *e, const Entrada *in,
                        uint8_t congela, uint8_t encerra, ModSaida *out) {
    avaliar_nucleo(m, e, in, 0, 0, 1, congela, encerra, out);
}

void modelo_avaliar_adotado(const ModCfg *m, ModEstado *e, const Entrada *in, uint8_t adota,
                            uint8_t intens_alvo, int8_t lado_alvo, uint8_t congela, uint8_t encerra,
                            ModSaida *out) {
    avaliar_nucleo(m, e, in, adota, intens_alvo, lado_alvo, congela, encerra, out);
}

void modelo_avaliar(const ModCfg *m, ModEstado *e, const Entrada *in, ModSaida *out) {
    modelo_avaliar_ext(m, e, in, 0, 0, out);
}

void modelo_so_ocultar(const ModCfg *m, ModSaida *out) {
    unsigned a, k;
    memset(out, 0, sizeof *out);
    for (a = 0; a < ALFA_ABAS; a++) {
        const Aba *ab = &m->alfa.abas[a];
        if (!ab->oculto) continue;
        for (k = 0; k < ab->n; k++) {
            const Fonte *f = &ab->fontes[k];
            if (f->tipo == FONTE_BOTAO) out->ocultar_botoes |= 1u << f->id;
            else if (f->tipo == FONTE_EIXO) out->ocultar_analog |= (uint16_t)(1u << f->id);
            else if (f->tipo == FONTE_GATILHO) out->ocultar_analog |= (uint16_t)(1u << (EIXO_N + f->id));  /* L2, R2 */
            else if (f->tipo == FONTE_DEDO) out->ocultar_toque = 1;
        }
    }
}
