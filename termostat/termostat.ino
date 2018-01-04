/*

  $ mosquitto_pub -h 192.168.1.25 -i testPublish -t inTopic -m '0'
  $ mosquitto_pub -h 192.168.1.25 -i testPublish -t inTopic -m '1'

  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the RELAY
    else switch it off

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.


*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>  // Include Wire if you're using I2C
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ########### WIFI + mqtt

const char* ssid = "rudego";
const char* password = "changeMe";
const char* mqtt_server = "192.168.1.25";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[64], msg2[64];
int value = 0;
int tempzadana;
String msgstring;

// ########### TEMP
#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);

char str_humidity[10], str_temperature[10];  // Rounded sensor values and as strings

// ########### OLED - using I2C on D1 and D2
#define OLED_RESET 255  // no
Adafruit_SSD1306 display(OLED_RESET);

// ########### RELAY - ne to reroute the shield as OLED uses default D1
#define RELAYPIN D7

// ########### PIR
#define PIRPIN D6
bool oldInputState;

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // init temp sensor
  dht.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.display();
  display.clearDisplay();
  // configure pin for relay as output
  pinMode(RELAYPIN, OUTPUT);
  // configure pin for PIR
  pinMode(PIRPIN, INPUT);
  oldInputState = !digitalRead(PIRPIN);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {

//  snprintf(msg2, length + 4, "p:%s", (char*) payload);
//  sprintf(msg, "INBOUND\nto:%s\n%s", topic, msg2);
//  write2oled(msg);

  msgstring = (char*) payload;

if(strcmp(topic, "intemp") == 0) {
  tempzadana=msgstring.toInt();
  sprintf(msg, "zadana: %d;", tempzadana);
  write2oled(msg);
}
/*
  Serial.printf("Message arrived [%s]: ", topic);
  snprintf(msg2, length + 1, "%s.", (char*) payload);
  Serial.println(msg2);
*/
  // Switch on the LED if an 1 was received as first character
/*
  if ((char)payload[0] == '1') {
    digitalWrite(RELAYPIN, LOW);
    Serial.println("Relay Low");

  } else {
    digitalWrite(RELAYPIN, HIGH);  // Turn the LED off by making the voltage HIGH
    Serial.println("Relay High");

  }
  */
  delay(500);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
      client.subscribe("intemp");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int inputState = digitalRead(PIRPIN);;

  if (inputState != oldInputState)
  {
    //    sendInputState(inputState);
    if (inputState) {
      write2oled("MOVE!");
      client.publish("outTopic", "movment detected");
    }
    else {
      write2oled("silence...");
      client.publish("outTopic", "silence detected");
    }
    oldInputState = inputState;
  }

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    /*    snprintf (msg, 75, "hello world #%ld", value);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("outTopic", msg);
    */
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) ) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    dtostrf(h, 1, 2, str_humidity);
    dtostrf(t, 1, 2, str_temperature);

    sprintf (msg, "T: %s; H: %s.", str_temperature, str_humidity);
    Serial.print("Publish temp: ");
    Serial.println(msg);
    client.publish("outTopic", msg);

    sprintf (msg, "T: %sC\nH: %s%%", str_temperature, str_humidity);
    write2oled(msg);
    if(t<(tempzadana-2)){
      write2oled("Bede grzal");
      digitalWrite(RELAYPIN, HIGH);
    }
    if(t>=tempzadana){
      write2oled("wylaczam grzanie");
      digitalWrite(RELAYPIN, LOW);
    }

  }
}

void write2oled(char* stline) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(stline);
  display.display();
  delay(500);

  /*  if (strlen(stline) > 10 || strlen(ndline) > 10) {
      delay(500);
      display.startscrollleft(0x00, 0x0F);
      delay(1000);
      display.stopscroll();
      delay(500);
      display.startscrollright(0x00, 0x0F);
      delay(1000);
      display.stopscroll();
      delay(500);
    }*/
}

