/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO ALVO 2 — o primeiro MOD faz o que diz, e so isso.
 *
 * Roda no Linux, sem PS5, contra o MESMO src/passagem.c que vai para o console.
 *
 * O QUE ELA EXIGE:
 *
 *  1. O RESULTADO ESPERADO E CALCULADO AQUI, por uma formula escrita de novo, e nao
 *     pelo codigo do roteador. Se a bancada reusasse o roteador, um erro igual nos
 *     dois passaria em silencio.
 *  2. SO O CAMPO `buttons` PODE MUDAR. O resto da struct, enchimento incluido, tem de
 *     ficar igual. Um MOD que mexesse num eixo por engano seria um defeito silencioso.
 *  3. A CONTAGEM DE DISPAROS e feita pela bancada, por conta propria, e tem de bater
 *     com o contador do roteador. Teste que passaria com o MOD nunca disparando nao
 *     prova nada.
 *  4. ENTRADA INTOCADA, conferida por copia, como na bancada do Alvo 1.
 *  5. ACORDE: dois botoes juntos disparam; um so, nao.
 *  6. ORDEM NAO MUDA O RESULTADO: a mesma tabela em ordem invertida da a mesma saida.
 *  7. CARGA INVALIDA E RECUSADA E NAO TROCA A TABELA ANTERIOR.
 *  8. CONFLITO TEM REGRA: quando um MOD solta um botao e outro o aperta, o aperto vence.
 */
#include "../src/dsig.h"
#include <stdio.h>
#include <string.h>

#define N_PACOTES 200000

static unsigned long long semente;
static unsigned char prox_byte(void) {
    semente = semente * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned char)(semente >> 33);
}

static void gera(ScePadData *p) {
    unsigned char *b = (unsigned char *)p;
    size_t k;
    for (k = 0; k < sizeof *p; k++) b[k] = prox_byte();
}

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

/* Igualdade total, exceto o campo `buttons`, que e o unico que o MOD pode alterar. */
static int iguais_exceto_botoes(const ScePadData *a, const ScePadData *b) {
    ScePadData x = *a, y = *b;
    x.buttons = 0;
    y.buttons = 0;
    return memcmp(&x, &y, sizeof x) == 0;
}

/* A formula do MOD, escrita aqui de novo, como especificacao: se todos os botoes da
 * origem estao pressionados, solta a origem e aperta o destino. */
static unsigned long esperado(unsigned long b, unsigned long origem, unsigned long destino) {
    if ((b & origem) == origem) return (b & ~origem) | destino;
    return b;
}

/* Roda N pacotes aleatorios pela tabela `mods` e confere, pacote a pacote, o campo
 * buttons contra `esperado_fn` e o resto da struct. Devolve os disparos contados. */
static unsigned long bateria(const DsigMod *mods, size_t n, unsigned long origem_teste,
                             unsigned long destino_teste, int *ok_botoes, int *ok_resto,
                             int *ok_entrada, unsigned long long *hash_saida) {
    unsigned long disparos_esperados = 0;
    unsigned long long hash = 1469598103934665603ULL;
    ScePadData entrada, copia, saida;
    size_t i, k;

    *ok_botoes = 1; *ok_resto = 1; *ok_entrada = 1;
    if (mods) { if (dsig_carregar(mods, n) != 0) { *ok_botoes = 0; return 0; } }

    for (i = 0; i < N_PACOTES; i++) {
        gera(&entrada);
        copia = entrada;
        memset(&saida, 0xA5, sizeof saida);      /* lixo: a saida tem de ser sobrescrita */
        dsig_rotear(&entrada, &saida);

        if (saida.buttons != (uint32_t)esperado(entrada.buttons, origem_teste, destino_teste))
            *ok_botoes = 0;
        if (!iguais_exceto_botoes(&saida, &entrada)) *ok_resto = 0;
        if (memcmp(&copia, &entrada, sizeof entrada) != 0) *ok_entrada = 0;
        if ((entrada.buttons & origem_teste) == origem_teste) disparos_esperados++;

        {
            const unsigned char *b = (const unsigned char *)&saida;
            for (k = 0; k < sizeof saida; k++) {
                hash ^= b[k];
                hash *= 1099511628211ULL;
            }
        }
    }
    if (hash_saida) *hash_saida = hash;
    return disparos_esperados;
}

