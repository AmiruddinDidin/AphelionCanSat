/*
  ============================================================================
  Ground Station — E220 LoRa <-> USB Serial Bridge
  ESP32 DevKit
  ============================================================================
  Purpose: bidirectional bridge between your PC (USB Serial) and the E220
  LoRa module talking to the CanSat.

    - Telemetry coming DOWN from the CanSat over LoRa -> printed to USB
      Serial, so it can feed cansat_gcs.html directly (select this ESP32's
      COM port in the dashboard - no separate USB-TTL adapter needed).
    - Commands typed in Serial Monitor (or sent by the web dashboard) ->
      transmitted UP to the CanSat over LoRa.

  Wiring:
    E220 VCC -> 3V3
    E220 GND -> GND
    E220 TX  -> GPIO16 (ESP32 RX2)
    E220 RX  -> GPIO17 (ESP32 TX2)
    E220 M0, M1 -> GND (Mode 0, Normal/Transparent - matches CanSat side)
  ============================================================================
*/

#define LORA_RX_PIN 16   // ESP32 RX2 <- E220 TX
#define LORA_TX_PIN 17   // ESP32 TX2 -> E220 RX

HardwareSerial LoRaSerial(2);  // UART2

void setup() {
  Serial.begin(115200);
  LoRaSerial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  Serial.println(F("=== Ground Station Bridge Ready ==="));
}

void loop() {
  // Downlink: LoRa -> USB Serial (telemetry from CanSat)
  while (LoRaSerial.available()) {
    Serial.write(LoRaSerial.read());
  }

  // Uplink: USB Serial -> LoRa (commands to CanSat)
  while (Serial.available()) {
    LoRaSerial.write(Serial.read());
  }
}
