// ******************************************************************
// *                                                                *
// *    Control por Monitor Serial: Posiciones 1, 2, 3 + AutoHome   *
// *                                                                *
// ******************************************************************

#include <ESP_FlexyStepper.h>

// Asignación de pines de E/S
const int MOTOR_STEP_PIN = 5;
const int MOTOR_DIRECTION_PIN = 23;
const int HOMING_SWITCH_PIN = 27;

// Crear el objeto del motor paso a paso
ESP_FlexyStepper stepper;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Conectar y configurar el motor a sus pines de E/S
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  // Configurar final de carrera
  pinMode(HOMING_SWITCH_PIN, INPUT_PULLUP);

  // Configurar velocidad y aceleración iniciales
  stepper.setSpeedInStepsPerSecond(200);
  stepper.setAccelerationInStepsPerSecondPerSecond(200);

  Serial.println();
  Serial.println("====================================");
  Serial.println("        SISTEMA LISTO");
  Serial.println("====================================");
  Serial.println("1 = Posicion 150");
  Serial.println("2 = Posicion 350");
  Serial.println("3 = Posicion 550");
  Serial.println("H = AutoHome");
  Serial.println("====================================");
}

void loop()
{
  // Verificar si hay datos disponibles en el monitor serial
  if (Serial.available() > 0) {

    char primerChar = Serial.peek(); // Lee el primer carácter sin sacarlo del buffer

    // ==============================================================
    // AUTO HOME
    // ==============================================================
    if (primerChar == 'H' || primerChar == 'h') {
      
      char comando = Serial.read(); // Consumimos la 'H'

      while (Serial.available() > 0) {
        Serial.read();
      }

      Serial.println();
      Serial.println("Iniciando AutoHome...");

      bool homingSuccess = stepper.moveToHomeInSteps(
                           -1,
                           150,
                           2000,
                           HOMING_SWITCH_PIN
                          );

      if (homingSuccess) {
        Serial.println("AutoHome completado.");
        Serial.println("Posicion actual = 0");
      }
      else {
        Serial.println("ERROR: No se detecto el final de carrera.");
      }

      Serial.println();
      Serial.println("Sistema listo.");

    } 
    // ==============================================================
    // MOVIMIENTOS FIJOS (1, 2 y 3)
    // ==============================================================
    else if (primerChar >= '1' && primerChar <= '3') {
      
      char comando = Serial.read(); // Consumimos el número

      while (Serial.available() > 0) {
        Serial.read();
      }

      long posicionDestino = 0;

      if (comando == '1') {
        posicionDestino = 150;
      }
      else if (comando == '2') {
        posicionDestino = 350;
      }
      else if (comando == '3') {
        posicionDestino = 550;
      }

      Serial.print("Moviendo a la posicion absoluta: ");
      Serial.println(posicionDestino);

      stepper.setSpeedInStepsPerSecond(150);
      stepper.setAccelerationInStepsPerSecondPerSecond(500);
      stepper.setDecelerationInStepsPerSecondPerSecond(250);

      stepper.moveToPositionInSteps(posicionDestino);

      Serial.println("Movimiento finalizado.");
    }
    // ==============================================================
    // MOVIMIENTO LIBRE (Cualquier otro número ingresado)
    // ==============================================================
    else {
      long posicionDestino = Serial.parseInt();

      while (Serial.available() > 0) {
        Serial.read();
      }

      Serial.print("Moviendo a la posicion absoluta: ");
      Serial.println(posicionDestino);

      stepper.setSpeedInStepsPerSecond(150);
      stepper.setAccelerationInStepsPerSecondPerSecond(500);
      stepper.setDecelerationInStepsPerSecondPerSecond(250);

      stepper.moveToPositionInSteps(posicionDestino);

      Serial.println("Movimiento finalizado.");
    }
  }
}