int main(void) {
    const DsigEstado *e;
    DsigMod trocar[1]  = { { DS_BTN_L1, DS_BTN_CROSS } };
    DsigMod acorde[1]  = { { DS_BTN_L1 | DS_BTN_R1, DS_BTN_CROSS } };
    unsigned long disparos_esperados;
    unsigned long long hash = 0;
    int ok_botoes, ok_resto, ok_entrada;

    printf("  MOD de teste: L1 entregue como CROSS\n");
    dsig_iniciar();
    semente = 20261111ULL;
    disparos_esperados = bateria(trocar, 1, DS_BTN_L1, DS_BTN_CROSS,
                                 &ok_botoes, &ok_resto, &ok_entrada, &hash);
    e = dsig_estado();
    passo("o botao sai do lugar certo", ok_botoes,
          "200000 pacotes, campo buttons contra a formula da bancada");
    passo("so o campo buttons muda", ok_resto,
          "resto da struct, enchimento incluido, igual a entrada");
    passo("a entrada nao foi tocada", ok_entrada, "o roteador so le");
    passo("o MOD disparou as vezes certas",
          e->disparos == disparos_esperados && disparos_esperados > 0,
          "contagem da bancada contra contagem do roteador");
    printf("  disparos: %lu de %d quadros\n", disparos_esperados, N_PACOTES);
    printf("  hash de comportamento (troca): %llu\n", hash);

    /* ACORDE: vetores explicitos, cada um com a saida esperada escrita a mao. */
    {
        struct { unsigned long entrada, saida_esperada; const char *nome; } casos[] = {
            { 0,                                     0,                        "nada pressionado" },
            { DS_BTN_L1,                             DS_BTN_L1,                "um botao do acorde" },
            { DS_BTN_R1,                             DS_BTN_R1,                "o outro botao do acorde" },
            { DS_BTN_L1 | DS_BTN_R1,                 DS_BTN_CROSS,             "acorde completo" },
            { DS_BTN_L1 | DS_BTN_R1 | DS_BTN_CIRCLE, DS_BTN_CROSS | DS_BTN_CIRCLE,
                                                                               "acorde mais um botao" },
        };
        int ok = 1;
        size_t i;
        dsig_iniciar();
        if (dsig_carregar(acorde, 1) != 0) ok = 0;
        for (i = 0; i < sizeof casos / sizeof casos[0]; i++) {
            ScePadData s, t;
            memset(&s, 0, sizeof s);
            memset(&t, 0, sizeof t);
            s.buttons = (uint32_t)casos[i].entrada;
            dsig_rotear(&s, &t);
            if (t.buttons != (uint32_t)casos[i].saida_esperada) ok = 0;
        }
        passo("acorde dispara so com os dois botoes", ok,
              "5 vetores escritos a mao");
    }

    /* ORDEM: a mesma tabela em ordem invertida, com conflito real entre os MODs.
     *   A: L1 -> CROSS          B: CROSS -> CIRCLE          C: R1|R2 -> TRIANGLE */
    {
        DsigMod ordem1[3] = {
            { DS_BTN_L1,            DS_BTN_CROSS    },
            { DS_BTN_CROSS,         DS_BTN_CIRCLE   },
            { DS_BTN_R1 | DS_BTN_R2, DS_BTN_TRIANGLE },
        };
        DsigMod ordem2[3] = { ordem1[2], ordem1[1], ordem1[0] };
        unsigned long diferencas = 0;
        int carga_ok = 1;
        size_t i;

        /* Cada pacote passa pelas duas ordens. A tabela e recarregada a cada volta, de
         * proposito: o roteador le a tabela em cada quadro, e a ordem tem de importar
         * so pelo conteudo da tabela, nunca pelo estado que sobrou da volta anterior. */
        dsig_iniciar();
        semente = 42ULL;
        for (i = 0; i < N_PACOTES; i++) {
            ScePadData entrada, s1, s2;
            gera(&entrada);
            if (dsig_carregar(ordem1, 3) != 0) { carga_ok = 0; break; }
            dsig_rotear(&entrada, &s1);
            if (dsig_carregar(ordem2, 3) != 0) { carga_ok = 0; break; }
            dsig_rotear(&entrada, &s2);
            if (s1.buttons != s2.buttons) diferencas++;
        }
        passo("a ordem da tabela nao muda a saida", carga_ok && diferencas == 0,
              "200000 pacotes, tabela em ordem normal e invertida");
    }

    /* CONFLITO: L1 -> CROSS, e CROSS -> CIRCLE. Com L1 sozinho, o segundo nao dispara.
     * Com L1 + CROSS, os dois disparam: CROSS sai pelo segundo e entra pelo primeiro.
     * O aperto vence: o resultado tem CROSS e CIRCLE, e nao tem L1. */
    {
        DsigMod conflito[2] = {
            { DS_BTN_L1,    DS_BTN_CROSS  },
            { DS_BTN_CROSS, DS_BTN_CIRCLE },
        };
        ScePadData s, t;
        int ok = 1;
        dsig_iniciar();
        if (dsig_carregar(conflito, 2) != 0) ok = 0;

        memset(&s, 0, sizeof s);
        s.buttons = DS_BTN_L1;
        dsig_rotear(&s, &t);
        if (t.buttons != DS_BTN_CROSS) ok = 0;

        memset(&s, 0, sizeof s);
        s.buttons = DS_BTN_L1 | DS_BTN_CROSS;
        dsig_rotear(&s, &t);
        if (t.buttons != (DS_BTN_CROSS | DS_BTN_CIRCLE)) ok = 0;

        passo("conflito: o aperto vence o solto", ok,
              "L1 sozinho -> CROSS; L1+CROSS -> CROSS+CIRCLE");
    }

    /* CARGA INVALIDA: recusada, e a tabela anterior continua valendo. */
    {
        DsigMod boa[1]   = { { DS_BTN_L1, DS_BTN_CROSS } };
        DsigMod vazia[2] = { { DS_BTN_L1, DS_BTN_CROSS }, { 0, DS_BTN_CIRCLE } };
        DsigMod muitas[DSIG_MAX_MODS + 1];
        ScePadData s, t;
        int ok = 1;
        size_t i;

        dsig_iniciar();
        if (dsig_carregar(boa, 1) != 0) ok = 0;
        if (dsig_carregar(vazia, 2) != -1) ok = 0;            /* origem vazia */
        if (dsig_carregar(boa, DSIG_MAX_MODS + 1) != -1) ok = 0; /* mais que o teto */
        for (i = 0; i < DSIG_MAX_MODS + 1; i++) muitas[i] = boa[0];
        if (dsig_carregar(muitas, DSIG_MAX_MODS + 1) != -1) ok = 0;
        if (dsig_carregar(0, 1) != -1) ok = 0;                /* nulo com n > 0 */

        memset(&s, 0, sizeof s);
        s.buttons = DS_BTN_L1;
        dsig_rotear(&s, &t);
        if (t.buttons != DS_BTN_CROSS) ok = 0;                /* a tabela boa sobreviveu */

        passo("carga invalida e recusada, tabela anterior preservada", ok,
              "origem vazia, mais que o teto, nulo com n>0");
    }

    /* SEM MOD: a passagem do Alvo 1 volta a valer, byte a byte. */
    {
        int ok = 1;
        ScePadData entrada, saida;
        size_t i;
        dsig_iniciar();
        if (dsig_carregar(0, 0) != 0) ok = 0;
        semente = 7ULL;
        for (i = 0; i < 20000; i++) {
            gera(&entrada);
            dsig_rotear(&entrada, &saida);
            if (memcmp(&saida, &entrada, sizeof saida) != 0) ok = 0;
        }
        passo("sem MOD carregado, a passagem e integral", ok, "20000 pacotes");
    }

    printf("\n");
    if (falhas) {
        printf("  => BANCADA DO MOD FECHADA: %d reprovaram\n", falhas);
        return 1;
    }
    printf("  => bancada do MOD aberta: todas as verificacoes passaram\n");
    return 0;
}
