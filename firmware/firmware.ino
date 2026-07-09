// INCLUSÃO DE BIBLIOTECAS
#include <Wire.h>
#include <HX711.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>
#include <SPI.h>
#include <Pushbutton.h>
#include <BluetoothSerial.h>
#include <Preferences.h>

#include "Pressure.h"

// Definições de pinos e constantes
#define LED_PIN 4         // Pino do LED
#define CS_PIN 5          // Pino do cartão SD
#define BUZZER_PIN 32     // Pino do buzzer
#define BTN_PIN 33        // Pino do botão
#define CELULA_DT_PIN 26  // Pino de dados da célula de carga
#define CELULA_SCK_PIN 27 // Pino de clock da célula de carga
#define PRESSURE_PIN 35   // Pino do sensor de pressão
#define INTERVALO 100     // Precisão Leitura Dados milissegundos

// Variáveis globais
const float VinPressure = 5.0;    // Tensão que alimenta o sensor
const float VminPressure = 0.27;  // Tensão de saída em 0 MPa
const float VmaxPressure = 4.5;   // Tensão de saída em 10 MPa
const float maxPressure = 10.0;   // Pressão máxima do sensor em MPa
const float R1 = 2200.0;          // Resistor conectado entre o sensor e o pino do ESP32
const float R2 = 3300.0;          // Resistor conectado entre o pino do ESP32 e o GND
const int RESOLUCAO_ADC = 4095;   // ESP32 tem ADC de 12 bits (2^12 - 1)
const float TENSAO_MAX_ADC = 3.3; // Tensão de referência do ADC do ESP32
float maxValues[2] = {0.0, 0.0};  // Vetor leituras de pico (peso, pressão)
unsigned long previousMillis = 0; // Controle de tempo
bool selectLoop = false;          // Modo de operação
float loadFactor = 0.0;           // Valor encontrado na calibração
String filedir = "";              // Arquivo
String leitura = "";              // Leitura dos dados

enum StorageType { STORAGE_NONE, STORAGE_SD, STORAGE_LITTLEFS };
StorageType storageType = STORAGE_NONE;

// Estado do modo de configuração (calibração)
static bool configMode = false;

// Instanciação de objetos
Pushbutton button(BTN_PIN);
PressureSensor pressureSensor(PRESSURE_PIN, R1, R2, RESOLUCAO_ADC, TENSAO_MAX_ADC, VminPressure, VmaxPressure, maxPressure);

HX711 escala;
BluetoothSerial SerialBT;
Preferences preferences;

void setup()
{
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT");

  preferences.begin("app", false);
  loadFactor = preferences.getFloat("loadFactor", -284.6);

  // Imprime o fator carregado para diagnóstico
  Serial.println("=== BOOT ===");
  Serial.print("loadFactor carregado: ");
  Serial.println(loadFactor, 4);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);

  pressureSensor.begin();

  if (setupStorageAndFile() && setupHX711())
  {
    printToSerials("Sistema configurado. Transmitindo...");
    buzzSignal("Sucesso");
  }
  else
  {
    printToSerials("Falha critica. Reiniciando...");
    buzzSignal("Alerta");
    delay(3000);
    ESP.restart();
  }
}

