/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO TOQUE. Roda no Linux, sem console.
 * A calibracao usada aqui (x_max = 1000, y_max = 500) e ARBITRARIA. Serve para testar a logica
 * (normalizacao, trava, presenca), nao para afirmar a faixa real do touchpad do PS5.
 */
#include "../src/toque.h"
#include "../src/alfa.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

static unsigned idx_ms_local(unsigned ms) {
    unsigned i, n = tempo_quantos();
    uint16_t x;
    for (i = 0; i < n; i++) if (tempo_ms(i, &x) == 0 && x == ms) return i;
    return 255u;
}

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

static ToqueCal CAL = { 1000, 500 };   /* arbitraria, ver o cabecalho */

static void sem_dedo(ToqueRaw *r) {
    memset(r, 0, sizeof *r);
}

static void aplica(ToqueEstado *e, const ToqueRaw *r, Entrada *in) {
    memset(in, 0, sizeof *in);
    toque_aplicar(&CAL, e, r, in);
}

static void testes_toque(void) {
    ToqueEstado e;
    ToqueRaw r;
    Entrada in;
    int ok;

    /* validacao da calibracao */
    ok = (toque_validar(&CAL) == 0);
    { ToqueCal z = {0, 500}; if (toque_validar(&z) != -1) ok = 0; }
    passo("calibracao: recusa faixa zero", ok, "");

    /* presenca: FINGER1 vai a 1 com o dedo e a 0 sem ele */
    ok = 1;
    toque_iniciar(&e);
    memset(&r, 0, sizeof r); r.presente[0] = 1; r.x[0] = 500; r.y[0] = 250;
    aplica(&e, &r, &in);
    if (in.dedo[0] != 1) ok = 0;
    sem_dedo(&r);
    aplica(&e, &r, &in);
    if (in.dedo[0] != 0) ok = 0;
    passo("FINGER1 como botao: 1 com dedo, 0 sem dedo", ok, "");

    /* normalizacao: centro -> 0; maximo -> +100; origem -> -100 */
    ok = 1;
    toque_iniciar(&e);
    memset(&r, 0, sizeof r); r.presente[0] = 1; r.x[0] = 500; r.y[0] = 250;
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 0 || in.eixo[EIXO_F1Y] != 0) ok = 0;
    r.x[0] = 1000; r.y[0] = 0;
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 100 || in.eixo[EIXO_F1Y] != -100) ok = 0;
    passo("eixo do dedo: centro 0, maximo +100, origem -100", ok, "");

    /* trava: ao soltar, o eixo fica no ultimo toque */
    ok = 1;
    sem_dedo(&r);
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 100 || in.eixo[EIXO_F1Y] != -100) ok = 0;
    passo("eixo do dedo trava no ultimo toque ao soltar", ok, "fica em +100 e -100");

    /* fora da faixa: limita a +100 */
    ok = 1;
    toque_iniciar(&e);
    memset(&r, 0, sizeof r); r.presente[0] = 1; r.x[0] = 60000; r.y[0] = 250;
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 100) ok = 0;
    passo("coordenada fora da faixa limita a +100", ok, "");

    /* dois dedos independentes */
    ok = 1;
    toque_iniciar(&e);
    memset(&r, 0, sizeof r);
    r.presente[0] = 1; r.x[0] = 1000; r.y[0] = 500;
    r.presente[1] = 1; r.x[1] = 0; r.y[1] = 0;
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 100 || in.eixo[EIXO_F1Y] != 100) ok = 0;
    if (in.eixo[EIXO_F2X] != -100 || in.eixo[EIXO_F2Y] != -100) ok = 0;
    if (in.dedo[0] != 1 || in.dedo[1] != 1) ok = 0;
    r.presente[0] = 0;                         /* solta o dedo 1; o dedo 2 segue */
    aplica(&e, &r, &in);
    if (in.eixo[EIXO_F1X] != 100 || in.eixo[EIXO_F2X] != -100) ok = 0;
    if (in.dedo[0] != 0 || in.dedo[1] != 1) ok = 0;
    passo("dois dedos independentes; soltar um nao mexe no outro", ok, "");
}

static void testes_alfa_dedo(void) {
    AlfaCfg c;
    AlfaEstado e;
    AlfaSaida s;
    Entrada in;
    int ok = 1;

    /* FINGER1 como fonte LIVRE: ativa com dedo, desativa sem */
    memset(&c, 0, sizeof c);
    c.janela = (uint8_t)idx_ms_local(50);
    c.abas[ABA_LIVRE].n = 1;
    c.abas[ABA_LIVRE].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    c.abas[ABA_LIVRE].fontes[0].tipo = FONTE_DEDO;
    c.abas[ABA_LIVRE].fontes[0].id = 0;
    if (alfa_validar(&c) != 0) ok = 0;
    memset(&in, 0, sizeof in); in.dedo[0] = 1;
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 1) ok = 0;
    memset(&in, 0, sizeof in);
    alfa_iniciar(&e); alfa_avaliar(&c, &e, &in, &s);
    if (s.ativa[ABA_LIVRE] != 0) ok = 0;
    passo("FINGER1 como fonte de botao na ALFA", ok, "");

    /* so dois dedos existem */
    ok = 1;
    c.abas[ABA_LIVRE].fontes[0].id = 2;
    if (alfa_validar(&c) != -1) ok = 0;
    passo("validacao: so FINGER1 e FINGER2 como botao", ok, "");
}

int main(void) {
    testes_toque();
    testes_alfa_dedo();
    if (falhas) { printf("  => BANCADA DO TOQUE FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do toque aberta: todas as verificacoes passaram\n");
    return 0;
}
