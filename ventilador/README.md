# Ejemplo original del puente H

[ventilador.ino](ventilador.ino) ejecuta una secuencia automática de PWM. No recibe comandos seriales y no mide caudal.

## Pines

| Señal | GPIO |
| --- | --- |
| R_EN | 5 |
| L_EN | 8 |
| RPWM | 6 |
| LPWM | 9 |

Ambos enables quedan en HIGH desde `setup()`. En un ESP32 los niveles de los GPIO son de 3,3 V; no confundir la señal de habilitación con la alimentación de potencia del driver. El sketch usa `analogWrite` sin fijar frecuencia o resolución.

## Secuencia repetida

1. RPWM sube de 0 a 255, con 15 ms por paso; LPWM permanece en cero.
2. Mantiene ese valor durante 2 segundos.
3. RPWM baja de 255 a 0, con 15 ms por paso, y espera 1 segundo.
4. LPWM sube de 0 a 255, con 15 ms por paso; RPWM permanece en cero.
5. Mantiene ese valor durante 2 segundos.
6. Pone ambos PWM en cero y espera 2 segundos. La detención del lado izquierdo es directa, sin rampa descendente.

El programa empieza a mover la carga automáticamente al arrancar. No hay comando `STOP`; las pausas mantienen los enables activos.

## Relación con el montaje actual

El montaje actual utiliza **el sentido izquierdo de este mismo ejemplo: LPWM en GPIO9 y RPWM en cero**. Ese sentido es el que se está usando para el ventilador y se conservó en [ventiladorFlujo](../ventiladorFlujo/README.md), agregando control manual por USB y lectura del sensor.

Este archivo original también ejecuta automáticamente la secuencia del otro sentido, mediante RPWM. La integración actual utiliza únicamente la parte correspondiente a LPWM. Los nombres izquierdo/derecho identifican las ramas de control del puente en el código, no una orientación física verificada de las aspas.

Revisión de código y documentación, sin nueva prueba física ni compilación por el asistente.
