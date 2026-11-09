// Maestro de prueba ESP32-S3: solo USB serial e I2C hacia el minirevolver.
#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "Protocolo.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Selecciona ESP32-S3 para este maestro de prueba."
#endif

// Si se usa otro PCB revolver como maestro, corresponden a su POGO.
constexpr int SDA_PIN = 6, SCL_PIN = 7;
uint8_t secuencia = 0, secuenciaPendiente = 0;
bool esperando = false;
bool secuenciaInicializada = false;
uint32_t envioMs = 0, consultaMs = 0, impresionMs = 0;
char linea[48];
size_t longitud = 0;
bool desbordada = false;

const char *nombreResultado(uint8_t valor) {
  switch (valor) {
    case RESULTADO_OK: return "OK";
    case OCUPADO: return "OCUPADO";
    case FUERA_RANGO: return "FUERA_RANGO";
    case SIN_ORIGEN: return "SIN_ORIGEN: ejecutar HOME";
    case HOME_NO_ENCONTRADO: return "HOME_NO_ENCONTRADO";
    case OPERACION_INVALIDA: return "OPERACION_INVALIDA";
    case TRAMA_INVALIDA: return "TRAMA_INVALIDA";
    case RTC_NO_DISPONIBLE: return "RTC_NO_DISPONIBLE";
    case ERROR_SD: return "ERROR_SD";
    case FECHA_NO_VALIDA: return "FECHA_NO_VALIDA: enviar fDD/MM/AAAA y luego hHH:MM:SS";
    default: return "DESCONOCIDO";
  }
}

bool consultar(uint8_t *p) {
  if (Wire.requestFrom(uint8_t(DIRECCION_REVOLVER), size_t(TAM_ESTADO), true) != TAM_ESTADO) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < TAM_ESTADO; ++i) p[i] = Wire.read();
  return p[0] == 0x5A && p[1] == VERSION_PROTOCOLO && crc8(p, TAM_ESTADO - 1) == p[TAM_ESTADO - 1];
}

