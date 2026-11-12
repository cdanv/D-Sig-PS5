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
# C11 por causa de stdatomic.h, usado pelo buffer duplo do perfil (src/perfil.c).
BANCADA_CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 \
                  -Werror=implicit-function-declaration
BANCADA_LIBS   := -pthread
FONTES         := src/passagem.c
SAIDA          := build

.PHONY: all bancada payload limpa
all: bancada

# Cada bancada e um binario proprio. Uma reprovacao para o make na hora.
bancada: $(SAIDA)/testa_passagem $(SAIDA)/testa_mod $(SAIDA)/testa_tempo $(SAIDA)/testa_perfil $(SAIDA)/testa_alfa $(SAIDA)/testa_delta $(SAIDA)/testa_modelo $(SAIDA)/testa_rede $(SAIDA)/testa_dominio $(SAIDA)/testa_acumula
	@$(SAIDA)/testa_passagem
	@$(SAIDA)/testa_mod
	@$(SAIDA)/testa_tempo
	@$(SAIDA)/testa_perfil
	@$(SAIDA)/testa_alfa
	@$(SAIDA)/testa_delta
	@$(SAIDA)/testa_modelo
	@$(SAIDA)/testa_rede
	@$(SAIDA)/testa_dominio
	@$(SAIDA)/testa_acumula

$(SAIDA)/testa_passagem: ferramentas/testa_passagem.c $(FONTES) src/dsig.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_passagem.c $(FONTES)

$(SAIDA)/testa_mod: ferramentas/testa_mod.c $(FONTES) src/dsig.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_mod.c $(FONTES)

$(SAIDA)/testa_tempo: ferramentas/testa_tempo.c src/tempo.c src/tempo.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_tempo.c src/tempo.c

$(SAIDA)/testa_perfil: ferramentas/testa_perfil.c src/perfil.c src/perfil.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_perfil.c src/perfil.c $(BANCADA_LIBS)

$(SAIDA)/testa_alfa: ferramentas/testa_alfa.c src/alfa.c src/alfa.h src/tempo.c src/tempo.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_alfa.c src/alfa.c src/tempo.c

$(SAIDA)/testa_delta: ferramentas/testa_delta.c src/delta.c src/delta.h src/tempo.c src/tempo.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_delta.c src/delta.c src/tempo.c

$(SAIDA)/testa_modelo: ferramentas/testa_modelo.c src/modelo.c src/modelo.h src/alfa.c src/alfa.h src/delta.c src/delta.h src/tempo.c src/tempo.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_modelo.c src/modelo.c src/alfa.c src/delta.c src/tempo.c

$(SAIDA)/testa_rede: ferramentas/testa_rede.c src/rede.c src/rede.h src/dominio.c src/dominio.h src/modelo.c src/modelo.h src/alfa.c src/alfa.h src/delta.c src/delta.h src/tempo.c src/tempo.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_rede.c src/rede.c src/dominio.c src/modelo.c src/alfa.c src/delta.c src/tempo.c

$(SAIDA)/testa_dominio: ferramentas/testa_dominio.c src/rede.c src/rede.h src/dominio.c src/dominio.h src/modelo.c src/modelo.h src/alfa.c src/alfa.h src/delta.c src/delta.h src/tempo.c src/tempo.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_dominio.c src/rede.c src/dominio.c src/modelo.c src/alfa.c src/delta.c src/tempo.c

$(SAIDA)/testa_acumula: ferramentas/testa_acumula.c src/rede.c src/rede.h src/dominio.c src/dominio.h src/modelo.c src/modelo.h src/alfa.c src/alfa.h src/delta.c src/delta.h src/tempo.c src/tempo.h src/pad.h | $(SAIDA)
	$(BANCADA_CC) $(BANCADA_CFLAGS) -o $@ ferramentas/testa_acumula.c src/rede.c src/dominio.c src/modelo.c src/alfa.c src/delta.c src/tempo.c

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
