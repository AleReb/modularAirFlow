// Central propuesto ESP32-S3. Arduino-ESP32 2.0.17, USB nativo.
// Dos buses independientes: sensor y modulos. Pines NUEVOS, ver README.
#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "Protocolo.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Selecciona ESP32-S3 para este central con dos buses I2C."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Activa USB CDC On Boot y usa USB nativo."
#endif

#include "I2C0Sensores.h"
#include "I2C1Modulos.h"

constexpr int R_EN = 4, L_EN = 5, RPWM = 6, LPWM = 7;
int pwmActual = 0;
uint32_t consultaMs = 0, reporteMs = 0;
char linea[64];
size_t longitud = 0;
bool desbordada = false;

void ajustarPWM(int valor) {
  digitalWrite(RPWM, LOW);
  if (!valor) { digitalWrite(R_EN, LOW); digitalWrite(L_EN, LOW); }
  analogWrite(LPWM, valor);
  if (valor) { digitalWrite(R_EN, HIGH); digitalWrite(L_EN, HIGH); }
  pwmActual = valor;
}

void mostrarEstado() {
  Serial.print("PWM: "); Serial.print(pwmActual);
  Serial.print(" | Flujo: ");
  if (flujoValido) { Serial.print(flujo, 2); Serial.print(" slm"); }
  else Serial.print("SIN LECTURA");
  if (!moduloValido) { Serial.println(" | REV: SIN RESPUESTA VALIDA"); return; }
  Serial.print(" | REV pos: "); Serial.print(leerInt32(estado + 5));
  Serial.print(" destino: "); Serial.print(leerInt32(estado + 9));
  Serial.print(" moviendo: "); Serial.print(bool(estado[2] & FLAG_MOVIENDO));
  Serial.print(" homing: "); Serial.print(bool(estado[2] & FLAG_HOME));
  Serial.print(" origen: "); Serial.print(bool(estado[2] & FLAG_REFERENCIADO));
  Serial.print(" switch: "); Serial.print(bool(estado[2] & FLAG_SWITCH));
  Serial.print(" resultado: "); Serial.println(estado[3]);
}

void ayuda() {
  Serial.println("PWM 0..255 | STOP (solo ventilador) | ESTADO | AYUDA");
  Serial.println("REV HOME | REV POS 1..3 | REV MOVE 0..2000 | REV STOP");
  Serial.println("Posiciones: 1=150, 2=350, 3=550 pasos. Hacer REV HOME primero.");
}

bool entero(const char *s, long minimo, long maximo, long &v) {
  if (!*s) return false;
  errno = 0;
  char *fin;
  v = strtol(s, &fin, 10);
  return fin != s && *fin == '\0' && errno != ERANGE && v >= minimo && v <= maximo;
}

void comando(char *s) {
  while (isspace(static_cast<unsigned char>(*s))) ++s;
  size_t n = strlen(s);
  while (n && isspace(static_cast<unsigned char>(s[n - 1]))) s[--n] = '\0';
  for (size_t i = 0; i < n; ++i) s[i] = toupper(static_cast<unsigned char>(s[i]));
  if (!n) return;
  long valor;
  if (!strcmp(s, "STOP")) ajustarPWM(0);
  else if (!strcmp(s, "ESTADO")) mostrarEstado();
  else if (!strcmp(s, "AYUDA")) ayuda();
  else if (!strcmp(s, "REV HOME")) enviarOrden(HOME);
  else if (!strcmp(s, "REV STOP")) enviarOrden(PARAR);
  else if (!strncmp(s, "PWM ", 4) && entero(s + 4, 0, 255, valor)) ajustarPWM(valor);
  else if (!strncmp(s, "REV MOVE ", 9) && entero(s + 9, 0, 2000, valor)) enviarOrden(MOVER, valor);
  else if (!strncmp(s, "REV POS ", 8) && entero(s + 8, 1, 3, valor)) {
    const int32_t posiciones[] = {150, 350, 550};
    enviarOrden(MOVER, posiciones[valor - 1]);
  } else Serial.println("Comando invalido. Escribe AYUDA.");
}

void atenderSerial() {
  for (uint8_t i = 0; i < 64 && Serial.available(); ++i) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (desbordada) Serial.println("Comando demasiado largo.");
      else { linea[longitud] = '\0'; comando(linea); }
      longitud = 0; desbordada = false;
    } else if (!desbordada) {
      if (longitud < sizeof(linea) - 1) linea[longitud++] = c;
      else desbordada = true;
    }
  }
}

void setup() {
  pinMode(R_EN, OUTPUT); pinMode(L_EN, OUTPUT);
  digitalWrite(R_EN, LOW); digitalWrite(L_EN, LOW);
  pinMode(RPWM, OUTPUT); digitalWrite(RPWM, LOW);
  pinMode(LPWM, OUTPUT); analogWrite(LPWM, 0);
  Serial.begin(115200);
  delay(1000);
  sensoresListos = busSensores.begin(SENSOR_SDA, SENSOR_SCL, 100000);
  modulosListos = busModulos.begin(MODULOS_SDA, MODULOS_SCL, 100000);
  if (sensoresListos) busSensores.setTimeOut(100);
  if (modulosListos) busModulos.setTimeOut(100);
  Serial.println("Central S3: sensor bus0 GPIO8/9; modulos bus1 GPIO10/11.");
  if (!sensoresListos || !modulosListos) Serial.println("Revisar inicializacion de buses I2C.");
  ayuda();
}

void loop() {
  atenderSerial();
  if (millis() - consultaMs >= 100) {
    consultaMs = millis();
    bool leido = leerFlujo();
    flujoValido = leido && !descartarPrimera;
    descartarPrimera = !leido;
    moduloValido = consultarModulo();
  }
  if (esperando && millis() - envioMs > 2000) {
    esperando = false;
    Serial.println("Sin confirmacion REV: ejecucion incierta. No se reenvia automaticamente.");
  }
  if (millis() - reporteMs >= 500) { reporteMs = millis(); mostrarEstado(); }
  atenderSerial();
}
