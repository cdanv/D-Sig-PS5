/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DA TABELA UNICA DE TEMPO.
 *
 * As listas abaixo sao escritas a mao, a partir do manual do Cronus 1.0.17, e nao
 * vem do gerador de src/tempo.c. Duas fontes independentes: a tabela e o manual.
 * Se as duas nao batem, a bancada reprova.
 */
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

/* HOLD: 63 valores, secao 13.3 */
static const unsigned HOLD_ESPERADO[63] = {
     50, 60, 70, 80, 90,100,110,120,130,140,150,160,170,180,190,200,210,220,230,240,250,260,270,280,290,300,
    350,400,450,500,550,600,650,700,750,800,850,900,950,1000,
    1100,1200,1300,1400,1500,1600,1700,1800,1900,2000,2100,2200,2300,2400,2500,2600,
    2800,3000,3200,3400,3600,3800,4000
};
/* AFTER: 8 valores, secao 13.4 */
static const unsigned AFTER_ESPERADO[8] = {50,100,150,200,300,500,1000,2000};
/* DELAY: OFF e 3 valores, secao 17 */
static const unsigned DELAY_ESPERADO[3] = {300,700,1200};
/* TURBO passo: 8 valores, secao 21 */
static const unsigned PASSO_ESPERADO[8] = {30,50,70,100,150,250,500,1000};
/* TURBO latencia: 15 valores (o "sem latencia" nao e tempo), secao 21 */
static const unsigned LAT_ESPERADO[15] = {10,20,30,40,50,60,80,100,120,150,200,250,350,500,750};

static int esta_na_tabela(unsigned v) {
    unsigned i, n = tempo_quantos();
    uint16_t x;
    for (i = 0; i < n; i++) {
        if (tempo_ms(i, &x) == 0 && x == v) return 1;
    }
    return 0;
}

static int contem_todos(const unsigned *lista, size_t n, const char **faltou) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (!esta_na_tabela(lista[i])) { *faltou = "valor do manual ausente"; return 0; }
    }
    return 1;
}

int main(void) {
    unsigned n = tempo_quantos(), i;
    uint16_t anterior = 0, atual = 0, ultimo = 0, primeiro = 0;
    int crescente = 1, extras = 0;
    const char *faltou = "";

    printf("  tabela com %u valores\n", n);

    passo("a tabela tem 67 valores", n == 67, "uniao de HOLD, AFTER, DELAY, passo e LAT");

    crescente = 1;
    for (i = 0; i < n; i++) {
        if (tempo_ms(i, &atual) != 0) { crescente = 0; break; }
        if (i == 0) primeiro = atual;
        else if (atual <= anterior) crescente = 0;
        anterior = atual;
    }
    ultimo = atual;
    passo("estritamente crescente, sem repetir", crescente, "");
    passo("primeiro valor = 10 ms, ultimo = 4000 ms", primeiro == 10 && ultimo == 4000, "");

    passo("contem os 63 valores do HOLD",
          contem_todos(HOLD_ESPERADO, 63, &faltou), faltou);
    passo("contem os 8 valores do AFTER",
          contem_todos(AFTER_ESPERADO, 8, &faltou), faltou);
    passo("contem os 3 valores do DELAY",
          contem_todos(DELAY_ESPERADO, 3, &faltou), faltou);
    passo("contem os 8 valores do passo do TURBO",
          contem_todos(PASSO_ESPERADO, 8, &faltou), faltou);
    passo("contem os 15 valores da latencia do TURBO",
          contem_todos(LAT_ESPERADO, 15, &faltou), faltou);

    /* Nenhum valor a mais: cada entrada da tabela deve estar em alguma lista do manual. */
    for (i = 0; i < n; i++) {
        unsigned j, achou = 0;
        tempo_ms(i, &atual);
        for (j = 0; j < 63 && !achou; j++) if (HOLD_ESPERADO[j] == atual) achou = 1;
        for (j = 0; j < 15 && !achou; j++) if (LAT_ESPERADO[j] == atual) achou = 1;
        if (!achou) extras++;
    }
    passo("nenhum valor fora do manual", extras == 0, "");

    /* Indice fora da lista e ponteiro nulo: recusados, sem escrever. */
    {
        uint16_t x = 999;
        int r1 = tempo_ms(n, &x);
        int r2 = tempo_ms(0, 0);
        passo("indice fora da lista e recusado", r1 == -1 && x == 999 && r2 == -1, "");
    }

    if (falhas) { printf("  => BANCADA DO TEMPO FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do tempo aberta: todas as verificacoes passaram\n");
    return 0;
}
