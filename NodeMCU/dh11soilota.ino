
//REGION: Librerias

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "PubSubClient.h"
#include <Wire.h>

//REGION: Configuración wireless

const char* ssid="tocateloswebos";
const char* password = "TOCA$mela1";
const char* namehost="NODE1";
const char* clientname = "NODE1";

//REGION: MQTT Configuración de la conexión

const char* mqtt_server = "192.168.2.178"; 
const char* mqtt_user = "admin"; 
const char* mqtt_password = "mypw91885"; 

//REGION: Conectividad

WiFiClient espClient; 
PubSubClient client(espClient); 
MDNSResponder mdns;

//REGION: Tags de MQTT donde se publica la información

#define humidity_topic "sensor/salon/humidity2" 
#define temperature_topic "sensor/salon/temperature2" 
#define heat_topic "sensor/salon/sensaciontermica"
#define soil_humidity "sensor/salon/soilhumidity"

//REGION: Configuración sensor de temperatura/humedad

#define DHTPIN 5           // Pin al que se conecta el sensor
#define DHTTYPE DHT11      // Tipo de sensor que hemos conectado
DHT dht(DHTPIN, DHTTYPE);  // Creamos la variable DHT
int timeSinceLastRead = 0; // Variable que usamos para los intervalos de lectura del sensor
const int analog_ip = A0;
int led_pin = 13;
#define N_DIMMERS 3
int dimmer_pin[] = {14, 5, 15};

static char celsiusTemp[7];     // Variable para mostrar grados celsius
static char celsiusHemp[7];
static char humidityTemp[7];    // Variable para mostrar la humedad
static char porcentaje[7];

static float celsiusT;     // Variable para mostrar grados celsius
static float humidityT;    // Variable para mostrar la humedad
static float celsiusH;     // Variable para mostrar la sensacion térmica
static float soil;

//REGION: Configuración del servidor web

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
String webPage = "";


void setup() 
{
    /* switch on led */
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  Wire.begin( 4,5);
  dht.begin();
//  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
//    Serial.println(F("BH1750 initialised"));
//  }
//  else {
//    Serial.println(F("Error initialising BH1750"));
//  }
  initSerial();  
  initWifi();
  initOTA();
  initWebServer();
  initMQTTServer();
   /* switch off led */
  digitalWrite(led_pin, HIGH);

}

void loop() 
{
    ArduinoOTA.handle();
    //Se pone a la escucha al servidor web
    server.handleClient();    

    //Nos aseguramos de que conecte
    if (!client.connected()) {
      reconnect();
    }  
    //MQTT comienza a funcionar
    client.loop();
    readData();
}

void initOTA()
{
  /* configure dimmers, and OTA server events */
  analogWriteRange(1000);
  analogWrite(led_pin, 990);

  for (int i = 0; i < N_DIMMERS; i++) {
    pinMode(dimmer_pin[i], OUTPUT);
    analogWrite(dimmer_pin[i], 50);
  }
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    for (int i = 0; i < N_DIMMERS; i++) {
      analogWrite(dimmer_pin[i], 0);
    }
    analogWrite(led_pin, 0);

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
     for (int i = 0; i < 30; i++) {
      analogWrite(led_pin, (i * 100) % 1001);
      delay(50);
    }
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
    ESP.restart();
  });
  ArduinoOTA.begin();
  Serial.println("Ready OTA UPDATE");
 

}
void initSerial()
{
  Serial.begin(115200);
  Serial.setTimeout(5000);

  // Inicialización del puerto serie
  while(!Serial) { }

  //Mensaje de bienvenida
  Serial.println("Booting ...... \n");
  Serial.println("-------------------------------------");
  Serial.println("\tDispositivo en marcha");
  Serial.println("-------------------------------------");
}

void initWifi()
{
  Serial.println();
  Serial.print("Wifi conectando a: ");
  Serial.println( ssid );

  WiFi.hostname(namehost);
  WiFi.begin(ssid,password);

  Serial.println();
  Serial.print("Conectando");

  while( WiFi.status() != WL_CONNECTED ){
      delay(500);
      Serial.print(".");        
  }
  Serial.println("Wifi conectada!");
  Serial.print("NodeMCU IP Address : ");
  Serial.println(WiFi.localIP() );

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS arrancado");
  }
}

void initMQTTServer()
{
 client.setServer(mqtt_server, 1883); 
}

//Vuelve a conectar al servidor de MQTT si se desconecta
void reconnect() 
{
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT ...");
    
    if (client.connect(clientname, mqtt_user, mqtt_password)) {
      Serial.println("OK");
    } else {
      Serial.print("KO, error : ");
      Serial.print(client.state());
      Serial.println(" Esperando 5 segundos para reintentar");
      delay(5000);
    }
  }
}

void initWebServer()
{
    server.on("/", []() {
    webPage = sendHTML(); //"";
//    webPage += "<!DOCTYPE HTML>";
//    webPage += "<html>";
//    webPage += "<head><title>ESP8266 - NODE1</title></head><body><h1>ESP8266 - Temperatura y humedad</h1><h3>Temperatura en Celsius: ";
//    webPage += celsiusTemp;
//    webPage += " *C</h3><h3>Temperatura aparente: ";
//    webPage += celsiusH;
//    webPage += " *C</h3><h3>Humedad: ";
//    webPage += humidityTemp;
//    webPage += " %</h3>";
//    webPage += "<h3>Humedad Suelo: ";
//    webPage += porcentaje;
//    webPage += " %</h3>";
//    webPage += "</body></html>";
    server.send(200, "text/html", webPage);
  });
  
  
  //MDNS.begin(host);
  httpUpdater.setup(&server);
  server.begin();
  //MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server arrancado");
}

