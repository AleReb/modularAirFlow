#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

constexpr int OLED_SDA = D8;
constexpr int OLED_SCL = D9;
constexpr int LINK_SDA = D6;
constexpr int LINK_SCL = D7;
constexpr uint8_t NODE_ADDRESS = 0x12;
constexpr size_t PACKET_SIZE = 9;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
int16_t accelMilliG[3] = {};
bool linkReady = false;
bool sensorReady = false;
uint32_t lastPollMs = 0;

bool readRemoteAcceleration() {
  const size_t received = Wire1.requestFrom(NODE_ADDRESS, static_cast<uint8_t>(PACKET_SIZE));
  if (received != PACKET_SIZE) {
    while (Wire1.available()) Wire1.read();
    linkReady = false;
    sensorReady = false;
    return false;
  }

  uint8_t packet[PACKET_SIZE];
  for (size_t i = 0; i < PACKET_SIZE; ++i) packet[i] = static_cast<uint8_t>(Wire1.read());

  uint8_t checksum = 0;
  for (size_t i = 0; i < PACKET_SIZE - 1; ++i) checksum ^= packet[i];
  if (checksum != packet[PACKET_SIZE - 1]) {
    linkReady = false;
    sensorReady = false;
    return false;
  }

  linkReady = true;
  sensorReady = packet[0] == 1;
  if (!sensorReady) return true;

  for (size_t axis = 0; axis < 3; ++axis) {
    const uint16_t raw = static_cast<uint16_t>(packet[2 + axis * 2]) |
                         (static_cast<uint16_t>(packet[3 + axis * 2]) << 8);
    accelMilliG[axis] = static_cast<int16_t>(raw);
  }
  return true;
}

void drawScreen() {
  char line[24];
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  if (!linkReady) {
    oled.drawStr(0, 14, "Sin enlace I2C");
    oled.drawStr(0, 30, "Revisa SDA/SCL/GND");
    oled.sendBuffer();
    return;
  }
  if (!sensorReady) {
    oled.drawStr(0, 14, "Nodo conectado");
    oled.drawStr(0, 30, "MPU-9250 no listo");
    oled.sendBuffer();
    return;
  }

  snprintf(line, sizeof(line), "Aceleracion (g)");
  oled.drawStr(0, 12, line);
  snprintf(line, sizeof(line), "X: %+.2f", accelMilliG[0] / 1000.0f);
  oled.drawStr(0, 28, line);
  snprintf(line, sizeof(line), "Y: %+.2f", accelMilliG[1] / 1000.0f);
  oled.drawStr(0, 42, line);
  snprintf(line, sizeof(line), "Z: %+.2f", accelMilliG[2] / 1000.0f);
  oled.drawStr(0, 56, line);
  oled.sendBuffer();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL, 100000);
  oled.begin();
  Wire1.begin(LINK_SDA, LINK_SCL, 100000);
  drawScreen();
  Serial.println("Principal: OLED Wire D8/D9, nodo Wire1 D6/D7.");
}

void loop() {
  if (millis() - lastPollMs >= 100) {
    lastPollMs = millis();
    readRemoteAcceleration();
    drawScreen();
  }
}