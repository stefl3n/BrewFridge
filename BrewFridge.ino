#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <IPAddress.h>
#include <ArduinoMqttClient.h>
#include <HttpClient.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "arduino_secrets.h"

// Data wire is conntec to the Arduino digital pin 4
#define ONE_WIRE_BUS 10

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Configuring two different WiFiClient: one for the MqttClient & one for HttpClient
// It is necessary because they need two different TCP Connections
WiFiClient mqttWifiClient;
WiFiClient httpWifiClient;
MqttClient mqttClient(mqttWifiClient);
HttpClient client = HttpClient(httpWifiClient, "192.168.1.180", 80);

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// these constants won't change.  But you can change the size of
// your LCD using them:
const int numRows = 2;
const int numCols = 16;

// MQTT ThingSpeak Configuration
const char broker[]    = "mqtt3.thingspeak.com";
int        port        = 1883;
const char * outTopic = "channels/2239528/publish";
bool retained = false;
int qos = 0;
bool dup = false;

// Temperatures variables
float MaxBeerTemperature = 22.00;
float fridgeTemperature;
float externalTemperature;

// Time variables
unsigned long instantFridgeOn = millis();
unsigned long timePassed;
unsigned long minTimeFridgeOn = 15000;

// Fridge Status
#define ON "1"
#define OFF "0"
char * frigo = OFF;

void setup() {
/* -------------------------------------------------------------------------- */  
  //Initialize serial and wait for port to open:
  Serial.begin(115200);

  // set up the LCD's number of columns and rows:
  lcd.begin(numCols, numRows);

  lcd.setCursor(0,0);
  lcd.write("Target T: ");

  lcd.setCursor(0,1);
  lcd.write("Actual T: ");
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
  
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network.
    status = WiFi.begin(ssid, pass);
     
    // wait 10 seconds for connection:
    delay(10000);
  }
  
  printWifiStatus();

  mqttClient.setId("KR8yEgonKT07KBAcIwUhMxU");
  mqttClient.setUsernamePassword("KR8yEgonKT07KBAcIwUhMxU", "U1dsMLlq8BP36UqZSeosl5ey");
  mqttClient.setCleanSession(true);
  
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  mqttClient.onMessage(onMqttMessage);
  
  mqttClient.subscribe("channels/2239528/subscribe/fields/field2");
  mqttClient.subscribe("channels/2239528/subscribe/fields/field4");
}

void loop() {
  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();

  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures();
  // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire
  fridgeTemperature = sensors.getTempCByIndex(0);
  externalTemperature = sensors.getTempCByIndex(1);
  
  Serial.print("Fridge temperature: ");
  Serial.print(fridgeTemperature);
  Serial.print("; External temperature: ");
  Serial.println(externalTemperature);

  lcd.setCursor(10, 0);  
  lcd.print(MaxBeerTemperature);
  lcd.setCursor(10,1);
  lcd.print(fridgeTemperature);

  if( fridgeTemperature > MaxBeerTemperature ){

    client.get("/cm?cmnd=Power%20On");
    // read the status code and body of the response
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);

    if(statusCode == 200) frigo = ON;

    instantFridgeOn = millis();

  }else{

    timePassed = millis() - instantFridgeOn;

    if ( timePassed > minTimeFridgeOn ){

      client.get("/cm?cmnd=Power%20Off");
      // read the status code and body of the response
      int statusCode = client.responseStatusCode();
      String response = client.responseBody();

      Serial.print("Status code: ");
      Serial.println(statusCode);
      Serial.print("Response: ");
      Serial.println(response);

      if(statusCode == 200) frigo = OFF;

    }  
  }

  String payload;

  payload = "field1=";
  payload += fridgeTemperature;
  payload += "&field3=";
  payload += frigo;
  payload += "&field5=";
  payload += externalTemperature;
  payload += "&status=MQTTPUBLISH";

  Serial.print("Sending message to topic: ");
  Serial.println(outTopic);
  Serial.println(payload); 

  mqttClient.beginMessage(outTopic, payload.length(), retained, qos, dup);
  mqttClient.print(payload);
  mqttClient.endMessage();

  Serial.println();

  delay(5000);
}

/* -------------------------------------------------------------------------- */
void printWifiStatus() {
/* -------------------------------------------------------------------------- */  
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

/* -------------------------------------------------------------------------- */
void onMqttMessage(int messageSize) {
/* -------------------------------------------------------------------------- */  
  // we received a message, print out the topic and contents

  String topic = mqttClient.messageTopic();
  String message = mqttClient.readString();

  Serial.print("Received a message with topic '");
  Serial.print(topic);
  Serial.print("', duplicate = ");
  Serial.print(mqttClient.messageDup() ? "true" : "false");
  Serial.print(", QoS = ");
  Serial.print(mqttClient.messageQoS());
  Serial.print(", retained = ");
  Serial.print(mqttClient.messageRetain() ? "true" : "false");
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  if(topic.equals("channels/2239528/subscribe/fields/field2")){
    MaxBeerTemperature = message.toFloat();

    Serial.print("New Beer Required Temperature: ");
    Serial.println(MaxBeerTemperature);
    Serial.println();
  }

  if(topic.equals("channels/2239528/subscribe/fields/field4")){
    int seconds = message.toInt();
    minTimeFridgeOn = (unsigned long) seconds * 1000;

    Serial.print("New MinTime Fridge ON: ");
    Serial.println(minTimeFridgeOn);
    Serial.println();
  }
}