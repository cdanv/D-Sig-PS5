/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A BANCADA DO FORMATO BINARIO DO PERFIL. Roda no Linux, sem console.
 * O CRC32 e conferido contra o vetor padrao; o resto, contra regras escritas a mao.
 */
#include "../src/formato.h"
#include "../src/rede.h"
#include "../src/tempo.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
#define B_CROSS 14
#define B_SQUARE 15
#define D_CIRCLE 7
#define D_L1 0

static int falhas = 0;
static void passo(const char *nome, int ok, const char *detalhe) {
    printf("  [%s] %s%s%s\n", ok ? "ok " : "FALHOU", nome,
           detalhe && *detalhe ? "   " : "", detalhe ? detalhe : "");
    if (!ok) falhas++;
}

static unsigned idx_ms(unsigned ms) {
    unsigned i, n = tempo_quantos();
    uint16_t x;
    for (i = 0; i < n; i++) if (tempo_ms(i, &x) == 0 && x == ms) return i;
    return 255u;
}

static Fonte botao(uint8_t bit) { Fonte f; memset(&f, 0, sizeof f); f.tipo = FONTE_BOTAO; f.id = bit; return f; }

static void mod_base(ModCfg *m) {
    unsigned a;
    memset(m, 0, sizeof *m);
    m->alfa.janela = (uint8_t)idx_ms(50);
    for (a = 0; a < ALFA_ABAS; a++) m->alfa.abas[a].delay = ALFA_DELAY_NAO_ESCOLHIDO;
    m->delta.lat = DELTA_SEM_LAT;
    m->delta.ciclos = 0;
    m->acao[ABA_LIVRE] = ACAO_LIGA;
}

/* rede de exemplo: MOD 0 (LIVRE CROSS -> DIRECT CIRCLE, D1); MOD 1 (LIVRE SQUARE -> STATIC L1,
 * D1 e D2, ASSOC ao MOD 0, EXCL ao MOD 0 na subida) */
static void rede_exemplo(RedeCfg *r) {
    memset(r, 0, sizeof *r);
    r->n = 2;
    mod_base(&r->mod[0]);
    r->mod[0].alfa.abas[ABA_LIVRE].n = 1;
    r->mod[0].alfa.abas[ABA_LIVRE].fontes[0] = botao(B_CROSS);
    r->mod[0].alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    r->mod[0].delta.tipo = DELTA_DIRECT; r->mod[0].delta.n_dest = 1; r->mod[0].delta.dest[0] = D_CIRCLE;
    mod_base(&r->mod[1]);
    r->mod[1].alfa.abas[ABA_LIVRE].n = 1;
    r->mod[1].alfa.abas[ABA_LIVRE].fontes[0] = botao(B_SQUARE);
    r->mod[1].alfa.abas[ABA_LIVRE].delay = ALFA_DELAY_OFF;
    r->mod[1].delta.tipo = DELTA_STATIC; r->mod[1].delta.n_dest = 1; r->mod[1].delta.dest[0] = D_L1;
    r->z[1].assoc = BIT(0);
    r->z[1].excl = BIT(0);
    r->z[1].excl_inic = 1;
    r->dom[0] = 0x1;
    r->dom[1] = 0x3;
}

static uint8_t buf[2048];
static uint8_t buf2[2048];

/* conserta o CRC depois de alterar um byte de proposito, para testar a checagem seguinte */
static void recalcula_crc(uint8_t *b, size_t len) {
    uint32_t c = formato_crc32(b, len - 4);
    b[len - 4] = (uint8_t)c; b[len - 3] = (uint8_t)(c >> 8);
    b[len - 2] = (uint8_t)(c >> 16); b[len - 1] = (uint8_t)(c >> 24);
}

