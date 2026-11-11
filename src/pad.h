/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O ESTADO DO CONTROLE, como o PS5 entrega ao jogo.
 *
 * ORIGEM: este layout veio do payload/pad_types.h do PoorDS4 v0.2.0
 * (github.com/ItsBlurf/PoorDS4), GPL-3.0-or-later. Ele descreve a ABI da
 * libScePad da Sony; o D-Sig-PS5 e GPL-3.0 por causa disto, e a atribuicao fica
 * aqui porque e de onde o conhecimento veio.
 *
 * O QUE MUDA EM RELACAO AO CRONUS. No Cronus o D-Sig via o controle por indices
 * (get_ival(PS5_LX)) e mexia um valor por vez. Aqui ele recebe a ESTRUTURA INTEIRA
 * por quadro: os 32 botoes num unico int, os quatro eixos, os dois gatilhos
 * analogicos, o giroscopio, o acelerometro, os dois dedos do touchpad e — o que
 * nao existia — um CARIMBO DE TEMPO de 64 bits.
 *
 * O carimbo de tempo encerra uma duvida aberta desde setembro: no Cronus, o script
 * supunha 10 ms por volta e nao tinha como conferir. Numa sessao a VM rodou a
 * 16,1 ms e todos os tempos sairam 1,63x mais longos, sem o script perceber. Aqui
 * o tempo decorrido e um dado que chega junto com a entrada.
 */
#pragma once
#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t  finger;
    uint8_t  reserved[3];
} ScePadTouch;

typedef struct {
    uint8_t     fingers;
    uint8_t     reserved0[3];
    uint32_t    reserved1;
    ScePadTouch touch[2];
} ScePadTouchData;

typedef struct {
    uint32_t buttons;                               /* um bit por botao */
    struct { uint8_t x, y; } leftStick;             /* 0..255, centro em 128 */
    struct { uint8_t x, y; } rightStick;
    struct { uint8_t l2, r2; } analogButtons;       /* 0..255 */
    uint16_t reserved0;
    struct { float x, y, z, w; } quat;              /* orientacao (giroscopio) */
    struct { float x, y, z; }    velocity;
    struct { float x, y, z; }    acceleration;
    ScePadTouchData touchData;
    uint8_t  connected;
    uint8_t  reserved1[3];
    uint64_t timestamp;                             /* microssegundos */
    uint8_t  extension[16];
    uint8_t  count;
    uint8_t  reserved2[15];
} ScePadData;

/* Os bits de `buttons`. Nomes do D-Sig, para a cabeca nao ter de traduzir duas
 * vezes: sao os mesmos destinos do app, na mesma ordem de leitura. */
#define DS_BTN_L1        0x00000400u
#define DS_BTN_L2        0x00000100u
#define DS_BTN_L3        0x00000002u
#define DS_BTN_R1        0x00000800u
#define DS_BTN_R2        0x00000200u
#define DS_BTN_R3        0x00000004u
#define DS_BTN_CROSS     0x00004000u
#define DS_BTN_CIRCLE    0x00002000u
#define DS_BTN_SQUARE    0x00008000u
#define DS_BTN_TRIANGLE  0x00001000u
#define DS_BTN_UP        0x00000010u
#define DS_BTN_DOWN      0x00000040u
#define DS_BTN_LEFT      0x00000080u
#define DS_BTN_RIGHT     0x00000020u
#define DS_BTN_OPTIONS   0x00000008u
#define DS_BTN_TOUCH     0x00100000u
