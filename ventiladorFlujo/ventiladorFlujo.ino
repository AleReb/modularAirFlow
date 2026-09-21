// ESP32-C3, Arduino-ESP32 2.0.17. USB nativo y USB CDC On Boot activado.
// AFM3000 + BTS7960: solo LPWM, manteniendo la polaridad del ventilador.
// Comandos a 115200 con nueva linea: 0..255, PWM 128, STOP, ESTADO, AYUDA.
#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "Selecciona una placa ESP32-C3."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Activa USB CDC On Boot para usar el monitor por USB nativo."
#endif

constexpr int R_EN = 5;
constexpr int L_EN = 8;
constexpr int RPWM = 6;
constexpr int LPWM = 9;
constexpr int SDA_PIN = 20;
constexpr int SCL_PIN = 21;
constexpr uint8_t AFM3000_ADDRESS = 0x40;
constexpr uint32_t I2C_FREQUENCY = 100000;
constexpr float FLOW_OFFSET = 32000.0f;
constexpr float FLOW_SCALE = 140.0f; // Aire/N2.

int pwmActual = 0;
bool i2cListo = false;
bool sensorValido = false;
bool descartarPrimeraLectura = true;
uint16_t ultimaLectura = 0;
float ultimoFlujo = 0.0f;
uint32_t ultimaConsulta = 0;
uint32_t ultimoReporte = 0;
char linea[48];
size_t longitudLinea = 0;
bool lineaDesbordada = false;

void ajustarPWM(int pwm) {
  digitalWrite(RPWM, LOW); // Sentido opuesto siempre apagado.
  if (pwm == 0) {
    digitalWrite(R_EN, LOW);
    digitalWrite(L_EN, LOW);
  }
  // Mismo metodo que pruebaPWM: analogWrite con configuracion predeterminada.
  analogWrite(LPWM, pwm);
  if (pwm > 0) {
    digitalWrite(R_EN, HIGH);
    digitalWrite(L_EN, HIGH);
  }
  pwmActual = pwm;
}

void mostrarEstado() {
  Serial.print("PWM izquierdo: ");
  Serial.print(pwmActual);
  Serial.print("/255 | ");
  if (!sensorValido) {
    Serial.println("Flujo: SIN LECTURA VALIDA");
    return;
  }
  Serial.print("RAW: ");
  Serial.print(ultimaLectura);
  Serial.print(" | Flujo: ");
  Serial.print(ultimoFlujo, 2);
  Serial.println(" slm");
}

void mostrarAyuda() {
  Serial.println("analogWrite: GPIO9, rango 0..255.");
  Serial.println("Comandos con Enter / Nueva linea:");
  Serial.println("  80 o PWM 80 -> ajustar potencia izquierda (0..255)");
  Serial.println("  STOP   -> apagar (tambien PWM 0)");
  Serial.println("  ESTADO -> potencia y flujo medido");
  Serial.println("  AYUDA  -> mostrar comandos");
  Serial.println("Control manual: PWM no es un caudal objetivo.");
}

void procesarComando(char *comando) {
  while (isspace(static_cast<unsigned char>(*comando))) ++comando;
  size_t largo = strlen(comando);
  while (largo && isspace(static_cast<unsigned char>(comando[largo - 1]))) {
    comando[--largo] = '\0';
  }
  for (size_t i = 0; i < largo; ++i) {
    comando[i] = toupper(static_cast<unsigned char>(comando[i]));
  }
  if (!largo) return;
  if (strcmp(comando, "STOP") == 0) {
    ajustarPWM(0);
    mostrarEstado();
  } else if (strcmp(comando, "ESTADO") == 0) {
    mostrarEstado();
  } else if (strcmp(comando, "AYUDA") == 0) {
    mostrarAyuda();
  } else {
    char *inicio = comando;
    if (strncmp(inicio, "PWM ", 4) == 0) inicio += 4;
    while (*inicio == ' ') ++inicio;
    size_t digitos = strspn(inicio, "0123456789");
    if (digitos == 0 || digitos > 3 || inicio[digitos] != '\0') {
      Serial.println("Envia 0..255, PWM 128, STOP, ESTADO o AYUDA.");
      return;
    }
    int valor = atoi(inicio);
    if (valor > 255) {
      Serial.println("PWM fuera de rango: usa 0..255.");
      return;
    }
    ajustarPWM(valor);
    mostrarEstado();
  }
}

void atenderSerial() {
  for (uint8_t i = 0; i < 64 && Serial.available(); ++i) {
    char caracter = Serial.read();
    if (caracter == '\n' || caracter == '\r') {
      if (lineaDesbordada) {
        Serial.println("Comando demasiado largo: descartado.");
      } else {
        linea[longitudLinea] = '\0';
        procesarComando(linea);
      }
      longitudLinea = 0;
      lineaDesbordada = false;
    } else if (!lineaDesbordada) {
      if (longitudLinea < sizeof(linea) - 1) linea[longitudLinea++] = caracter;
      else lineaDesbordada = true;
    }
  }
}

uint8_t calcularCRC8(const uint8_t *datos, size_t cantidad) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < cantidad; ++i) {
    crc ^= datos[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
    }
  }
  return crc;
}

bool leerFlujo(uint16_t &lectura) {
  if (!i2cListo) return false;
  Wire.beginTransmission(AFM3000_ADDRESS);
  Wire.write(uint8_t(0x10));
  Wire.write(uint8_t(0x00));
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    return false;
  }
  delay(20); // Misma espera que el ejemplo del sensor ya probado.
  const size_t recibidos = Wire.requestFrom(AFM3000_ADDRESS, size_t(3));
  if (recibidos != 3) {
    while (Wire.available()) Wire.read();
    return false;
  }
  uint8_t datos[3];
  for (uint8_t i = 0; i < 3; ++i) datos[i] = Wire.read();
  if (calcularCRC8(datos, 2) != datos[2]) {
    return false;
  }
  lectura = (uint16_t(datos[0]) << 8) | datos[1];
  return true;
}

void setup() {
  // Driver deshabilitado durante toda la inicializacion.
  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);
  digitalWrite(R_EN, LOW);
  digitalWrite(L_EN, LOW);
  pinMode(RPWM, OUTPUT);
  digitalWrite(RPWM, LOW);
  pinMode(LPWM, OUTPUT);
  analogWrite(LPWM, 0); // Arranque apagado; 8 bits y 1 kHz en core 2.0.17.
  Serial.begin(115200);
  delay(1500);

  i2cListo = Wire.begin(SDA_PIN, SCL_PIN, I2C_FREQUENCY);
  if (i2cListo) Wire.setTimeOut(100);
  else Serial.println("No se pudo inicializar I2C.");
  Serial.println("AFM3000: SDA=20, SCL=21, direccion=0x40.");
  mostrarAyuda();
}

void loop() {
  atenderSerial();
  uint32_t ahora = millis();
  if (ahora - ultimaConsulta >= 100) {
    ultimaConsulta = ahora;
    uint16_t lectura;
    if (!leerFlujo(lectura)) {
      sensorValido = false;
      descartarPrimeraLectura = true;
      // La lectura del sensor nunca modifica el PWM manual.
    } else if (descartarPrimeraLectura) {
      descartarPrimeraLectura = false;
      sensorValido = false;
    } else {
      ultimaLectura = lectura;
      ultimoFlujo = (float(lectura) - FLOW_OFFSET) / FLOW_SCALE;
      sensorValido = true;
    }
  }
  atenderSerial();
  if (millis() - ultimoReporte >= 500) {
    ultimoReporte = millis();
    mostrarEstado();
  }
}
