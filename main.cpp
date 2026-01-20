// ================= ESP32 FULL CODE (38-PIN SAFE) =================

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_ADS1X15.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <RF24.h>

/* ================= PIN MAP ================= */

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// OneWire (DS18B20) — moved from GPIO15 (strap pin)
#define ONE_WIRE_PIN 27

// nRF24L01
#define NRF_CE   16
#define NRF_CSN  17

// SPI (VSPI)
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// SD Card
#define SD_CS 5   // safer than GPIO4

// Indicators
#define LED_PIN     32
#define BUZZER_PIN  33

/* ================= OBJECTS ================= */

Adafruit_BMP280 bmp;
Adafruit_MPU6050 mpu;
Adafruit_ADS1115 ads;

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);

RF24 radio(NRF_CE, NRF_CSN);

/* ================= FSM ================= */

enum SystemState {
  STATE_BOOT,
  STATE_INIT,
  STATE_IDLE,
  STATE_READ_SENSORS,
  STATE_ERROR
};

SystemState currentState = STATE_BOOT;

/* ================= TIMERS ================= */

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000;

/* ================= SETUP ================= */

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

/* ================= LOOP ================= */

void loop() {
  switch (currentState) {

    case STATE_BOOT:
      bootState();
      break;

    case STATE_INIT:
      initState();
      break;

    case STATE_IDLE:
      idleState();
      break;

    case STATE_READ_SENSORS:
      readSensorsState();
      break;

    case STATE_ERROR:
      errorState();
      break;
  }
}

/* ================= STATES ================= */

void bootState() {
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  currentState = STATE_INIT;
}

void initState() {
  if (!bmp.begin(0x76)) goto fail;
  if (!mpu.begin()) goto fail;
  if (!ads.begin()) goto fail;

  ads.setGain(GAIN_ONE);

  ds18b20.begin();

  if (!SD.begin(SD_CS)) goto fail;

  if (!radio.begin()) goto fail;
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();

  digitalWrite(LED_PIN, HIGH);
  beep(2);

  Serial.println("SYSTEM ONLINE");
  currentState = STATE_IDLE;
  return;

fail:
  currentState = STATE_ERROR;
}

void idleState() {
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    currentState = STATE_READ_SENSORS;
  }
}

void readSensorsState() {
  readBMP280();
  readMPU6050();
  readUV();
  readDS18B20();

  currentState = STATE_IDLE;
}

void errorState() {
  static unsigned long lastBlink = 0;

  if (millis() - lastBlink > 300) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
  }
}

/* ================= SENSOR READERS ================= */

void readBMP280() {
  Serial.print("BMP | T=");
  Serial.print(bmp.readTemperature());
  Serial.print(" C P=");
  Serial.print(bmp.readPressure() / 100.0);
  Serial.println(" hPa");
}

void readMPU6050() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  Serial.print("MPU | AX=");
  Serial.print(a.acceleration.x);
  Serial.print(" AY=");
  Serial.print(a.acceleration.y);
  Serial.print(" AZ=");
  Serial.println(a.acceleration.z);
}

void readUV() {
  int16_t uv1 = ads.readADC_SingleEnded(0);
  int16_t uv2 = ads.readADC_SingleEnded(1);

  Serial.print("UV | 1=");
  Serial.print(uv1);
  Serial.print(" 2=");
  Serial.println(uv2);
}

void readDS18B20() {
  ds18b20.requestTemperatures();
  Serial.print("DS18B20 | T=");
  Serial.print(ds18b20.getTempCByIndex(0));
  Serial.println(" C");
}

/* ================= UTIL ================= */

void beep(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
    delay(80);
  }
}
