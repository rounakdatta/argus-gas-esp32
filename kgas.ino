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

const int LOADCELL_DOUT_PIN = 12;
const int LOADCELL_SCK_PIN = 27;

char ssid[32] = "ro.d";
char password[32] = "fo45#cuu";

HX711 scale;

float calibration_factor = -6075; // this calibration factor is adjusted according to my load cell
float units;

void setup() {
  Serial.begin(115200);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  Serial.println("HX711 calibration sketch");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");

  scale.set_scale();
  scale.tare();  //Reset the scale to 0

  long zero_factor = scale.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  // set up WiFi
  delay(4000);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi ...");
  }
}

void loop() {

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
