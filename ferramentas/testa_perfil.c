/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO BUFFER DUPLO COM VERSAO. Roda no Linux, com duas threads de verdade.
 *
 * O escritor publica N perfis. O perfil de numero n tem TODOS os campos iguais a n
 * e versao n. Assim, um perfil inteiro se reconhece sozinho: se a copia misturou dois
 * perfis, os campos diferem entre si ou a soma nao bate.
 *
 * O QUE ELA EXIGE:
 *  1. Toda copia ACEITA (retorno 0) e inteira: campos iguais entre si, iguais a versao,
 *     e soma consistente.
 *  2. As versoes aceitas nunca regridem: o leitor nunca volta a um perfil mais antigo.
 *  3. Ao fim, uma leitura sem escritor concorrente e aceita e traz a ultima versao.
 *  4. Houve leituras aceitas e descartadas: a bancada so prova algo se a concorrencia
 *     de fato aconteceu.
 */
#include "../src/perfil.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define N_PUBLICACOES 200000u

static atomic_int g_terminou = 0;
static unsigned long g_aceitos = 0, g_descartados = 0;
static unsigned long g_inconsistentes = 0, g_regressoes = 0;
static uint32_t g_ultima_aceita = 0;

static void *escritor(void *arg) {
    PerfilDados p;
    uint32_t n;
    (void)arg;
    for (n = 1; n <= N_PUBLICACOES; n++) {
        unsigned i;
        for (i = 0; i < PERFIL_CAMPOS; i++) p.campos[i] = n;
        p.versao = n;
        perfil_publicar(&p);
    }
    atomic_store(&g_terminou, 1);
    return 0;
}

static void *leitor(void *arg) {
    PerfilDados q;
    (void)arg;
    while (!atomic_load(&g_terminou)) {
        if (perfil_ler(&q) == 0) {
            unsigned i;
            uint32_t soma = 0;
            int ok = 1;
            for (i = 0; i < PERFIL_CAMPOS; i++) {
                soma += q.campos[i];
                if (q.campos[i] != q.campos[0]) ok = 0;
            }
            if (!ok || soma != q.soma || q.campos[0] != q.versao) g_inconsistentes++;
            if (q.versao < g_ultima_aceita) g_regressoes++;
            g_ultima_aceita = q.versao;
            g_aceitos++;
        } else {
            g_descartados++;
        }
    }
    return 0;
}

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

int main(void) {
    pthread_t tw, tr;
    PerfilDados final;
    char d[160];

    pthread_create(&tr, 0, leitor, 0);
    pthread_create(&tw, 0, escritor, 0);
    pthread_join(tw, 0);
    pthread_join(tr, 0);

    printf("  %u publicacoes, leituras aceitas: %lu, descartadas: %lu\n",
           N_PUBLICACOES, g_aceitos, g_descartados);

    passo("toda copia aceita e inteira", g_inconsistentes == 0,
          "campos iguais entre si, iguais a versao, soma consistente");
    passo("versoes aceitas nunca regridem", g_regressoes == 0, "");
    passo("houve concorrencia real: copias aceitas e descartadas",
          g_aceitos > 0 && g_descartados > 0,
          "sem descartes, a bancada nao testou o caminho de risco");

    {
        int r = perfil_ler(&final);
        snprintf(d, sizeof d, "versao final = %u", (unsigned)final.versao);
        passo("leitura sem escritor e aceita e traz a ultima versao",
              r == 0 && final.versao == N_PUBLICACOES && perfil_versao_publicada() == N_PUBLICACOES,
              d);
    }

    if (falhas) { printf("  => BANCADA DO PERFIL FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do perfil aberta: todas as verificacoes passaram\n");
    return 0;
}
