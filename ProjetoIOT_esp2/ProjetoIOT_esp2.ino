#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include "MFRC522.h"
#include <PubSubClient.h>   // Read the rest of the article
#include <stdlib.h>
#include "DHTesp.h"
#include <Servo.h>

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4 
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/

#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  4  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4 
#define DHTPIN D0
#define SERVO D3

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
DHTesp dht;
Servo tranca;

const char *ssid =  "brisa-484946";   // cannot be longer than 32 characters!
const char *pass =  "hlr046f3";   //
 
const char *mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char *mqtt_user = "MagdielCAS";
const char *mqtt_pass = "8549402a0cff41c9b3498bb6347ef22a";
const char *mqtt_client_name = "espClient2"; // Client connections cant have the same connection name
 
String BASE_TOPIC = "MagdielCAS/feeds/";

WiFiClient wclient;  //Declares a WifiClient Object using ESP8266WiFi
PubSubClient client(wclient, mqtt_server,  mqtt_port);  //instanciates client object

const  char*  host  = "api.pushbullet.com";
const  int  httpsPort  =  443;
const  char*  PushBulletAPIKEY  =  "o.D98UrGVDgGOjlI7YeBtwfqFQBieqMtfB";

unsigned long lastDHT = 0;
unsigned long lastRFID = 0;
boolean ativarTemperatura = true;
boolean ativarUmidade = true;
int ldr = 0;
 
//Function is called when, a message is recieved in the MQTT server.
void callback(const MQTT::Publish& pub) {
  Serial.print(pub.topic());
  Serial.print(" => ");
  String payload = pub.payload_string();
  //JSON PARSING
  const size_t bufferSize = 2*JSON_OBJECT_SIZE(1) + 20;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(payload);
  

  if(pub.topic() == BASE_TOPIC+"activateTEMP"){
    ativarTemperatura = payload == "ON";
  }
  if(pub.topic() == BASE_TOPIC+"activateHUMIDITY"){
    ativarUmidade = payload == "ON";
  }
  if(pub.topic() == BASE_TOPIC+"ldr"){
    ldr = payload.toInt();
  }
  

  Serial.println(payload);
}


void setup() {
  lastDHT = millis();
  lastRFID = millis();
  Serial.begin(115200);    // Initialize serial communications
  delay(250);
  Serial.println(F("Booting...."));
  dht.setup(DHTPIN);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  tranca.attach(SERVO);
  tranca.write(30);
}

void loop() { 
   if (WiFi.status() != WL_CONNECTED) {  //wifi not connected?
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);
 
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }
 
  if (WiFi.status() == WL_CONNECTED) {
  //client object makes connection to server
    if (!client.connected()) {
      Serial.println("Connecting to MQTT server");
    //Authenticating the client object
      if (client.connect(MQTT::Connect("mqtt_client_name")
                         .set_auth(mqtt_user, mqtt_pass))) {
        Serial.println("Connected to MQTT server");
        
    //Subscribe code
        client.set_callback(callback);
        client.subscribe(BASE_TOPIC+"activateTEMP");
        client.subscribe(BASE_TOPIC+"activateHUMIDITY");
        client.subscribe(BASE_TOPIC+"ldr");
      } else {
        Serial.println("Could not connect to MQTT server");   
      }
    }
 
    if (client.connected())
      client.loop();
  }
  //Sensores
  leituraDHT();
  verificaRFID();
}

void leituraDHT(){
  if((millis()-lastDHT)/1000 >= 20 ){
    lastDHT = millis();
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    if (isnan(humidity) || isnan(temperature) ) {
      Serial.println("Falha na leitura do sensor!");
      return;
    }
    if(ativarTemperatura){client.publish(BASE_TOPIC+"temp",String(temperature));}
    if(ativarUmidade){client.publish(BASE_TOPIC+"humidity",String(humidity));}
  }
}

void verificaRFID() {
  // Procura por cartao RFID
  if((millis()-lastRFID)/1000 < 10){
    return;
  }
  if (!mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Seleciona o cartao RFID
  if (!mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }
  //Mostra UID na serial
  lastRFID = millis();
  Serial.print("UID da tag :");
  String conteudo= "";
  byte letra;
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
//     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
//     Serial.print(mfrc522.uid.uidByte[i], HEX);
     conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  conteudo.toUpperCase();
  Serial.print("RFID: ");
  Serial.println(conteudo.substring(1));
  if (conteudo.substring(1) == "49 0B A0 1A" ||conteudo.substring(1) == "14 B7 31 05"){
    Serial.println("Liberado !");
    client.publish(BASE_TOPIC+"rfidSession",conteudo+": Liberado");
    notificar(conteudo+": Liberado");
    if(ldr>700){
      client.publish(BASE_TOPIC+"light","ON");
    }
    for(int i= 30;i <= 150; i+=5){
      tranca.write(i);
    }
    delay(3000);
    for(int i= 150;i >= 30; i-=5){
      tranca.write(i);
    }
  }else{
    Serial.println("Acesso negado!");
    client.publish(BASE_TOPIC+"rfidSession",conteudo+": Negado");
    notificar(conteudo+": Negado");
  }
  delay(100);
}


void notificar(String msg){
    // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String url = "/v2/pushes";
  String messagebody = "{\"type\": \"note\", \"title\": \"Projeto IoT\", \"body\": \""+msg+"\"}\r\n";

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Bearer " + PushBulletAPIKEY + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " +
               String(messagebody.length()) + "\r\n\r\n");
  client.print(messagebody);

  Serial.println("Requisicao enviada, verifique o celular");

}

