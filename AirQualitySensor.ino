#include <Wire.h>
#include <SPI.h>                        // SPI lib
#include <Adafruit_BMP280.h>    // BMP lib
#include "SdsDustSensor.h"         // SDS lib
#include <ESP8266WiFi.h>           // Wifi lib
#include <ArduinoJson.h>            // JSON lib
#include <PubSubClient.h>           // MQTT lib
#include <ESP8266HTTPClient.h>  // HTTP lib

/************ WIFI and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
#define wifi_ssid "YOUR_SSID" //type your WIFI information inside the quotes
#define wifi_password "YOUR_PASSWORD"
#define mqtt_server "192.168.1.x"
#define mqtt_user "MQTT_USER" 
#define mqtt_password "MQTT_PASSWORD"
#define mqtt_port 1883
#define SENSORNAME "SDS011"

/************* MQTT TOPICS (change these topics as you wish)  **************************/
#define mqtt_topic "tele/SDS011/SENSOR"
#define mqtt_state "tele/SDS011/STATE"

/************ SDS Definition (CHANGE THESE FOR YOUR SETUP) *****************************/
float local_pressure = 1013;       //Change the value to your city current barrometric pressure (https://www.wunderground.com)
int rxPin = D7;
int txPin = D8;

/************ AQICN Definition https://aqicn.org/publishingdata/ (CHANGE THESE FOR YOUR SETUP) *****************************/
String aqicn_token = "1234567890azerty";
String aqicn_station = "station_id";
String aqicn_name = "station_name";
String latitude = "your_latitude";
String longitude = "your_longitude";

/************ Sensor.community Definition https://devices.sensor.community/ (CHANGE THESE FOR YOUR SETUP) *****************************/
String scom_sensorid = "esp8266-123456";

/************ Opensensemap Definition https://opensensemap.org/ (CHANGE THESE FOR YOUR SETUP WITH DATA YOU RECEIVE, if you have more sensors just adapt the code later on for extra POST) *****************************/
String osm_token = "1234567890azerty";
String SENSEBOX_ID = "1234567890azerty";
// Temperature
String SENSOR1_ID = "1234567890azerty";
// PM2.5
String SENSOR2_ID = "1234567890azerty";
// PM10
String SENSOR3_ID = "1234567890azerty";
// Pression
String SENSOR4_ID = "1234567890azerty";

// BMP Definition for device I2C address: 0x76 or 0x77 (0x77 is library default address)
#define BMP280_I2C_ADDRESS  0x76

