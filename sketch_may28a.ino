/*................... Program.........................*/


#define BLYNK_TEMPLATE_ID "TMPL3sPNX6wD5"
#define BLYNK_TEMPLATE_NAME "PowerSage"
#define BLYNK_AUTH_TOKEN "OXcFY6Rik8maskGfBjBW9hy6lHXbwlnF"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// WiFi Credentials
char ssid[] = "vivo 1606";
char pass[] = "vivo 1606";

// Hardware Pins
const int pirPin = 12;     // PIR Sensor
const int relayPin = 4;    // Relay Control

// Blynk Virtual Pins
#define V_BUTTON V0        // Manual control
#define V_STATUS V1        // State indicator
#define V_NOTIFY V2        // Notification trigger
#define V_RESPONSE V3      // Response button (1=Yes, 0=No)
#define V_TIMER V4         // Timer display

// Timing Constants
const unsigned long RELAY_ACTIVE_TIME = 10000;  // 10s until notification
const unsigned long RESPONSE_TIMEOUT = 20000;    // 20s to respond

// State Variables
bool relayState = LOW;
bool lastPirState = LOW;
unsigned long relayStartTime = 0;
bool notificationSent = false;
bool waitingForResponse = false;
bool manualMode = false;
bool userResponded = false;
bool disableAutoCutoff = false;

BLYNK_WRITE(V_BUTTON) {
  bool newState = param.asInt();
  if (newState != relayState) {
    relayState = newState;
    digitalWrite(relayPin, relayState);
    Blynk.virtualWrite(V_STATUS, relayState);
    
    if (relayState) {
      manualMode = true;
      waitingForResponse = false;
      disableAutoCutoff = true;
      Serial.println("Manually ON: Auto-cutoff disabled");
    } else {
      manualMode = false;
      disableAutoCutoff = false;
    }
  }
}

BLYNK_WRITE(V_RESPONSE) {
  int response = param.asInt();
  Serial.printf("Received response: %d\n", response);
  
  if (waitingForResponse) {
    userResponded = true;
    waitingForResponse = false;
    
    if (response == 0) { // YES - Turn OFF
      digitalWrite(relayPin, LOW);
      relayState = LOW;
      Blynk.virtualWrite(V_BUTTON, LOW);  // Sync button state
      Blynk.virtualWrite(V_STATUS, LOW);
      disableAutoCutoff = false;
      Serial.println("User choose: Power OFF");
    } 
    else if (response == 1) { // NO - Keep ON
      disableAutoCutoff = true;
      Serial.println("User choose: Power stays ON");
    }
  }
}

void sendNotification() {
  Blynk.virtualWrite(V_NOTIFY, 1);
  Blynk.logEvent("notification", "Power ON! \tSend 1 for Stay ON and 0 for OFF.");
  Serial.println("Notification sent: Notification send to the User and waiting for response...");
  waitingForResponse = true;
  userResponded = false;
}

void autoCutoff() {
  digitalWrite(relayPin, LOW);
  relayState = LOW;
  Blynk.virtualWrite(V_BUTTON, LOW);  // Critical fix: Sync button state
  Blynk.virtualWrite(V_STATUS, LOW);
  Serial.println("Auto-OFF: No response received");
  waitingForResponse = false;
}

void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Blynk.syncVirtual(V_BUTTON); // Initial sync
  Serial.println("System Ready");
}

void loop() {
  Blynk.run();
  bool currentPirState = digitalRead(pirPin);

  // PIR Detection (only in auto-mode)
  if (!manualMode && currentPirState == HIGH && lastPirState == LOW) {
    relayState = HIGH;
    digitalWrite(relayPin, HIGH);
    Blynk.virtualWrite(V_BUTTON, HIGH);
    Blynk.virtualWrite(V_STATUS, HIGH);
    relayStartTime = millis();
    notificationSent = false;
    waitingForResponse = false;
    disableAutoCutoff = false;
    Serial.println("Motion Detected: Power ON");
  }

  // Auto-timers (only when auto-cutoff is enabled)
  if (!manualMode && relayState && !disableAutoCutoff) {
    if (!notificationSent && (millis() - relayStartTime >= RELAY_ACTIVE_TIME)) {
      sendNotification();
      notificationSent = true;
    }

    if (waitingForResponse && !userResponded && 
        (millis() - relayStartTime >= RESPONSE_TIMEOUT)) {
      autoCutoff();
    }
  }

  lastPirState = currentPirState;
  delay(100);
}