#line 1 "C:\\Users\\Ale\\Downloads\\modularAirFlow\\protominirevolver\\Perifericos.h"
#pragma once
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <stdio.h>

// Bus interno maestro, independiente de Wire (slave POGO en controlador 0).
TwoWire busInterno(1);
RTC_DS3231 rtc;
constexpr int RTC_SDA = 8, RTC_SCL = 9;
constexpr int SD_CS = 10, SD_MOSI = 11, SD_CLK = 12, SD_MISO = 13;
bool busInternoListo = false, rtcListo = false, horaValida = false;
bool sdLista = false, errorSD = false;
uint32_t epochBase = 0, relojBaseMs = 0, consultaRtcMs = 0;

struct Evento {
  uint32_t epoch, ms;
  const char *nombre;
  uint8_t secuencia, operacion, resultado;
  int32_t posicion, destino;
};
constexpr uint8_t CAPACIDAD_EVENTOS = 32;
Evento eventos[CAPACIDAD_EVENTOS];
uint8_t inicioEventos = 0, cantidadEventos = 0;
uint32_t eventosPerdidos = 0;

uint32_t horaUnix() {
  return horaValida ? epochBase + uint32_t(millis() - relojBaseMs) / 1000 : 0;
}

bool detectarRTC() {
  if (!busInternoListo) return false;
  busInterno.beginTransmission(uint8_t(0x68));
  return busInterno.endTransmission() == 0;
}

void leerReloj() {
  consultaRtcMs = millis();
  rtcListo = detectarRTC();
  horaValida = false;
  if (!rtcListo) return;
  DateTime fecha = rtc.now();
  if (!rtc.lostPower() && fecha.isValid()) {
    epochBase = fecha.unixtime();
    relojBaseMs = millis();
    horaValida = epochBase >= UNIX_MIN && epochBase <= UNIX_MAX;
  }
}

bool ajustarReloj(uint32_t epoch) {
  if (!detectarRTC()) { rtcListo = horaValida = false; return false; }
  rtc.adjust(DateTime(epoch));
  leerReloj();
  // adjust() no devuelve error: verificar el valor leido, admitiendo un segundo.
  return horaValida && epochBase >= epoch && epochBase - epoch <= 1;
}

void registrarEvento(const char *nombre, uint8_t secuencia, uint8_t operacion,
                     uint8_t resultado, int32_t posicion, int32_t destino) {
  if (cantidadEventos == CAPACIDAD_EVENTOS) {
    ++eventosPerdidos; // Se conserva lo ya encolado; descartar el evento nuevo.
    return;
  }
  uint8_t indice = (inicioEventos + cantidadEventos) % CAPACIDAD_EVENTOS;
  eventos[indice] = {horaUnix(), millis(), nombre, secuencia, operacion,
                     resultado, posicion, destino};
  ++cantidadEventos;
}

bool montarSD() {
  SD.end();
  sdLista = SD.begin(SD_CS, SPI, 4000000) && SD.cardType() != CARD_NONE;
  errorSD = !sdLista;
  return sdLista;
}

// Llamar solo con motor detenido: abrir/escribir/cerrar puede tardar.
bool guardarEvento() {
  if (!sdLista || !cantidadEventos) return false;
  File archivo = SD.open("/revolver.csv", FILE_APPEND);
  if (!archivo) { sdLista = false; errorSD = true; return false; }
  bool correcto = true;
  if (archivo.size() == 0) {
    const char *cabecera = "unix_utc,millis,evento,secuencia,operacion,resultado,posicion,destino,perdidos\n";
    size_t n = strlen(cabecera);
    correcto = archivo.write(reinterpret_cast<const uint8_t *>(cabecera), n) == n;
  }
  const Evento &e = eventos[inicioEventos];
  char fila[160];
  int n = snprintf(fila, sizeof(fila), "%lu,%lu,%s,%u,%u,%u,%ld,%ld,%lu\n",
                   static_cast<unsigned long>(e.epoch), static_cast<unsigned long>(e.ms),
                   e.nombre, unsigned(e.secuencia), unsigned(e.operacion), unsigned(e.resultado),
                   static_cast<long>(e.posicion), static_cast<long>(e.destino),
                   static_cast<unsigned long>(eventosPerdidos));
  correcto = correcto && n > 0 && size_t(n) < sizeof(fila);
  if (correcto) correcto = archivo.write(reinterpret_cast<const uint8_t *>(fila), size_t(n)) == size_t(n);
  archivo.flush();
  correcto = correcto && !archivo.getWriteError();
  archivo.close();
  if (!correcto) { sdLista = false; errorSD = true; return false; }
  inicioEventos = (inicioEventos + 1) % CAPACIDAD_EVENTOS;
  --cantidadEventos;
  return true;
}

void iniciarPerifericos() {
  busInternoListo = busInterno.begin(RTC_SDA, RTC_SCL, uint32_t(100000));
  if (busInternoListo) busInterno.setTimeOut(50);
  if (busInternoListo && rtc.begin(&busInterno)) leerReloj();
  // Nunca sobrescribir la hora automaticamente con la fecha de compilacion.
  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  montarSD(); // No formatea la tarjeta.
  Serial.print("RTC detectado="); Serial.print(rtcListo);
  Serial.print(" hora valida="); Serial.print(horaValida);
  Serial.print(" SD lista="); Serial.println(sdLista);
}
