# Roteiro da medição (PS5 FAT 11.20)

O payload grava 90 segundos de leitura do controle, a cada 10 ms, em `/data/dsig/medicao.csv`.
Siga a sequência abaixo, com o controle na mão, sem pressa. Cada faixa de tempo é de 5 segundos.
O tempo está no CSV (coluna `tempo_s`), então não precisa de nenhum sinal na tela.

| tempo (s) | o que fazer | o que a medição confirma |
|---|---|---|
| 0 a 5 | deixar o controle parado, sem tocar nos analógicos | valor de repouso (centro dos analógicos, gatilhos em zero) |
| 5 a 10 | empurrar o analógico **esquerdo para a frente** | sinal de LY para frente (convenção vertical) |
| 10 a 15 | empurrar o analógico **esquerdo para trás** | sinal de LY para trás |
| 15 a 20 | empurrar o analógico **esquerdo para a direita** | sinal de LX para a direita |
| 20 a 25 | empurrar o analógico **esquerdo para a esquerda** | sinal de LX para a esquerda |
| 25 a 30 | empurrar o analógico **direito para a frente** | sinal de RY para frente |
| 30 a 35 | empurrar o analógico **direito para trás** | sinal de RY para trás |
| 35 a 40 | empurrar o analógico **direito para a direita** | sinal de RX |
| 40 a 42 | apertar o botão **PS** uma vez | bit de PS em `buttons` (ou o sistema consome o botão) |
| 43 a 45 | apertar o **SHARE** uma vez | bit de SHARE em `buttons` (ou o sistema consome o botão) |
| 45 a 50 | apertar o **L2** até a metade, e soltar | escala de L2 (0 a 255) |
| 50 a 55 | apertar o **R2** até o fundo, e soltar | escala de R2 (0 a 255) |
| 55 a 60 | deslizar **um dedo** no touchpad, de um canto ao outro | faixa de `touch1_x` e `touch1_y` |
| 60 a 65 | inclinar o controle **para a frente** (45 graus) | sinal de acelerômetro e giroscópio |
| 65 a 70 | inclinar o controle **para trás** | idem, no sentido oposto |
| 70 a 75 | inclinar o controle **para a esquerda** | idem |
| 75 a 80 | inclinar o controle **para a direita** | idem |
| 80 a 90 | deixar o controle parado de novo | deriva e zona morta dos sensores |

## Como rodar

1. O elfldr do seu PS5 precisa estar escutando na porta **9021** (ver a análise do Payload Manager).
2. Do PC, com o IP do console (Configurações › Rede):

```bash
export PS5_PAYLOAD_SDK=/caminho/para/ps5-payload-sdk
make -C payload/medicao
make -C payload/medicao test PS5_HOST=<IP_DO_PS5> PS5_PORT=9021
```

3. Depois da execução, o arquivo está em `/data/dsig/medicao.csv` no console. Para trazê-lo ao PC,
   é preciso um acesso a arquivos do console (FTP ou outro método que você usa). Me diga qual é, e eu
   descrevo o passo exato.

## Registro

Na primeira linha do CSV ficam os códigos de retorno de `scePadInit` e `scePadOpen`. Se qualquer um
for negativo, anote e me envie: a assinatura declarada no payload pode estar errada, e isso é o que
a primeira execução precisa revelar. Se `scePadReadState` retornar erro, o CSV registra o código em
linhas de comentário, com o instante.