String sendHTML()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<link href=\"https://fonts.googleapis.com/css?family=Open+Sans:300,400,600\" rel=\"stylesheet\">\n";
  ptr +="<title>ESP8266 Weather Report</title>\n";
  ptr +="<style>html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  ptr +="body{margin-top: 50px;}\n";
  ptr +="h1 {margin: 50px auto 30px;}\n";
  ptr +=".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
  ptr +=".humidity-icon{background-color: #3498db;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;}\n";
  ptr +=".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  ptr +=".humidity{font-weight: 300;font-size: 60px;color: #3498db;}\n";
  ptr +=".temperature-icon{background-color: #f39c12;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;}\n";
  ptr +=".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  ptr +=".temperature{font-weight: 300;font-size: 60px;color: #f39c12;}\n";
  ptr +=".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -20px;top: 15px;}\n";
  ptr +=".data{padding: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  
   ptr +="<div id=\"webpage\">\n";
   
   ptr +="<h1>ESP8266 Weather Report</h1>\n";
   ptr +="<div class=\"data\">\n";
   ptr +="<div class=\"side-by-side temperature-icon\">\n";
   ptr +="<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
   ptr +="width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
   ptr +="<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
   ptr +="c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
   ptr +="c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
   ptr +="c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
   ptr +="c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
   ptr +="</svg>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side temperature-text\">Temperature</div>\n";
   ptr +="<div class=\"side-by-side temperature\">";
   ptr +=celsiusTemp;
   ptr +="<span class=\"superscript\">  ºC</span></div>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side temperature-icon\">\n";
   ptr +="<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
   ptr +="width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
   ptr +="<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
   ptr +="c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
   ptr +="c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
   ptr +="c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
   ptr +="c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
   ptr +="</svg>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side temperature-text\">Relative Temperature</div>\n";
   ptr +="<div class=\"side-by-side temperature\">";
   ptr +=celsiusH;
   ptr +="<span class=\"superscript\">  ºC</span></div>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"data\">\n";
   ptr +="<div class=\"side-by-side humidity-icon\">\n";
   ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
   ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
   ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
   ptr +="</svg>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side humidity-text\">Humidity</div>\n";
   ptr +="<div class=\"side-by-side humidity\">";
   ptr +=humidityTemp;
   ptr +="<span class=\"superscript\"> %</span></div>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side humidity-icon\">\n";
   ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
   ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
   ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
   ptr +="</svg>\n";
   ptr +="</div>\n";
   ptr +="<div class=\"side-by-side humidity-text\">Humidity Planta</div>\n";
   ptr +="<div class=\"side-by-side humidity\">";
   ptr +=porcentaje;
   ptr +="<span class=\"superscript\"> %</span></div>\n";
   ptr +="</div>\n";

  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}


void readData()
{           
      if(timeSinceLastRead > 50000) {

    // El sensor tarda en tomar la lectura 250 milisegundos, por lo que tomamos un tiempo de 2 segundos para reportar las nuevas lecturas.
    // La temperatura se lee por defecto en grados celsius, si queremos mostrar farenheit necesitamos indicarlo con el parametro true.

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float f = dht.readTemperature(true);
    // Calculo para grados celsius
    float hic = dht.computeHeatIndex(t, h, false);
    celsiusT= t;
    celsiusH = hic;
    humidityT = h;
  
    // Aqui ponemos el medidor de humedad del suelo    
    float reading = analogRead(analog_ip);
    float percent = (reading/1024)*100;
    soil = percent;

    //Aqui convertimos los float a cadena de caracteres
    dtostrf(t, 6, 2, celsiusTemp);
    dtostrf(hic, 6, 2, celsiusHemp);
    dtostrf(h, 6, 2, humidityTemp);
    dtostrf(percent,4,2,porcentaje);
    
    //Imprimimos por el puerto serie los datos
    Serial.println(WiFi.localIP() );
    Serial.print("Humedad: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" *C \n");
    Serial.print(f);
    Serial.print(" *F\n");
    Serial.print("Sensación térmica: ");
    Serial.print(hic);
    Serial.print(" *C \n");        
    Serial.print ("Valor humedad suelo:\t");
    Serial.print (percent);
    Serial.println("\n\n************************************\n");      
    
    client.publish(temperature_topic, String(celsiusTemp).c_str(), true);
    Serial.println("update mqtt");
    client.publish(humidity_topic, String(humidityTemp).c_str(), true);
    Serial.println("update mqtt");
    client.publish(heat_topic,String(celsiusHemp).c_str(),true);
    Serial.println("update mqtt");
    client.publish(soil_humidity,String(percent).c_str(),true);
    Serial.println("update mqtt");
    
    
    String result = "Temperatura: ";
    result = result + celsiusTemp;
    char charBuf[result.length() + 1];
    result.toCharArray(charBuf,result.length());

    //Reiniciamos el contador de tiempo
    timeSinceLastRead = 0;
  }
  //Volvemos a sumar tiempo al contador
  delay(10000);
  timeSinceLastRead += 10000;
}
