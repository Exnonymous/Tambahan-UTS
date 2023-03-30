#include <Arduino.h>
#include <Ticker.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <BH1750.h>
#define LED_GREEN  16
#define LED_YELLOW 17
#define LED_RED    5
#define DHT_PIN 19

#define WIFI_SSID "coba"
#define WIFI_PASSWORD "apaituitu"
#define MQTT_BROKER  "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH   "esp32_farrel/2540125390/data"
#define MQTT_TOPIC_SUBSCRIBE "esp32_farrel/2540125390/data"  

float globalHumidity = 0, globalTemp = 0, globalLux = 0;
int g_nCount=0;
Ticker timer1Sec;
Ticker timerLedBuiltinOff;
Ticker timerLedRedOff;
Ticker timerYellowOff;
Ticker timerReadSensor;
Ticker timerPublish;
Ticker timerMqtt;

DHTesp dht;
BH1750 lightMeter;

char g_szDeviceId[30];
WiFiClient espClient;
PubSubClient mqtt(espClient);
boolean mqttConnect();
void WifiConnect();

void sendLux()
{
  char szMsg[50];
  static int nMsgCount=0;
  sprintf(szMsg, "Current Lux Value: %.2f ", globalLux);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
}

void sendTemp()
{
  char szMsg[50];
  static int nMsgCount=0;
  sprintf(szMsg, "Current Temp Value: %.2f ", globalTemp);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
}

void sendHumidity()
{
  char szMsg[50];
  static int nMsgCount=0;
  sprintf(szMsg, "Current Humidity Value: %.2f ", globalHumidity);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();
}
boolean mqttConnect() {
  sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);

  boolean fMqttConnected = false;
  for (int i=0; i<3 && !fMqttConnected; i++) {
    Serial.print("Connecting to mqtt broker...");
    fMqttConnected = mqtt.connect(g_szDeviceId);
    if (fMqttConnected == false) {
      Serial.print(" fail, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }

  if (fMqttConnected)
  {
    Serial.println(" success");
    mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
    Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
    sendLux();
    sendTemp();
    sendHumidity();
  }
  return mqtt.connected();
}

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}


void OnTimer1Sec() {
  digitalWrite(LED_BUILTIN, HIGH);
  timerLedBuiltinOff.once_ms(50, []() {
    digitalWrite(LED_BUILTIN, LOW);
  });
}

volatile bool g_fUpdated = false;
IRAM_ATTR void isrPushButton()
{
  g_nCount++;
  g_fUpdated = true;
}

void OnReadSensor()
{
  float fHumidity = dht.getHumidity();
  float fTemperature = dht.getTemperature();
  float lux = lightMeter.readLightLevel();
  globalLux = lux;
  globalHumidity= fHumidity;
  globalTemp = fTemperature;
  Serial.printf("Humidity: %.2f %%, Temperature: %.2f°C, Light: %.2f\n", 
    fHumidity, fTemperature, lux);

  if (fHumidity > 80 && fTemperature > 28) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
    } else if (60 > fHumidity > 80 &&  fTemperature > 28) {
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, LOW);
    } else if (fHumidity < 60 &&  28 > fTemperature ) {
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
    } else {
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
    }

    if (lux > 400) {
      Serial.printf("Pintu terbuka\n");
    } else{
      Serial.printf("Pintu tertutup\n");
  }
}

void setup() {
  // init Leds
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);


  Serial.begin(9600);
  dht.setup(DHT_PIN, DHTesp::DHT11);
  Wire.begin();
  lightMeter.begin();

  timer1Sec.attach_ms(1000, OnTimer1Sec);
  timerReadSensor.attach_ms(2000, OnReadSensor);
  Serial.println("System ready");

  WifiConnect();
  mqttConnect();
  timerPublish.attach_ms(4000, sendLux);
  timerPublish.attach_ms(5000, sendTemp);
  timerPublish.attach_ms(6000, sendHumidity);
}

int g_nLastCount = 0;
void loop() {
  if (g_fUpdated) {
    g_fUpdated = false;
    Serial.printf("Count: %d\n", g_nCount);
    if (g_nCount - g_nLastCount>1) {
      Serial.printf("Bounching detected! Count: %d, LastCount: %d\n", g_nCount, g_nLastCount);
      digitalWrite(LED_RED, HIGH);
      timerLedRedOff.once_ms(50, []() {
        digitalWrite(LED_RED, LOW);
      });
    }
    g_nLastCount = g_nCount;

  }
}

