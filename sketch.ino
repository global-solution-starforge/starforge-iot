// ═══════════════════════════════════════════════════════════════
// StarForge — Painel CubeSat
// ESP32 — MQTT bidirecional + Múltiplas Missões + Ranking
// ═══════════════════════════════════════════════════════════════

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ── Credenciais Wi-Fi ──────────────────────────────────────────
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ── MQTT ───────────────────────────────────────────────────────
const char* MQTT_BROKER    = "broker.emqx.io";
const int   MQTT_PORT      = 1883;
const char* MQTT_CLIENT_ID = "starforge-esp32-001";

// ── Tópicos publicados ─────────────────────────────────────────
const char* TOPIC_TELEMETRIA = "starforge/mission/telemetria";
const char* TOPIC_PROGRESS   = "starforge/mission/progress";
const char* TOPIC_STATUS     = "starforge/mission/status";
const char* TOPIC_RANKING    = "starforge/mission/ranking";
const char* TOPIC_UNLOCKED   = "starforge/mission/unlocked";
const char* TOPIC_LIST       = "starforge/mission/list";

// ── Tópicos assinados ──────────────────────────────────────────
const char* TOPIC_CMD_CONTRIBUTE = "starforge/cmd/contribute";
const char* TOPIC_CMD_RESET      = "starforge/cmd/reset";
const char* TOPIC_CMD_SELECT     = "starforge/cmd/select";

// ── Pinos ──────────────────────────────────────────────────────
const int PINO_POT    = 34;
const int PINO_BOTAO  = 14;
const int PINO_LED_R  = 25;
const int PINO_LED_Y  = 26;
const int PINO_LED_G  = 27;
const int PINO_BUZZER = 32;

// ── Display OLED ───────────────────────────────────────────────
#define OLED_LARGURA  128
#define OLED_ALTURA    64
#define OLED_RESET     -1
#define OLED_ENDERECO 0x3C
Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, OLED_RESET);

// ── Servidor HTTP ──────────────────────────────────────────────
WebServer server(80);

// ── MQTT client ────────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ── Estados da missão ──────────────────────────────────────────
enum EstadoMissao { AGUARDANDO, FINANCIANDO, DESBLOQUEADA };

// ── Estrutura de missão ────────────────────────────────────────
struct Missao {
  const char* nome;
  const char* descricao;
  const char* prazo;
  int         progresso;
  bool        desbloqueada;
  EstadoMissao estado;
};

// ── 3 Missões ──────────────────────────────────────────────────
const int TOTAL_MISSOES = 3;
Missao missoes[TOTAL_MISSOES] = {
  { "CubeSat BR-01", "Monitoramento climatico",   "2025-06-30", 0, false, AGUARDANDO },
  { "CubeSat BR-02", "Imageamento orbital",       "2025-09-30", 0, false, AGUARDANDO },
  { "CubeSat BR-03", "Comunicacao de emergencia", "2025-12-31", 0, false, AGUARDANDO }
};

int missaoAtual = 0;

// ── Ranking ────────────────────────────────────────────────────
const int MAX_RANKING = 5;
struct Contribuidor { char nome[32]; int valor; };
Contribuidor ranking[MAX_RANKING];
int totalContribuicoes = 0;

// ── Variáveis globais ──────────────────────────────────────────
float tempSatelite = 22.0;

// ── Debounce ───────────────────────────────────────────────────
bool          ultimoEstadoBotao = HIGH;
bool          estadoBotao       = HIGH;
unsigned long ultimoDebounce    = 0;
const unsigned long DEBOUNCE_MS = 50;

// ── Timers ─────────────────────────────────────────────────────
unsigned long ultimaTemp  = 0;
unsigned long ultimoOLED  = 0;
unsigned long ultimoBlink = 0;
bool          estadoBlink = false;

// ── Flag de comando recebido via MQTT ──────────────────────────
bool comandoPendente     = false;
char comandoTipo[64]     = "";
char comandoPayload[256] = "";

// ── Protótipos ─────────────────────────────────────────────────
void publicarTudo();
void publicarStatusMQTT();
void publicarProgressoMQTT();
void publicarTelemetriaMQTT();
void publicarRankingMQTT();
void publicarMissoesMQTT();
void publicarDesbloqueadaMQTT();
void beepConfirmacao();
void melodiaCelebracao();
void avancarMissao();
void conectarMQTT();

// ──────────────────────────────────────────────────────────────
// Utilitários
// ──────────────────────────────────────────────────────────────
String nomeEstado() {
  switch (missoes[missaoAtual].estado) {
    case AGUARDANDO:   return "aguardando";
    case FINANCIANDO:  return "financiando";
    case DESBLOQUEADA: return "em orbita";
    default:           return "desconhecido";
  }
}

