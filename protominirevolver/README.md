# Prototipo minirevolver: pines del PCB central

Asignación tomada del esquema [untitled.sch](../pcb/revolver/untitled.sch) y contrastada con las conexiones del PCB [untitled.brd](../pcb/revolver/untitled.brd), en `pcb/revolver`. El controlador es **U1: ESP32-S3 SuperMini TH**.

Los números GPIO son los del microcontrolador. Los números de pin de los conectores son los pads definidos en EAGLE; identificar el pin 1 en el diseño antes de cablear, sin deducirlo por izquierda/derecha.

## Firmware de este prototipo

[protominirevolver.ino](protominirevolver.ino) implementa el **slave I2C 0x30** por el POGO, con la biblioteca `ESP_FlexyStepper`. Se basa en el movimiento de `minirevolver` y la comunicación no bloqueante de `minirevolveri2c`. El [maestro de prueba](../protominirevolvermaster/README.md) solo recibe comandos por USB y los envía por I2C.

| Función del firmware | GPIO | Conexión |
| --- | --- | --- |
| STEP | 3 | JP3.5 |
| DIR | 4 | JP3.4 |
| ENA | 2 | JP3.6 |
| HOME | 1 | JP2.3, conector de servicio |
| LED RGB direccionable | 48 | Indicador del contacto HOME |
| SDA del slave | 6 | POGO.4 |
| SCL del slave | 7 | POGO.3 |

**HOME se asigna al conector de servicio JP2**: conectar un contacto normalmente abierto entre **JP2.3 (GPIO1)** y **JP2.2 (GND)**. Se configura `INPUT_PULLUP`, activo LOW. JP2.1 es 3,3 V y no se necesita para un contacto seco. BTNA/GPIO5 queda sin usar por este sketch.

Seleccionar ESP32-S3 en Arduino e instalar `ESP_FlexyStepper` y **RTClib de Adafruit** (con su dependencia Adafruit BusIO). `SPI` y `SD` vienen con el core ESP32. Para ver mensajes por USB nativo, activar **USB CDC On Boot**. Abrir el `.ino` de esta carpeta; `Protocolo.h` y `Perifericos.h` deben permanecer junto a él. El maestro no necesita la biblioteca del motor.

El firmware asume **ENA activo LOW**. Si el driver conectado habilita con HIGH, cambiar `ENA_ACTIVO` antes de probar. Arranca sin movimiento y con ENA deshabilitado; al aceptar HOME o un movimiento lo habilita y lo mantiene habilitado para conservar par al terminar. STOP detiene los pulsos y mantiene ENA; no desconecta potencia.

Primero ejecutar `HOME` desde el maestro. Si HOME ya está activo, se retira hasta liberarlo, con límite de 200 pasos; después busca en sentido negativo hasta 2000 pasos. Al detectar HOME fija cero. Si no lo encuentra o no logra liberarlo, informa `HOME_NO_ENCONTRADO`. No hay homing automático al encender. No implementa filtrado de rebotes ni doble aproximación.

Una vez referenciado, `1`, `2`, `3` seleccionan 150, 350 y 550 pasos absolutos, como en el ejemplo original. `M pasos` permite destinos absolutos de 0 a 2000. Velocidad: 150 pasos/s; aceleración: 500 pasos/s²; desaceleración: 250 pasos/s². Los límites son provisionales y deben ajustarse a la mecánica. La posición se calcula con los pasos enviados; no hay encoder.

El movimiento se procesa en `loop()`, sin servicio de motor en segundo plano. Los callbacks I2C solo copian tramas. Se usa una extensión del protocolo de `minirevolveri2c`, **versión 2**: orden de 10 bytes, estado de 22 bytes, CRC8 y secuencia. Actualizar también el maestro: el protocolo versión 1 de los otros ejemplos no es compatible con este estado ampliado. Una respuesta de aceptación no significa que el movimiento terminó: observar `moviendo=0` y `homing=0`. Durante un movimiento se rechazan nuevas órdenes de HOME/MOVER con `OCUPADO`; STOP puede reemplazar una orden pendiente. Si STOP interrumpe homing se pierde el origen. Una pérdida de I2C no cancela un movimiento ya aceptado.

