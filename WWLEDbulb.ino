/*
   W. Hoogervorstm feb 2020, based on W. Hoogervorst Sonoff B1 firmware
   Broadlink LED bulb, designed for Magic home smart home
   Replaced Broadlink chip with ESP8285 chip
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <credentials.h>

// base values
const char* software_version = "version 3";
const char* devicename = "WWLEDbulb1";
const char* chip_type = "ESP8285";

// for HTTPupdate
char host[20];
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/*credentials & definitions */
//MQTT
char mqtt_id[20];
WiFiClient espClient;
PubSubClient client(espClient);
const char* status_topic_a = "/status";
const char* DIM_topic_a = "/DIM";
const char* scene1_topic_a = "/scene1";
const char* scene2_topic_a = "/scene2";

char status_topic[20], DIM_topic[20], scene1_topic[20], scene2_topic[20];

#define WHITEPIN 4

#define WIFI_CONNECT_TIMEOUT_S 15

int scene = 0;
boolean flasher = true;
boolean rising = true;

boolean updated = false;

// initialize led values
int WWLedValint = 80;  // start with WW light on as a normal bulb at 80 brightness

#define SERIAL_DEBUG 0

uint32_t time1;

void setup() {
  updateLED();
  // if serial is not initialized all following calls to serial end dead.
  if (SERIAL_DEBUG)
  {
    Serial.begin(115200);
    delay(10);
    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println(F("Started from reset"));
  }
  strcpy(mqtt_id, devicename);
  strcpy(host, devicename);

  strcpy(status_topic, devicename);
  strcpy(DIM_topic, devicename);
  strcpy(scene1_topic, devicename);
  strcpy(scene2_topic, devicename);

  strcat (status_topic, status_topic_a);
  strcat (DIM_topic, DIM_topic_a);
  strcat (scene1_topic, scene1_topic_a);
  strcat (scene2_topic, scene2_topic_a);

  WiFi.mode(WIFI_STA);

  setup_wifi();

  // for HTTPudate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  httpServer.on("/", handleRoot);
  httpServer.on("/power_on", handle_power_on);
  httpServer.on("/power_off", handle_power_off);
  httpServer.on("/dim10", handle_dim10);
  httpServer.on("/dim50", handle_dim50);
  httpServer.on("/dim100", handle_dim100);
  httpServer.on("/scene1", handle_scene1);
  httpServer.on("/scene2", handle_scene2);
  //httpServer.on("/scene3", handle_scene3);
  httpServer.onNotFound(handle_NotFound);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
}


void loop() {

  client.loop();
  httpServer.handleClient();    // for HTTPupdate
  delay(10);
  if (!client.connected())
  {
    reconnect();
  }

  switch (scene)
  {
    case 0: break;
    case 1:
      {
        scene1();
        break;
      }
    case 2:
      {
        scene2();
        break;
      }
  }

  if (updated)
    updateLED();
}

void scene1(void)
{
  if (flasher)
    WWLedValint = 100;
  else
    WWLedValint = 0;
  flasher = !flasher;
  updateLED();
  //Serial.print("WWLedValint: "); Serial.println(WWLedValint);
  delay(100);
}
void scene2(void)
{
  if (rising)
  {
    if (WWLedValint < 100)
      WWLedValint++;
    else rising = false;
  }
  else
  {
    if (WWLedValint > 0)
      WWLedValint--;
    else rising = true;
  }
  //Serial.print("WWLedValint: "); Serial.println(WWLedValint);
  updateLED();
  delay(1);
}

