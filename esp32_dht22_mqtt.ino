#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_HOST[] = "172.16.3.215";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char STUDENT_USERNAME[] = "replace_with_your_username";
constexpr char MQTT_PASSWORD[] = "YOUR_MQTT_PASSWORD";

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t LED_PIN = 2;
constexpr uint8_t DHT_TYPE = DHT22;
constexpr unsigned long PUBLISH_INTERVAL_MS = 3000;

const String sensorTopic = String("classroom/") + STUDENT_USERNAME + "/sensor/temp";
const String controlTopic = String("classroom/") + STUDENT_USERNAME + "/control/led";

WiFiClient networkClient;
PubSubClient mqttClient(networkClient);
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastPublished = 0;

void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
  if (String(topic) != controlTopic) return;

  String command;
  command.reserve(length);
  for (unsigned int index = 0; index < length; ++index) {
    command += static_cast<char>(payload[index]);
  }

  if (command == "ON") {
    digitalWrite(LED_PIN, HIGH);
  } else if (command == "OFF") {
    digitalWrite(LED_PIN, LOW);
  }
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("Wi-Fi connected; IP: ");
  Serial.println(WiFi.localIP());
}

void connectToMqtt() {
  while (!mqttClient.connected()) {
    const String clientId = String("iot-") + STUDENT_USERNAME + "-" +
                            String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect(clientId.c_str(), STUDENT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      if (!mqttClient.subscribe(controlTopic.c_str())) {
        Serial.println("MQTT subscribe failed");
      }
    } else {
      Serial.print("failed, state=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();
  connectToWiFi();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(handleMqttMessage);
  mqttClient.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  if (!mqttClient.connected()) connectToMqtt();
  mqttClient.loop();

  const unsigned long now = millis();
  if (now - lastPublished < PUBLISH_INTERVAL_MS) return;
  lastPublished = now;

  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 read failed");
    return;
  }

  char payload[80];
  snprintf(payload, sizeof(payload),
           "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);
  if (!mqttClient.publish(sensorTopic.c_str(), payload)) {
    Serial.println("MQTT publish failed");
  }
}
