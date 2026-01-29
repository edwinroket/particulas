//conexion wifi
#include <ESP8266WiFi.h>

//para el cliente ntp
#include <time.h> 
#define MY_NTP_SERVER "at.pool.ntp.org"           
#define MY_TZ "<-04>4<-03>,M9.1.6/24,M4.1.6/24"  
time_t now;                         // this are the seconds since Epoch (1970) - UTC
tm tm;                              // the structure tm holds time information in a more convenient way 

//para el dht22
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTTYPE    DHT11     // DHT 22 (AM2302)
#define DHTPIN 0     // Digital pin connected to the DHT sensor 
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

//parametros wifi y utc-4 para ntp

const char *ssid     = "WiFi_Mesh-941575";
const char *password = "xGKxPSX5";
//compensacion utc -4 4*60*60
const long utcOffsetInSeconds = -14400;

//para el sensor de particulas
#include "SdsDustSensor.h"

//mqtt
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

//mqtt

const char* mqtt_server  = "broker.hivemq.com";
const int mqtt_port  = 1883;

// Tópicos MQTT para colegio2 CAMBIAR PARA LOS DEMAS COLEGIOS
const char topic1[] = "/florida/Aire/tt";
const char topic2[] = "/florida/Aire/hh";
const char topic3[] = "/florida/Aire/pm25";
const char topic4[] = "/florida/Aire/pm10";
const char topic5[] = "/florida/Aire/fecha";


//parametros del sensore d eparticulas sds011
int rxPin = 5; /*d2 va a tx del sensor*/
int txPin = 4;
SdsDustSensor sds(rxPin, txPin);

// Genera un ID de cliente único
    String clientId = "SSTTTlk-" + String(random(0xffff), HEX);


void reconnect() {
  // Intenta reconectarse al servidor MQTT hasta que tenga éxito
  while (!client.connected()) {
    Serial.print("Intentando conectar al servidor MQTT...");

    // Intenta conectarse al servidor
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!LG");

      // Suscríbete a cualquier tópico (opcional)
      // client.subscribe("meu_topico");
    } else {
      Serial.print("fallo, rc=");
      Serial.print(client.state());
      Serial.println(" intentar otra vez en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  //para el dht22
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;

  
  
  
  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }  
  configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!
  
 
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port); 

  
  //SDS sensor
  sds.begin();
/*  asi estaba en ciclos de lectura
  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setActiveReportingMode().toString()); // ensures sensor is in 'active' reporting mode
  //Serial.println(sds.setContinuousWorkingPeriod().toString()); // ensures sensor has continuous working period - default but not recommended
  delay(5000); //espera 5 seg antes de iniciar el ciclo
  Serial.println(sds.setCustomWorkingPeriod(2).toString()); //mide en ciclos de 2 minutos  
  */
  //ahora en ciclo de query

  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setQueryReportingMode().toString()); // ensures sensor is in 'query' reporting mode 

}

void loop() {
  //lee el server ntp hora
  time(&now); 
  localtime_r(&now, &tm);           // update the structure tm with the current time
  //lee el dht22
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  float tt = event.temperature;
  dht.humidity().getEvent(&event);
  float hh = event.relative_humidity;
  char fecha[19];
  sprintf( fecha, "%.2d/%.2d/%.4d %.2d:%.2d:%.2d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

  //ciclo de query del sds
  sds.wakeup();
  Serial.println("Despertando el sensor..");
  delay(30000); // working 30 seconds


  
  //PmResult pm = sds.readPm();
  PmResult pm = sds.queryPm();  //lee contra peticion

  if (pm.isOk()) {
    

    Serial.print(fecha); 
    Serial.print("-> Temp: ");
    Serial.print(tt);
    Serial.print(F("°C, Hum Rel: "));
    Serial.print(hh);
    Serial.print(F("%, "));
    Serial.print("PM2.5 = ");
    Serial.print(pm.pm25);
    Serial.print(", PM10 = ");
    Serial.println(pm.pm10);


    WiFi.begin(ssid, password);

    while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );
    }  

   if (!client.connected()) {
     reconnect();
   }
   client.loop();
   //String clientId = "SSTT-" + String(random(0xffff), HEX);
    
   if (client.connect(clientId.c_str())) {
      Serial.println("connected!Lg2" + clientId);
      // Once connected, publish an announcement...
      //client.publish("/colegio2/Aire/tt", "Hola ");
      // ... and resubscribe
      String mensaje = String(tt);
      client.publish(topic1, mensaje.c_str());
      delay(500);

      mensaje = String(hh);
      client.publish(topic2, mensaje.c_str());
      delay(500);

      mensaje = String(pm.pm25);
      client.publish(topic3, mensaje.c_str());

      mensaje = String(pm.pm10);
      client.publish(topic4, mensaje.c_str());
      delay(500);
      
      mensaje = String(fecha);
      client.publish(topic5, mensaje.c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      loop(); 
    }
  }
  else {
    Serial.print("Could not read values from sensor, reason: ");
    Serial.println(pm.statusToString());
  }



  WorkingStateResult state = sds.sleep();
  if (state.isWorking()) {
    Serial.println("Problem with sleeping the sensor.");
  } else {
    Serial.println("Sensor is sleeping");
    delay(300000); // wait 5 minute con los 30 segs de espera del bucle 
  }

}