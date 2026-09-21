# Central ESP32-S3: I2C 0 e I2C 1 separados

Nuevo sketch [centralI2C.ino](centralI2C.ino) para la [propuesta modular](../sistemasI2C/README.md). No reemplaza el `ventiladorFlujo` confirmado en ESP32-C3. No compilado ni probado en hardware por el asistente.

## Archivos

| Archivo | Responsabilidad |
| --- | --- |
| `centralI2C.ino` | USB serial, PWM manual y coordinación |
| `I2C0Sensores.h` | `TwoWire(0)`, lectura AFM3000 y conversión a slm |
| `I2C1Modulos.h` | `TwoWire(1)`, órdenes y respuestas del minirevolver |
| `Protocolo.h` | Formato de mensajes, CRC y constantes compartidas |

Los headers de buses se incluyen una vez desde el sketch y contienen su estado y funciones. Mantenerlos dentro de la carpeta al abrirla en Arduino IDE.

## Placa y pines propuestos

ESP32-S3, Arduino-ESP32 **2.0.17**, USB nativo, **USB CDC On Boot activado**, monitor a **115200**. Comprobar que la placa S3 elegida exponga estos GPIO y no los use para componentes propios.

| Función | GPIO S3 |
| --- | --- |
| I2C 0 SDA: AFM3000 | 8 |
| I2C 0 SCL: AFM3000 | 9 |
| I2C 1 SDA: módulos | 10 |
| I2C 1 SCL: módulos | 11 |
| BTS7960 R_EN | 4 |
| BTS7960 L_EN | 5 |
| BTS7960 RPWM: siempre LOW | 6 |
| BTS7960 LPWM: PWM manual | 7 |

**Estos pines son nuevos:** GPIO9 aquí es SCL del sensor, no PWM. No copiar el cableado del C3 anterior. Conectar AFM3000 mediante conversor I2C 3,3/5 V, y el minirevolver por I2C de 3,3 V. Ver [cableado y buses](../sistemasI2C/README.md).

## Comandos

Seleccionar **Nueva línea** o **Ambos NL y CR**. El ventilador arranca apagado y el minirevolver no se mueve por encender el central.

| Comando | Acción |
| --- | --- |
| `PWM 128` | Ajustar PWM del ventilador a 128/255 |
| `STOP` | Apagar solamente el ventilador |
| `REV HOME` | Buscar origen del minirevolver |
| `REV POS 1` | Mover a 150 pasos |
| `REV POS 2` | Mover a 350 pasos |
| `REV POS 3` | Mover a 550 pasos |
| `REV MOVE 160` | Destino absoluto explícito de 160 pasos |
| `REV STOP` | Pedir parada del minirevolver, sin modificar el ventilador |
| `ESTADO` | Mostrar última lectura y estado del módulo |
| `AYUDA` | Mostrar comandos |

Este central exige el prefijo `PWM` para el ventilador; no acepta números solos. Los destinos provisionales son 0..2000 pasos. Completar `REV HOME` antes de mover a posiciones; el módulo informa `origen: 1`. Las posiciones son contadas por pasos, no medidas por encoder.

Consulta ambos buses aproximadamente cada 100 ms y muestra datos cada 500 ms. Un sensor o módulo ausente se indica como lectura/respuesta no válida y no corta el PWM del ventilador. Se mantiene el control manual: sin PID, calibración adicional, impulso de arranque ni mínimo automático.

Una orden enviada no implica movimiento terminado. Consultar `moviendo`, `homing`, `origen` y `resultado`; los códigos están en [el protocolo](../sistemasI2C/README.md#protocolo-versión-1). No se reenvían movimientos automáticamente ante un fallo de comunicación. Si se desconecta el central, el módulo continúa hasta su destino finito; no hay watchdog de enlace.

## Primera prueba

1. Revisar el cableado y los límites mecánicos del módulo; cargar cada sketch en su placa correspondiente.
2. Verificar con `ESTADO` que hay respuesta del minirevolver y que el estado del switch cambia al accionarlo.
3. Enviar `REV HOME` y comprobar el sentido físico de búsqueda y el final de carrera.
4. Con `origen: 1`, probar `REV POS 1` y observar posición/destino; usar `REV STOP` para pedir una parada.
5. Probar el ventilador con `PWM n` y observar el caudal. `STOP` afecta únicamente al ventilador.

El asistente no compila ni carga. Guardar UTF-8 sin BOM.
