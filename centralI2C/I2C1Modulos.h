#pragma once
// Bus fisico I2C 1: minirevolver 0x30 y futuros modulos.
TwoWire busModulos(1);
constexpr int MODULOS_SDA = 10, MODULOS_SCL = 11;
bool modulosListos = false;
uint8_t estado[TAM_ESTADO] = {};
bool moduloValido = false;
uint8_t secuencia = 0, secuenciaPendiente = 0;
bool esperando = false, secuenciaInicializada = false;
uint32_t envioMs = 0;

bool consultarModulo() {
  if (!modulosListos) return false;
  if (busModulos.requestFrom(DIRECCION_REVOLVER, TAM_ESTADO, true) != TAM_ESTADO) {
    while (busModulos.available()) busModulos.read();
    return false;
  }
  uint8_t p[TAM_ESTADO];
  for (size_t i = 0; i < TAM_ESTADO; ++i) p[i] = busModulos.read();
  if (p[0] != 0x5A || p[1] != VERSION_PROTOCOLO || crc8(p, 13) != p[13]) return false;
  memcpy(estado, p, TAM_ESTADO);
  if (!secuenciaInicializada) { secuencia = p[4]; secuenciaInicializada = true; }
  if (esperando && p[4] == secuenciaPendiente) {
    esperando = false;
    Serial.print("REV recibio orden "); Serial.print(p[4]);
    Serial.print("; resultado = "); Serial.println(p[3]);
    // La confirmacion no significa que el movimiento haya terminado.
  }
  return true;
}

void enviarOrden(uint8_t operacion, int32_t argumento = 0) {
  if (!modulosListos) { Serial.println("Bus de modulos no inicializado."); return; }
  if (esperando && operacion != PARAR) {
    Serial.println("Esperando confirmacion. REV STOP sigue disponible."); return;
  }
  // Exigir al menos una respuesta valida para sincronizar la secuencia inicial.
  if (!secuenciaInicializada && !consultarModulo()) {
    Serial.println("Minirevolver no responde; orden no enviada."); return;
  }
  uint8_t p[TAM_ORDEN] = {0xA5, VERSION_PROTOCOLO, operacion};
  p[3] = ++secuencia;
  ponerInt32(p + 4, argumento);
  p[9] = crc8(p, 9);
  busModulos.beginTransmission(DIRECCION_REVOLVER);
  busModulos.write(p, TAM_ORDEN);
  uint8_t error = busModulos.endTransmission();
  if (error) {
    Serial.print("Error de envio I2C: "); Serial.println(error);
    Serial.println("Estado de ejecucion incierto; consultar antes de repetir.");
    return;
  }
  secuenciaPendiente = p[3];
  envioMs = millis();
  esperando = true;
  Serial.print("Orden enviada, secuencia "); Serial.println(p[3]);
}

