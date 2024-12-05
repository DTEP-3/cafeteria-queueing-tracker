#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SharpIR.h>

#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <WiFiNINA.h>

bool isConnected = false;
int lastTriggerTime = 0;
int analogPinA0 = A0;
int analogPinA4 = A4;
int count = 0;
const int timeout = 1000;
int counterForSensor1 = 0;
int counterForSensor2 = 0;
 
unsigned long lastAttemptTime = 0;
const unsigned long reconnectInterval = 10000;

SharpIR sharp1(SharpIR::GP2Y0A02YK0F, analogPinA0);
SharpIR sharp2(SharpIR::GP2Y0A02YK0F, analogPinA4);

enum State {
  NONE,
  ENTRANCE,
  EXIT
};

State currentState = NONE;
LiquidCrystal_I2C lcd(0x27, 16, 2);

char ssid[] = "";  // WiFi SSID
char pass[] = "";  // WiFi password
int status = WL_IDLE_STATUS;

char serverAddress[] = "";  // server address
int port = 443;
 
WiFiSSLClient sslClient;  

void setup() {
  Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Occupancy:");
  lcd.setCursor(0, 1);
  lcd.print(count);

   //Initialize serial and wait for port to open:
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(7000);
  }

  connectToServer();  // Attempt initial connection
}


void connectToServer() {
  // Only attempt to connect if enough time has passed since the last attempt
  if (!isConnected && (millis() - lastAttemptTime >= reconnectInterval)) {
    Serial.println("Attempting to connect to server...");
    sslClient.stop();       // Ensure previous connection is cleared
    sslClient.setTimeout(10000);  // Set SSL timeout to avoid frequent retries
    lastAttemptTime = millis();   // Update last attempt time

    if (sslClient.connect(serverAddress, port)) {
      Serial.println("Connected to server");
      isConnected = true;
    } else {
      Serial.println("Connection failed");
      isConnected = false; // Ensure flag is set if connection fails
    }
  }
  
}
 
void sendCounterToServer() {
  if (sslClient.connected())  {
    Serial.println("Connected to server, sending data...");
    Serial.println("counter=" + String(count));
    String postData = "counter=" + String(count);
    sslClient.println("POST / HTTP/1.1"); // enter the server endpoint after '/'
    sslClient.println("Host: " + String(serverAddress));
    sslClient.println("Content-Type: application/x-www-form-urlencoded");
    sslClient.println("Content-Length: " + String(postData.length()));
    //  sslClient.println("Connection: close"); // Don't close the connection as the connecting to the server blocks the PINS of the sensors
    sslClient.println();
    sslClient.println(postData);
    isConnected = false;  // Reset connection flag after sending data
  } else {
    sslClient.connect(serverAddress, port);
  }
}

bool sensorIsOccupied(SharpIR sensor) {
  return sensor.getDistance() < 30;
}

void changeOccupancyNumber() {
  switch (currentState) {
    case NONE:
      if (sensorIsOccupied(sharp1)) {                                               // Sensor 1 triggered first (possible entrance)
        counterForSensor1++;
        currentState = ENTRANCE;
      } else if (sensorIsOccupied(sharp2)) {                                        // Sensor 2 triggered first (possible exit)
        counterForSensor2++;
        currentState = EXIT;
      }
      lastTriggerTime = currentTime;
      break;

    case ENTRANCE:
      if (sensorIsOccupied(sharp1)) {
        counterForSensor1++;
      }
      if (sensorIsOccupied(sharp2) && counterForSensor1 > 1 && (currentTime - lastTriggerTime < timeout)) {  // Sensor 2 triggered within timeout
        count++;

        lcd.setCursor(0, 1);
        lcd.print("                ");                                    // Clear previous count
        lcd.setCursor(0, 1);
        lcd.print(count);
        
        currentState = NONE;
        counterForSensor1 = 0;
        counterForSensor2 = 0;
															   
        sendCounterToServer();
        delay(500);                                                       // Delay to avoid double-counting
      } else if (currentTime - lastTriggerTime >= timeout) {              // Sensor 2 triggered AFTER timeout
        currentState = NONE;
        counterForSensor1 = 0;
        counterForSensor2 = 0;
      }
      break;

    case EXIT:
      if (sensorIsOccupied(sharp2)) {
        counterForSensor2++;
      }
      if (sensorIsOccupied(sharp1) && counterForSensor2 > 1 && (currentTime - lastTriggerTime < timeout)) {  // Sensor 1 triggered within timeout
        count = max(0, count - 1);   

        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(count);
        
        currentState = NONE;
        counterForSensor1 = 0;
        counterForSensor2 = 0;
																	   
        sendCounterToServer();
        delay(500);
      } else if (currentTime - lastTriggerTime >= timeout) {
        counterForSensor1 = 0;
        counterForSensor2 = 0;
        currentState = NONE;
      }
      break;
  }
}

void resetCounters() {
  if (currentTime - lastTriggerTime > timeout) {
    currentState = NONE;
    counterForSensor1 = 0;
    counterForSensor2 = 0;
  }
}

void attemptConnectionIfDisconnected() {
  if (!sslClient.connected()) {
    isConnected = false;
    connectToServer();
  }
}

void loop() { 
  int currentTime = millis();

  changeOccupancyNumber();

  resetCounters();
  
  delay(120);

  attemptConnectionIfDisconnected();
}

