# SPDX-License-Identifier: GPL-3.0-or-later
#
# DOIS ALVOS, UM FONTE. O mesmo src/*.c compila para o PS5 e para a bancada do Linux.
# E o que permite testar sem console — a licao mais cara do projeto do Cronus, onde
# cada conferencia exigia o aparelho na mesa.
#
#   make bancada   -> compila e roda os testes aqui, sem PS5. Nao precisa de SDK.
#   make payload   -> gera o .elf para o console. Precisa do PS5_PAYLOAD_SDK.
#
# O alvo `bancada` e o padrao de proposito: o que se roda cem vezes por dia tem de ser
# o que custa menos para rodar.

BANCADA_CC     ?= gcc
BANCADA_CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 \
                  -Werror=implicit-function-declaration
FONTES         := src/passagem.c
TESTES         := ferramentas/testa_passagem.c
SAIDA          := build

.PHONY: all bancada payload limpa
all: bancada

bancada: $(SAIDA)/testa_passagem
	@$(SAIDA)/testa_passagem

$(SAIDA)/testa_passagem: $(TESTES) $(FONTES) src/dsig.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ $(TESTES) $(FONTES)

$(SAIDA):
	@mkdir -p $(SAIDA)

# --- o console ---------------------------------------------------------------
# As opcoes seguem as do PoorDS4, que e de onde veio o conhecimento da plataforma:
# sem protetor de pilha e sem tabelas de salto, porque o payload roda injetado e nao
# pode depender do ambiente de execucao normal.
payload:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK nao esta definido. Veja o README)
endif
	@echo "  (o alvo do console entra quando a ponte com a libScePad existir)"
	@echo "  hoje o projeto tem o roteador e a bancada; a ponte e o proximo passo"

limpa:
	rm -rf $(SAIDA)
