# Maestro I2C sencillo para probar el minirevolver

[protominirevolvermaster.ino](protominirevolvermaster.ino) se carga en **otro ESP32-S3**. Recibe comandos por monitor serie y los envía al [prototipo slave](../protominirevolver/README.md), dirección `0x30`. Solo usa `Arduino.h`, `Wire` y el `Protocolo.h` incluido; no controla un motor local ni utiliza sensores.

## Cableado

Por defecto, el maestro usa **SDA=GPIO6** y **SCL=GPIO7**. Si también es un PCB revolver, corresponden a su POGO. Se pueden cambiar `SDA_PIN` y `SCL_PIN` si se usa otra placa S3.

| Maestro | Slave PCB revolver | Función |
| --- | --- | --- |
| GPIO6 / POGO.4 | POGO.4 / GPIO6 | SDA |
| GPIO7 / POGO.3 | POGO.3 / GPIO7 | SCL |
| GND / POGO.2 | POGO.2 / GND | Masa común |

Para la prueba, alimentar el maestro por su USB y el slave según su montaje. **Dejar POGO.1 (12 V) desconectado del maestro** si se usan cables de señal hacia una placa de desarrollo. No llevar 12 V a sus GPIO o a su pin de 3,3 V.

Usar pull-ups de SDA y SCL a **3,3 V**: como punto de partida para cables cortos, 4,7 kΩ por línea si no existen ya. X1/X2 y su RTC están en otro bus y no proporcionan pull-ups al POGO. Frecuencia configurada: **100 kHz**. No cruzar SDA y SCL.

HOME del slave: contacto normalmente abierto entre **JP2.3 (GPIO1)** y **JP2.2 (GND)**. Revisar también `ENA_ACTIVO` del slave según el driver.

## Carga y comandos

Abrir el `.ino` de esta carpeta y seleccionar ESP32-S3. Mantener `Protocolo.h` junto al sketch. Si se usa USB nativo, activar **USB CDC On Boot**. Monitor serie a **115200**, con **Nueva línea** o **Ambos NL y CR**. No se manda ningún movimiento al arrancar.

| Comando | Acción |
| --- | --- |
| `H` | Buscar HOME y establecer posición cero. |
| `1` | Ir a 150 pasos absolutos. |
| `2` | Ir a 350 pasos absolutos. |
| `3` | Ir a 550 pasos absolutos. |
| `M 800` | Ir a 800 pasos absolutos; admite 0..2000. |
| `M 0` | Volver al cero calculado, sin repetir homing. |
| `S` | Parar la generación de pasos. |
| `E` | Consultar el estado. |
| `RTC` | Consultar hora Unix UTC y estado del RTC. |
| `RTC 1794222000` | Ejemplo: ajustar a 2026-11-09 11:00:00 UTC. Reemplazar el timestamp por el deseado. |
| `SD` | Remontar la SD y probar escritura del registro. |
| `?` | Mostrar ayuda. |

Enviar un comando por línea. Se aceptan minúsculas. Los números solos `1/2/3` son posiciones predefinidas; para un destino libre usar `M pasos`, evitando la ambigüedad del parser del sketch original.

Para probar: cargar ambos sketches, enviar `E`, comprobar que `switch` cambie al accionar HOME, enviar `H` y esperar `origen=1`, `homing=0`, `moviendo=0`. Después enviar `1`, `2`, `3` o `M pasos` de uno en uno. Una posición sin homing se rechaza con `SIN_ORIGEN`.

El maestro consulta el estado cada 250 ms y muestra posición, destino, movimiento, homing, origen, contacto HOME, resultado, RTC detectado, hora válida, Unix UTC, SD lista, error de SD y registros descartados. La confirmación indica que el slave procesó la orden; el movimiento puede seguir en curso. Las posiciones son conteos de pasos, sin encoder.

Se usa el protocolo binario **versión 2**, con órdenes de 10 bytes, estado de 22 bytes, CRC8 y secuencia. Cargar también el slave actualizado y conservar idénticos ambos `Protocolo.h`; no es compatible con la versión 1 de los otros sketches. Solo hay una orden pendiente; antes de mandar otra se espera confirmación, salvo `S`. El slave rechaza movimientos nuevos mientras está ocupado. Si falta confirmación durante 2 segundos, el maestro informa ejecución incierta y no reenvía automáticamente. Consultar `E` antes de repetir una orden; no existe eliminación de duplicados. Si se desconecta I2C, el slave continúa el movimiento ya aceptado.

La integración actual de RTC y SD se revisó sin compilar, según lo solicitado, y no se probó en placas. La simulación anterior correspondía al protocolo versión 1 y no valida estas funciones nuevas. Las APIs utilizadas se contrastaron con [Wire de Arduino-ESP32 2.0.17](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/Wire/src/Wire.h) y [ESP-FlexyStepper](https://github.com/pkerspe/ESP-FlexyStepper).

El maestro continúa sin necesitar RTClib ni SD. Instalar RTClib de Adafruit solo para el slave, que maneja los periféricos. `RTC unixUTC` admite 946684800..4102444799 (2000..2099); el RTC se maneja en UTC y conserva su hora al reiniciar. Si informa `horaValida=0`, ajustar antes de registrar eventos que necesiten fecha. Los comandos de ajuste y prueba SD se rechazan mientras el motor está ocupado. Ver el [registro CSV y el protocolo ampliado](../protominirevolver/README.md#rtc-y-tarjeta-sd).
