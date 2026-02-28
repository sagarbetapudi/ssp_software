#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RF24.h>

/* ================= PIN MAP ================= */

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// LED & Buzzer
#define LED_PIN 32
#define BUZZER_PIN 33

// DS18B20
#define ONE_WIRE_PIN 27

// UV Analog
#define UV1_PIN 34
#define UV2_PIN 25

// NRF24 (VSPI)
#define NRF_CE   16
#define NRF_CSN  17
#define NRF_SCK  18
#define NRF_MISO 19
#define NRF_MOSI 23

// microSD (HSPI custom)
#define SD_CS   5
#define SD_SCK  14
#define SD_MISO 35
#define SD_MOSI 13

/* ================= OBJECTS ================= */

Adafruit_BMP280 bmp;
Adafruit_MPU6050 mpu;

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18(&oneWire);

SPIClass SPI_SD(HSPI);
RF24 radio(NRF_CE, NRF_CSN);

/* ================= FSM ================= */

enum State {
  BOOT,
  INIT,
  IDLE,
  READ_SENSORS,
  ERROR_STATE
};

State currentState = BOOT;

unsigned long lastRead = 0;
const unsigned long interval = 2000;

/* ================= SETUP ================= */

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(UV1_PIN, INPUT);
  pinMode(UV2_PIN, INPUT);
}

/* ================= LOOP ================= */

void loop() {
  switch (currentState) {

    case BOOT:
      Wire.begin(SDA_PIN, SCL_PIN);

      SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
      SPI_SD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

      currentState = INIT;
      break;

    case INIT:
      if (!bmp.begin(0x76)) goto fail;
      if (!mpu.begin()) goto fail;

      ds18.begin();

      if (!SD.begin(SD_CS, SPI_SD)) goto fail;

      if (!radio.begin()) goto fail;
      radio.setPALevel(RF24_PA_LOW);
      radio.stopListening();

      digitalWrite(LED_PIN, HIGH);
      currentState = IDLE;
      break;

    case IDLE:
      if (millis() - lastRead > interval) {
        lastRead = millis();
        currentState = READ_SENSORS;
      }
      break;

    case READ_SENSORS:
      readAllSensors();
      currentState = IDLE;
      break;

    case ERROR_STATE:
      blinkError();
      break;
  }

  return;

fail:
  currentState = ERROR_STATE;
}

/* ================= SENSOR FUNCTION ================= */

void readAllSensors() {

  // BMP280
  float temp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;

  // MPU6050
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // DS18B20
  ds18.requestTemperatures();
  float tempProbe = ds18.getTempCByIndex(0);

  // UV Sensors
  int uv1 = analogRead(UV1_PIN);
  int uv2 = analogRead(UV2_PIN);

  Serial.println("------ DATA ------");
  Serial.printf("BMP Temp: %.2f C\n", temp);
  Serial.printf("Pressure: %.2f hPa\n", pressure);
  Serial.printf("Accel X: %.2f\n", a.acceleration.x);
  Serial.printf("DS18 Temp: %.2f C\n", tempProbe);
  Serial.printf("UV1: %d\n", uv1);
  Serial.printf("UV2: %d\n", uv2);

  logToSD(temp, pressure, tempProbe, uv1, uv2);
}

/* ================= SD LOGGING ================= */

void logToSD(float t, float p, float t2, int uv1, int uv2) {

  File file = SD.open("/data.csv", FILE_APPEND);

  if (file) {
    file.printf("%.2f,%.2f,%.2f,%d,%d\n", t, p, t2, uv1, uv2);
    file.close();
  }
}

/* ================= ERROR ================= */

void blinkError() {
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 300) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
