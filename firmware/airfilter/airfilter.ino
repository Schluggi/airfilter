#include <EspMQTTClient.h>

/****************** CONFIG START *******************/
/* Wifi Settings */
static String hostname = "Livingroom";    //  Unique name for ESP32. The name detected by your router and MQTT.
static const char* ssid = "MyWifi";       //  WIFI SSID.
static const char* password = "12345678"; //  WIFI password.

/* MQTT Settings */
static const char* mqttServerIp = "192.168.1.111";  //  MQTT Broker server ip
static const char* mqttUsername = "";               //  MQTT Broker username. If empty or NULL, no authentication will be used
static const char* mqttPassword = "";               //  MQTT Broker password
static const short mqttServerPort = 1883;           //  MQTT Port
static String mqttMainTopic = "airfilter";          //  MQTT main topic

/* MQTT Settings - Home Assistant */
static bool homeAssistantMqttDiscovery = true;            // Enable to publish Home Assistant MQTT Discovery config
static String homeAssistantMqttPrefix = "homeassistant";  // MQTT Home Assistant prefix

/* Pin Settings */
const int inputPinLevelOff = 26;
const int inputPinLevelOne = 25;
const int inputPinLevelTwo = 33;
const int inputPinLevelThree = 32; 

const int outputPinLevelOff = 13;
const int outputPinLevelOne = 5;
const int outputPinLevelTwo = 14;
const int outputPinLevelThree = 27;
/****************** CONFIG END *******************/

static String mqttTopic = mqttMainTopic + "/" + hostname;
static String lastWillTopic = mqttTopic + "/lastWill";
static String stateTopic = mqttTopic + "/state";
static String commandTopic = mqttTopic + "/set";
static String presetStateTopic = mqttTopic + "/preset/state";
static String presetModeStateTopic = mqttTopic + "/preset/set";


static long lastOnlinePublished = 0;

int currentLevel = 0;
int lastLevel = 0;


static EspMQTTClient client(
  ssid,
  password,
  mqttServerIp,
  (mqttUsername == NULL || strlen(mqttUsername)<1) ? NULL : mqttUsername,                    
  (mqttUsername == NULL || strlen(mqttUsername)<1) ? NULL : mqttPassword,
  hostname.c_str(),
  mqttServerPort
);


int getLevel(){
  if ( digitalRead(inputPinLevelOne) && digitalRead(inputPinLevelTwo) && digitalRead(inputPinLevelThree) && !digitalRead(inputPinLevelOff)){
    return 0;
  }
  else if ( !digitalRead(inputPinLevelOne) && digitalRead(inputPinLevelTwo) && digitalRead(inputPinLevelThree) && digitalRead(inputPinLevelOff)){
    return 1;
  }
  else if ( digitalRead(inputPinLevelOne) && !digitalRead(inputPinLevelTwo) && digitalRead(inputPinLevelThree) && digitalRead(inputPinLevelOff)){
    return 2;
  }
  else if ( digitalRead(inputPinLevelOne) && digitalRead(inputPinLevelTwo) && !digitalRead(inputPinLevelThree) && digitalRead(inputPinLevelOff)){
    return 3;
  }

  return -1;
}

void setLevel(int level){
  Serial.println("setting level to: " + String(level));

  if (level) {
    Serial.println("on");
    client.publish(stateTopic.c_str(), "on", true);
    client.publish(presetModeStateTopic.c_str(), String(level), true);
    lastLevel = level;
  } else {
    Serial.println("off");
    client.publish(stateTopic.c_str(), "off", true);
  }
  
  
  switch (level) {
    case 0:
      digitalWrite(outputPinLevelOff, HIGH);
      digitalWrite(outputPinLevelOne, LOW);
      digitalWrite(outputPinLevelTwo, LOW);
      digitalWrite(outputPinLevelThree, LOW);
      break;
    case 1:
      digitalWrite(outputPinLevelOff, LOW);
      digitalWrite(outputPinLevelOne, HIGH);
      digitalWrite(outputPinLevelTwo, LOW);
      digitalWrite(outputPinLevelThree, LOW);
      break;
    case 2:
      digitalWrite(outputPinLevelOff, LOW);
      digitalWrite(outputPinLevelOne, LOW);
      digitalWrite(outputPinLevelTwo, HIGH);
      digitalWrite(outputPinLevelThree, LOW);
      break;
    case 3:
      digitalWrite(outputPinLevelOff, LOW);
      digitalWrite(outputPinLevelOne, LOW);
      digitalWrite(outputPinLevelTwo, LOW);
      digitalWrite(outputPinLevelThree, HIGH);
      break;      
  }
}


void onConnectionEstablished(){  
  // home assistnat discovery
  if (homeAssistantMqttDiscovery){
    String payload = "{\"name\":\"" + hostname + " Airfilter\",\"stat_t\":\"airfilter/" + hostname + "/state\",\"cmd_t\":\"airfilter/" + hostname + "/set\",\"pr_modes\":[\"1\",\"2\",\"3\"],\"pr_mode_cmd_t\":\"airfilter/" + hostname + "/preset/preset_mode\",\"pr_mode_stat_t\":\"airfilter/" + hostname + "/preset/preset_mode_state\",\"pl_on\":\"on\",\"pl_off\":\"off\"}";
    client.publish(homeAssistantMqttPrefix + "/fan/" + hostname + "/config", payload, true);
  }
  
  // presets
  client.subscribe(presetStateTopic.c_str(), [] (const String& payload){
    Serial.println("preset_mode received: " + payload);  
    setLevel(payload.toInt());
  });

  // on & off / state
  client.subscribe(commandTopic.c_str(), [] (const String& payload){
    Serial.println("command received: " + payload);  
    if (payload == "on"){
      setLevel(lastLevel);
    } else {
      setLevel(0);
    }
  });
}
  
void setup() {  
  Serial.begin(115200);
  Serial.println("Booted!"); 

  // setup mqtt
  client.setMqttReconnectionAttemptDelay(100);
  client.enableLastWillMessage(lastWillTopic.c_str(), "offline");
  client.setKeepAlive(60);
  client.setMaxPacketSize(1024);

  // setup output pins
  pinMode(outputPinLevelOff, OUTPUT);
  pinMode(outputPinLevelOne, OUTPUT);
  pinMode(outputPinLevelTwo, OUTPUT);
  pinMode(outputPinLevelThree, OUTPUT);

  // setup input pins
  pinMode(inputPinLevelOff, INPUT_PULLUP);
  pinMode(inputPinLevelOne, INPUT_PULLUP);
  pinMode(inputPinLevelTwo, INPUT_PULLUP);
  pinMode(inputPinLevelThree, INPUT_PULLUP);

  // set startup level
  setLevel(getLevel());
}

void loop() {
  // starting mqtt client
  client.loop();

  // update last will every 30s
  if ((millis() - lastOnlinePublished) > 30000) {
    if (client.isConnected()) {
      client.publish(lastWillTopic.c_str(), "online", true);
      lastOnlinePublished = millis();
    }
  }
  
  // check for knob overwrites
  int newLevel = getLevel();
  if (newLevel != currentLevel && newLevel != -1){
    currentLevel = newLevel;
    setLevel(newLevel);
  }
  
  delay(100);
}
