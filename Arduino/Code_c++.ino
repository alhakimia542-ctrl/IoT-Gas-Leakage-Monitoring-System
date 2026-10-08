#include "WiFiEsp.h"
#include "SoftwareSerial.h"
#include <PubSubClient.h>

// --- إعدادات الشبكة ---
char ssid[] = "XXXXX"; 
char pass[] = "XXXXX"; 

const char* mqtt_server = "192.168.137.1"; 
const int mqtt_port = 1883;
// ----------------------

const int gasPin = A0;
const int buzzerPin = 8;
const int gasThreshold = 400; // مستوى الغاز الذي يعمل عنده الإنذار التلقائي

SoftwareSerial esp8266(2, 3); 

WiFiEspClient espClient;
PubSubClient client(espClient);

unsigned long lastSend = 0;
bool manualOverride = false; // متغير لمعرفة إذا كان المستخدم يتحكم بالجرس يدوياً

void setup() {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); 

  Serial.begin(9600);
  esp8266.begin(9600);

  WiFi.init(&esp8266);
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not present!");
    while (true);
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    delay(2000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.print("Attempting initial MQTT connection...");
  String clientId = "AhmedGasNode-";
  clientId += String(random(0xffff), HEX);
  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    client.subscribe("ahmed/buzzer_control"); 
  } else {
    Serial.println("failed, will try in loop");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim(); 

  Serial.print("\n>>> Message arrived on topic: ");
  Serial.println(topic);
  Serial.print(">>> Payload: [");
  Serial.print(msg);
  Serial.println("]");

  if (String(topic) == "ahmed/buzzer_control") {
    if (msg == "true" || msg == "1" || msg == "on" || msg == "ON") {
      manualOverride = true; // تفعيل التحكم اليدوي
      digitalWrite(buzzerPin, HIGH);
      Serial.println(">>> Buzzer Turned ON Manually");
    } 
    else if (msg == "false" || msg == "0" || msg == "off" || msg == "OFF") {
      manualOverride = false; // إيقاف التحكم اليدوي
      digitalWrite(buzzerPin, LOW);
      Serial.println(">>> Buzzer Turned OFF Manually");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "AhmedGasNode-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("ahmed/buzzer_control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  
  // استدعاء loop بشكل متكرر لضمان عدم ضياع أي رسالة قادمة
  client.loop(); 

  if (millis() - lastSend > 1000) { 
    int gasLevel = analogRead(gasPin);
    String gasStr = String(gasLevel);
    
    client.publish("ahmed/gas_sensor/level", gasStr.c_str());
    
    Serial.print("Gas Level Sent: ");
    Serial.println(gasLevel);

    // --- الإنذار التلقائي للغاز ---
    // إذا لم يكن المستخدم قد شغل الجرس يدوياً، دع الأردوينو يتحكم به حسب الغاز
    if (!manualOverride) {
      if (gasLevel >= gasThreshold) {
        digitalWrite(buzzerPin, HIGH); // تشغيل الإنذار تلقائياً
        Serial.println("--- DANGER: Auto Alarm TRIGGERED! ---");
      } else {
        digitalWrite(buzzerPin, LOW); // إيقاف الإنذار عند زوال الخطر
      }
    }
    
    lastSend = millis();
  }
}