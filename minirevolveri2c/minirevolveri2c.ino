// Modulo I2C: recibe ordenes y genera los pasos localmente.
#include <Arduino.h>
#include <Wire.h>
#include <ESP_FlexyStepper.h>
#include <string.h>
#include "Protocolo.h"

#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32 clasico: conserva STEP/DIR/HOME del sketch original.
constexpr int STEP_PIN = 5, DIR_PIN = 23, HOME_PIN = 27;
constexpr int SDA_PIN = 21, SCL_PIN = 22;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// Asignacion propuesta para C3: requiere recablear el modulo.
constexpr int STEP_PIN = 5, DIR_PIN = 6, HOME_PIN = 7;
constexpr int SDA_PIN = 0, SCL_PIN = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr int STEP_PIN = 5, DIR_PIN = 6, HOME_PIN = 7;
constexpr int SDA_PIN = 10, SCL_PIN = 11;
#else
#error "Ejemplo previsto para ESP32, ESP32-C3 o ESP32-S3."
#endif

constexpr int HOME_ACTIVO = LOW; // Contacto normalmente abierto hacia GND.
constexpr long MAX_HOME = 2000;
constexpr long MAX_POSICION = 2000; // Limite provisional: ajustar a la mecanica.
constexpr long RETIRADA_HOME = 200; // Maximo para liberar un switch ya pulsado.
ESP_FlexyStepper motor;
portMUX_TYPE buzonMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t buzon[TAM_ORDEN];
bool pendiente = false;
uint8_t estadoPublicado[TAM_ESTADO] = {};
bool tramaMala = false;
bool referenciado = false;
uint8_t faseHome = 0; // 0=inactivo, 1=liberar switch, 2=buscar switch.
uint8_t resultado = OK, ultimaSecuencia = 0;
uint32_t ultimoEstado = 0;

void publicarEstado() {
  uint8_t p[TAM_ESTADO] = {0x5A, VERSION_PROTOCOLO};
  if (!motor.motionComplete()) p[2] |= FLAG_MOVIENDO;
  if (faseHome) p[2] |= FLAG_HOME;
  if (referenciado) p[2] |= FLAG_REFERENCIADO;
  if (digitalRead(HOME_PIN) == HOME_ACTIVO) p[2] |= FLAG_SWITCH;
  p[3] = resultado;
  p[4] = ultimaSecuencia;
  ponerInt32(p + 5, motor.getCurrentPositionInSteps());
  ponerInt32(p + 9, motor.getTargetPositionInSteps());
  p[13] = crc8(p, 13);
  portENTER_CRITICAL(&buzonMux);
  memcpy(estadoPublicado, p, TAM_ESTADO);
  portEXIT_CRITICAL(&buzonMux);
}

// Callbacks cortos: no mover el motor, imprimir ni esperar aqui.
void recibir(int cantidad) {
  uint8_t p[TAM_ORDEN];
  size_t n = 0;
  while (Wire.available()) {
    int dato = Wire.read();
    if (n < TAM_ORDEN) p[n] = uint8_t(dato);
    ++n;
  }
  bool valida = cantidad == TAM_ORDEN && n == TAM_ORDEN;
  if (valida) valida = p[0] == 0xA5 && p[1] == VERSION_PROTOCOLO &&
                      p[8] == 0 && crc8(p, 9) == p[9];
  portENTER_CRITICAL(&buzonMux);
  if (!valida) tramaMala = true;
  else if (!pendiente || p[2] == PARAR) {
    memcpy(buzon, p, TAM_ORDEN);
    pendiente = true;
  }
  // Si el buzon esta lleno, no confirmar otra orden; PARAR tiene prioridad.
  portEXIT_CRITICAL(&buzonMux);
}

void responder() {
  uint8_t p[TAM_ESTADO];
  portENTER_CRITICAL(&buzonMux);
  memcpy(p, estadoPublicado, TAM_ESTADO);
  portEXIT_CRITICAL(&buzonMux);
  Wire.write(p, TAM_ESTADO);
}

void pararAhora() {
  motor.emergencyStop();
  motor.processMovement(); // Aplica la parada en la biblioteca.
}

void ejecutar(const uint8_t *p) {
  ultimaSecuencia = p[3];
  resultado = OK;
  int32_t destino = leerInt32(p + 4);
  switch (p[2]) {
    case PARAR:
      if (faseHome) referenciado = false;
      faseHome = 0;
      pararAhora();
      break;
    case HOME:
      if (!motor.motionComplete() || faseHome) { resultado = OCUPADO; break; }
      referenciado = false;
      motor.setSpeedInStepsPerSecond(150);
      if (digitalRead(HOME_PIN) == HOME_ACTIVO) {
        faseHome = 1;
        motor.setTargetPositionRelativeInSteps(RETIRADA_HOME);
      } else {
        faseHome = 2;
        motor.setTargetPositionRelativeInSteps(-MAX_HOME);
      }
      break;
    case MOVER:
      if (!motor.motionComplete() || faseHome) { resultado = OCUPADO; break; }
      if (!referenciado) { resultado = SIN_ORIGEN; break; }
      if (destino < 0 || destino > MAX_POSICION) { resultado = FUERA_RANGO; break; }
      motor.setTargetPositionInSteps(destino);
      break;
    default: resultado = OPERACION_INVALIDA; break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(HOME_PIN, INPUT_PULLUP);
  motor.connectToPins(STEP_PIN, DIR_PIN);
  motor.setSpeedInStepsPerSecond(150);
  motor.setAccelerationInStepsPerSecondPerSecond(500);
  motor.setDecelerationInStepsPerSecondPerSecond(250);
  publicarEstado();
  Wire.onReceive(recibir);
  Wire.onRequest(responder);
  if (!Wire.begin(uint8_t(DIRECCION_REVOLVER), SDA_PIN, SCL_PIN, uint32_t(100000))) {
    Serial.println("No se pudo iniciar I2C del modulo.");
  }
#if defined(CONFIG_IDF_TARGET_ESP32)
  // El ESP32 clasico requiere precargar la primera respuesta.
  Wire.slaveWrite(estadoPublicado, TAM_ESTADO);
#endif
  Serial.println("Minirevolver I2C 0x30. Esperando ordenes; sin movimiento al arrancar.");
}

void loop() {
  uint8_t p[TAM_ORDEN];
  bool hayOrden;
  portENTER_CRITICAL(&buzonMux);
  hayOrden = pendiente;
  if (hayOrden) { memcpy(p, buzon, TAM_ORDEN); pendiente = false; }
  bool mala = tramaMala;
  tramaMala = false;
  portEXIT_CRITICAL(&buzonMux);
  if (mala) resultado = TRAMA_INVALIDA;
  if (hayOrden) ejecutar(p);

  if (faseHome == 1 && digitalRead(HOME_PIN) != HOME_ACTIVO) {
    pararAhora();
    faseHome = 2;
    motor.setTargetPositionRelativeInSteps(-MAX_HOME);
  } else if (faseHome == 2 && digitalRead(HOME_PIN) == HOME_ACTIVO) {
    motor.setCurrentPositionAsHomeAndStop();
    referenciado = true;
    faseHome = 0;
  }
  motor.processMovement(); // Movimiento no bloqueante: I2C sigue disponible.
  if (faseHome && motor.motionComplete()) {
    faseHome = 0;
    resultado = HOME_NO_ENCONTRADO;
    referenciado = false;
  }
  if (hayOrden || millis() - ultimoEstado >= 10) {
    ultimoEstado = millis();
    publicarEstado();
  }
  delay(1); // Cede CPU; ejemplo ajustado a 150 pasos/s.
}
