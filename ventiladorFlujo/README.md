# Control manual del ventilador y lectura del AFM3000

ESP32-C3 con Arduino-ESP32 **2.0.17**, **USB nativo**, **USB CDC On Boot activado** y monitor a **115200**. Seleccionar terminacion **Nueva linea** o **Ambos NL y CR**.

El programa aplica el PWM indicado por serial y muestra el caudal. El sensor no modifica la potencia: no hay regulacion automatica, bloqueo de arranque por lectura ni parada por fallo del sensor. Arranca apagado y conserva el ultimo PWM hasta recibir otro comando o reiniciarse.

## Pines

| Senal | GPIO |
| --- | --- |
| AFM3000 SDA | 20 |
| AFM3000 SCL | 21 |
| BTS7960 R_EN | 5 |
| BTS7960 L_EN | 8 |
| BTS7960 RPWM (siempre LOW) | 6 |
| BTS7960 LPWM | 9 |

Ventilador brushless de dos cables y 12 V, conectado al BTS7960 con la polaridad que funciona. Ambos enables se activan para encender y se desactivan con STOP. Solo LPWM recibe PWM.

Este es **el mismo sentido izquierdo del ejemplo original `ventilador`**, que se está usando en el montaje: LPWM en GPIO9 y RPWM en cero. La denominación izquierdo corresponde a la rama del puente seleccionada en el código.

El AFM3000 usa alimentacion de 5 V y conversor bidireccional I2C de 3,3 V a 5 V, con GND comun. Ver `../airSensor/README.md` para el cableado del sensor.

## PWM y comandos

Se usa `analogWrite(9, valor)`, igual que en `pruebaPWM`, cuya prueba fue exitosa en el otro hardware ESP32-C3. Se conserva la configuracion predeterminada: 8 bits y 1 kHz en el core 2.0.17. El sketch no configura canales LEDC directamente. Esta integracion arranca apagada.

| Comando | Accion |
| --- | --- |
| `PWM 64` | Duty aproximado del 25 % |
| `PWM 128` | Duty aproximado del 50 % |
| `128` | Igual que `PWM 128`; acepta tambien numeros solos |
| `PWM 255` | Duty del 100 % |
| `PWM 0` o `STOP` | Apagar |
| `ESTADO` | Mostrar PWM y lectura |
| `AYUDA` | Mostrar comandos |

Se aceptan enteros de 0 a 255 y comandos en minusculas. Un comando invalido no cambia la potencia. No es necesario que el sensor responda para ajustar el PWM. Cerrar el monitor serial no detiene el ventilador si el montaje sigue alimentado.

## Lecturas

La unidad **slm** significa **litros estándar por minuto**, referidos a **20 °C y 1013 mbar** según el manual del AFM3000. No es una medición de velocidad en m/s. El volumen real por minuto puede diferir si cambian la temperatura o la presión. Por ejemplo, 2 slm equivalen a 0,033 litros estándar/s y 35 slm a 0,583 litros estándar/s.

Se consulta el sensor aproximadamente cada 100 ms y se muestra el estado cada 500 ms. Se mantiene la conversion para aire `(RAW - 32000) / 140`, el CRC y el descarte de la primera lectura. Una lectura fallida se muestra como `SIN LECTURA VALIDA`, sin cambiar el PWM ni entrar en un bucle de error.

```text
PWM izquierdo: 128/255 | RAW: 33400 | Flujo: 10.00 slm
```

La lectura I2C conserva una espera de 20 ms y un timeout de 100 ms. El PWM continua por hardware durante esas operaciones. Si falla la inicializacion I2C se informa, sin detener el programa en un bucle de error ni bloquear el PWM.

## Observaciones del montaje

El usuario confirmó el funcionamiento de esta integración después de probar otro hardware ESP32-C3 y cambiar a un ventilador que responde al PWM. No se determinó una causa única para las dificultades anteriores; la respuesta al PWM no debe generalizarse a todos los ventiladores brushless.

| Observación comunicada | Alcance |
| --- | --- |
| Caudal aproximado de 2 a 35 slm | Rango observado con el ventilador y conducto actuales, no rango nominal del sensor |
| Con PWM 160 puede no arrancar desde reposo | No establece un umbral exacto de arranque |
| Si ya está girando, puede mantenerse con PWM 160 | El mínimo de marcha es distinto del mínimo de arranque |
| Al bajar el PWM se detiene cerca de 60 | Umbral aproximado observado, no límite aplicado por el programa |

Se mantiene la propuesta inicial: control manual por serial y observación del caudal. Para explorar caudales bajos, arrancar manualmente con un PWM que consiga iniciar el giro y después bajarlo. El porcentaje de PWM no equivale directamente al porcentaje de caudal, velocidad o potencia eléctrica.

Se propusieron un impulso de PWM 255 durante 300 ms y un mínimo de marcha de 70, pero **no están implementados ni validados**. El código acepta cualquier valor de 0 a 255 sin elevarlo automáticamente. Tampoco detecta si el rotor está detenido.

Archivo guardado sin BOM. La revisión actual fue de código y documentación; el asistente no compila ni carga, esas tareas las realiza el usuario.
