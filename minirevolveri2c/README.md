# Minirevolver como módulo I2C

Nuevo sketch [minirevolveri2c.ino](minirevolveri2c.ino), dirección **0x30**, para el bus **I2C 1 del central**. Mantiene el sketch original en `minirevolver` como referencia. El módulo usa su propio `Wire` en modo dispositivo; no necesita dos buses para recibir órdenes y responder.

Requiere **ESP_FlexyStepper**. Se revisó la API pública del repositorio del autor; verificar que la versión instalada incluya `setTargetPositionInSteps`, `setTargetPositionRelativeInSteps`, `processMovement`, `motionComplete`, `emergencyStop` y `setCurrentPositionAsHomeAndStop`. No se compiló ni se probó este nuevo sketch en hardware.

## Pines según la placa seleccionada

| Señal | ESP32 clásico | ESP32-C3 | ESP32-S3 |
| --- | --- | --- | --- |
| STEP | 5 | 5 | 5 |
| DIR | 23 | 6 | 6 |
| HOME | 27 | 7 | 7 |
| SDA | 21 | 0 | 10 |
| SCL | 22 | 1 | 11 |

La rama ESP32 clásico conserva los pines de motor del ejemplo original. C3/S3 usan una asignación nueva, seleccionada por la placa al compilar. Verificar disponibilidad física y usos propios de cada placa.

Conectar **SDA del módulo a GPIO10 del central S3**, **SCL del módulo a GPIO11 del central** y GND común. No cruzar SDA/SCL. Bus a **100 kHz y lógica de 3,3 V**, con pull-ups según [la propuesta](../sistemasI2C/README.md). Cada placa puede usar números de GPIO distintos para la misma señal del cable.

HOME está configurado como `INPUT_PULLUP`, contacto normalmente abierto hacia GND y activo LOW. La alimentación del motor, la corriente y los micropasos se configuran en su driver. Este código no maneja un pin ENABLE del driver.

## Ejecución local y respuesta

El central manda `REV HOME`, `REV POS n`, `REV MOVE pasos` o `REV STOP`. El módulo responde con posición estimada, destino, flags y resultado. No usa comandos locales por serial; ese puerto solo informa el arranque a 115200. Para leerlo en C3/S3 con USB nativo, activar USB CDC On Boot.

El movimiento se ejecuta en `loop()` con `processMovement`, sin llamadas de movimiento bloqueantes. Los callbacks I2C solo copian órdenes o una instantánea del estado. Así puede contestar mientras el motor gira. Velocidad de ejemplo: **150 pasos/s**, aceleración **500 pasos/s²**, desaceleración **250 pasos/s²**; el bucle cede CPU durante 1 ms. Este ejemplo no está dimensionado para altas tasas de pasos.

## Origen y límites del ejemplo

- Arranca sin mover y con origen desconocido. A diferencia del original, exige homing antes de aceptar destinos absolutos.
- `REV HOME` busca en sentido negativo, hasta **2000 pasos**. Si el switch ya está activo, primero se retira en sentido positivo hasta liberarlo, con límite de **200 pasos**, y luego busca en negativo.
- Al detectar el switch durante la búsqueda fija posición cero y detiene los pasos. No implementa doble aproximación lenta ni filtrado de rebotes; ajustar esto si la repetibilidad del switch lo requiere.
- El destino permitido es **0..2000 pasos**, un límite provisional que debe adaptarse a la mecánica en ambos sketches. No es una medida del recorrido real.
- Durante un movimiento, nuevas órdenes de movimiento/home se rechazan con ocupado. `REV STOP` interrumpe la generación de pasos; no corta la alimentación del driver ni garantiza una parada mecánica sin pérdida de pasos. Si interrumpe homing, el origen queda desconocido.
- La posición es un conteo de pasos enviados, sin encoder: no detecta bloqueos ni pasos perdidos. El switch se usa para homing, no como protección general de recorrido durante todos los movimientos.

## Comunicación

La [especificación del protocolo](../sistemasI2C/README.md#protocolo-versión-1) define órdenes de 10 bytes y respuestas de 14 bytes con CRC8. `Protocolo.h` debe ser idéntico al del central.

El buzón admite una orden pendiente; una parada puede reemplazarla. El central debe esperar la confirmación antes de mandar otra orden normal. Una orden descartada por buzón lleno no recibe confirmación de aplicación. No hay eliminación de duplicados ni reenvío automático. Una pérdida del bus no detiene el movimiento local ya aceptado: continúa hasta su destino o límite de búsqueda.

Para ESP32 clásico se precarga la primera respuesta mediante `Wire.slaveWrite`, como el ejemplo oficial del core 2.0.17. Puede verse un estado anterior durante una consulta; la lectura periódica permite recibir el siguiente. Comprobar esta comunicación con las versiones y placas reales antes de integrar la mecánica.

Guardar los archivos sin BOM. Compilación y carga a cargo del usuario.