void callback(char* topic, byte * payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //payload2[i] = payload[i];
  }
  Serial.println();
  Serial.print("length: ");
  Serial.print(length);
  updated = true;
  String stringOne = (char*)payload;
  Serial.println();
  Serial.print("StringOne: ");
  Serial.println(stringOne);
  if ((char)topic[11] == 'D') // search for   WWLEDbulb1/DIM
  {
    if ((char)payload[1] == 'N') // check for 'N' in ON
      WWLedValint = 100;
    else if ((char)payload[1] == 'F') // check for 'F' in OFF
      WWLedValint = 0;
    else
    {
      String sbrightness = stringOne.substring(0, length);
      WWLedValint = sbrightness.toInt();
      scene = 0;
    }
  }
  else if ((char)topic[12] == 'c') // search for   WWLEDbulb1/scene#
  {
    scene = (char)topic[16] - '0';
  }

  Serial.print("WWLedValint: ");
  Serial.println(WWLedValint);
  Serial.print("scene: ");
  Serial.println(scene);

  //updateLED();
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(mySSID);
  WiFi.begin(mySSID, myPASSWORD);
  time1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() > time1 + (WIFI_CONNECT_TIMEOUT_S * 1000))
      ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  Serial.println("Attempting MQTT connection...");
  if (client.connect(host)) {
    Serial.println("connected");
    // ... and resubscribe
    client.subscribe(DIM_topic);
    client.subscribe(scene1_topic);
    client.subscribe(scene2_topic);
    client.publish(status_topic, "connected");
    char buf[5];
    sprintf(buf, "%ld", WWLedValint);
    client.publish(DIM_topic, buf);
  }
  Serial.println(client.connected());
  return client.connected();
}


void handleRoot() {
  Serial.println("Connected to client");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_power_off() {
  client.publish(DIM_topic, "0");
  WWLedValint = 0;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_power_on() {
  client.publish(DIM_topic, "100");
  WWLedValint = 100;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim100() {
  client.publish(DIM_topic, "100");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim50() {
  client.publish(DIM_topic, "50");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_dim10() {
  client.publish(DIM_topic, "10");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene1() {
  client.publish(scene1_topic, "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene2() {
  client.publish(scene2_topic, "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_scene3() {
  client.publish("LEDbulb1/scene3", "ON");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_NotFound() {
  httpServer.send(404, "text/plain", "Not found");
}


String SendHTML() {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>";
  ptr += host;
  ptr += "</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 25px auto 30px;} h3 {color: #444444;margin-bottom: 20px;}\n";
  ptr += ".button {width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px 10px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-red {background-color: #ff0000;}\n";
  ptr += ".button-red:active {background-color: #ea0000;}\n";
  ptr += ".button-green {background-color: #00ff00;}\n";
  ptr += ".button-green:active {background-color: #00ea00;}\n";
  ptr += ".button-blue {background-color: #0000ff;}\n";
  ptr += ".button-blue:active {background-color: #0000ea;}\n";
  ptr += ".button-dim {background-color: #0088ea;}\n";
  ptr += ".button-dim:active {background-color: #0072c4;}\n";
  ptr += ".button-pwr {background-color: #34495e;}\n";
  ptr += ".button-pwr:active {background-color: #2c3e50;}\n";
  ptr += ".button-update {background-color: #a32267;}\n";
  ptr += ".button-update:active {background-color: #fa00ff;}\n";
  ptr += ".button-scene {background-color: #d700db;}\n";
  ptr += ".button-scene:active {background-color: #961f5f;}\n";
  ptr += "p {font-size: 18px;color: #383535;margin-bottom: 15px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>LEDBulb1 Web Control</h1>\n";
  ptr += "<h3>Control of Lights and link to HTTPWebUpdate</h3>\n";
  ptr += "<p>WimIOT\nDevice: ";
  ptr += host;
  ptr += "<br>Software version: ";
  ptr += software_version;
  ptr += "<br>Chip type: ";
  ptr += chip_type;
  ptr += "<br><br></p>";

  ptr += "<a class=\"button button-pwr\" href=\"/power_on\">Power ON</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-pwr\" href=\"/power_off\">Power OFF</a><br><br><br>\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim10\">Dim 10%</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim50\">Dim 50%</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-dim\" href=\"/dim100\">Dim 100%</a><br><br><br><br>\n";
  ptr += "<p>Scene</p>";
  ptr += "<a class=\"button button-scene\" href=\"/scene1\">Scene 1</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-scene\" href=\"/scene2\">Scene 2</a>&nbsp;&nbsp;\n";
  //ptr += "<a class=\"button button-scene\" href=\"/scene3\">Scene 3</a><br><br><br>\n";
  //  ptr += "<br><br></p>\n";
  ptr += "<p>Click for update page</p><a class =\"button button-update\" href=\"/update\">Update</a>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void updateLED(void)
{
  analogWrite(WHITEPIN, WWLedValint * 10.23 * 0.8);   // I set it to 80% of its maximum power
  updated = false;
}
