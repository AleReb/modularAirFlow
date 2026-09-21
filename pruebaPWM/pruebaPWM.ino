#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char linea[32];
size_t longitud = 0;
bool desbordada = false;

// Prueba aislada para ESP32-C3 con Arduino-ESP32 2.0.17.
// USB nativo: activar USB CDC On Boot. Monitor: 115200.
constexpr int PWM_PIN = 9;
constexpr uint32_t PWM_DUTY = 128; // Aproximadamente 50 %.

#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Activa USB CDC On Boot para usar el monitor por USB nativo."
#endif

void procesarLinea(char *texto) {
  while (isspace(static_cast<unsigned char>(*texto))) ++texto;
  size_t largo = strlen(texto);
  while (largo && isspace(static_cast<unsigned char>(texto[largo - 1]))) {
    texto[--largo] = '\0';
  }
  if (!largo) return;
  for (size_t i = 0; i < largo; ++i) {
    texto[i] = toupper(static_cast<unsigned char>(texto[i]));
  }
  int valor = 0;
  if (strcmp(texto, "STOP") != 0) {
    if (strncmp(texto, "PWM ", 4) == 0) texto += 4;
    while (*texto == ' ') ++texto;
    size_t digitos = strspn(texto, "0123456789");
    if (digitos == 0 || digitos > 3 || texto[digitos] != '\0') {
      Serial.println("Envia 0..255, PWM 128 o STOP, con Nueva linea.");
      return;
    }
    valor = atoi(texto);
    if (valor > 255) {
      Serial.println("Rango permitido: 0..255.");
      return;
    }
  }
  analogWrite(PWM_PIN, valor);
  Serial.print("PWM GPIO9 = ");
  Serial.print(valor);
  Serial.println("/255");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Como en Fade: usar analogWrite con su configuracion predeterminada.
  // En el core 2.0.17: 8 bits (0..255), frecuencia predeterminada de 1 kHz.
  pinMode(PWM_PIN, OUTPUT);
  analogWrite(PWM_PIN, PWM_DUTY);

  Serial.print("analogWrite en GPIO9; duty: ");
  Serial.print(PWM_DUTY);
  Serial.println("/255.");
  Serial.println("Envia 0..255 (o PWM 128) con Nueva linea. STOP = 0.");
}

void loop() {
  // El PWM continua mientras recibimos comandos.
  while (Serial.available()) {
    char caracter = Serial.read();
    if (caracter == '\n' || caracter == '\r') {
      if (desbordada) {
        Serial.println("Comando demasiado largo: descartado.");
      } else {
        linea[longitud] = '\0';
        procesarLinea(linea);
      }
      longitud = 0;
      desbordada = false;
    } else if (!desbordada) {
      if (longitud < sizeof(linea) - 1) linea[longitud++] = caracter;
      else desbordada = true;
    }
  }
}
