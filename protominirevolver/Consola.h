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
    case FECHA_NO_VALIDA: return "Fecha no confiable: enviar fDD/MM/AAAA y luego hHH:MM:SS.";
    default: return "ERROR";
  }
}

void estadoConsola() {
  char texto[280];
  char fechaHora[48] = "fecha=--/--/---- hora=--:--:--";
  if (horaValida) {
    DateTime fecha(horaUnix());
    snprintf(fechaHora, sizeof(fechaHora), "fecha=%02u/%02u/%04u hora=%02u:%02u:%02u",
             unsigned(fecha.day()), unsigned(fecha.month()), unsigned(fecha.year()),
             unsigned(fecha.hour()), unsigned(fecha.minute()), unsigned(fecha.second()));
  }
  snprintf(texto, sizeof(texto),
           "[estado] ms=%lu I2C_iniciado=%u RTC=%u hora_valida=%u %s (UTC) SD=%u error_SD=%u pos=%ld destino=%ld mov=%u HOME=%u origen=%u",
           static_cast<unsigned long>(millis()), unsigned(pogo), unsigned(rtcListo), unsigned(horaValida), fechaHora,
           unsigned(sdLista), unsigned(errorSD), motor.getCurrentPositionInSteps(),
           motor.getTargetPositionInSteps(), unsigned(!motor.motionComplete() || faseHome),
           unsigned(digitalRead(HOME_PIN) == HOME_ACTIVO), unsigned(referenciado));
  decirConsola(texto);
  if (!horaValida) decirConsola("Inicializar: fDD/MM/AAAA y luego hHH:MM:SS. Escribe h para ayuda.");
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
  if (op == AJUSTAR_RTC || op == AJUSTAR_FECHA || op == AJUSTAR_HORA) estadoConsola();
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
    decirConsola("fDD/MM/AAAA = fecha, 2000..2099. Ejemplo: f09/11/2026");
    decirConsola("Primera vez: f09/11/2026 y luego h11:11:11 (usar fecha/hora actuales).");
    decirConsola("f conserva hora valida; si no existe, inicia a las 00:00:00.");
    decirConsola("hHH:MM:SS = hora 24 h, conserva fecha valida del RTC (UTC).");
    decirConsola("HOME = buscar origen | S / STOP = parar | M pasos = mover absoluto 0..2000");
    decirConsola("1 / 2 / 3 = mover a 150 / 350 / 550 pasos | E / RTC = estado");
    decirConsola("RTC unixUTC = ajustar fecha y hora (2000..2099) | SD = remontar y probar SD");
  } else if (s[0] == 'F') {
    uint32_t fecha;
    if (parsearFecha(s, fecha)) ordenConsola(AJUSTAR_FECHA, fecha);
    else decirConsola("Fecha invalida. Usar fDD/MM/AAAA, por ejemplo f09/11/2026 (2000..2099).");
  } else if (s[0] == 'H' && strcmp(s, "HOME")) {
    uint32_t segundos;
    if (parsearHora(s, segundos)) ordenConsola(AJUSTAR_HORA, segundos);
    else decirConsola("Hora invalida. Usar hHH:MM:SS, entre h00:00:00 y h23:59:59.");
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
