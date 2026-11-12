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

clean:
	rm -f $(ELF)