void resetarRanking() {
  for (int i = 0; i < MAX_RANKING; i++) {
    ranking[i].nome[0] = '\0';
    ranking[i].valor   = 0;
  }
  totalContribuicoes = 0;
}

void adicionarRanking(const char* nome, int valor) {
  for (int i = 0; i < MAX_RANKING; i++) {
    if (strlen(ranking[i].nome) > 0 && strcmp(ranking[i].nome, nome) == 0) {
      ranking[i].valor += valor;
      for (int j = i; j > 0 && ranking[j].valor > ranking[j-1].valor; j--) {
        Contribuidor tmp = ranking[j];
        ranking[j]       = ranking[j-1];
        ranking[j-1]     = tmp;
      }
      totalContribuicoes++;
      return;
    }
  }
  int posicao = -1;
  for (int i = 0; i < MAX_RANKING; i++) {
    if (strlen(ranking[i].nome) == 0 || valor > ranking[i].valor) {
      posicao = i;
      break;
    }
  }
  if (posicao == -1) { totalContribuicoes++; return; }
  for (int i = MAX_RANKING - 1; i > posicao; i--) ranking[i] = ranking[i-1];
  strncpy(ranking[posicao].nome, nome, 31);
  ranking[posicao].nome[31] = '\0';
  ranking[posicao].valor    = valor;
  totalContribuicoes++;
}

// ──────────────────────────────────────────────────────────────
// Hardware
// ──────────────────────────────────────────────────────────────
void beepConfirmacao() {
  tone(PINO_BUZZER, 1000, 150);
}

void melodiaCelebracao() {
  int notas[]    = {523, 659, 784, 1047, 784, 1047};
  int duracoes[] = {150, 150, 150, 300,  150, 400};
  for (int i = 0; i < 6; i++) {
    tone(PINO_BUZZER, notas[i], duracoes[i]);
    delay(duracoes[i] + 30);
  }
  noTone(PINO_BUZZER);
}

void simularTemperatura() {
  float variacao = (random(-10, 11)) / 10.0;
  tempSatelite += variacao;
  if (tempSatelite < -20.0) tempSatelite = -20.0;
  if (tempSatelite > 60.0)  tempSatelite = 60.0;
}

int lerPotenciometro() {
  int leitura = analogRead(PINO_POT);
  return map(leitura, 0, 4095, 1, 20);
}

bool botaoPressionado() {
  bool leitura = digitalRead(PINO_BOTAO);
  if (leitura != ultimoEstadoBotao) ultimoDebounce = millis();
  ultimoEstadoBotao = leitura;
  if ((millis() - ultimoDebounce) > DEBOUNCE_MS) {
    if (leitura != estadoBotao) {
      estadoBotao = leitura;
      if (estadoBotao == LOW) return true;
    }
  }
  return false;
}

void atualizarLEDs() {
  digitalWrite(PINO_LED_R, LOW);
  digitalWrite(PINO_LED_Y, LOW);
  digitalWrite(PINO_LED_G, LOW);
  if (missoes[missaoAtual].estado == DESBLOQUEADA) {
    if (millis() - ultimoBlink > 400) {
      ultimoBlink = millis();
      estadoBlink = !estadoBlink;
    }
    digitalWrite(PINO_LED_G, estadoBlink ? HIGH : LOW);
  } else if (missoes[missaoAtual].progresso >= 50) {
    digitalWrite(PINO_LED_Y, HIGH);
  } else {
    digitalWrite(PINO_LED_R, HIGH);
  }
}

void atualizarOLED() {
  Missao& m = missoes[missaoAtual];
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(m.nome);
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 13);
  display.print(m.progresso);
  display.println("%");
  int larguraBarra = map(m.progresso, 0, 100, 0, 124);
  display.drawRect(0, 33, 124, 10, SSD1306_WHITE);
  display.fillRect(0, 33, larguraBarra, 10, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print("Status: ");
  display.println(nomeEstado());
  display.setCursor(0, 56);
  display.print("T:");
  display.print(tempSatelite, 1);
  display.print("C M:");
  display.print(missaoAtual + 1);
  display.print("/");
  display.print(TOTAL_MISSOES);
  display.display();
}

// ──────────────────────────────────────────────────────────────
// Publicações MQTT
// ──────────────────────────────────────────────────────────────
void publicarProgressoMQTT() {
  if (!mqtt.connected()) return;
  char payload[8];
  itoa(missoes[missaoAtual].progresso, payload, 10);
  mqtt.publish(TOPIC_PROGRESS, payload, true);
}

