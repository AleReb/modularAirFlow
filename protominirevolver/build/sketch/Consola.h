#line 1 "C:\\Users\\Ale\\Downloads\\modularAirFlow\\protominirevolver\\Consola.h"
#pragma once
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

// Cola de salida: nunca esperar al USB mientras se generan pasos.
char salidaConsola[2048];
size_t inicioConsola = 0, finConsola = 0;

void decirConsola(const char *texto) {
  size_t n = strlen(texto);
  size_t libre = (inicioConsola + sizeof(salidaConsola) - finConsola - 1) % sizeof(salidaConsola);
  if (n + 1 > libre) return;
  for (size_t i = 0; i < n; ++i) {
    salidaConsola[finConsola] = texto[i];
    finConsola = (finConsola + 1) % sizeof(salidaConsola);
  }
  salidaConsola[finConsola] = '\n';
  finConsola = (finConsola + 1) % sizeof(salidaConsola);
}

void transmitirConsola() {
  if (!Serial || inicioConsola == finConsola) return;
  int libre = Serial.availableForWrite();
  if (libre <= 0) return;
  size_t n = finConsola > inicioConsola ? finConsola - inicioConsola : sizeof(salidaConsola) - inicioConsola;
  if (n > 32) n = 32;
  if (n > size_t(libre)) n = size_t(libre);
  size_t enviados = Serial.write(reinterpret_cast<const uint8_t *>(salidaConsola + inicioConsola), n);
  inicioConsola = (inicioConsola + enviados) % sizeof(salidaConsola);
}

const char *resultadoConsola(uint8_t r) {
  switch (r) {
    case RESULTADO_OK: return "OK";
    case OCUPADO: return "OCUPADO";
    case FUERA_RANGO: return "FUERA_RANGO";
    case SIN_ORIGEN: return "SIN_ORIGEN: ejecutar HOME";
    case HOME_NO_ENCONTRADO: return "HOME_NO_ENCONTRADO";
    case RTC_NO_DISPONIBLE: return "RTC_NO_DISPONIBLE";
    case ERROR_SD: return "ERROR_SD";
    default: return "ERROR";
  }
}

void estadoConsola() {
  char texto[240];
  DateTime fecha(horaUnix());
  snprintf(texto, sizeof(texto),
           "RTC=%u hora_valida=%u fecha=%04u-%02u-%02u hora=%02u:%02u:%02u (UTC) SD=%u error_SD=%u pos=%ld destino=%ld mov=%u HOME=%u origen=%u",
           unsigned(rtcListo), unsigned(horaValida), unsigned(fecha.year()), unsigned(fecha.month()),
           unsigned(fecha.day()), unsigned(fecha.hour()), unsigned(fecha.minute()), unsigned(fecha.second()),
           unsigned(sdLista), unsigned(errorSD), motor.getCurrentPositionInSteps(),
           motor.getTargetPositionInSteps(), unsigned(!motor.motionComplete() || faseHome),
           unsigned(digitalRead(HOME_PIN) == HOME_ACTIVO), unsigned(referenciado));
  decirConsola(texto);
  if (!horaValida) decirConsola("Fecha/hora no validas: inicializar con RTC unixUTC.");
}

void ordenConsola(uint8_t op, uint32_t argumento) {
  static uint8_t secuenciaSerial = 0;
  uint8_t p[TAM_ORDEN] = {0xA5, VERSION_PROTOCOLO, op, ++secuenciaSerial};
  ponerUint32(p + 4, argumento);
  // No confirmar accidentalmente una secuencia pendiente del maestro POGO.
  uint8_t secuenciaPogo = ultimaSecuencia, resultadoPogo = resultado;
  ejecutar(p);
  registrarEvento("SERIAL", p[3], op, resultado,
                  motor.getCurrentPositionInSteps(), motor.getTargetPositionInSteps());
  decirConsola(resultadoConsola(resultado));
  ultimaSecuencia = secuenciaPogo;
  resultado = resultadoPogo;
  publicarEstado();
  if (op == AJUSTAR_RTC) estadoConsola();
}

bool numeroConsola(const char *s, uint32_t minimo, uint32_t maximo, uint32_t &valor) {
  if (!*s) return false;
  for (const char *c = s; *c; ++c) if (*c < '0' || *c > '9') return false;
  errno = 0;
  char *fin;
  unsigned long n = strtoul(s, &fin, 10);
  if (*fin || errno == ERANGE || n < minimo || n > maximo) return false;
  valor = uint32_t(n);
  return true;
}

