#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

#define LED D4
#define PWR D2
#define PINA D5 //4051
#define PINB D6
#define PINC D7
#define INPUTPIN A0

DHTesp dht;

// Update these with values suitable for your network.
const char *ssid = "********";
const char *password = "********";
const char *mqtt_server = "********";

WiFiClient espClient;
PubSubClient client(espClient);

// 4051
int inputval[3] = {0, 0, 0};

//Voltage
//  GND > R2 > MP > R1 > VCC

const float R1 = 470000;
const float R2 = 100000;

float resistorFactor = 1023 * (R2 / (R1 + R2));

//Data
typedef struct
{
  float temp;
  float hum;
  float heatindex;
  int soil;
  float solar;
  int power;
  float vcc;
  int storage;
} datastruct;

datastruct data;

//Proto
void setup_wifi();
void get_data();
void publish_data();
void reconnect();

void setup()
{
  //INI
  pinMode(PWR, OUTPUT);
  digitalWrite(PWR, HIGH);

  pinMode(PINA, OUTPUT);
  pinMode(PINB, OUTPUT);
  pinMode(PINC, OUTPUT);
  pinMode(INPUTPIN, INPUT);
  digitalWrite(PINA, LOW);
  digitalWrite(PINB, LOW);
  digitalWrite(PINC, LOW);

  //Serial
  Serial.begin(9600);
  delay(10);
  Serial.println("");
  Serial.println("Waether-Station");

  //Wifi
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  dht.setup(05, DHTesp::DHT22); //GPIO 05
}

void loop()
{
  delay(250);

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  get_data();
  publish_data();

  delay(500);

  //DeepSleep
  if (data.vcc >= 3.50)
  {
    Serial.println("DeepSleep 15min");
    ESP.deepSleep(600e6);
  }
  else if (data.vcc <= 3.49 && data.vcc >= 3.20)
  {
    Serial.println("DeepSleep 30min");
    ESP.deepSleep(1800e6);
  }
  else
  {
    Serial.println("DeepSleep 1h");
    ESP.deepSleep(3600e6);
  }
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting connection...");
    // Attempt to connect
    if (client.connect("WeatherClient_1"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

int readMux(int channel)
{
  int controlPin[] = {PINC, PINB, PINA};

  int muxChannel[8][3] = {
      {0, 0, 0}, //channel 0
      {0, 0, 1}, //channel 1
      {0, 1, 0}, //channel 2
      {0, 1, 1}, //channel 3
      {1, 0, 0}, //channel 4
      {1, 0, 1}, //channel 5
      {1, 1, 0}, //channel 6
      {1, 1, 1}  //channel 7
  };

  //loop through the 3 sig
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  }

  //read the value at the SIG pin
  int val = analogRead(INPUTPIN);

  //return the value
  return val;
}

void get_data()
{
  //DHT
  delay(dht.getMinimumSamplingPeriod());
  data.hum = dht.getHumidity();
  data.temp = dht.getTemperature();
  data.heatindex = dht.computeHeatIndex(data.temp, data.hum, false);

  if (strcmp(dht.getStatusString(), "OK") == 1)
  {
    data.hum = dht.getHumidity();
    data.temp = dht.getTemperature();
    data.heatindex = dht.computeHeatIndex(data.temp, data.hum, false);
  }

  //4051
  data.soil = map(constrain(readMux(0), 440, 850), 850, 440, 0, 100); // Feuchte in %
  data.solar = (readMux(1) / resistorFactor) - 0.13;
  data.solar = constrain(data.solar, 0, 10);
  data.power = constrain(map(readMux(1), 0, 1024, 0, 100), 0, 100); //Gen Leistung in %
  data.vcc = constrain(((readMux(2) / resistorFactor) - 0.15), 0, 10);
  data.storage = map(constrain(readMux(2), 556, 780), 556, 780, 0, 100); //Ladung in %

  Serial.println("Feuchte: " + String(data.soil) + "%");
  Serial.println("Spannung Solar: " + String(data.solar));
  Serial.println("Leistung: " + String(data.power) + "%");
  Serial.println("Spannung VCC: " + String(data.vcc));
  Serial.println("Ladung: " + String(data.storage) + "%");
}

void publish_data()
{
  client.publish("weather/temp", String(data.temp).c_str(), true);
  client.publish("weather/humidity", String(data.hum).c_str(), true);
  client.publish("weather/heatindex", String(data.heatindex).c_str(), true);

  client.publish("weather/soil", String(data.soil).c_str(), true);

  client.publish("weather/solar", String(data.solar).c_str(), true);
  client.publish("weather/vcc", String(data.vcc).c_str(), true);
  client.publish("weather/power", String(data.power).c_str(), true);
  client.publish("weather/storage", String(data.storage).c_str(), true);
}