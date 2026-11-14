/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PROCESSO-ALVO DO TESTE DE VIABILIDADE (ARQUITETURA.md, secao 42, opcao 2).
 *
 * Nao e um jogo e nao tem gancho. Serve so para medir, no PS5 FAT 11.20, o que acontece com um
 * processo proprio enquanto outro processo (ferramenta do autor, fora deste arquivo) o anexa,
 * le e escreve nele e o solta. Grava um CSV com tempo, valor do marcador e contador. Pausas
 * aparecem como saltos grandes de tempo entre linhas.
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define DURACAO_S   60
#define PERIODO_US  100000     /* 100 ms */
#define SAIDA       "/mnt/usb0/dsig/viabilidade.csv"

/* Marcador conhecido: o teste confere se a escrita externa muda este valor. */
volatile uint64_t marcador = 0x1122334455667788ULL;
volatile uint32_t contador = 0;

static double agora_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    FILE *f;
    double t0, t;

    f = fopen(SAIDA, "w");
    if (!f) f = fopen("/data/dsig/viabilidade.csv", "w");
    if (!f) return 1;
    fprintf(f, "# pid=%d marcador_inicial=0x%016llx\n", (int)getpid(),
            (unsigned long long)marcador);
    fprintf(f, "# tempo_s,contador,marcador\n");
    fflush(f);

    t0 = agora_s();
    while ((t = agora_s() - t0) < DURACAO_S) {
        contador++;
        fprintf(f, "%.3f,%u,0x%016llx\n", t, (unsigned)contador,
                (unsigned long long)marcador);
        fflush(f);
        usleep(PERIODO_US);
    }
    fprintf(f, "# fim normal, contador=%u\n", (unsigned)contador);
    fclose(f);
    return 0;
}
