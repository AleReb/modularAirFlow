// Pines de control
const int R_EN = 5; // Enable del driver; nivel GPIO de 3.3 V en ESP32
const int L_EN = 8; // Enable del driver; nivel GPIO de 3.3 V en ESP32
const int RPWM = 6; // Pin PWM giro derecha (debe soportar PWM)
const int LPWM = 9; // Pin PWM giro izquierda (debe soportar PWM)
void setup() {
  // Configurar pines como salidas
  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  
  // Habilitar el módulo en ambas direcciones
  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);
}
void loop() {
  // 1. Girar a la derecha aumentando velocidad
  for (int speed = 0; speed <= 255; speed++) {
    analogWrite(RPWM, speed);
    analogWrite(LPWM, 0); // Sentido opuesto apagado
    delay(15);
  }
  delay(2000); // Mantener velocidad máxima 2 segundos
  
  // Detener suavemente
  for (int speed = 255; speed >= 0; speed--) {
    analogWrite(RPWM, speed);
    analogWrite(LPWM, 0);
    delay(15);
  }
  delay(1000); // Pausa detenido
  
  // 2. Girar a la izquierda aumentando velocidad
  for (int speed = 0; speed <= 255; speed++) {
    analogWrite(RPWM, 0); // Sentido opuesto apagado
    analogWrite(LPWM, speed);
    delay(15);
  }
  delay(2000);
  
  // Apagar motor antes del siguiente bucle
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  delay(2000);
}
