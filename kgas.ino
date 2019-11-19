/*
 Setup your scale and start the sketch WITHOUT a weight on the scale
 Once readings are displayed place the weight on the scale
 Press +/- or a/z to adjust the calibration_factor until the output readings match the known weight
 Arduino pin 5 -> HX711 CLK
 Arduino pin 6 -> HX711 DOUT
 Arduino pin 5V -> HX711 VCC
 Arduino pin GND -> HX711 GND 
*/

#include "HX711.h"
#include <WiFi.h>
#include <HTTPClient.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID           "ffe0" // UART service UUID
#define CHARACTERISTIC_UUID_RX "ffe1"
#define CHARACTERISTIC_UUID_TX "ffe1"

const int LOADCELL_DOUT_PIN = 12;
const int LOADCELL_SCK_PIN = 27;

char ssid[32] = "ro.d";
char password[32] = "fo45#cuu";

HX711 scale;

float calibration_factor = -6075; // this calibration factor is adjusted according to my load cell
float units;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void constructSsid(std::string payload) {
  int counter = 0;
  uint8_t ssidLength = payload.length();
  for (int i = 1; i < (ssidLength / 2); i++) {
     ssid[counter++] = payload[i*2];
  }
  ssid[counter] = '\0';
}

void constructPassword(std::string payload) {
  int counter = 0;
  uint8_t passwordLength = payload.length();
  for (int i = 1; i < (passwordLength / 2); i++) {
     password[counter++] = payload[i*2];
  }
  password[counter] = '\0';
}

bool connectToWifi() {
  Serial.println(ssid);
  Serial.println(password);
  WiFi.begin(ssid, password);
  uint8_t retryCount = 0;
  while ((WiFi.status() != WL_CONNECTED) && retryCount <= 4) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    retryCount += 1;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  return false;
}

bool compareData(std::string received, std::string predefined) {
  int receivedLength = received.length();
  int predefinedLength = predefined.length();
  
  if ((receivedLength / 2) != predefinedLength) {
    return false;
  }

  for (int i = 0; i < predefinedLength; i++) {
    if (received[i * 2] != predefined[i]) {
      return false;
    }
  }

  return true;
}

void sendSomeDataBLE(uint8_t *message, int messageSize) {
        uint8_t txValue = 0;

        while (txValue < messageSize) {
          pTxCharacteristic->setValue(&message[txValue], 1);
          pTxCharacteristic->notify();
          txValue++;
          delay(100); // bluetooth stack will go into congestion, if too many packets are sent
        }
}

String listOfWifiNetworks(int *messageLength) {
  int n = min((int16_t)3, WiFi.scanNetworks());
  String commaSeparatedNetworks = "{";
  for (int i = 0; i < n; i++) {
    commaSeparatedNetworks += WiFi.SSID(i);

    if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
      commaSeparatedNetworks += "$";
    } else {
      commaSeparatedNetworks += "#";
    }
    
    if (i != (n - 1)) {
      commaSeparatedNetworks += ", ";
    }
  }

  commaSeparatedNetworks += "}";
  *messageLength = commaSeparatedNetworks.length();
  return commaSeparatedNetworks;
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      
      std::string lwnCommand = "lwn";
      if (compareData(rxValue, lwnCommand)) {
        Serial.println("lwn command received");

        int messageLength = 0;
        String netMessage = listOfWifiNetworks(&messageLength);
        Serial.println(netMessage);
        uint8_t* message = (uint8_t*)malloc(sizeof(uint8_t)*messageLength);
        for (int i = 0; i < messageLength; i++) {
          message[i] = netMessage[i];
        }
        
        // uint8_t* message[netMessage.length()] = netMessage.c_str(); // "{srm hostels, srm campus}";
        sendSomeDataBLE(&message[0], messageLength);
        
      }

      std::string mdCommand = "md";
      if (compareData(rxValue, mdCommand)) {
        Serial.println("md command received");

        uint8_t message[] = "{Wheat, 7.6, 12, 1, 2, 55, 1}";
        sendSomeDataBLE(&message[0], sizeof(message)/sizeof(uint8_t));
        
      }

      if (rxValue[0] == '$') {
        Serial.println("wifi ssid received");
        constructSsid(rxValue);
      }


      if (rxValue[0] == '%') {
        Serial.println("wifi password received");
        constructPassword(rxValue);
      }

      std::string wcrCommand = "wcr";
      if (compareData(rxValue, wcrCommand)) {
        Serial.println("wcr command received");
        
        uint8_t wcStatus[] = "N";
        if (connectToWifi()) {
          wcStatus[0] = 'Y';
        }
        
        Serial.print("Connection status: ");
        Serial.println(wcStatus[0]);
        sendSomeDataBLE(&wcStatus[0], sizeof(wcStatus)/sizeof(uint8_t));
      }

    }
};

void setup() {
  Serial.begin(115200);

  // load cell initialization
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  Serial.println("HX711 calibration sketch");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");

  scale.set_scale();
  scale.tare();  // Reset the scale to 0

  long zero_factor = scale.read_average(); // Get a baseline reading
  Serial.print("Zero factor: "); // This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  // BLE initialization
  // Create the BLE Device
  BLEDevice::init("MA-404-00000");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  // set up WiFi configurations
  WiFi.mode(WIFI_STA);
  delay(100);

}

void loop() {

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

  scale.set_scale(calibration_factor);

  Serial.print("Reading: ");
  units = scale.get_units(), 10;

  float grams = units*17.5;
  Serial.print(grams);
  Serial.print(" grams"); 
  Serial.print(" calibration_factor: ");
  Serial.print(calibration_factor);
  Serial.println();

  // if calibration input is received
  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += 1;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= 1;
  }

  // POST the data to the server
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin("http://genesisapp.ml/kgas/api/update/level/");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String requestData = "customerId=ROUNAK123&level=" + String(grams);
    Serial.println(requestData);
    int httpResponseCode = http.POST(requestData);
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.println("Error sending POST request");
    }
  }

  // wait for 1 minute
  delay(60000);
  // delay(5000);
}
