// Modulo I2C: recibe ordenes y genera los pasos localmente.
#include <Arduino.h>
#include <Wire.h>
#include <ESP_FlexyStepper.h>
#include <string.h>
#include "Protocolo.h"
#include "Perifericos.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Selecciona ESP32-S3: este sketch usa los pines del PCB revolver."
#endif

// PCB pcb/revolver: JP3 motor, JP2 servicio/HOME y POGO I2C.
constexpr int STEP_PIN = 3, DIR_PIN = 4, HOME_PIN = 1;
constexpr int ENA_PIN = 2;
constexpr int SDA_PIN = 6, SCL_PIN = 7;
// Ajustar si el driver conectado habilita con nivel HIGH.
constexpr int ENA_ACTIVO = LOW;

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
uint8_t resultado = RESULTADO_OK, ultimaSecuencia = 0;
uint8_t secuenciaMovimiento = 0, operacionMovimiento = 0;
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
  if (rtcListo) p[2] |= FLAG_RTC;
  if (horaValida) p[2] |= FLAG_HORA_VALIDA;
  if (sdLista) p[2] |= FLAG_SD;
  if (errorSD) p[2] |= FLAG_ERROR_SD;
  ponerUint32(p + 13, horaUnix());
  ponerUint32(p + 17, eventosPerdidos);
  p[TAM_ESTADO - 1] = crc8(p, TAM_ESTADO - 1);
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

void habilitarMotor() {
  digitalWrite(ENA_PIN, ENA_ACTIVO);
  delayMicroseconds(20); // Tiempo de establecimiento antes del primer paso.
}

void ejecutar(const uint8_t *p) {
  ultimaSecuencia = p[3];
  resultado = RESULTADO_OK;
  int32_t destino = leerInt32(p + 4);
  switch (p[2]) {
    case PARAR:
      if (faseHome) referenciado = false;
      faseHome = 0;
      pararAhora();
      break;
    case HOME:
      if (!motor.motionComplete() || faseHome) { resultado = OCUPADO; break; }
      habilitarMotor();
      secuenciaMovimiento = ultimaSecuencia;
      operacionMovimiento = HOME;
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
      habilitarMotor();
      secuenciaMovimiento = ultimaSecuencia;
      operacionMovimiento = MOVER;
      motor.setTargetPositionInSteps(destino);
      break;
    case AJUSTAR_RTC: {
      if (!motor.motionComplete() || faseHome) { resultado = OCUPADO; break; }
      uint32_t epoch = leerUint32(p + 4);
      if (epoch < UNIX_MIN || epoch > UNIX_MAX) { resultado = FUERA_RANGO; break; }
      if (!ajustarReloj(epoch)) resultado = RTC_NO_DISPONIBLE;
      break;
    }
    case PROBAR_SD:
      if (!motor.motionComplete() || faseHome) { resultado = OCUPADO; break; }
      if (!montarSD()) { resultado = ERROR_SD; break; }
      registrarEvento("SD_PRUEBA", ultimaSecuencia, PROBAR_SD, RESULTADO_OK,
                      motor.getCurrentPositionInSteps(), motor.getTargetPositionInSteps());
      if (!guardarEvento()) resultado = ERROR_SD;
      break;
    default: resultado = OPERACION_INVALIDA; break;
  }
}

void setup() {
  // Deshabilitado al arrancar; se habilita al aceptar HOME o MOVER.
  digitalWrite(ENA_PIN, !ENA_ACTIVO);
  pinMode(ENA_PIN, OUTPUT);
  Serial.begin(115200);
  pinMode(HOME_PIN, INPUT_PULLUP);
  motor.connectToPins(STEP_PIN, DIR_PIN);
  motor.setSpeedInStepsPerSecond(150);
  motor.setAccelerationInStepsPerSecondPerSecond(500);
  motor.setDecelerationInStepsPerSecondPerSecond(250);
  iniciarPerifericos();
  registrarEvento("ARRANQUE", 0, 0, RESULTADO_OK, 0, 0);
  publicarEstado();
  Wire.onReceive(recibir);
  Wire.onRequest(responder);
  if (!Wire.begin(uint8_t(DIRECCION_REVOLVER), SDA_PIN, SCL_PIN, uint32_t(100000))) {
    Serial.println("No se pudo iniciar I2C POGO; motor deshabilitado.");
    while (true) delay(1000);
  }
  Serial.println("Prototipo slave 0x30: POGO SDA=6 SCL=7; STEP=3 DIR=4 ENA=2 HOME=1.");
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
  if (hayOrden) {
    ejecutar(p);
    registrarEvento("ORDEN", ultimaSecuencia, p[2], resultado,
                    motor.getCurrentPositionInSteps(), motor.getTargetPositionInSteps());
  }
  bool estabaMoviendo = !motor.motionComplete() || faseHome;

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
  bool detenido = motor.motionComplete() && !faseHome;
  if (estabaMoviendo && detenido) {
    uint8_t resultadoMovimiento = operacionMovimiento == HOME && !referenciado
                                   ? HOME_NO_ENCONTRADO : RESULTADO_OK;
    registrarEvento("FIN", secuenciaMovimiento, operacionMovimiento, resultadoMovimiento,
                    motor.getCurrentPositionInSteps(), motor.getTargetPositionInSteps());
  }
  // Ningun acceso a SD/RTC durante el movimiento ni desde callbacks I2C.
  // La hora publicada mientras gira se extrapola desde la ultima lectura del RTC.
  if (detenido) {
    if (millis() - consultaRtcMs >= 1000) leerReloj();
    guardarEvento(); // Una fila por vuelta; conservar cola si la SD falla.
  }
  if (hayOrden || millis() - ultimoEstado >= 10) {
    ultimoEstado = millis();
    publicarEstado();
  }
  delay(1); // Cede CPU; ejemplo ajustado a 150 pasos/s.
}
