# Plano: próximo passo do D-Sig-PS5

Este documento lista o que falta para o teste de viabilidade da opção 2 (ARQUITETURA.md, seção 42),
separando o que o projeto faz dentro do repositório e o que depende de um programador.

## Recuperação, registrada aqui

Se o console travar durante um teste, a recuperação é a reinstalação limpa do sistema na mesma versão
(11.20 no FAT). Quem executar o teste deve ter o procedimento de reinstalação e o arquivo de firmware
prontos antes de começar.

## Parte 1: o que o projeto faz (sem programação)

| tarefa | critério de conclusão |
|---|---|
| Processo-alvo do teste de viabilidade | `payload/viabilidade/main.c` compila com `-Wall -Werror` (feito) |
| Roteiro do teste no console | `payload/viabilidade/ROTEIRO.md` com passos e critério de aprovação (feito) |
| Desenho do instalador, sem código | documento com fluxo, validações e restauração, referenciando a seção 2 do CLAUDE.md |
| Lista dos valores originais que o instalador deve guardar | tabela das cinco funções de leitura e o que cada uma guarda |
| Teste de restauração em documento | casos: valor atual é o do gancho; valor atual é outro; processo sai no meio |
| Revisão do código recebido | cada linha de código de terceiros revisada antes de rodar no console |

## Parte 2: o que depende de um programador

| tarefa | o que a pessoa escreve | como verificar |
|---|---|---|
| Função de escrita de fora | a única peça que escreve na memória de outro processo | roda no processo-alvo do teste de viabilidade, no console, e passa no critério do ROTEIRO.md |
| Acompanhamento da pausa | medir o tempo em que o processo-alvo fica parado | o CSV mostra a maior pausa entre duas linhas |

## Parte 3: depois da viabilidade

Se o teste passar: o instalador completo, com validação de ABI, restauração condicional e a lista de
slots, revisado antes de rodar com um jogo. Se reprovar: voltar à decisão da opção 1, com a exceção da
regra 4 por escrito.

## Texto para quem for ajudar

> Estou desenvolvendo um roteador de sinais de controle para acessibilidade no PS5 com jailbreak
> (licença GPL-3.0-or-later, código em https://github.com/cdanv/D-Sig-PS5). O projeto precisa de uma
> função pequena que escreve na memória de outro processo no console, para um teste de viabilidade no
> PS5 FAT firmware 11.20. Já tenho o processo-alvo, o roteiro de teste e a lógica do instalador. Falta a
> função de escrita. Você teria tempo para revisar ou escrever essa parte? Posso explicar o contexto
> completo e as regras do projeto.