void publicarStatusMQTT() {
  if (!mqtt.connected()) return;
  mqtt.publish(TOPIC_STATUS, nomeEstado().c_str(), true);
}

void publicarTelemetriaMQTT() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> doc;
  doc["missao"]        = missoes[missaoAtual].nome;
  doc["missao_idx"]    = missaoAtual;
  doc["progresso"]     = missoes[missaoAtual].progresso;
  doc["status"]        = nomeEstado();
  doc["temperatura"]   = serialized(String(tempSatelite, 1));
  doc["meta_atingida"] = missoes[missaoAtual].desbloqueada;
  doc["ip"]            = WiFi.localIP().toString();
  char payload[256];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_TELEMETRIA, payload, true);
}

void publicarRankingMQTT() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<512> doc;
  doc["missao"] = missoes[missaoAtual].nome;
  doc["total"]  = totalContribuicoes;
  JsonArray lista = doc.createNestedArray("ranking");
  for (int i = 0; i < MAX_RANKING; i++) {
    if (strlen(ranking[i].nome) > 0) {
      JsonObject item = lista.createNestedObject();
      item["pos"]   = i + 1;
      item["nome"]  = ranking[i].nome;
      item["valor"] = ranking[i].valor;
    }
  }
  char payload[512];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_RANKING, payload, true);
}

void publicarDesbloqueadaMQTT() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<128> doc;
  doc["missao"] = missoes[missaoAtual].nome;
  doc["status"] = "desbloqueada";
  char payload[128];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_UNLOCKED, payload, true);
}

void publicarMissoesMQTT() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<512> doc;
  JsonArray lista = doc.createNestedArray("missoes");
  for (int i = 0; i < TOTAL_MISSOES; i++) {
    JsonObject obj = lista.createNestedObject();
    obj["idx"]          = i;
    obj["nome"]         = missoes[i].nome;
    obj["progresso"]    = missoes[i].progresso;
    obj["desbloqueada"] = missoes[i].desbloqueada;
    obj["ativa"]        = (i == missaoAtual);
  }
  char payload[512];
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_LIST, payload, true);
}

void publicarTudo() {
  publicarStatusMQTT();
  publicarProgressoMQTT();
  publicarTelemetriaMQTT();
  publicarRankingMQTT();
  publicarMissoesMQTT();
}

// ──────────────────────────────────────────────────────────────
// Lógica de missão
// ──────────────────────────────────────────────────────────────
void atualizarEstado() {
  Missao& m = missoes[missaoAtual];
  EstadoMissao novoEstado;
  if (m.progresso <= 0)       novoEstado = AGUARDANDO;
  else if (m.progresso < 100) novoEstado = FINANCIANDO;
  else {
    novoEstado     = DESBLOQUEADA;
    m.progresso    = 100;
    m.desbloqueada = true;
  }
  if (novoEstado != m.estado) {
    m.estado = novoEstado;
    Serial.println(">> Estado: " + nomeEstado());
  }
}

void avancarMissao() {
  for (int i = 0; i < TOTAL_MISSOES; i++) {
    int proximo = (missaoAtual + 1 + i) % TOTAL_MISSOES;
    if (!missoes[proximo].desbloqueada) {
      missaoAtual  = proximo;
      tempSatelite = 22.0;
      resetarRanking();
      Serial.println(">> Avancando para: " + String(missoes[missaoAtual].nome));
      return;
    }
  }
  Serial.println(">> Todas as missoes desbloqueadas!");
}

void processarContribuicao(const char* nome, int valor) {
  Missao& m = missoes[missaoAtual];
  if (m.desbloqueada) return;
  if (valor > 20) valor = 20;
  if (valor < 1)  valor = 1;
  m.progresso += valor;
  if (m.progresso > 100) m.progresso = 100;
  adicionarRanking(nome, valor);
  atualizarEstado();
  publicarTudo();
  Serial.println(">> Contribuicao: " + String(nome) + " +" + String(valor) + "% | Total: " + String(m.progresso) + "%");
  if (m.desbloqueada) {
    publicarDesbloqueadaMQTT();
    melodiaCelebracao();
    delay(500);
    avancarMissao();
    publicarTudo();
  } else {
    beepConfirmacao();
  }
}

void processarReset() {
  for (int i = 0; i < TOTAL_MISSOES; i++) {
    missoes[i].progresso    = 0;
    missoes[i].desbloqueada = false;
    missoes[i].estado       = AGUARDANDO;
  }
  missaoAtual  = 0;
  tempSatelite = 22.0;
  resetarRanking();
  publicarTudo();
  Serial.println(">> Reset geral efetuado.");
}

