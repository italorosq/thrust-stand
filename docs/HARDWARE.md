# 🔌 Hardware - Sistema de Teste Estático

## 📋 Visão Geral do Sistema

Sistema embarcado baseado em ESP32 para aquisição de empuxo e pressão em testes estáticos de motores de foguetes, com gravação local de dados no cartão SD e telemetria via Serial/Bluetooth.

Controles físicos do esquemático atual:

- `GPIO33`: botão para TARE (zerar a célula de carga)
- `GPIO25`: LED de indicador de funcionamento (aceso durante gravação no SD)
- `GPIO32`: buzzer de sinalização

## 🧩 Lista de Componentes (BOM)

| Componente                | Quantidade | Especificações                                |
| ------------------------- | ---------- | --------------------------------------------- |
| ESP32 DevKit V1           | 1          | 30 pinos (DOIT), WiFi/BT — a pinagem abaixo é a de 30 |
| Célula de carga           | 1          | SZ51X-500kg, 2 mV/V (ponte de Wheatstone)     |
| Módulo ADS1256 (placa azul) | 1        | ADC 24 bits, 8 canais, SPI, referência 2,5 V  |
| Sensor de pressão         | 1          | 0-10 MPa, saída analógica 0.5-4.5V            |
| Módulo MAX31855K          | 1          | Leitor de termopar tipo K, SPI, 3,3 V         |
| Termopar tipo K           | 1          | Até 1350 °C (MAX31855K)                        |
| Módulo microSD            | 1          | Interface SPI                                 |
| LED                       | 1          | 5mm, vermelho                                 |
| Botão tátil (TARE)        | 1          | 6x6mm                                         |
| Buzzer                    | 1          | 5V, passivo (o firmware usa `tone()`)          |
| Resistores do divisor     | 2          | 2.2kΩ e 3.3kΩ                                 |
| Resistores de pull-up     | 3          | 10kΩ (CS do ADS, MAX e SD)                    |
| Resistor do LED           | 1          | 220Ω (valor típico)                           |
| Fonte de alimentação      | 1          | 5V 2A (mínimo recomendado)                    |

## 🔄 Mapeamento de Conexões

| ESP32 | Pino | Coluna | Função no sistema |
| ----- | :--: | :----: | ----------------- |
| `3V3`    | 1 | esq. | 3,3 V para microSD e MAX31855 |
| `GND`    | 2 / 29 | esq./dir. | Terra comum |
| `GPIO4`  | 5 | esq. | ADS1256 `DRDY` |
| `GPIO13` | 28 | dir. | ADS1256 `CS` |
| `GPIO14` | 26 | dir. | MAX31855 `CS` |
| `GPIO16` | 6 | esq. | ADS1256 `DIN` (MOSI) |
| `GPIO17` | 7 | esq. | ADS1256 `SCLK` + MAX31855 `SCK` |
| `GPIO18` | 9 | esq. | ADS1256 `DOUT` (MISO) + MAX31855 `SO` |
| `GPIO19` | 10 | esq. | microSD `CS` |
| `GPIO21` | 11 | esq. | microSD `MOSI` |
| `GPIO22` | 14 | esq. | microSD `SCK` |
| `GPIO23` | 15 | esq. | microSD `MISO` |
| `GPIO25` | 23 | dir. | LED de gravação |
| `GPIO32` | 21 | dir. | Buzzer |
| `GPIO33` | 22 | dir. | Botão TARE |
| `VIN`    | 30 | dir. | Alimentação 5V dos módulos |

Numeração física do DevKit de 30 pinos, com o USB para baixo: 1–15 na coluna
esquerda (`3V3` no 1) e 16–30 na direita (`VIN` no 30).

A divisão é por coluna de propósito: todo o barramento SPI sai pela coluna
esquerda e o painel (botão, LED, buzzer) pela direita. Assim nenhuma trilha
precisa atravessar por baixo do módulo, e a placa fecha em uma face de cobre
(ou em duas, com a face superior como plano de GND).

Pinos livres:

- `GPIO2`, `GPIO5`, `GPIO12` e `GPIO15`: strapping — não usar.
- `GPIO34`–`GPIO39`: somente entrada, reservados.
- `GPIO1` e `GPIO3`: UART0 (Serial), reservados para os programas de calibração.

Observações de ligação:

- Botão: um terminal no `GPIO33` e o outro no `GND`. O firmware usa
  `INPUT_PULLUP`; um 10 kΩ externo + 100 nF é recomendado se o fio até o
  painel for longo.
