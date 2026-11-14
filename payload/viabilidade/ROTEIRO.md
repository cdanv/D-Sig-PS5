# Roteiro: teste de viabilidade da opção 2 (depuração do processo)

Objetivo: responder três perguntas no PS5 FAT 11.20, com um processo de teste (não o jogo):

1. Um processo externo consegue anexar, ler, escrever e soltar outro processo nosso?
2. Quanto tempo o processo-alvo fica pausado?
3. O processo-alvo volta ao normal depois de solto, e a escrita aparece nele?

A ferramenta que anexa e escreve **não está neste repositório**: é de responsabilidade do autor (ver
ARQUITETURA.md, seção 42). Este roteiro descreve o que medir e o que conta como aprovado.

## Preparação

1. No notebook, compile o alvo:
   ```bash
   cd ~/d-sig-ps5/payload/viabilidade && export PS5_PAYLOAD_SDK=$HOME/sdk/ps5-payload-sdk && make
   ```
2. Copie `viabilidade.elf` para `pldmgr/` no HD (`usb0`), pelo Prospero Manager.
3. Deixe só o HD conectado (sem pendrive), para o CSV ir para `usb0/dsig/`.
4. Confirme que o Payload Manager e o jailbreak estão rodando.

## Execução

1. No Payload Manager, execute `viabilidade.elf`. O processo roda 60 segundos e grava
   `usb0/dsig/viabilidade.csv`, uma linha a cada 100 ms.
2. Durante a execução, rode a ferramenta do autor contra o PID escrito na primeira linha do CSV:
   anexar, ler o marcador, escrever um valor de teste (por exemplo `0xAAAABBBBCCCCDDDD`), ler de volta,
   soltar. Anote o horário de cada passo.
3. Espere o processo terminar. Baixe o CSV pelo Prospero.

## O que medir

- **Pausa:** a maior diferença entre dois `tempo_s` consecutivos no CSV. Um intervalo normal é 0,100 s.
  Anote a diferença máxima e o momento em que aconteceu, e compare com os horários anotados no passo 2.
- **Escrita aparece:** depois da escrita, as linhas seguintes devem mostrar o valor de teste no campo
  `marcador`, não o valor inicial.
- **Processo volta ao normal:** o contador continua subindo, e a linha final diz `fim normal`.
- **Quantas vezes:** repita o anexo e o desanexo pelo menos três vezes na mesma execução, e anote o resultado
  de cada uma.

## Critério de aprovação

Aprovado se, nas três repetições:
- a escrita aparece no CSV na linha seguinte ao passo de escrita (dentro de 200 ms);
- a maior pausa fica abaixo de 1 segundo;
- o processo termina com `fim normal`, sem reiniciar nem travar.

## Reprovado

- Qualquer repetição em que o processo trave ou o contador pare de subir.
- Escrita que não aparece, ou que aparece com outro valor.
- Pausa acima de 1 segundo.

Se reprovar: **não** avance para o gancho do jogo. Registre o resultado na seção 42 do ARQUITETURA.md e
decida com o autor se a opção 1 volta à mesa (com exceção da regra 4, aprovada por escrito).

## Recuperação

Se o console travar, reinicie. O jailbreak não é persistente, então o estado volta ao normal, e nada
fica gravado no sistema. Anote o que estava sendo feito no momento do travamento.

## Registro

Me mande o CSV e as anotações dos horários. Eu registro o resultado na seção 42 do ARQUITETURA.md, com as
medições, e a decisão seguinte fica com você.