/* a mesma rede, da leitura e da original, produz a mesma saida num cronograma */
static int mesmo_comportamento(const RedeCfg *a, const RedeCfg *b) {
    RedeEstado ea, eb; RedeSaida sa, sb; uint64_t t;
    rede_iniciar(&ea); rede_iniciar(&eb);
    ea.dom.atual = 1; eb.dom.atual = 1;
    for (t = 0; t <= 600; t += 10) {
        Entrada in;
        memset(&in, 0, sizeof in);
        if (t >= 100 && t < 200) in.botoes |= BIT(B_CROSS);
        if (t >= 300 && t < 400) in.botoes |= BIT(B_SQUARE);
        in.t_us = t * 1000u;
        rede_avaliar(a, &ea, &in, &sa);
        rede_avaliar(b, &eb, &in, &sb);
        if (sa.botoes != sb.botoes) return 0;
    }
    return 1;
}

static void testes(void) {
    RedeCfg r, lido; size_t len = 0, len2 = 0;
    int ok;

    /* ida e volta: tamanho exato, e regravar a leitura da o mesmo arquivo byte a byte */
    rede_exemplo(&r);
    ok = (formato_gravar(&r, buf, sizeof buf, &len) == 0);
    if (len != FORMATO_CABECALHO + 2 * (FORMATO_BLOCO_ALFA_DELTA + FORMATO_ZETA_DOM) + FORMATO_RODAPE) ok = 0;
    passo("gravar: escreve a rede e o tamanho bate com o formato", ok, "8 + 2 x 316 + 4 = 644 bytes");

    ok = (formato_ler(buf, len, &lido) == 0);
    passo("ler: aceita o arquivo que acabou de ser gravado", ok, "");

    ok = (formato_gravar(&lido, buf2, sizeof buf2, &len2) == 0) && len2 == len && memcmp(buf, buf2, len) == 0;
    passo("ida e volta: regravar a leitura da o mesmo arquivo, byte a byte", ok, "");

    ok = mesmo_comportamento(&r, &lido);
    passo("a rede lida se comporta igual a original num cronograma", ok, "");

    /* corrupcao: um byte no meio, sem recalcular o CRC: rejeitada pelo CRC */
    ok = 1;
    { uint8_t c[2048]; memcpy(c, buf, len); c[len / 2] ^= 0x01;
      if (formato_ler(c, len, &lido) != -1) ok = 0; }
    passo("corrupcao de um byte e rejeitada (CRC)", ok, "");

    /* magic errado, com CRC recalculado: rejeitada pelo magic */
    ok = 1;
    { uint8_t c[2048]; memcpy(c, buf, len); c[0] = 'X'; recalcula_crc(c, len);
      if (formato_ler(c, len, &lido) != -1) ok = 0; }
    passo("magic errado e rejeitado mesmo com CRC valido", ok, "");

    /* versao diferente, com CRC recalculado: rejeitada pela versao */
    ok = 1;
    { uint8_t c[2048]; memcpy(c, buf, len); c[4] = 2; recalcula_crc(c, len);
      if (formato_ler(c, len, &lido) != -1) ok = 0; }
    passo("versao diferente e rejeitada mesmo com CRC valido", ok, "");

    /* truncado */
    ok = (formato_ler(buf, len - 5, &lido) == -1);
    passo("arquivo truncado e rejeitado", ok, "");

    /* rede invalida nao e gravada (relacao consigo) */
    ok = 1;
    { RedeCfg x = r; x.z[0].bloq = BIT(0);
      if (formato_gravar(&x, buf2, sizeof buf2, &len2) != -1) ok = 0; }
    passo("gravar recusa rede invalida", ok, "");

    /* buffer pequeno demais */
    ok = (formato_gravar(&r, buf2, 100, &len2) == -1);
    passo("gravar recusa buffer pequeno demais", ok, "");
}

int main(void) {
    /* conferencia do CRC32 com o vetor padrao */
    passo("CRC32 confere com o vetor padrao (123456789 -> cbf43926)",
          formato_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u, "");
    testes();
    if (falhas) { printf("  => BANCADA DO FORMATO FECHADA: %d reprovaram\n", falhas); return 1; }
    printf("  => bancada do formato aberta: todas as verificacoes passaram\n");
    return 0;
}
