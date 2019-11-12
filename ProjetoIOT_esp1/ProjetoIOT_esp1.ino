#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>   // Read the rest of the article
#include <stdlib.h>
 
const char *ssid =  "brisa-484946";   // cannot be longer than 32 characters!
const char *pass =  "hlr046f3";   //
 
const char *mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char *mqtt_user = "MagdielCAS";
const char *mqtt_pass = "8549402a0cff41c9b3498bb6347ef22a";
const char *mqtt_client_name = "espClient1"; // Client connections cant have the same connection name
 
#define BUFFER_SIZE 100
#define RELE_PIN D1
#define BTN_PIN D0
String BASE_TOPIC = "MagdielCAS/feeds/";

unsigned long previousMillis = 0;
const long interval = 10000;

unsigned long last = 0;

boolean sensorAtivado = true;
boolean luzLigada = false;

WiFiClient wclient;  //Declares a WifiClient Object using ESP8266WiFi
PubSubClient client(wclient, mqtt_server,  mqtt_port);  //instanciates client object

//Function is called when, a message is recieved in the MQTT server.
void callback(const MQTT::Publish& pub) {
  Serial.print(pub.topic());
  Serial.print(" => ");
  String payload = pub.payload_string();
  //JSON PARSING
  const size_t bufferSize = 2*JSON_OBJECT_SIZE(1) + 20;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(payload);
  
  if(pub.topic() == BASE_TOPIC+"activateLDR"){
    sensorAtivado = payload == "ON";
  }
  if(pub.topic() == BASE_TOPIC+"light"){
    luzLigada = payload == "ON";
    digitalWrite(RELE_PIN, luzLigada ? HIGH : LOW);
  }

  Serial.println(payload);
}

void setup() {
  // Setup console
  last = millis();
  pinMode(RELE_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  digitalWrite(BTN_PIN, LOW);
  Serial.begin(115200);  //set the baud rate
  delay(10);
  Serial.println();
  Serial.println();
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
        client.subscribe(BASE_TOPIC+"activateLDR");
        client.subscribe(BASE_TOPIC+"light");
      } else {
        Serial.println("Could not connect to MQTT server");   
      }
    }
 
    if (client.connected())
      client.loop();
  }
  pressButton();
  lerSensor();
}

void lerSensor(){
  if((millis() - last)/1000 >= 20 && sensorAtivado){
    last = millis();
    int var = analogRead(A0);
    client.publish(BASE_TOPIC+"ldr",String(var));
  }
}

void pressButton(){
  if(digitalRead(BTN_PIN) == HIGH){
    client.publish(BASE_TOPIC+"light",luzLigada ? "OFF" : "ON");
    delay(50);
  }
}
 
