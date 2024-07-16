/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-aws-iot
 */

#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC "esp32/esp32-to-aws"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/aws-to-esp32"

#define PUBLISH_INTERVAL 4000  // 4 seconds

WiFiClientSecure net;
MQTTClient client = MQTTClient(256);

unsigned long lastPublishTime = 0;

void setupNetwork() {
  Serial.begin(9600);
  setupWifi();

  setupMQTT();
}

void setupWifi() {
  delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void keepMQTTAlive() {
  client.loop();
}

void reconnect() {

  Serial.print("ESP32 connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("ESP32  - AWS IoT Connected!");
}

void setupMQTT() {
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a handler for incoming messages
  client.onMessage(messageHandler);

  checkMQTT();
}

void checkMQTT() {

  if (!client.connected()) {
    reconnect();
  }
}

void messageHandler(String &topic, String &payload) {
  Serial.println("received:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

  // You can process the incoming data as json object, then control something
  /*
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  */
}

void publishMessage(const char* jsonMessage) {
  // send message, the Print interface can be used to set the message contents
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonMessage);
  Serial.println("Message published.");
}
