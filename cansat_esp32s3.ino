/*
  ============================================================================
  CanSat Flight Firmware — ESP32-S3 Super Mini by Didin Amiruddin
  ============================================================================
  Sensors / modules (per wiring table):
    - Ebyte E220 LoRa module   -> UART1, Mode 0 (Normal/Transparent), M0=M1=GND
    - u-blox NEO-M8N GPS       -> UART2 (NMEA)
    - Bosch BNO055 IMU (I2C)
    - MS5611 barometer (I2C)

  Wiring:
    LoRa E220   TX -> GP1  (ESP32 RX1)
    LoRa E220   RX -> GP2  (ESP32 TX1)
    LoRa E220   M0, M1 -> GND
    I2C SDA -> GP13
    I2C SCL -> GP12
    GPS TX  -> GP18 (ESP32 RX2)
    GPS RX  -> GP17 (ESP32 TX2)   

  Required libraries (Arduino Library Manager):
    - TinyGPSPlus        by Mikal Hart
    - Adafruit BNO055    by Adafruit
    - Adafruit Unified Sensor (dependency of BNO055)
    - MS5611              by Rob Tillaart
  ============================================================================
*/

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <MS5611.h>
#include <SPI.h>
#include <SD.h>

// ---------------- Pin definitions ----------------
#define I2C_SDA        13
#define I2C_SCL        12

#define LORA_RX_PIN    1     // ESP32 RX1 <- E220 TX
#define LORA_TX_PIN    2     // ESP32 TX1 -> E220 RX

#define GPS_RX_PIN     18    // ESP32 RX2 <- GPS TX
#define GPS_TX_PIN     17    // ESP32 TX2 -> GPS RX

#define SD_CS_PIN      4
#define SD_MOSI_PIN    5
#define SD_CLK_PIN     6
#define SD_MISO_PIN    7

// ---------------- UARTs ----------------
HardwareSerial LoRaSerial(1);   // UART1 -> E220
HardwareSerial GPSSerial(2);    // UART2 -> NEO-M7M

// ---------------- Sensor objects ----------------
TinyGPSPlus gps;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29, &Wire);
MS5611 ms5611(0x77);
SPIClass sdSPI(HSPI);
File logFile;

// ---------------- Config ----------------
const uint32_t TELEMETRY_INTERVAL_MS = 1000;
const float    SEA_LEVEL_HPA = 1013.25;

uint32_t lastTelemetryMs = 0;
bool bnoOK = false;
bool baroOK = false;

bool sdOK = false;
const char* LOG_FILENAME = "/Aphelion_log.csv";

// ---------------- SD card setup ----------------
void setupSD() {
  sdSPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, sdSPI)) {
    Serial.println(F("SD card NOT FOUND"));
    sdOK = false;
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println(F("No SD card attached"));
    sdOK = false;
    return;
  }

  sdOK = true;
  Serial.println(F("SD card OK"));

  bool fileExists = SD.exists(LOG_FILENAME);
  logFile = SD.open(LOG_FILENAME, FILE_APPEND);
  if (logFile) {
    if (!fileExists) {
      logFile.println(F("tag,millis,lat,lon,gpsAlt,sats,pressHpa,baroAlt,baroTemp,roll,pitch,yaw,ax,ay,az,calSys,calGyro,calAcc,calMag"));
      logFile.flush();
    }
    logFile.close();
  } else {
    Serial.println(F("Failed to open log file"));
    sdOK = false;
  }
}

void logToSD(const char* packet) {
  if (!sdOK) return;

  logFile = SD.open(LOG_FILENAME, FILE_APPEND);
  if (logFile) {
    logFile.print(packet);
    logFile.flush();
    logFile.close();
  } else {
    Serial.println(F("SD write failed"));
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("=== CanSat Initialize ==="));

  // I2C bus
  Wire.begin(I2C_SDA, I2C_SCL);

  // LoRa E220 UART 
  LoRaSerial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);

  // GPS UART
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // BNO055
  if (bno.begin()) {
    bno.setExtCrystalUse(true);
    bnoOK = true;
    Serial.println(F("BNO055 OK"));
  } else {
    Serial.println(F("BNO055 NOT FOUND"));
  }

  // MS5611
  if (ms5611.begin()) {
    ms5611.reset(1);
    ms5611.setOversampling(OSR_ULTRA_HIGH);
    baroOK = true;
    Serial.println(F("MS5611 OK"));
  } else {
    Serial.println(F("MS5611 NOT FOUND"));
  }

  // SD card
  setupSD();

  Serial.println(F("=== Init complete ==="));
}

// ---------------- Main loop ----------------
void loop() {
  // Feed GPS parser continuously
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
  }

  if (millis() - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = millis();
    sendTelemetry();
  }

  // Pass through any incoming LoRa data
  while (LoRaSerial.available()) {
    Serial.write(LoRaSerial.read());
  }
}

// ---------------- Telemetry packet builder + sender ----------------
void sendTelemetry() {
  // --- GPS ---
  double lat = gps.location.isValid() ? gps.location.lat() : 0.0;
  double lon = gps.location.isValid() ? gps.location.lng() : 0.0;
  double gpsAlt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
  int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;

  // --- Barometer ---
  float pressureHpa = 0.0f, baroAlt = 0.0f, baroTemp = 0.0f;
  if (baroOK) {
    ms5611.read();
    pressureHpa = ms5611.getPressure();
    baroTemp    = ms5611.getTemperature();
    baroAlt     = 44330.0f * (1.0f - pow(pressureHpa / SEA_LEVEL_HPA, 0.1903f));
  }

  // --- IMU ---
  float roll = 0, pitch = 0, yaw = 0;
  float ax = 0, ay = 0, az = 0;
  uint8_t sysCal = 0, gyroCal = 0, accelCal = 0, magCal = 0;
  if (bnoOK) {
    imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
    yaw   = euler.x();
    roll  = euler.y();
    pitch = euler.z();

    imu::Vector<3> accel = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
    ax = accel.x(); ay = accel.y(); az = accel.z();

    bno.getCalibration(&sysCal, &gyroCal, &accelCal, &magCal);
  }

  // --- CSV packet ---
  // $TLM,millis,lat,lon,gpsAlt,sats,pressHpa,baroAlt,baroTemp,roll,pitch,yaw,ax,ay,az,calSys,calGyro,calAcc,calMag
  char packet[220];
  snprintf(packet, sizeof(packet),
    "$TLM,%lu,%.6f,%.6f,%.1f,%d,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%d,%d,%d,%d\n",
    millis(),
    lat, lon, gpsAlt, sats,
    pressureHpa, baroAlt, baroTemp,
    roll, pitch, yaw,
    ax, ay, az,
    sysCal, gyroCal, accelCal, magCal
  );

  // Send over LoRa
  LoRaSerial.print(packet);

  // Mirror to USB serial for ground debugging
  Serial.print(packet);

  // Log to SD card as black-box backup
  logToSD(packet);
}