- LED: ligado ao `GPIO25` com resistor em série.
- Buzzer: ligado ao `GPIO32`. Se o buzzer puxar mais de 20 mA, use um
  transistor (2N7000 ou BC337) com resistor de 1 kΩ na base/gate e diodo de
  roda-livre; o `tone()` do firmware continua igual.
- Os três `CS` (`GPIO13`, `GPIO14` e `GPIO19`) levam pull-up de 10 kΩ para
  3,3 V junto dos conectores. Esses pinos não têm pull-up interno no reset,
  então o pull-up mantém os módulos desselecionados no boot e com o cabo
  desconectado.
- Sensor de pressão: saída analógica passa por divisor resistivo antes de
  chegar à entrada `AIN2` do ADS1256.

Ligações no módulo ADS1256:

| Módulo ADS1256 | Ligação |
| -------------- | ------- |
| `AIN0` / `AIN1` | Sinal da célula de carga (`S+` / `S-`) |
| `AIN2` | Sensor de pressão (ponto do divisor resistivo) |
| `AINCOM` | `GND` |
| `PDWN` | `3,3V` (fixo; o módulo azul não tem pino `RESET`, o firmware reinicia o chip por comando) |
| `5V` / `GND` | Alimentação do módulo (`VIN` / `GND` do ESP32) |

- Excitação da célula: `E+` no `5V` e `E-` no `GND`, com o mesmo GND do ESP32 e do módulo. Um `E-` solto já causou leitura oscilando.
- Antes de ligar, confira no módulo se os sinais SPI (`SCLK`, `DIN`, `DOUT`, `CS`, `DRDY`) são de 3,3 V.
- O ADS1256 usa um barramento SPI próprio (HSPI), separado do microSD (VSPI).

Ligações no módulo MAX31855:

| Módulo MAX31855 | Ligação |
| --------------- | ------- |
| `VCC` / `GND` | `3,3V` / `GND` do ESP32 |
| `SCK` | `GPIO17` (mesmo fio do `SCLK` do ADS1256) |
| `SO` | `GPIO18` (mesmo fio do `DOUT` do ADS1256) |
| `CS` | `GPIO14` |
| `T+` / `T-` | Termopar tipo K (respeite a polaridade: no fio tipo K, o `T-` costuma ser o vermelho) |

- Os dois chips dividem o barramento porque cada um só fala quando o seu `CS` está em nível baixo.
- O termopar é opcional: sem ele, o firmware avisa no boot e grava a temperatura como `nan`.
- O MAX31855 já faz a compensação de junta fria; mantenha o módulo longe de fontes de calor para essa compensação ficar correta.

## 🛠️ Procedimento de Montagem

1. Preparação dos componentes

- Verifique todos os itens da BOM
- Teste ESP32 e cartão SD separadamente antes da montagem final

2. Sequência recomendada

- Conecte alimentação (`VIN` e `GND`)
- Conecte o módulo ADS1256 (SPI) e a célula de carga
- Conecte módulo microSD (SPI)
- Conecte sensor de pressão com divisor resistivo
- Conecte o módulo MAX31855 e o termopar
- Conecte LED de status
- Conecte o botão (`GPIO33`)

3. Divisor resistivo para o sensor de pressão

```
Vsensor (0.5-4.5V) --- R1 (2.2kΩ) --- AIN2 (ADS1256)
                         |
                        R2 (3.3kΩ)
                         |
                        GND
```

Cálculo do divisor:

```
Vout = Vsensor * (R2 / (R1 + R2))

# Exemplo
Vout_max = 4.5V * (3300 / (2200 + 3300)) = 2.7V
```

## 📐 Esquemático

O esquemático atual está no projeto do KiCad: [`teste_estatico.kicad_sch`](../hardware/teste_estatico/kicad/pcb/teste_estatico.kicad_sch). Para visualizar, abra no KiCad (versão 10 ou superior).

O esquemático antigo, feito no Fritzing (com o HX711 nos pinos 26/27), foi movido para [`Legacy/`](../Legacy/).

## ⚠️ Considerações de Projeto

- [x] Divisor resistivo no sensor de pressão
- [x] Terra comum entre todos os módulos
- [x] Botão dedicado para TARE
- [x] LED dedicado para indicação de gravação no SD
- [x] Buzzer para sinalização
- [ ] Isolamento contra vibrações
- [ ] Proteção contra poeira e umidade

## 🧪 Testes de Hardware

### Testes pré-montagem

- Continuidade dos cabos
- Tensão de alimentação estável
- Comunicação SPI com microSD
- Leitura de sensores individuais

### Testes pós-montagem

- Botão `GPIO33` realiza TARE
- LED acende durante gravação no SD
- Leituras de empuxo e pressão estáveis
