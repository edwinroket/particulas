// conexion wifi
#include <ESP8266WiFi.h>

// para el cliente ntp
#include <time.h>
#define MY_NTP_SERVER "at.pool.ntp.org"
#define MY_TZ "<-04>4<-03>,M9.1.6/24,M4.1.6/24"
time_t now;
tm tm;

// para el dht11
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTTYPE DHT11
#define DHTPIN 0
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

// para el sensor de particulas
#include "SdsDustSensor.h"

// WebSocket + MQTT  ← NUEVO
#include <WebSocketsClient.h>
#include <MQTT.h>

const char *ssid     = "WiFi_Mesh-941575";
const char *password = "xGKxPSX5";

// Tópicos MQTT
const char topic1[] = "/florida/Aire/tt";
const char topic2[] = "/florida/Aire/hh";
const char topic3[] = "/florida/Aire/pm25";
const char topic4[] = "/florida/Aire/pm10";
const char topic5[] = "/florida/Aire/fecha";

// SDS011
int rxPin = 5;
int txPin = 4;
SdsDustSensor sds(rxPin, txPin);

// ── NUEVO: cliente WebSocket y MQTT ──────────────────────────
WebSocketsClient webSocket;
MQTTClient mqttClient(512);          // buffer 512 bytes

const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_ws_port = 8000;     // HiveMQ WebSocket (sin TLS)
// Si quieres TLS usa puerto 8884 y webSocket.beginSSL(...)

String clientId = "SSTTTlk-" + String(random(0xffff), HEX);
// ─────────────────────────────────────────────────────────────

void conectarMQTT() {
  Serial.print("Conectando MQTT...");
  while (!mqttClient.connect(clientId.c_str())) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" Conectado!");
}

void setup() {
  Serial.begin(115200);

  // DHT
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado — IP: " + WiFi.localIP().toString());

  // NTP
  configTime(MY_TZ, MY_NTP_SERVER);

  // ── NUEVO: iniciar WebSocket apuntando al path WS de HiveMQ ──
  webSocket.begin(mqtt_server, mqtt_ws_port, "/mqtt"); // path obligatorio
  webSocket.setReconnectInterval(5000);

  // Vincular WebSocket al cliente MQTT
  mqttClient.begin(webSocket);        // ← clave: pasa el WS, no un WiFiClient
  // ─────────────────────────────────────────────────────────────

  conectarMQTT();

  // SDS011
  Serial.println(sds.queryFirmwareVersion().toString());
  Serial.println(sds.setQueryReportingMode().toString());
}

void loop() {
  // ── NUEVO: mantener vivo el WebSocket y MQTT ──
  webSocket.loop();
  mqttClient.loop();
  if (!mqttClient.connected()) {
    conectarMQTT();
  }
  // ──────────────────────────────────────────────

  // Hora NTP
  time(&now);
  localtime_r(&now, &tm);
  char fecha[19];
  sprintf(fecha, "%.2d/%.2d/%.4d %.2d:%.2d:%.2d",
          tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
          tm.tm_hour, tm.tm_min, tm.tm_sec);

  // DHT11
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  float tt = event.temperature;
  dht.humidity().getEvent(&event);
  float hh = event.relative_humidity;

  // SDS011 — ciclo query
  sds.wakeup();
  Serial.println("Despertando sensor...");
  delay(30000);

  PmResult pm = sds.queryPm();

  if (pm.isOk()) {
    Serial.printf("%s -> Temp: %.1f°C, Hum: %.1f%%, PM2.5: %.1f, PM10: %.1f\n",
                  fecha, tt, hh, pm.pm25, pm.pm10);

    // ── NUEVO: publicar directo, sin reconectar WiFi ni re-connect ──
    mqttClient.publish(topic1, String(tt));
    delay(200);
    mqttClient.publish(topic2, String(hh));
    delay(200);
    mqttClient.publish(topic3, String(pm.pm25));
    delay(200);
    mqttClient.publish(topic4, String(pm.pm10));
    delay(200);
    mqttClient.publish(topic5, String(fecha));
    // ───────────────────────────────────────────────────────────────

    Serial.println("Datos publicados por WebSocket.");
  } else {
    Serial.println("Error leyendo SDS: " + pm.statusToString());
  }

  // Dormir sensor 5 min
  WorkingStateResult state = sds.sleep();
  if (state.isWorking()) {
    Serial.println("Problema al dormir el sensor.");
  } else {
    Serial.println("Sensor dormido.");
    delay(300000);
  }
}
