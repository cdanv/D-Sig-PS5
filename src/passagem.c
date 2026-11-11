/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ALVO 1 — A PASSAGEM PURA.
 *
 * Este arquivo nao roteia nada. Ele recebe o pacote do controle e devolve o pacote
 * do controle, byte por byte. E de proposito.
 *
 * POR QUE COMECAR PELO QUE NAO FAZ NADA. O D-Sig-PS5 vai rodar DENTRO do processo do
 * jogo, substituindo as chamadas que o jogo faz a libScePad. Um defeito aqui nao faz
 * o controle agir estranho como no Cronus — faz o jogo travar. Antes de qualquer MOD,
 * e preciso provar que a ponte, sozinha, e invisivel: que o jogo recebe exatamente o
 * que receberia sem ela.
 *
 * Sem esta prova, todo MOD que vier depois esta apoiado no vazio. Se o jogo engasgar
 * na terceira semana, nao havera como saber se foi o MOD ou a ponte.
 *
 * O QUE "INVISIVEL" QUER DIZER, e e mais estrito do que parece: todos os campos
 * IGUAIS, inclusive os `reserved` e o enchimento entre campos. O jogo pode ler o que
 * quiser da struct, e um byte de lixo diferente ja e uma diferenca. Por isso a
 * bancada compara a struct INTEIRA com memcmp, e nao campo a campo.
 *
 * ESTE ARQUIVO NAO SABE QUE EXISTE PS5, e e regra, nao acidente: nenhum cabecalho de
 * SDK, nenhuma chamada de sistema. Por isso o gcc do Linux o compila como esta, e a
 * bancada o linka e roda. Quando a ponte com a libScePad existir, ela entra em arquivo
 * separado — o roteador continua compilando dos dois lados.
 *
 * E a mesma ideia do gpc2c.py no projeto do Cronus: testar o que vai rodar, nao uma
 * imitacao dele. So que aqui sem transpilador, porque ja e C.
 */
#include "dsig.h"
#include <string.h>

/* Estado do roteador. Por enquanto so conta — e a contagem ja serve para a bancada
 * provar que a ponte foi chamada, em vez de supor. Teste que passaria com a funcao
 * nunca chamada nao testa nada. */
static DsigEstado g_estado;

void dsig_iniciar(void) {
    memset(&g_estado, 0, sizeof g_estado);
}

const DsigEstado *dsig_estado(void) {
    return &g_estado;
}

/* O CORACAO, por enquanto vazio.
 *
 * `entrada`  — o pacote como o controle o entregou. Nunca e escrito: e o equivalente
 *              do get_ival do Cronus, e a licao de la vale aqui. Por isso o const.
 * `saida`    — o pacote que o jogo vai receber. Equivale ao get_val/set_val.
 *
 * Separar os dois em ponteiros distintos, em vez de mexer num so, e decisao e nao
 * detalhe: no Cronus a confusao entre "o que entrou" e "o que vai sair" foi a origem
 * de defeitos que nao davam sinal — um MOD acima zerava um eixo e o de baixo parava
 * de funcionar em silencio. Aqui o compilador impede: `entrada` e const.
 */
void dsig_rotear(const ScePadData *entrada, ScePadData *saida) {
    g_estado.quadros++;
    if (entrada == 0 || saida == 0) {        /* o jogo pode chamar com lixo */
        g_estado.recusas++;
        return;
    }
    *saida = *entrada;                        /* a passagem: copia integral */
    g_estado.passagens++;
}