El bus POGO funciona a 100 kHz con señales de 3,3 V. BTNA queda sin usar. X1/X2 comparten el bus interno del RTC; no se consultan otros dispositivos ni la memoria AT24C32 del módulo.

## Consola USB del propio revolver

Al iniciar se encola un estado completo con `HOME=1` si el contacto está activo (LOW) o `HOME=0` si está inactivo. No se inicia movimiento automáticamente.

### Pines e indicador RGB de HOME

| Señal | Pin / conexión | Configuración |
| --- | --- | --- |
| Contacto HOME | GPIO1, JP2.3 | Entrada `INPUT_PULLUP`, activa en LOW. |
| Retorno del contacto | GND, JP2.2 | Cerrar el contacto entre JP2.3 y JP2.2 activa HOME. |
| LED RGB direccionable | GPIO48 | Un único pin de datos controla rojo, verde y azul mediante `neopixelWrite`; no hay un GPIO separado para cada color. |

El LED muestra el contacto desde el arranque y se actualiza cuando cambia. El brillo configurado es **16 sobre 255** (`BRILLO_HOME`).

| Color | Valor RGB (R, G, B) | Estado del contacto | Consola USB |
| --- | --- | --- | --- |
| **Rojo** | `(16, 0, 0)` | Inactivo: contacto abierto, GPIO1 en HIGH. | `HOME=0` |
| **Verde** | `(0, 16, 0)` | Activo: contacto cerrado hacia GND, GPIO1 en LOW. | `HOME=1` |

**Rojo significa contacto inactivo**, que es normal cuando el motor está fuera del sensor. El firmware actual no asigna azul, otros colores ni parpadeos a movimiento, errores, RTC o SD.

### Otros estados y cómo interpretarlos

El color representa la lectura física del sensor. La referencia del motor se establece al completar el comando `HOME` y se consulta con `origen` en la consola.

| Situación | Estado / resultado | LED RGB |
| --- | --- | --- |
| Recién encendido | `mov=0`, `origen=0`; ENA deshabilitado. | Rojo o verde según el contacto. |
| Buscando origen | `mov=1`, `origen=0`; flag I2C `FLAG_HOME` activo. Primero libera el contacto si ya estaba activo y luego lo busca. | Sigue el contacto durante la búsqueda. |
| HOME completado | `mov=0`, `origen=1`, posición cero. | Verde mientras el contacto siga activo. |
| Movimiento a una posición | `mov=1`, `origen=1`. | Rojo al salir del contacto; conserva `origen=1`. |
| Detenido tras un movimiento | `mov=0`, `origen=1`. | Según el contacto, aunque conserve la referencia. |
| HOME no encontrado o contacto que no se libera | Resultado `HOME_NO_ENCONTRADO`, `mov=0`, `origen=0`. | Rojo si está inactivo; verde si sigue activo. No tiene un color exclusivo de error. |
| STOP durante HOME | `mov=0`, `origen=0`; se cancela la búsqueda. | Según el contacto. |
| STOP fuera de HOME | `mov=0`; conserva el estado de referencia previo. | Según el contacto. |

Consultar con `E` o `RTC`. En USB, `HOME` indica el **contacto** y `mov` incluye movimiento o búsqueda de origen; en I2C, `FLAG_SWITCH` indica el contacto, `FLAG_HOME` la búsqueda y `FLAG_REFERENCIADO` la referencia. Los campos `RTC`, `hora_valida`, `SD` y `error_SD` informan los periféricos por consola y no cambian el color del LED.

### Comandos USB

Abrir el monitor serie a **115200** y enviar cada comando con **Enter / Nueva linea** (acepta LF, CR o CRLF). La consola funciona junto con el slave POGO; no hace falta conectar el maestro. Acepta mayusculas o minusculas.

