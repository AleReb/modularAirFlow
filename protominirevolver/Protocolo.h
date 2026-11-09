#pragma once
#include <Arduino.h>

// Mismos valores del protocolo en ambos sketches. Bytes explicitos, sin enviar structs.
constexpr uint8_t DIRECCION_REVOLVER = 0x30;
constexpr uint8_t VERSION_PROTOCOLO = 2;
constexpr size_t TAM_ORDEN = 10;
constexpr size_t TAM_ESTADO = 22;
enum Operacion : uint8_t { MOVER = 1, HOME = 2, PARAR = 3, AJUSTAR_RTC = 4, PROBAR_SD = 5 };
enum Resultado : uint8_t { RESULTADO_OK = 0, OCUPADO = 1, FUERA_RANGO = 2,
  SIN_ORIGEN = 3, HOME_NO_ENCONTRADO = 4, OPERACION_INVALIDA = 5,
  TRAMA_INVALIDA = 6, RTC_NO_DISPONIBLE = 7, ERROR_SD = 8 };
constexpr uint8_t FLAG_MOVIENDO = 1;
constexpr uint8_t FLAG_HOME = 2;
constexpr uint8_t FLAG_REFERENCIADO = 4;
constexpr uint8_t FLAG_SWITCH = 8;

constexpr uint8_t FLAG_RTC = 16;
constexpr uint8_t FLAG_HORA_VALIDA = 32;
constexpr uint8_t FLAG_SD = 64;
constexpr uint8_t FLAG_ERROR_SD = 128;
constexpr uint32_t UNIX_MIN = 946684800UL;  // 2000-01-01 UTC.
constexpr uint32_t UNIX_MAX = 4102444799UL; // 2099-12-31 UTC.

inline void ponerUint32(uint8_t *p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
inline uint32_t leerUint32(const uint8_t *p) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
  return v;
}

inline uint8_t crc8(const uint8_t *datos, size_t n) {
  uint8_t crc = 0;
  for (size_t i = 0; i < n; ++i) {
    crc ^= datos[i];
    for (uint8_t b = 0; b < 8; ++b)
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
  }
  return crc;
}
inline void ponerInt32(uint8_t *p, int32_t valor) {
  uint32_t v = static_cast<uint32_t>(valor);
  for (uint8_t i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
inline int32_t leerInt32(const uint8_t *p) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
  // Conversion explicita de complemento a dos.
  return (v & 0x80000000UL) ? int32_t(int64_t(v) - 0x100000000LL) : int32_t(v);
}