void loop()
{
  if (!configMode)
  {
    staticTest();
  }

  if (button.getSingleDebouncedPress())
  {
    buzzSignal("Beep");
    printToSerials("Celula Zerada!");
    escala.tare();
  }

  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("TARE"))
    {
      buzzSignal("Beep");
      printToSerials("Celula Zerada!");
      escala.tare();
      return;
    }

    // Comando para imprimir o fator atual sem alterar nada
    if (command.equalsIgnoreCase("GET LOAD FACTOR"))
    {
      Serial.print("loadFactor atual: ");
      Serial.println(loadFactor, 4);
      return;
    }

    if (command.startsWith("INIT CONFIG"))
    {
      escala.tare();
      configMode = true;
      Serial.println("Aguardando fator de carga...");
      return;
    }

    if (configMode && command.startsWith("SET LOAD FACTOR"))
    {
      int lastSpaceIndex = command.lastIndexOf(' ');
      if (lastSpaceIndex != -1)
      {
        String factorStr = command.substring(lastSpaceIndex + 1);
        float factor = factorStr.toFloat();
        if (!isnan(factor) && factor != 0.0)
        {
          setLoadFactor(factor);
          configMode = false;
          Serial.println("Modo configuracao finalizado");
        }
        else
        {
          printToSerials("Valor invalido. Tente novamente.");
          buzzSignal("Alerta");
        }
      }
      return;
    }
  }

  // Stream de calibração: RAW contínuo enquanto aguarda o fator
  if (configMode)
  {
    if (escala.is_ready()) {
      Serial.println(escala.get_value(1));
    }
    delay(100);
  }
}

// Configuração do fator de carga
void setLoadFactor(float factor)
{
  loadFactor = factor;
  escala.set_scale(loadFactor);
  preferences.putFloat("loadFactor", loadFactor);
  printToSerials("Fator de carga atualizado: " + String(loadFactor, 4));
  buzzSignal("Sucesso");
}

// Sinalização com o buzzer
void buzzSignal(String signal)
{
  int frequency = 1000;
  if (signal == "Alerta")
  {
    for (int i = 0; i < 5; i++)
    {
      tone(BUZZER_PIN, frequency, 200);
      delay(200 + 150);
    }
  }
  else if (signal == "Sucesso")
  {
    for (int i = 0; i < 3; i++)
    {
      tone(BUZZER_PIN, frequency, 100);
      delay(100 + 100);
    }
  }
  else if (signal == "Ativado")
  {
    tone(BUZZER_PIN, frequency, 500);
  }
  else if (signal == "Beep")
  {
    tone(BUZZER_PIN, frequency, 50);
  }
  else
  {
    Serial.println("Sinal invalido!");
  }
}

String generateFileName()
{
  const char* prefix = (storageType == STORAGE_SD) ? "Dados" : "dados";

  for (int i = 1; i <= 999; i++)
  {
    char candidate[32];
    snprintf(candidate, sizeof(candidate), "/%s_%03d.txt", prefix, i);

    bool exists = false;
    if (storageType == STORAGE_SD) {
      exists = SD.exists(candidate);
    } else if (storageType == STORAGE_LITTLEFS) {
      exists = LittleFS.exists(candidate);
    }

    if (!exists)
      return String(candidate);
  }

  return "/overflow.txt";
}