| Comando | Accion |
| --- | --- |
| `h` o `?` | Mostrar ayuda, incluyendo como inicializar fecha y hora. |
| `f09/11/2026` | Fijar fecha con `fDD/MM/AAAA` (2000..2099); conserva hora valida o inicia a las 00:00:00. |
| `h11:11:11` | Ajustar solo la hora, formato `hHH:MM:SS` de 24 horas, conservando la fecha valida del RTC. |
| `HOME` | Buscar el origen del motor. En esta consola `h` significa ayuda. |
| `S` o `STOP` | Parar el movimiento. |
| `M 350` | Mover a una posicion absoluta entre 0 y 2000 pasos; requiere HOME previo. |
| `1`, `2`, `3` | Mover a 150, 350 o 550 pasos. |
| `E` o `RTC` | Mostrar estado y fecha/hora. |
| `RTC 1794222000` | Ajustar fecha y hora completas mediante segundos Unix UTC. El numero es un ejemplo, no la hora actual. |
| `SD` | Remontar la tarjeta y probar el registro. |

La hora se mantiene en **UTC**, igual que el protocolo POGO y los registros; `hHH:MM:SS` no aplica conversion de zona horaria. Si el RTC perdio alimentacion o su fecha no es valida, inicializar con `fDD/MM/AAAA` y luego `hHH:MM:SS`. `RTC unixUTC` sigue disponible como alternativa. Con `hora_valida=0`, ignorar la fecha/hora mostrada. No se inventa una fecha ni se utiliza automaticamente la de compilacion.

Las ordenes usan las mismas comprobaciones del control POGO: RTC/SD se rechazan durante movimiento y MOVER requiere origen. Una respuesta `OK` a HOME/MOVER indica aceptacion; consultar `E` para ver si termino. La entrada se lee incrementalmente, como maximo 16 caracteres y una orden por vuelta, sin esperar por una linea completa. Una linea demasiado larga se descarta hasta Enter. Las respuestas se transmiten desde una cola acotada, sin esperar por USB; si se llena, se pueden perder mensajes. Cada 5 segundos muestra fecha DD/MM/AAAA, hora HH:MM:SS, posicion actual y destino en pasos, movimiento y estado de perifericos. Si la fecha no es valida muestra guiones. Durante movimiento extrapola la hora sin consultar el RTC.

Una orden POGO disponible tiene prioridad en esa vuelta. Las ordenes seriales se registran como `SERIAL` y no sustituyen la secuencia ni el resultado de confirmacion del maestro; el estado fisico del motor y perifericos es compartido entre ambas interfaces.

### Inicializar fecha y hora en formato humano (USB o maestro I2C)

Con el motor detenido, monitor a **115200** y **Nueva linea**, enviar una orden por vez y esperar su respuesta:

```text
h
f09/11/2026
h11:11:11
RTC
```

`h` muestra la ayuda. Los valores son ejemplos: sustituirlos por la fecha y hora deseadas. `fDD/MM/AAAA` admite fechas reales entre 2000 y 2099, incluyendo anos bisiestos. Por ejemplo, rechaza `f31/04/2026` y `f29/02/2025`.

Si el RTC no tenia fecha/hora valida, `f09/11/2026` lo inicializa al **09/11/2026 a las 00:00:00**. Si ya era valido, conserva su hora. Luego `h11:11:11` fija las **11:11:11** sin cambiar la fecha. La hora admite `h00:00:00`..`h23:59:59`, sin espacios y con dos digitos por campo. `RTC` permite verificar `hora_valida=1` (en el maestro se muestra como `horaValida=1`).

Si se envia la hora antes de inicializar una fecha confiable, responde:

```text
Fecha no confiable: enviar fDD/MM/AAAA y luego hHH:MM:SS.
```

Por I2C se devuelve `FECHA_NO_VALIDA=9`. Si responde `OCUPADO`, esperar a que el motor termine o detenerlo con `S`. Si responde `RTC_NO_DISPONIBLE`, revisar el RTC y su conexion.

