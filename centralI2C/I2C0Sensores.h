#pragma once
// Bus fisico I2C 0: AFM3000 y futuros sensores.
TwoWire busSensores(0);
constexpr int SENSOR_SDA = 8, SENSOR_SCL = 9;
constexpr uint8_t AFM3000 = 0x40;
bool sensoresListos = false;
bool flujoValido = false, descartarPrimera = true;
float flujo = 0;

bool leerFlujo() {
  if (!sensoresListos) return false;
  busSensores.beginTransmission(AFM3000);
  busSensores.write(uint8_t(0x10));
  busSensores.write(uint8_t(0x00));
  if (busSensores.endTransmission() != 0) return false;
  delay(20);
  if (busSensores.requestFrom(AFM3000, size_t(3), true) != 3) {
    while (busSensores.available()) busSensores.read();
    return false;
  }
  uint8_t p[3];
  for (uint8_t i = 0; i < 3; ++i) p[i] = busSensores.read();
  if (crc8(p, 2) != p[2]) return false;
  uint16_t raw = (uint16_t(p[0]) << 8) | p[1];
  flujo = (float(raw) - 32000.0f) / 140.0f;
  return true;
}

