#include <WiFi.h>
#include <ESP32Servo.h> // WROOM 32 in my case
#include <WebServer.h>

// turn HS100 on and off respectively
const char* ON = "{\"system\":{\"set_relay_state\":{\"state\":1}}}";
const char* OFF = "{\"system\":{\"set_relay_state\":{\"state\":0}}}";

// wifi my HS100 is connected to (change)
const char* ssid = "WIFI";
const char* password = "pass";

const char* HS100_IP = "192.168.12.127"; // my HS100 IP
const int HS100_PORT = 9999; // default for tplink shit

WebServer server(80);

Servo servoOn;
Servo servoOff;
int servoOnPin = 18; // change pins as necessary
int servoOffPin = 19;


// logic can be found here thanks to jkbenaim
// https://github.com/jkbenaim/hs100/blob/master/comms.c
void xor_encrypt(uint8_t* dest, const char* src, size_t len) {
  uint8_t key = 0xAB; // key for HS100
  for (size_t i = 0; i < len; i++) {
    dest[i] = key ^ src[i];
    key = dest[i];
  }
}

// encode the command (on or off)
uint8_t* req_encode(size_t* outlen, const char* msg) {
  size_t msg_len = strlen(msg);
  *outlen = msg_len + 4; // add 4 byte length header
  uint8_t* encoded = (uint8_t*)calloc(*outlen, 1);

  uint32_t msg_len_be = htonl(msg_len);
  memcpy(encoded, &msg_len_be, 4);

  xor_encrypt(encoded + 4, msg, msg_len);

  return encoded;
}

// function to send ON/OFF to tplink smart-plug
void fanControl(WiFiClient client, const char* STATE) {
  size_t encodedLen;
  uint8_t* payload = req_encode(&encodedLen, STATE);

  if (payload) { // in case pointer is null for some reason
    Serial.print("[+] Sending encoded payload, length: ");
    Serial.println(encodedLen);
    client.write(payload, encodedLen);
    free(payload);
  }
}

void handleControl() {
  // Example:
  // $ curl http://192.168.12.127/?state=on
  String lightState = server.arg("state");

  WiFiClient client;
  if (!client.connect(HS100_IP, HS100_PORT)) {
    Serial.println("[-] Connection to HS100 failed!");
    server.send(500, "text/plain", "Connection failed...");
    return;
  }
  delay(100);
  
  if (state == "on") {
    Serial.println("Turning light ON");
    servoOn.write(20); // adjust turn as necessary
    delay(500);
    servoOn.write(0); // reset position
    server.send(200, "text/plain", "Light is ON");
    // turn off fan since im up and runnin
    fanControl(client, OFF);
  } else if (state == "off") {
    Serial.println("Turning light OFF");
    servoOff.write(20);
    delay(500);
    servoOff.write(0);
    server.send(200, "text/plain", "Light is OFF");

    // turn ON my fan when i turn off my light
    // this is so I don't have to turn them both on/off manually 
    // when going to bed...
    fanControl(client, ON); // nice and cold baby
  }
  
  client.stop();
  delay(100);
}

void setup() {
  Serial.begin(9600);

  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("Server IP: ");
  Serial.println(WiFi.localIP());

  servoOn.attach(servoOnPin);
  servoOff.attach(servoOffPin);

  server.on("/", HTTP_GET, handleControl);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
