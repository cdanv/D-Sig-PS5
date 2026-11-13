# Build do payload de medicao. Precisa de PS5_PAYLOAD_SDK apontando para a distribuicao binaria.
ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK nao esta definido)
endif

ELF := medicao.elf
CFLAGS := -Wall -Werror -O2 -lScePad

all: $(ELF)

$(ELF): main.c ../../src/pad.h
	$(CC) $(CFLAGS) -o $@ main.c

PS5_HOST ?= ps5
PS5_PORT ?= 9021

test: $(ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

clean:
	rm -f $(ELF)