// initialize library
Adafruit_BMP280  bmp280;
SdsDustSensor sds(rxPin, txPin);
WiFiClient espClient;
WiFiClient osmclient;
WiFiClientSecure httpsclient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);
  Serial.println("Starting Node named " + String(SENSORNAME));
  delay(600);
  sds.begin(); // this line will begin with given baud rate (9600 by default)
  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setQueryReportingMode().toString()); // ensures sensor is in 'query' reporting mode

  Wire.begin(D2, D1);  // set I2C pins [SDA = D2, SCL = D1], default clock is 100kHz
  if( bmp280.begin(BMP280_I2C_ADDRESS) == 0 ) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or try a different address!"));
  }

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  if (!client.connected()) {
    reconnect();
  }
}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_state, "SDS Sensor Connected");
    } else {
      Serial.print("failed");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void postOSM (String measurement, String sensorId) {
  //Json creation
  Serial.println("Sending Air Quality values to OSeM");
  String osm_json = "{\"value\":";
  osm_json += measurement;
  osm_json += "}";
  String osm_url = "https://ingress.opensensemap.org/boxes/" + SENSEBOX_ID + "/" + sensorId;
  httpsclient.setInsecure();
  HTTPClient osm;
  // Send request
  osm.begin(httpsclient, osm_url);
  osm.addHeader("Content-Type", "application/json");
  osm.addHeader("Authorization", osm_token);
  osm.POST(osm_json);
  // Read response
  Serial.println(osm.getString());
  // Disconnect
  osm.end();
}

void loop() {
  client.loop();
  Serial.println("Please wait... Analysing air quality for 30 seconds");
  sds.wakeup();
  delay(30000); // working 30 seconds

  PmResult pm = sds.queryPm();
  if (pm.isOk()) {
    Serial.print("PM2.5 = ");
    Serial.print(pm.pm25);
    Serial.print(", PM10 = ");
    Serial.println(pm.pm10);
  } else {
    Serial.print("Could not read values from sensor, reason: ");
    Serial.println(pm.statusToString());
  }
  // read temperature and pressure from BMP280 sensor
  float temp     = bmp280.readTemperature();              // get temperature
  float pressure = bmp280.readPressure()/100;             // get pressure
  int altitude   = bmp280.readAltitude (local_pressure);  // get altitude relative
  
  Serial.print(F("Temperature = "));
  Serial.print(temp);
  Serial.println(" °C");
  Serial.print(F("Pressure = "));
  Serial.print(pressure);
  Serial.println(" hPa");
  Serial.print(F("Altitude = "));
  Serial.print(altitude);
  Serial.println(" m");
  
  // Json preparation & publish to MQTT
  String Jstring = "{\"BMP280\":{\"Temperature\":" + (String)temp + ",\"Pressure\":" + (String)pressure + ",\"Altitude\":" + (String)altitude + "},\"SDS0X1\":{\"PM25\":" + (String)pm.pm25 + ",\"PM10\":" + pm.pm10 + "},\"PressureUnit\":\"hPa\",\"TempUnit\":\"°C\",\"AltUnit\":\"m\"}";
  unsigned int Jsize = Jstring.length()+1;
  char JsonToSend[Jsize];
  Jstring.toCharArray(JsonToSend,Jsize);
  if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
    client.publish(mqtt_topic, JsonToSend);
    Serial.println(JsonToSend);
  } else {
    Serial.println("Problem with MQTT connection");
    reconnect();
  }
  
  // Json preparation & publish to aqicn
  Serial.println("Sending Air Quality values to aqicn");
  String aqicn_json = "{\"token\":\"" +  aqicn_token + "\",\"station\":{\"id\":\"" + aqicn_station + "\",\"name\":\"" + aqicn_name + "\",\"latitude\":" + latitude + ",\"longitude\":" + longitude + "},\"readings\":[{\"specie\":\"pm2.5\",\"value\":" + (String)pm.pm25 + "},{\"specie\":\"pm10\",\"value\":" + (String)pm.pm10 + "}]}";
  httpsclient.setInsecure();
  HTTPClient aqicn;
  // Send request
  aqicn.begin(httpsclient, "https://aqicn.org/sensor/upload/");
  aqicn.addHeader("Content-Type", "application/x-www-form-urlencoded");
  aqicn.POST(aqicn_json);
  // Read response
  Serial.println(aqicn.getString());
  // Disconnect
  aqicn.end();

// Json preparation & publish to https://sensor.community/en/  - I have problem to push pressure, will fix that some day !
  Serial.println("Sending Air Quality values to sensor.community");
  String scom_json = "{\"software_version\": \"1.0\", \"sensordatavalues\":[{\"value_type\":\"P1\",\"value\":\"" + (String)pm.pm10 + "\"}, {\"value_type\":\"P2\",\"value\":\"" + (String)pm.pm25 + "\"}, {\"value_type\":\"temperature\",\"value\":\"" + (String)temp + "\"}]}";
  httpsclient.setInsecure();
  HTTPClient scom;
  // Send request
  scom.begin(httpsclient, "https://api.sensor.community/v1/push-sensor-data/");
  scom.addHeader("Content-Type", "application/json");
  scom.addHeader("X-Sensor", scom_sensorid);
  scom.addHeader("X-Pin", "1");
  scom.POST(scom_json);
  // Read response
  Serial.println(scom.getString());
  // Disconnect
  scom.end();

  // Publish to openSenseMap
  postOSM((String)temp, SENSOR1_ID);
  postOSM((String)pressure, SENSOR4_ID);
  postOSM((String)pm.pm25, SENSOR2_ID);
  postOSM((String)pm.pm10, SENSOR3_ID);

  WorkingStateResult state = sds.sleep();
  if (state.isWorking()) {
    Serial.println("Problem with sleeping the sensor.");
  } else {
    Serial.println("Sensor is sleeping");
    Serial.println();
    delay(300000); // wait 5 minute
  }
}