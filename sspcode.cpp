#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_ADS1X15.h>
#include <MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RF24.h>

/* ================= PIN DEFINITIONS ================= */
#define SD_CS 5
#define LED_PIN 32
#define BUZZER_PIN 33
#define ONE_WIRE_BUS 15
#define NRF_CE 4
#define NRF_CSN 17

/* ================= OBJECTS ================= */
Adafruit_BMP280 bmp;
Adafruit_ADS1115 ads;
MPU6050 mpu;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
RF24 radio(NRF_CE, NRF_CSN);
File logFile;

/* ================= VARIABLES ================= */
float uv1, uv2;
float pressure, altitude;
float temperature;
int16_t ax, ay, az, gx, gy, gz;

/* ================= SETUP ================= */
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(115200);
  Wire.begin(21, 22);

  // BMP280
  bmp.begin(0x76);

  // ADS1115
  ads.begin();

  // MPU6050
  mpu.initialize();

  // DS18B20
  tempSensor.begin();

  // SD Card
  SD.begin(SD_CS);
  logFile = SD.open("/ozone_data.csv", FILE_WRITE);
  logFile.println("Time,UV1,UV2,Pressure,Altitude,Temp,AX,AY,AZ");

  // NRF24
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(0xF0F0F0F0E1LL);
  radio.stopListening();

  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}

/* ================= LOOP ================= */
void loop() {
  unsigned long t = millis();

  /* ---- UV Sensors ---- */
  uv1 = ads.readADC_SingleEnded(0) * 0.1875 / 1000.0;
  uv2 = ads.readADC_SingleEnded(1) * 0.1875 / 1000.0;

  /* ---- BMP280 ---- */
  pressure = bmp.readPressure() / 100.0;
  altitude = bmp.readAltitude(1013.25);

  /* ---- MPU6050 ---- */
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  /* ---- Temperature ---- */
  tempSensor.requestTemperatures();
  temperature = tempSensor.getTempCByIndex(0);

  /* ---- LOGGING ---- */
  logFile.print(t); logFile.print(",");
  logFile.print(uv1); logFile.print(",");
  logFile.print(uv2); logFile.print(",");
  logFile.print(pressure); logFile.print(",");
  logFile.print(altitude); logFile.print(",");
  logFile.print(temperature); logFile.print(",");
  logFile.print(ax); logFile.print(",");
  logFile.print(ay); logFile.print(",");
  logFile.println(az);
  logFile.flush();

  /* ---- TELEMETRY ---- */
  char packet[64];
  snprintf(packet, sizeof(packet),
           "UV1:%.2f UV2:%.2f ALT:%.1f",
           uv1, uv2, altitude);
  radio.write(&packet, sizeof(packet));

  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  delay(100); // 10 Hz
}