El reloj conserva la convencion **UTC** del protocolo y del CSV: no convierte automaticamente la hora local. Por ejemplo, las 15:05 en UTC-4 corresponden a `h19:05:00`. Si la conversion cambia el dia, ingresar tambien la fecha UTC correcta con `f`.

Como alternativa, `RTC unixUTC` sigue ajustando fecha y hora completas. PowerShell puede generar ese comando con la hora actual del PC:

```powershell
"RTC $([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())"
```

Copiar enseguida el resultado al monitor serie. Para controlar por I2C, cargar tambien el [maestro actualizado](../protominirevolvermaster/README.md): acepta los mismos comandos `f`, `hHH:MM:SS`, `HOME`, `M pasos`, `S`, `E`, `RTC` y `SD`. **`h`/`H` ahora muestra ayuda en ambas consolas; usar `HOME` para buscar origen.**

## RTC y tarjeta SD

[Perifericos.h](Perifericos.h) integra el **DS3231** en `TwoWire(1)` como maestro local, con **SDA=8, SCL=9**, dirección `0x68`. `Wire` sigue usando el controlador 0 como slave del POGO en GPIO6/7. Los dos buses son independientes.

La **SD usa SPI** con **CS=10, MOSI=11, CLK=12 y MISO=13**, inicialmente a 4 MHz. Se monta al arrancar sin formatearla. Usar una tarjeta preparada en FAT32. Si falta RTC o SD, el control del motor sigue disponible y su estado se informa al maestro.

Desde el maestro:

- `E` o `RTC`: muestra detección del RTC, validez de hora, segundos Unix UTC, estado de SD y cantidad de registros descartados.
- `f09/11/2026` y luego `h11:11:11`: ajustar fecha y hora en formato humano, esperando confirmacion entre ordenes.
- `RTC 1794222000`: ejemplo de ajuste a 2026-11-09 11:00:00 UTC. Reemplazar por el timestamp UTC deseado; rango admitido 2000..2099. El RTC no aplica huso horario ni horario de verano.
- `SD`: vuelve a montar la SD y prueba una escritura en el archivo de registro. No borra ni formatea.

El RTC se conserva al reiniciar y **no se ajusta automáticamente con la fecha de compilación**. Si perdió alimentación del oscilador o tiene una fecha inválida, `horaValida=0` y los registros llevan Unix=0 hasta ajustarlo. El timestamp se guarda al encolar cada evento; no se corrigen registros anteriores al ajuste.

El archivo **`/revolver.csv`** se abre para añadir filas, conservando los datos existentes. Registra arranque, órdenes (incluidos rechazos), final de movimiento/homing y pruebas de SD. Columnas: `unix_utc,millis,evento,secuencia,operacion,resultado,posicion,destino,perdidos`. Un evento FIN conserva la secuencia y operación del movimiento original, aunque se hayan rechazado órdenes posteriores. Una parada se registra como ORDEN/PARAR. Los códigos de operación/resultado están en `Protocolo.h`.

Para no introducir pausas entre pasos, no se accede al RTC ni a la SD mientras hay movimiento/homing: se extrapola la hora desde la última lectura y se guardan hasta **32 eventos en RAM**. Al detenerse se refresca el RTC cada segundo y se escribe una fila por vuelta. Si la cola se llena, se descartan eventos nuevos y aumenta `logsPerdidos`. Si falla la SD, la cola se conserva y se puede reintentar con `SD`. El comando SD intenta escribir la primera fila pendiente, que puede ser un evento anterior a la prueba. Los eventos pendientes se pierden al reiniciar; esto no es un registro continuo de cada paso.

Los comandos de ajustar RTC y probar SD devuelven `OCUPADO` durante movimiento. La E/S de SD con motor detenido puede retrasar el procesamiento de la siguiente orden; no se realiza desde callbacks I2C. El RTC se consulta al arrancar y en reposo, por lo que su detección durante movimiento representa la última lectura.

### Estado I2C versión 2

