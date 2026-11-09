#pragma once
#include <Arduino.h>
#include <string.h>

// Mismos valores del protocolo en ambos sketches. Bytes explicitos, sin enviar structs.
constexpr uint8_t DIRECCION_REVOLVER = 0x30;
constexpr uint8_t VERSION_PROTOCOLO = 2;
constexpr size_t TAM_ORDEN = 10;
constexpr size_t TAM_ESTADO = 22;
enum Operacion : uint8_t { MOVER = 1, HOME = 2, PARAR = 3, AJUSTAR_RTC = 4, PROBAR_SD = 5,
  AJUSTAR_FECHA = 6, AJUSTAR_HORA = 7 };
enum Resultado : uint8_t { RESULTADO_OK = 0, OCUPADO = 1, FUERA_RANGO = 2,
  SIN_ORIGEN = 3, HOME_NO_ENCONTRADO = 4, OPERACION_INVALIDA = 5,
  TRAMA_INVALIDA = 6, RTC_NO_DISPONIBLE = 7, ERROR_SD = 8, FECHA_NO_VALIDA = 9 };
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

// AJUSTAR_FECHA: argumento decimal AAAAMMDD. AJUSTAR_HORA: segundos desde medianoche.
inline bool fechaPermitida(uint32_t fecha) {
  uint32_t anio = fecha / 10000, mes = (fecha / 100) % 100, dia = fecha % 100;
  if (anio < 2000 || anio > 2099 || mes < 1 || mes > 12 || dia < 1) return false;
  const uint8_t dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return dia <= unsigned(dias[mes - 1] + (mes == 2 && anio % 4 == 0));
}

inline bool parsearFecha(const char *s, uint32_t &fecha) {
  if (strlen(s) != 11 || (s[0] != 'f' && s[0] != 'F') || s[3] != '/' || s[6] != '/') return false;
  for (uint8_t i = 1; i < 11; ++i)
    if (i != 3 && i != 6 && (s[i] < '0' || s[i] > '9')) return false;
  uint32_t dia = (s[1] - '0') * 10 + s[2] - '0';
  uint32_t mes = (s[4] - '0') * 10 + s[5] - '0';
  uint32_t anio = (s[7] - '0') * 1000 + (s[8] - '0') * 100 + (s[9] - '0') * 10 + s[10] - '0';
  fecha = anio * 10000 + mes * 100 + dia;
  return fechaPermitida(fecha);
}

inline bool parsearHora(const char *s, uint32_t &segundos) {
  if (strlen(s) != 9 || (s[0] != 'h' && s[0] != 'H') || s[3] != ':' || s[6] != ':') return false;
  for (uint8_t i = 1; i < 9; ++i)
    if (i != 3 && i != 6 && (s[i] < '0' || s[i] > '9')) return false;
  uint32_t hh = (s[1] - '0') * 10 + s[2] - '0';
  uint32_t mm = (s[4] - '0') * 10 + s[5] - '0';
  uint32_t ss = (s[7] - '0') * 10 + s[8] - '0';
  segundos = hh * 3600 + mm * 60 + ss;
  return hh < 24 && mm < 60 && ss < 60;
}

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
