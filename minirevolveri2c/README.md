# Minirevolver como módulo I2C

Nuevo sketch [minirevolveri2c.ino](minirevolveri2c.ino), dirección **0x30**, para el bus **I2C 1 del central**. Mantiene el sketch original en `minirevolver` como referencia. El módulo usa `TwoWire(1)` en modo esclavo, SDA GPIO8 y SCL GPIO9. El primer I2C queda reservado en GPIO6/7 para periféricos locales futuros y no se inicializa en este sketch.

La plataforma de este módulo es **ESP32-S3**, igual que la central y todos los módulos nuevos. El sketch requiere seleccionar S3; los ejemplos anteriores permanecen en sus carpetas de referencia.

Requiere **ESP_FlexyStepper**. Se revisó la API pública del repositorio del autor; verificar que la versión instalada incluya `setTargetPositionInSteps`, `setTargetPositionRelativeInSteps`, `processMovement`, `motionComplete`, `emergencyStop` y `setCurrentPositionAsHomeAndStop`. No se compiló ni se probó este nuevo sketch en hardware.

## Pines ESP32-S3

| Señal | GPIO |
| --- | --- |
| STEP | 5 |
| DIR | 10 |
| HOME | 11 |
| I2C 1 SDA: enlace esclavo | 8 |
| I2C 1 SCL: enlace esclavo | 9 |
| I2C 0 SDA/SCL: reservado, sin inicializar | 6 / 7 |

DIR y HOME pasan a GPIO10/11 para reservar GPIO6/7 para el primer I2C. Verificar disponibilidad física y usos propios de la placa.

Conectar **GPIO8 SDA del módulo a GPIO8 SDA del central S3**, **GPIO9 SCL del módulo a GPIO9 SCL del central** y GND común. No cruzar SDA/SCL. Bus a **100 kHz y lógica de 3,3 V**, con pull-ups según [la propuesta](../sistemasI2C/README.md).

HOME está configurado como `INPUT_PULLUP`, contacto normalmente abierto hacia GND y activo LOW. La alimentación del motor, la corriente y los micropasos se configuran en su driver. Este código no maneja un pin ENABLE del driver.

## Ejecución local y respuesta

El central manda `REV HOME`, `REV POS n`, `REV MOVE pasos` o `REV STOP`. El módulo responde con posición estimada, destino, flags y resultado. No usa comandos locales por serial; ese puerto solo informa el arranque a 115200. Para leerlo en S3 con USB nativo, activar USB CDC On Boot.

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

La respuesta es una instantánea publicada por el bucle; puede verse un estado anterior durante una consulta y la lectura periódica permite recibir el siguiente. Comprobar esta comunicación con las versiones y placas reales antes de integrar la mecánica.

Guardar los archivos sin BOM. Compilación y carga a cargo del usuario.