void mostrarEstado(const uint8_t *p) {
  if (p[2] & FLAG_HORA_VALIDA) {
    // Aritmetica uint32: tambien funciona despues de 2038 con cores time_t de 32 bits.
    uint32_t epoch = leerUint32(p + 13);
    uint32_t dias = (epoch - UNIX_MIN) / 86400;
    unsigned anio = 2000, mes = 1;
    while (dias >= unsigned(anio % 4 == 0 ? 366 : 365)) {
      dias -= anio % 4 == 0 ? 366 : 365;
      ++anio;
    }
    const uint8_t diasMes[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while (mes < 12) {
      unsigned limite = diasMes[mes - 1] + (mes == 2 && anio % 4 == 0);
      if (dias < limite) break;
      dias -= limite;
      ++mes;
    }
    char texto[64];
    snprintf(texto, sizeof(texto), "Fecha=%02u/%02u/%04u hora=%02u:%02u:%02u UTC ",
             unsigned(dias + 1), mes, anio, unsigned((epoch / 3600) % 24),
             unsigned((epoch / 60) % 60), unsigned(epoch % 60));
    Serial.print(texto);
  } else Serial.print("Fecha=--/--/---- hora=--:--:-- ");
  Serial.print("Posicion="); Serial.print(leerInt32(p + 5));
  Serial.print(" destino="); Serial.print(leerInt32(p + 9));
  Serial.print(" moviendo="); Serial.print(bool(p[2] & FLAG_MOVIENDO));
  Serial.print(" homing="); Serial.print(bool(p[2] & FLAG_HOME));
  Serial.print(" origen="); Serial.print(bool(p[2] & FLAG_REFERENCIADO));
  Serial.print(" switch="); Serial.print(bool(p[2] & FLAG_SWITCH));
  Serial.print(" RTC="); Serial.print(bool(p[2] & FLAG_RTC));
  Serial.print(" horaValida="); Serial.print(bool(p[2] & FLAG_HORA_VALIDA));
  Serial.print(" unixUTC="); Serial.print(leerUint32(p + 13));
  Serial.print(" SD="); Serial.print(bool(p[2] & FLAG_SD));
  Serial.print(" errorSD="); Serial.print(bool(p[2] & FLAG_ERROR_SD));
  Serial.print(" logsPerdidos="); Serial.print(leerUint32(p + 17));
  Serial.print(" resultado="); Serial.println(nombreResultado(p[3]));
}

void enviar(uint8_t operacion, uint32_t argumento = 0) {
  if (esperando && operacion != PARAR) {
    Serial.println("Esperando confirmacion. S sigue disponible.");
    return;
  }
  // Leer antes de enviar evita reutilizar la ultima secuencia si el maestro reinicia.
  uint8_t estado[TAM_ESTADO];
  if (!consultar(estado)) {
    Serial.println("Slave 0x30 sin respuesta valida; orden no enviada.");
    return;
  }
  // Sincronizar solo al arrancar; tras un timeout no reutilizar la secuencia enviada.
  if (!secuenciaInicializada) {
    secuencia = estado[4];
    secuenciaInicializada = true;
  }
  uint8_t p[TAM_ORDEN] = {0xA5, VERSION_PROTOCOLO, operacion};
  p[3] = ++secuencia;
  ponerUint32(p + 4, argumento);
  p[9] = crc8(p, 9);
  Wire.beginTransmission(DIRECCION_REVOLVER);
  Wire.write(p, TAM_ORDEN);
  uint8_t error = Wire.endTransmission();
  if (error) {
    Serial.print("Error I2C: "); Serial.println(error);
    Serial.println("Ejecucion incierta; consultar E antes de repetir.");
    return;
  }
  secuenciaPendiente = p[3];
  esperando = true;
  envioMs = millis();
  Serial.print("Orden enviada, secuencia "); Serial.println(p[3]);
}

void ayuda() {
  Serial.println("h / ? = ayuda | HOME = buscar origen | 1/2/3 = 150/350/550 pasos | M pasos = destino absoluto 0..2000");
  Serial.println("fDD/MM/AAAA = fecha (2000..2099) | hHH:MM:SS = hora 24 h UTC");
  Serial.println("Inicializar: f09/11/2026 y luego h11:11:11; usar fecha/hora actuales. Esperar confirmacion entre ordenes.");
  Serial.println("f conserva hora valida; si no existe inicia a las 00:00:00. h conserva fecha valida.");
  Serial.println("RTC = consultar | RTC unixUTC = ajustar hora (2000..2099) | SD = remontar y probar registro");
  Serial.println("S = parar | E = estado | ? = ayuda. Enviar con Nueva linea.");
}

void comando(char *s) {
  while (isspace(static_cast<unsigned char>(*s))) ++s;
  size_t n = strlen(s);
  while (n && isspace(static_cast<unsigned char>(s[n - 1]))) s[--n] = '\0';
  for (size_t i = 0; i < n; ++i) s[i] = toupper(static_cast<unsigned char>(s[i]));
  if (!n) return;
  if (!strcmp(s, "H") || !strcmp(s, "?")) ayuda();
  else if (!strcmp(s, "HOME")) enviar(HOME);
  else if (!strcmp(s, "S") || !strcmp(s, "STOP")) enviar(PARAR);
  else if (s[0] == 'F') {
    uint32_t fecha;
    if (parsearFecha(s, fecha)) enviar(AJUSTAR_FECHA, fecha);
    else Serial.println("Fecha invalida: usar fDD/MM/AAAA (2000..2099).");
  }
  else if (s[0] == 'H') {
    uint32_t segundos;
    if (parsearHora(s, segundos)) enviar(AJUSTAR_HORA, segundos);
    else Serial.println("Hora invalida: usar hHH:MM:SS (00:00:00..23:59:59).");
  }
  else if (!strcmp(s, "SD")) enviar(PROBAR_SD);
  else if (!strncmp(s, "RTC ", 4)) {
    char *inicio = s + 4;
    while (isspace(static_cast<unsigned char>(*inicio))) ++inicio;
    char *fin;
    errno = 0;
    unsigned long epoch = strtoul(inicio, &fin, 10);
    if (*inicio >= '0' && *inicio <= '9' && fin != inicio && *fin == '\0' &&
        errno != ERANGE && epoch >= UNIX_MIN && epoch <= UNIX_MAX)
      enviar(AJUSTAR_RTC, uint32_t(epoch));
    else Serial.println("Usar RTC unixUTC, entre 946684800 y 4102444799.");
  }
  else if (!strcmp(s, "E") || !strcmp(s, "RTC")) {
    uint8_t estado[TAM_ESTADO];
    if (consultar(estado)) mostrarEstado(estado);
    else Serial.println("Slave 0x30 sin respuesta valida.");
  } else if (n == 1 && s[0] >= '1' && s[0] <= '3') {
    const int32_t posiciones[] = {150, 350, 550};
    enviar(MOVER, posiciones[s[0] - '1']);
  } else if (!strncmp(s, "M ", 2)) {
    char *fin;
    errno = 0;
    long pasos = strtol(s + 2, &fin, 10);
    if (fin != s + 2 && *fin == '\0' && errno != ERANGE && pasos >= 0 && pasos <= 2000)
      enviar(MOVER, uint32_t(pasos));
    else Serial.println("Usar M pasos, entre 0 y 2000.");
  } else Serial.println("Comando invalido. Escribe ?.");
}

void atenderSerial() {
  // Presupuesto por vuelta para seguir consultando el slave durante la entrada.
  for (uint8_t i = 0; i < 64 && Serial.available(); ++i) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (desbordada) Serial.println("Comando demasiado largo; descartado.");
      else { linea[longitud] = '\0'; comando(linea); }
      longitud = 0;
      desbordada = false;
    } else if (!desbordada) {
      if (longitud < sizeof(linea) - 1) linea[longitud++] = c;
      else desbordada = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  if (!Wire.begin(SDA_PIN, SCL_PIN, uint32_t(100000))) {
    Serial.println("No se pudo iniciar I2C master.");
    while (true) delay(1000);
  }
  Wire.setTimeOut(100);
  delay(1000);
  Serial.println("Master: SDA=6 SCL=7; slave=0x30. Sin ordenes automaticas al arrancar.");
  ayuda();
}

void loop() {
  atenderSerial();
  if (millis() - consultaMs >= 250) {
    consultaMs = millis();
    uint8_t estado[TAM_ESTADO];
    if (consultar(estado)) {
      if (esperando && estado[4] == secuenciaPendiente) {
        esperando = false;
        Serial.print("Orden procesada: "); Serial.println(nombreResultado(estado[3]));
      }
      if (millis() - impresionMs >= 5000) {
        impresionMs = millis();
        mostrarEstado(estado);
      }
    }
  }
  if (esperando && millis() - envioMs >= 2000) {
    esperando = false;
    Serial.println("Sin confirmacion: ejecucion incierta. Consultar E; no se reenvia.");
  }
  delay(1);
}
