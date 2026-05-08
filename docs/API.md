# 📡 API e Protocolos - Sistema de Teste Estático

## 📋 Visão Geral

Este documento descreve os protocolos de comunicação e as interfaces de controle do sistema de teste estático.

## 🔌 Protocolos Suportados

| Protocolo  | Tipo     | Velocidade  | Alcance | Caso de Uso                    |
| ---------- | -------- | ----------- | ------- | ------------------------------ |
| Serial USB | Fio      | 115200 baud | Local   | Configuração, calibração, debug |
| Bluetooth  | Wireless | 115200 baud | ~10 m   | Telemetria/monitoramento       |

## 💻 Comunicação Serial/Bluetooth

### Configuração padrão

```
BAUD_RATE = 115200
DATA_BITS = 8
PARITY = NONE
STOP_BITS = 1
FLOW_CONTROL = NONE
```

### Estrutura de comandos

```
COMANDO [PARAMETRO]
```

## 🎮 Comandos Disponíveis

### 1. Modo de configuração (calibração)

```
COMANDO: INIT CONFIG
RESPOSTA: Aguardando fator de carga...
USO: entra no modo de calibração da célula de carga

COMANDO: SET LOAD FACTOR <valor>
EXEMPLO: SET LOAD FACTOR 277306.0
RESPOSTA: Fator de carga atualizado
USO: salva o fator de calibração na memória persistente
```

### 2. Controle da célula de carga

```
COMANDO: TARE
RESPOSTA: Célula Zerada!
USO: zera a leitura atual da célula de carga
```

## 🕹️ Controles Físicos

| Entrada física | Pino ESP32 | Função |
| -------------- | ---------- | ------ |
| Botão 1 | `GPIO32` | TARE da célula de carga |
| Botão 2 | `GPIO33` | Iniciar novo arquivo de log no SD |
| LED | `GPIO4` | Indicador de funcionamento (aceso ao gravar no SD) |

## 🎯 Exemplo de Interação

### Calibração da célula de carga

```
# Terminal -> ESP32
INIT CONFIG

# ESP32 -> Terminal
Aguardando fator de carga...
# (ESP32 envia leituras RAW continuamente)
152345
152348
152350

# Terminal -> ESP32
SET LOAD FACTOR 277306.0

# ESP32 -> Terminal
Fator de carga atualizado: 277306.00
Modo configuração finalizado
```