void comandoConsola(char *s) {
  while (isspace(static_cast<unsigned char>(*s))) ++s;
  size_t n = strlen(s);
  while (n && isspace(static_cast<unsigned char>(s[n - 1]))) s[--n] = 0;
  for (size_t i = 0; i < n; ++i) s[i] = toupper(static_cast<unsigned char>(s[i]));
  if (!n) return;
  if (!strcmp(s, "H") || !strcmp(s, "?")) {
    decirConsola("h / ? = ayuda | Enviar comandos con Enter (NL o CR).");
    decirConsola("hHH:MM:SS = hora 24 h, conserva fecha valida del RTC (UTC).");
    decirConsola("HOME = buscar origen | S / STOP = parar | M pasos = mover absoluto 0..2000");
    decirConsola("1 / 2 / 3 = mover a 150 / 350 / 550 pasos | E / RTC = estado");
    decirConsola("RTC unixUTC = ajustar fecha y hora (2000..2099) | SD = remontar y probar SD");
  } else if (s[0] == 'H' && strcmp(s, "HOME")) {
    bool formato = n == 9 && s[3] == ':' && s[6] == ':';
    for (size_t i = 1; formato && i < 9; ++i)
      if (i != 3 && i != 6 && (s[i] < '0' || s[i] > '9')) formato = false;
    if (!formato) { decirConsola("Usar hHH:MM:SS, por ejemplo h11:11:11."); return; }
    uint8_t hh = (s[1] - '0') * 10 + s[2] - '0';
    uint8_t mm = (s[4] - '0') * 10 + s[5] - '0';
    uint8_t ss = (s[7] - '0') * 10 + s[8] - '0';
    if (hh > 23 || mm > 59 || ss > 59) { decirConsola("Hora fuera de rango: 00:00:00..23:59:59."); return; }
    if (!motor.motionComplete() || faseHome) { decirConsola("OCUPADO"); return; }
    leerReloj();
    if (!rtcListo) { decirConsola("RTC_NO_DISPONIBLE"); return; }
    if (!horaValida) { decirConsola("Fecha no confiable: usar primero RTC unixUTC para fijar fecha y hora."); return; }
    DateTime fecha(horaUnix());
    DateTime nueva(fecha.year(), fecha.month(), fecha.day(), hh, mm, ss);
    ordenConsola(AJUSTAR_RTC, nueva.unixtime());
  } else if (!strcmp(s, "HOME")) ordenConsola(HOME, 0);
  else if (!strcmp(s, "S") || !strcmp(s, "STOP")) ordenConsola(PARAR, 0);
  else if (!strcmp(s, "SD")) ordenConsola(PROBAR_SD, 0);
  else if (!strcmp(s, "E") || !strcmp(s, "RTC")) estadoConsola();
  else if (n == 1 && s[0] >= '1' && s[0] <= '3') {
    const uint32_t posiciones[] = {150, 350, 550};
    ordenConsola(MOVER, posiciones[s[0] - '1']);
  } else {
    uint32_t valor;
    if (!strncmp(s, "M ", 2) && numeroConsola(s + 2, 0, MAX_POSICION, valor)) ordenConsola(MOVER, valor);
    else if (!strncmp(s, "RTC ", 4) && numeroConsola(s + 4, UNIX_MIN, UNIX_MAX, valor)) ordenConsola(AJUSTAR_RTC, valor);
    else decirConsola("Comando invalido o fuera de rango. Escribe h y Enter.");
  }
}

void atenderConsola() {
  static char linea[48];
  static size_t longitud = 0;
  static bool desbordada = false;
  // Lectura incremental; no readString ni esperas por fin de linea.
  for (uint8_t i = 0; i < 16 && Serial.available(); ++i) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (desbordada) decirConsola("Comando demasiado largo; descartado.");
      else { linea[longitud] = 0; comandoConsola(linea); }
      longitud = 0;
      desbordada = false;
      break; // Como maximo una orden por vuelta.
    } else if (!desbordada) {
      if (longitud < sizeof(linea) - 1) linea[longitud++] = c;
      else desbordada = true;
    }
  }
}
