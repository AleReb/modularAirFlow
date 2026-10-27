#include <Arduino.h>
#include <Wire.h>
#include <FastIMU.h>
#include <math.h>

constexpr int IMU_SDA = D8;
constexpr int IMU_SCL = D9;
constexpr int LINK_SDA = D6;
constexpr int LINK_SCL = D7;
constexpr uint8_t IMU_ADDRESS = 0x69;
constexpr uint8_t NODE_ADDRESS = 0x12;
constexpr size_t PACKET_SIZE = 9;

MPU9250 IMU;
calData calib = {0};
AccelData accelData;
uint8_t publishedPacket[PACKET_SIZE] = {};
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
bool imuReady = false;
uint8_t sequence = 0;
uint32_t lastReadMs = 0;

int16_t toMilliG(float value) {
  if (!isfinite(value)) return 0;
  float milliG = value * 1000.0f;
  if (milliG > 32767.0f) milliG = 32767.0f;
  if (milliG < -32768.0f) milliG = -32768.0f;
  return static_cast<int16_t>(lroundf(milliG));
}

void responder() {
  uint8_t packet[PACKET_SIZE];
  portENTER_CRITICAL(&packetMux);
  memcpy(packet, publishedPacket, PACKET_SIZE);
  portEXIT_CRITICAL(&packetMux);
  Wire1.write(packet, PACKET_SIZE);
}

void publishAcceleration() {
  uint8_t packet[PACKET_SIZE] = {};
  packet[0] = imuReady ? 1 : 0;
  packet[1] = ++sequence;

  if (imuReady) {
    IMU.update();
    IMU.getAccel(&accelData);
    const int16_t axes[] = {
      toMilliG(accelData.accelX),
      toMilliG(accelData.accelY),
      toMilliG(accelData.accelZ)
    };
    for (size_t axis = 0; axis < 3; ++axis) {
      packet[2 + axis * 2] = static_cast<uint8_t>(axes[axis] & 0xFF);
      packet[3 + axis * 2] = static_cast<uint8_t>((axes[axis] >> 8) & 0xFF);
    }
  }

  for (size_t i = 0; i < PACKET_SIZE - 1; ++i) packet[PACKET_SIZE - 1] ^= packet[i];
  portENTER_CRITICAL(&packetMux);
  memcpy(publishedPacket, packet, PACKET_SIZE);
  portEXIT_CRITICAL(&packetMux);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(IMU_SDA, IMU_SCL, 100000);
  const int error = IMU.init(calib, IMU_ADDRESS);
  imuReady = error == 0;
  if (!imuReady) {
    Serial.print("MPU-9250 init error: ");
    Serial.println(error);
  } else {
    Serial.println("MPU-9250 listo en 0x69.");
  }

  publishAcceleration();
  Wire1.onRequest(responder);
  if (!Wire1.begin(NODE_ADDRESS, LINK_SDA, LINK_SCL, 100000)) {
    Serial.println("No se pudo iniciar el bus esclavo Wire1.");
  } else {
    Serial.println("Nodo I2C listo en 0x12, SDA D6 / SCL D7.");
  }
}

void loop() {
  if (millis() - lastReadMs >= 20) {
    lastReadMs = millis();
    publishAcceleration();
  }
}