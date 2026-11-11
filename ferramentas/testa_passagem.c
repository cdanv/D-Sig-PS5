/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO ALVO 1 — prova que a passagem e invisivel.
 *
 * Roda no Linux, sem PS5, contra o MESMO passagem.c que vai para o console.
 *
 * O QUE ELA EXIGE, e cada item existe porque o oposto e um defeito que nao da sinal:
 *
 *  1. IGUALDADE TOTAL, por memcmp, nao campo a campo. O jogo le a struct inteira,
 *     inclusive os `reserved` e o enchimento entre campos. Comparar campo a campo
 *     passaria por cima de lixo diferente no enchimento, que e diferenca real.
 *
 *  2. 200.000 PACOTES ALEATORIOS, byte a byte. E o fuzz do Cronus trazido para ca:
 *     um `*saida = *entrada` parece obviamente certo, e e — mas o dia em que alguem
 *     puser um campo calculado no meio, o obvio deixa de valer sem avisar.
 *
 *  3. A ENTRADA NAO PODE SER TOCADA. O roteador recebe `const`, mas const se burla
 *     com um cast, e a bancada nao confia na promessa: ela guarda uma copia da
 *     entrada antes e confere depois. No Cronus, escrever onde se devia so ler foi
 *     a origem de defeitos silenciosos a sessao inteira.
 *
 *  4. HASH DE COMPORTAMENTO. O mesmo truque do portao do Cronus: um numero que
 *     resume as 200.000 saidas. Se ele mudar sem ninguem pedir, alguma coisa mudou
 *     o comportamento. E o unico jeito de perceber uma alteracao que passa nos
 *     testes.
 *
 *  5. A PONTE FOI MESMO CHAMADA. O contador tem de bater com o numero de pacotes.
 *     Teste que passaria com a funcao nunca chamada nao prova nada.
 *
 *  6. PONTEIRO INVALIDO NAO DERRUBA. O jogo pode chamar com nulo; dentro do
 *     processo dele, um acesso invalido nao e um controle estranho, e uma tela
 *     preta.
 */
#include "../src/dsig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define N_PACOTES 200000

/* Gerador deterministico: a mesma semente da a mesma bateria, em qualquer maquina.
 * Bateria que muda sozinha nao serve de linha de base. */
static unsigned long long semente;
static unsigned char prox_byte(void) {
    semente = semente * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned char)(semente >> 33);
}

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

int main(void) {
    ScePadData entrada, copia, saida;
    unsigned long long hash = 1469598103934665603ULL;   /* FNV-1a 64 */
    size_t i, k;
    int iguais = 1, entrada_intacta = 1;
    size_t primeira_diferenca = (size_t)-1;

    printf("  sizeof(ScePadData) = %zu bytes\n", sizeof(ScePadData));
    dsig_iniciar();

    semente = 20261111ULL;
    for (i = 0; i < N_PACOTES; i++) {
        unsigned char *p = (unsigned char *)&entrada;
        for (k = 0; k < sizeof entrada; k++) p[k] = prox_byte();
        copia = entrada;                       /* guarda para conferir depois */
        memset(&saida, 0xA5, sizeof saida);    /* lixo conhecido: a saida tem de
                                                  ser SOBRESCRITA, nao herdada */
        dsig_rotear(&entrada, &saida);

        if (memcmp(&saida, &entrada, sizeof saida) != 0) {
            if (iguais) {
                const unsigned char *a = (const unsigned char *)&entrada;
                const unsigned char *b = (const unsigned char *)&saida;
                for (k = 0; k < sizeof saida; k++)
                    if (a[k] != b[k]) { primeira_diferenca = k; break; }
            }
            iguais = 0;
        }
        if (memcmp(&copia, &entrada, sizeof entrada) != 0) entrada_intacta = 0;

        {   /* o hash corre sobre a SAIDA: e o comportamento observavel */
            const unsigned char *b = (const unsigned char *)&saida;
            for (k = 0; k < sizeof saida; k++) {
                hash ^= b[k];
                hash *= 1099511628211ULL;
            }
        }
    }

    {
        char d[128];
        snprintf(d, sizeof d, "%d pacotes aleatorios, byte a byte", N_PACOTES);
        passo("a saida e identica a entrada", iguais,
              iguais ? d : (snprintf(d, sizeof d,
                            "primeira diferenca no byte %zu", primeira_diferenca), d));
    }
    passo("a entrada nao foi tocada", entrada_intacta, "o roteador so le");

    {
        const DsigEstado *e = dsig_estado();
        char d[128];
        snprintf(d, sizeof d, "%lu quadros, %lu passagens", e->quadros, e->passagens);
        passo("a ponte foi mesmo chamada",
              e->quadros == N_PACOTES && e->passagens == N_PACOTES, d);
    }

    /* ponteiro invalido: tem de recusar, nao de cair */
    {
        const DsigEstado *e;
        unsigned long antes;
        e = dsig_estado(); antes = e->recusas;
        dsig_rotear(0, &saida);
        dsig_rotear(&entrada, 0);
        dsig_rotear(0, 0);
        e = dsig_estado();
        passo("ponteiro invalido e recusado, nao derruba",
              e->recusas == antes + 3, "tres chamadas invalidas, tres recusas");
    }

    printf("\n  hash de comportamento: %llu\n", hash);
    if (falhas) {
        printf("  => BANCADA FECHADA: %d reprovaram\n", falhas);
        return 1;
    }
    printf("  => bancada aberta: 4 verificacoes passaram\n");
    return 0;
}
