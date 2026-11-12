/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * O BUFFER DUPLO COM VERSAO: como o perfil ativo chega ao gancho.
 * Ver docs/ARQUITETURA.md, secao 2.
 *
 * O escritor (o daemon) grava o perfil novo no buffer INATIVO e so depois publica a
 * versao. O leitor (o gancho, a cada quadro) copia o buffer ativo e confere a versao
 * antes e depois da copia. Se mudou durante a copia, a copia e descartada.
 *
 * O conteudo aqui e um payload generico de tamanho fixo. O formato binario do perfil
 * (MODs, tempos, slots) entra depois, por cima deste mecanismo.
 */
#pragma once
#include <stdint.h>

#define PERFIL_CAMPOS 32

typedef struct {
    uint32_t versao;                  /* numero da publicacao que gerou este conteudo */
    uint32_t campos[PERFIL_CAMPOS];
    uint32_t soma;                    /* soma de campos[], para detectar copia pela metade */
} PerfilDados;

/* Escritor: uma unica thread. Devolve 0. */
int perfil_publicar(const PerfilDados *novo);

/* Leitor: uma tentativa, sem laco. Copia o perfil ativo em *saida.
 * Devolve 0 se a copia e consistente, -1 se foi descartada (houve publicacao durante
 * a copia). Em -1, *saida nao deve ser usada. */
int perfil_ler(PerfilDados *saida);

/* Versao da ultima publicacao concluida (0 se nunca houve). */
uint32_t perfil_versao_publicada(void);
