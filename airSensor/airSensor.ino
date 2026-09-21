/*
 * AFM3000 + ESP32-C3 (Arduino).
 * SDA = GPIO20; SCL = GPIO21; I2C = 100 kHz.
 * Herramientas -> USB CDC On Boot -> Enabled. Monitor: 115200.
 * Usar USB nativo: UART0 comparte GPIO20/21 con este bus I2C.
 *
 * Sensor alimentado a 5 V, GND comun y conversor bidireccional I2C
 * de 3.3 V <-> 5 V entre ESP32 y sensor. NO conectar I2C de 5 V
 * directamente al ESP32. Ver README.md para las conexiones.
 * Basado en el manual AFM3000 local, secciones 3.2, 3.5, 3.6 y 3.7.
 */
#include <Arduino.h>
#include <Wire.h>

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "Selecciona una placa ESP32-C3."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Activa USB CDC On Boot para no usar UART0 en GPIO20/21."
#endif

constexpr int SDA_PIN = 20;
constexpr int SCL_PIN = 21;
constexpr uint32_t I2C_FREQUENCY = 100000;
constexpr uint8_t AFM3000_ADDRESS = 0x40; // Direccion de 7 bits para Wire.
constexpr float FLOW_OFFSET = 32000.0f;
constexpr float FLOW_SCALE = 140.0f; // Aire/N2. Para O2: 142.8f.

bool descartarPrimeraLectura = true;

uint8_t calcularCRC8(const uint8_t *datos, size_t cantidad) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < cantidad; ++i) {
    crc ^= datos[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      // 0x31 equivale a 0x131 al conservar solo los 8 bits bajos.
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31)
                         : uint8_t(crc << 1);
    }
  }
  return crc;
}

bool leerFlujo(uint16_t &lectura) {
  Wire.beginTransmission(AFM3000_ADDRESS);
  Wire.write(uint8_t(0x10));
  Wire.write(uint8_t(0x00));
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.print("Error al enviar 0x1000, codigo I2C: ");
    Serial.println(error);
    return false;
  }

  // Margen conservador sobre el ciclo de muestreo de 10 ms del manual.
  delay(20);
  const size_t recibidos = Wire.requestFrom(AFM3000_ADDRESS, size_t(3));
  if (recibidos != 3) {
    while (Wire.available()) Wire.read();
    Serial.print("Lectura incompleta. Bytes recibidos: ");
    Serial.println(recibidos);
    return false;
  }

  uint8_t datos[3];
  for (uint8_t i = 0; i < 3; ++i) datos[i] = Wire.read();
  if (calcularCRC8(datos, 2) != datos[2]) {
    Serial.println("Error CRC: lectura descartada.");
    return false;
  }
  lectura = (uint16_t(datos[0]) << 8) | datos[1];
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500); // Espera de arranque; no bloquea esperando al monitor USB.
  Serial.println("AFM3000: SDA=20, SCL=21, direccion=0x40");
  if (!Wire.begin(SDA_PIN, SCL_PIN, I2C_FREQUENCY)) {
    Serial.println("No se pudo iniciar I2C.");
    while (true) delay(1000);
  }
  Wire.setTimeOut(100);
}

void loop() {
  uint16_t lectura;
  if (!leerFlujo(lectura)) {
    // Reintenta; descarta la primera lectura al recuperar la comunicacion.
    descartarPrimeraLectura = true;
    delay(1000);
    return;
  }

  // El manual indica que la primera medicion tras inicializar no es valida.
  if (descartarPrimeraLectura) {
    descartarPrimeraLectura = false;
    delay(100);
    return;
  }

  // Convertir antes de restar permite representar caudales negativos.
  const float flujoSlm = (float(lectura) - FLOW_OFFSET) / FLOW_SCALE;
  Serial.print("RAW: ");
  Serial.print(lectura);
  Serial.print(" | Flujo: ");
  Serial.print(flujoSlm, 2);
  Serial.println(" slm"); // Litros estandar por minuto (20 C, 1013 mbar).
  delay(100);
}
