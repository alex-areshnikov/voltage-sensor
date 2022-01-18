#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include <PubSubClient.h>

#include <ArduinoOTA.h>

#include <Config.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

float voltages[6];
float totalVoltage;


float r1s[] = {0.0, 979, 1495, 23500, 35500, 35500};
float r2s[] = {1.0, 978, 669, 7370, 7400, 7400};

//2.31 4.62 6.92 9.26 11.49 13.81
//2.31 2.31 2.74 2.30 2.91 2.64

float boardVoltage = 3.16;

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      //mqttClient.subscribe(MQTT_HOLODILNIC_FAN_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {  
  // Do nothing
}

void processMqtt() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  
  mqttClient.loop();
}

void initializeBoard() {
  Serial.begin(115200);  

  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
}

void initializeSensor() {

} 

void initializeWifi() {
  delay(10);
    
  Serial.print("\nConnecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);  // station mode
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.print("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeMqtt() {
  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  mqttClient.setCallback(mqttCallback); 
}

void initializeOTA() {
  Serial.print("Initialize OTA... ");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  // ArduinoOTA.setPassword(OTA_PWD);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
}

void reportSensorValue() {
  char buffer[64];
  sprintf(buffer, "{\"total\":%.2f,\"s1\":%.2f,\"s2\":%.2f,\"s3\":%.2f,\"s4\":%.2f,\"s5\":%.2f,\"s6\":%.2f}", totalVoltage, voltages[0], voltages[1], voltages[2], voltages[3], voltages[4], voltages[5]);
  mqttClient.publish(MQTT_VOLTAGE_TOPIC, buffer);
  mqttClient.loop();
}

void processOTA() {
  ArduinoOTA.handle();
}

void channel(int number) {
  digitalWrite(D0, bitRead(number, 0));
  digitalWrite(D1, bitRead(number, 1));
  digitalWrite(D2, bitRead(number, 2));
  digitalWrite(D3, bitRead(number, 3));
}

float calcVoltageDivider(float vout, int index) {
  float r1 = r1s[index];
  float r2 = r2s[index];

  return(vout * ((r1 + r2) / r2));
}

void calcIndividualVoltages() {
  for(int index=5; index >= 0; index--) {
    voltages[index+1] = voltages[index+1] - voltages[index];
  }
}

void processSensor() {
  for(int index=0; index<6; index++) {
    channel(index);
    delay(50);
    voltages[index] = analogRead(A0);
    voltages[index] = ((boardVoltage * voltages[index]) / 1024);
    voltages[index] = calcVoltageDivider(voltages[index], index);
  }

  totalVoltage = voltages[5];
  //calcIndividualVoltages();
}
 
void setup() {
  initializeBoard();
  initializeSensor();
    
  initializeWifi();
  initializeMqtt(); 
  
  initializeOTA();
}

void loop() {
  // if(counter++ == 9) {
    processSensor();
    reportSensorValue();
    processMqtt();

  //   counter = 0;
  // }

  processOTA();

  delay(1000);
}