| Bytes | Contenido |
| --- | --- |
| 0 | Cabecera `0x5A` |
| 1 | Versión `2` |
| 2 | Flags: moviendo=1, homing=2, referenciado=4, HOME activo=8, RTC detectado=16, hora válida=32, SD lista=64, error SD=128 |
| 3 | Resultado de la última orden / homing |
| 4 | Secuencia de la última orden procesada |
| 5..8 | Posición int32, little endian |
| 9..12 | Destino int32, little endian |
| 13..16 | Tiempo Unix UTC uint32, little endian; 0 si no hay hora válida |
| 17..20 | Eventos descartados uint32, little endian |
| 21 | CRC8 de los bytes 0..20 |

Las órdenes conservan 10 bytes: cabecera `0xA5`, versión, operación, secuencia, argumento de 4 bytes little endian, reservado=0 y CRC8 de los primeros 9 bytes. Operaciones: MOVER=1, HOME=2, PARAR=3, AJUSTAR_RTC=4 (argumento Unix uint32), PROBAR_SD=5, AJUSTAR_FECHA=6 y AJUSTAR_HORA=7. Resultados añadidos: RTC_NO_DISPONIBLE=7, ERROR_SD=8 y FECHA_NO_VALIDA=9. El argumento de MOVER conserva interpretación int32 y rango 0..2000.

### Fecha, hora y posicion por I2C

El transporte POGO sigue siendo **binario**, no texto ASCII. El maestro convierte los comandos humanos a tramas de 10 bytes; no enviar directamente la cadena `f09/11/2026` con `Wire.write`. Se conserva version 2 y el estado de 22 bytes: las operaciones nuevas amplian el protocolo sin cambiar los campos anteriores. Un slave anterior no entiende las operaciones 6 y 7; actualizar ambos sketches y sus `Protocolo.h`.

| Accion | Operacion | Argumento en bytes 4..7, little endian |
| --- | --- | --- |
| Fecha `f09/11/2026` | 6 (AJUSTAR_FECHA) | uint32 decimal `20261109`, formato numerico AAAAMMDD. |
| Hora `h11:11:11` | 7 (AJUSTAR_HORA) | uint32 `40271`, segundos desde medianoche (`hora*3600 + minuto*60 + segundo`). |
| Destino `M 350` | 1 (MOVER) | int32 `350`, posicion absoluta en pasos; requiere HOME previo. |
| Buscar origen `HOME` | 2 (HOME) | 0. |
| Parar `S` | 3 (PARAR) | 0. |

Para obtener la **posicion actual**, el maestro solicita 22 bytes a `0x30`, valida cabecera/version/CRC y decodifica `leerInt32(estado + 5)`. El destino esta en `leerInt32(estado + 9)` y fecha/hora UTC en `leerUint32(estado + 13)`. No se necesita enviar una orden de movimiento para consultar. La posicion es un conteo de pasos generado por el firmware, no una medicion por encoder; comprobar `FLAG_REFERENCIADO` para saber si se hizo HOME.

El maestro consulta cada 250 ms para confirmar ordenes y muestra fecha, hora y posicion cada 5 segundos; `E` las muestra a pedido. El slave publica una instantanea aproximadamente cada 10 ms, salvo demoras de E/S. Los callbacks solo intercambian tramas; el ajuste del RTC se ejecuta desde `loop()` con el motor detenido.