// Configuração do Storage (SD com LittleFS fallback)
bool setupStorage()
{
  Serial.println("Inicializando SD...");
  if (SD.begin(CS_PIN) && SD.cardType() != CARD_NONE)
  {
    storageType = STORAGE_SD;
    Serial.println("SD iniciado!");
    Serial.printf("Tipo: %s\n",
      SD.cardType() == CARD_MMC ? "MMC" :
      SD.cardType() == CARD_SD  ? "SDSC" :
      SD.cardType() == CARD_SDHC ? "SDHC" : "UNKNOWN");
    return true;
  }

  Serial.println("SD falhou. Tentando LittleFS...");
  yield();

  if (LittleFS.begin(true))
  {
    storageType = STORAGE_LITTLEFS;
    Serial.println("LittleFS iniciado!");
    Serial.printf("Total: %u bytes, Usado: %u bytes\n",
      (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes());
    return true;
  }

  Serial.println("ERRO: Nenhum storage disponivel!");
  yield();
  return false;
}

// Configuração do Storage e Arquivo
bool setupStorageAndFile()
{
  if (!setupStorage())
  {
    return false;
  }

  filedir = generateFileName();
  Serial.print("Arquivo: ");
  Serial.println(filedir);

  if (!writeFile(filedir, "Tempo,Empuxo,Pressao\n"))
  {
    printToSerials("Falha ao criar arquivo");
    return false;
  }

  printToSerials("Arquivo criado: " + filedir);
  return true;
}

// Configuração da célula de carga HX711
bool setupHX711()
{
  Serial.println("Iniciando HX711...");
  escala.begin(CELULA_DT_PIN, CELULA_SCK_PIN);

  // Aguarda HX711 ficar pronto com timeout de 5 segundos
  unsigned long timeout = millis();
  while (!escala.is_ready()) {
    if (millis() - timeout > 5000) {
      Serial.println("ERRO: HX711 nao respondeu (timeout 5s). Verifique DT=26, SCK=27 e alimentacao.");
      return false;
    }
    delay(10);
  }

  Serial.println("HX711 detectado OK.");
  escala.set_scale(loadFactor);
  Serial.print("set_scale com fator: ");
  Serial.println(loadFactor, 4);

  // Warmup: descarta as primeiras leituras para o HX711 estabilizar
  Serial.println("Aguardando estabilizacao do HX711...");
  for (int i = 0; i < 10; i++) {
    escala.get_value(1);
    delay(100);
  }

  escala.tare(10); // média de 10 amostras para a tara ser mais precisa
  Serial.println("tare OK");

  printToSerials("HX711 conectado");
  return true;
}

// Função para registrar e imprimir os dados do momento
// Formato: Tempo (ms), Empuxo (Kg), Pressão (MPa)
void logData(unsigned long ms)
{
  // Verifica se o HX711 está pronto antes de ler
  if (!escala.is_ready()) {
    Serial.println("AVISO: HX711 nao pronto, pulando leitura.");
    return;
  }

  float peso = abs(escala.get_units());
  float pressao = pressureSensor.readMPa();

  if (peso > maxValues[0]) maxValues[0] = peso;
  if (pressao > maxValues[1]) maxValues[1] = pressao;

  leitura = String(ms) + "," + String(peso, 6) + "," + String(pressao);

  printToSerials(leitura);
  appendFile(filedir, leitura);
}

// Escrita de dados em um arquivo
bool writeFile(const String &path, const String &message)
{
  File file;

  if (storageType == STORAGE_SD) {
    file = SD.open(path, FILE_WRITE);
  } else if (storageType == STORAGE_LITTLEFS) {
    file = LittleFS.open(path, FILE_WRITE);
  } else {
    return false;
  }

  if (!file)
  {
    printToSerials("Falha ao abrir o arquivo");
    return false;
  }
  if (file.print(message))
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    printToSerials("Falha ao escrever no arquivo");
    digitalWrite(LED_PIN, LOW);
    file.close();
    return false;
  }
  file.close();
  return true;
}

// Anexação de dados em um arquivo
void appendFile(const String &path, const String &message)
{
  File file;

  if (storageType == STORAGE_SD) {
    file = SD.open(path, FILE_APPEND);
  } else if (storageType == STORAGE_LITTLEFS) {
    file = LittleFS.open(path, FILE_APPEND);
  } else {
    return;
  }

  if (!file)
  {
    printToSerials("Falha ao abrir o arquivo");
    return;
  }
  if (file.print(message + "\n"))
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    printToSerials("Falha ao escrever no arquivo");
    digitalWrite(LED_PIN, LOW);
  }
  file.close();
}

// Criação de diretório
void createDir(fs::FS &fs, const String &path)
{
  if (fs.mkdir(path))
  {
    printToSerials("Dir created");
  }
  else
  {
    printToSerials("mkdir failed");
  }
}

// Função para imprimir na Serial e SerialBT
void printToSerials(const String &message)
{
  Serial.println(message);
  SerialBT.println(message);
}

// Função de teste estático
void staticTest()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > INTERVALO)
  {
    logData(currentMillis);
    previousMillis = currentMillis;
  }
}