void processarSelect(int idx) {
  if (idx < 0 || idx >= TOTAL_MISSOES) return;
  missaoAtual  = idx;
  tempSatelite = 22.0;
  resetarRanking();
  publicarTudo();
  Serial.println(">> Missao selecionada: " + String(missoes[missaoAtual].nome));
}

// ──────────────────────────────────────────────────────────────
// MQTT callback
// ──────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buffer[256];
  unsigned int len = length < 255 ? length : 255;
  memcpy(buffer, payload, len);
  buffer[len] = '\0';
  Serial.println("[MQTT CMD] " + String(topic) + " -> " + String(buffer));
  strncpy(comandoTipo,    topic,  63);
  strncpy(comandoPayload, buffer, 255);
  comandoTipo[63]     = '\0';
  comandoPayload[255] = '\0';
  comandoPendente = true;
}

// ──────────────────────────────────────────────────────────────
// MQTT conecta
// ──────────────────────────────────────────────────────────────
void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao MQTT... ");
    if (mqtt.connect(MQTT_CLIENT_ID)) {
      Serial.println("OK");
      mqtt.subscribe(TOPIC_CMD_CONTRIBUTE);
      mqtt.subscribe(TOPIC_CMD_RESET);
      mqtt.subscribe(TOPIC_CMD_SELECT);
      Serial.println("Assinando topicos de comando...");
      publicarTudo();
    } else {
      Serial.print("falhou rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// ──────────────────────────────────────────────────────────────
// Handlers HTTP
// ──────────────────────────────────────────────────────────────
void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["missao"]        = missoes[missaoAtual].nome;
  doc["missao_idx"]    = missaoAtual;
  doc["progresso"]     = missoes[missaoAtual].progresso;
  doc["status"]        = nomeEstado();
  doc["temperatura"]   = serialized(String(tempSatelite, 1));
  doc["meta_atingida"] = missoes[missaoAtual].desbloqueada;
  doc["ip"]            = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "application/json", json);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) { handleOptions(); return; }
  server.send(404, "text/plain", "Rota nao encontrada");
}

// ──────────────────────────────────────────────────────────────
// SETUP
// ──────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  pinMode(PINO_BOTAO,  INPUT_PULLUP);
  pinMode(PINO_LED_R,  OUTPUT);
  pinMode(PINO_LED_Y,  OUTPUT);
  pinMode(PINO_LED_G,  OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  analogReadResolution(12);

  resetarRanking();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO)) {
    Serial.println("OLED nao encontrado");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("StarForge");
    display.setTextSize(1);
    display.setCursor(15, 35);
    display.println("CubeSat Mission");
    display.setCursor(20, 50);
    display.println("Iniciando...");
    display.display();
    delay(1500);
  }

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFalha no Wi-Fi! Reiniciando...");
    ESP.restart();
  }
  Serial.println("\nWi-Fi conectado! IP: " + WiFi.localIP().toString());

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  conectarMQTT();

  server.on("/status",  HTTP_GET,     handleStatus);
  server.on("/status",  HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Servidor HTTP iniciado na porta 80");
  Serial.println("Missao ativa: " + String(missoes[missaoAtual].nome));
}

// ──────────────────────────────────────────────────────────────
// LOOP
// ──────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  if (comandoPendente) {
    comandoPendente = false;
    if (strcmp(comandoTipo, TOPIC_CMD_CONTRIBUTE) == 0) {
      StaticJsonDocument<128> doc;
      if (!deserializeJson(doc, comandoPayload)) {
        const char* nome = doc["nome"] | "Anonimo";
        int valor        = doc["valor"] | 10;
        processarContribuicao(nome, valor);
      }
    } else if (strcmp(comandoTipo, TOPIC_CMD_RESET) == 0) {
      processarReset();
    } else if (strcmp(comandoTipo, TOPIC_CMD_SELECT) == 0) {
      StaticJsonDocument<64> doc;
      if (!deserializeJson(doc, comandoPayload)) {
        int idx = doc["idx"] | 0;
        processarSelect(idx);
      }
    }
  }

  if (botaoPressionado()) {
    int contribuicao = lerPotenciometro();
    processarContribuicao("Painel Fisico", contribuicao);
  }

  if (millis() - ultimaTemp > 3000) {
    ultimaTemp = millis();
    simularTemperatura();
    publicarTelemetriaMQTT();
    publicarMissoesMQTT();
  }

  if (millis() - ultimoOLED > 500) {
    ultimoOLED = millis();
    atualizarOLED();
  }

  atualizarLEDs();
  delay(20);
}