Las APIs se contrastaron con [RTClib DS3231 de Adafruit](https://github.com/adafruit/RTClib/blob/master/src/RTC_DS3231.cpp) y [SD de Arduino-ESP32](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/SD/src/SD.h).

## GPIO utilizados

| GPIO de U1 | Red del esquema | Destino | Para qué sirve |
| --- | --- | --- | --- |
| 1 | `1` | JP2, pin 3 | HOME en este firmware, usando el conector de servicio. El esquema solo nombra la red como `1`. |
| 2 | `ENA` | JP3, pin 6 | Habilitación del driver del motor. La polaridad depende del driver conectado. |
| 3 | `STEP` | JP3, pin 5 | Pulsos de avance del motor paso a paso. |
| 4 | `DIR` | JP3, pin 4 | Sentido de giro del motor. |
| 5 | `BTNA` | X3, pin 1 | Entrada para botón/contacto hacia GND. El esquema no define si se usa como botón de usuario o final de carrera. |
| 6 | `SDAP` | POGO, pin 4 | Datos del bus I2C por contactos pogo. |
| 7 | `SCLP` | POGO, pin 3 | Reloj del bus I2C por contactos pogo. |
| 8 | `SDA` | X1 y X2, pin 1; RTC U$1, J2.2 | Datos del bus I2C compartido con el RTC DS3231/AT24C32 y los conectores X1/X2. |
| 9 | `SCL` | X1 y X2, pin 2; RTC U$1, J2.1 | Reloj del mismo bus I2C. |
| 10 | `CS` | Módulo SD U$2, CS | Selección de la tarjeta SD en el bus SPI. |
| 11 | `MOSI` | Módulo SD U$2, MOSI | Datos SPI del ESP32 hacia la tarjeta SD. |
| 12 | `CLK` | Módulo SD U$2, CLK | Reloj SPI de la tarjeta SD. |
| 13 | `MISO` | Módulo SD U$2, MISO | Datos SPI de la tarjeta SD hacia el ESP32. |

Los pads **RX** y **TX** de U1 aparecen en las redes `N$8` y `N$9`, respectivamente, sin otro componente conectado. El símbolo llama `XT` a la señal que corresponde al pad físico `TX`; no se utiliza como enlace externo en este PCB.

## Conectores externos

### X1 y X2 — I2C, JST-XH de 4 pines

Ambos conectores están en paralelo y comparten el bus con el RTC.

| Pin de cada conector | Red | Función |
| --- | --- | --- |
| 1 | SDA | Datos I2C, GPIO8. |
| 2 | SCL | Reloj I2C, GPIO9. |
| 3 | GND | Masa común. |
| 4 | 3V3 | Alimentación de 3,3 V para el periférico. |

### POGO — alimentación y segundo bus I2C

| Pin | Red | Función |
| --- | --- | --- |
| 1 | 12V | Línea de 12 V compartida con J1.1 y X5.2. |
| 2 | GND | Masa común. |
| 3 | SCLP | Reloj I2C, GPIO7. |
| 4 | SDAP | Datos I2C, GPIO6. |

Las señales de este bus están separadas de SDA/SCL de X1 y X2. El esquema no determina el rol maestro/dispositivo de cada bus; eso debe configurarlo el firmware.

### JP2 — conector de señal de 3 pines

| Pin | Red | Función |
| --- | --- | --- |
| 1 | 3V3 | Alimentación de 3,3 V. |
| 2 | GND | Masa común. |
| 3 | 1 | GPIO1: entrada HOME en el firmware de este prototipo. |

### JP3 — conexión al driver del motor

| Pin | Red | Función |
| --- | --- | --- |
| 1 | 12VB | Alimentación de la rama de 12 V posterior a X5. |
| 2 | GND | Masa común. |
| 3 | 5V | Alimentación desde la salida del LM2596. |
| 4 | DIR | Sentido, GPIO4. |
| 5 | STEP | Pulsos de paso, GPIO3. |
| 6 | ENA | Habilitación, GPIO2. |

Este conector lleva alimentación y señales al driver; las bobinas del motor no se conectan a los GPIO. El modelo de driver no está definido en el esquema.

### X3 — botón/contacto, JST-XH de 2 pines

| Pin | Red | Función |
| --- | --- | --- |
| 1 | BTNA | Entrada GPIO5. |
| 2 | GND | Retorno del contacto. |

Si se usa un contacto que cierre a masa, el firmware debe configurar la entrada con pull-up interno o prever uno externo. El esquema no muestra una resistencia pull-up para BTNA.

### J1 — entrada de alimentación, bornera de 2 pines

| Pin | Red | Función |
| --- | --- | --- |
| 1 | 12V | Positivo de la alimentación de 12 V. |
| 2 | GND | Negativo/masa. |

### X5 — enlace entre las ramas de 12 V, JST-XH de 2 pines

| Pin | Red | Función |
| --- | --- | --- |
| 1 | 12VB | Hacia IN+ del LM2596 y JP3.1. |
| 2 | 12V | Hacia J1.1 y POGO.1. |

**X5 no es un conector positivo/GND:** ambos pines son líneas positivas distintas. Puede usarse para un interruptor o puente externo entre `12V` y `12VB`. No hay unión directa entre esas redes en el esquema; sin un enlace externo en X5, la entrada J1 no alimenta el LM2596 ni la rama JP3.1.

## Módulos y pines de alimentación

| Componente | Pin/pad | Red o destino | Función |
| --- | --- | --- | --- |
| U1, ESP32-S3 | 5V | 5V | Alimentación desde OUT+ del LM2596. |
| U1, ESP32-S3 | 3.3 | 3V3 | Alimentación de RTC, SD, X1, X2 y JP2. |
| U1, ESP32-S3 | G | GND | Masa común. |
| U$1, RTC DS3231/AT24C32 | J2.1 | SCL, GPIO9 | Reloj I2C. |
| U$1, RTC DS3231/AT24C32 | J2.2 | SDA, GPIO8 | Datos I2C para reloj y memoria del módulo. |
| U$1, RTC DS3231/AT24C32 | J2.3 | 3V3 | Alimentación del módulo. |
| U$1, RTC DS3231/AT24C32 | J2.4 | GND | Masa del módulo. |
| U$2, módulo SD | CS | GPIO10 | Selección SPI. |
| U$2, módulo SD | MOSI | GPIO11 | Datos hacia SD. |
| U$2, módulo SD | CLK | GPIO12 | Reloj SPI. |
| U$2, módulo SD | MISO | GPIO13 | Datos desde SD. |
| U$2, módulo SD | 3V3 | 3V3 | Alimentación de la SD. |
| U$2, módulo SD | GND | GND | Masa de la SD. |
| U$3, LM2596 | IN+ | 12VB | Entrada positiva desde X5.1. |
| U$3, LM2596 | IN- | GND | Negativo de entrada. |
| U$3, LM2596 | OUT+ | 5V | Salida destinada a U1 y JP3.3; ajustar el módulo a 5 V antes de conectarlos. |
| U$3, LM2596 | OUT- | GND | Negativo de salida, unido a la masa común. |

Las líneas I2C y SPI corresponden a lógica de **3,3 V**; los 12 V del conector POGO son exclusivamente alimentación. El esquema no incluye resistencias pull-up I2C discretas: comprobar las que incorporen los módulos conectados.

## Diferencias con los ejemplos de firmware

Este README describe el PCB existente. Los sketches actuales tienen otras asignaciones y requieren adaptación antes de usarse con esta placa:

- [minirevolver](../minirevolver/README.md) usa STEP=5, DIR=23 y HOME=27.
- [minirevolveri2c](../minirevolveri2c/README.md), para S3, usa STEP=5, DIR=6, HOME=7 y SDA/SCL=10/11. No controla ENA. En este PCB, STEP/DIR/ENA son 3/4/2, GPIO6/7 van al POGO y GPIO10/11 a la SD.
- [centralI2C](../centralI2C/README.md) comparte SDA/SCL=8/9 con X1/X2, pero propone el segundo bus en 10/11 y control de ventilador en 4/5/6/7; esas funciones difieren del PCB.

El nuevo sketch de esta carpeta adapta los pines al PCB: GPIO1 es HOME y el POGO es slave. BTNA sigue reservado. La polaridad ENA debe ajustarse al driver real. Slave y maestro compilaron con Arduino-ESP32 2.0.17 para ESP32-S3. Se probaron en simulación de periféricos la validación de fechas/horas, las órdenes USB/I2C y el estado con posición; no se probó en hardware.
