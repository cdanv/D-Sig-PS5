# Processo-alvo do teste de viabilidade (opcao 2). Requer PS5_PAYLOAD_SDK.
ifndef PS5_PAYLOAD_SDK
$(error PS5_PAYLOAD_SDK nao definido: aponte para a distribuicao binaria do SDK)
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF := viabilidade.elf
CFLAGS := -Wall -Werror -O2

all: $(ELF)
$(ELF): main.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f $(ELF)
