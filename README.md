# Modular Air Flow

Sistema modular para generar, medir y dirigir flujo de aire. La propuesta es usar **ESP32-S3 en la central y en todos los módulos**, con una central que actúe como **interfaz de usuario y coordinador**, y módulos que ejecuten localmente el control de sus actuadores y sensores.

El repositorio contiene tanto las pruebas que ya funcionaron como los primeros ejemplos de comunicación entre módulos. Cada carpeta con un `.ino` es un sketch independiente: abrir y cargar únicamente el correspondiente a cada placa.

## Arquitectura propuesta

La central será el punto desde el que el usuario selecciona acciones, ajusta parámetros y consulta el estado del sistema. La interfaz inicial es el monitor USB serial; una pantalla con controles físicos o una interfaz web son opciones futuras, todavía sin implementar ni seleccionar.

Cada módulo tendrá su propio ESP32-S3 y una función concreta. La central enviará órdenes de alto nivel, como una posición o un valor de PWM, y consultará el resultado. Los pulsos del motor, la lectura de sensores y la ejecución de las órdenes se resolverán en el módulo correspondiente.

```mermaid
flowchart TB
  U[Usuario] <--> C[Central ESP32-S3: interfaz y coordinación]
  C <-->|I2C 0: periféricos locales| L[Periféricos de interfaz o sensores locales]
  C <-->|I2C 1: bus de módulos| R[ESP32-S3: minirevolver]
  C <-->|I2C 1: mismo bus| F[ESP32-S3: módulo de flujo propuesto]
  C <-->|I2C 1: mismo bus| X[Otros módulos ESP32-S3 futuros]
  R --> M[Driver STEP/DIR y motor]
  H[Final de carrera] --> R
  F --> V[Driver y ventilador]
  A[AFM3000] --> F
```

El diagrama representa **la arquitectura objetivo**, no todo el código disponible hoy. El firmware `centralI2C` todavía lee el AFM3000 y controla el ventilador directamente desde la central. Separar esas funciones en un módulo ESP32-S3 de flujo es el siguiente paso de arquitectura; ese firmware aún no existe.

### Responsabilidades

| Elemento | Función propuesta | Estado actual |
| --- | --- | --- |
| Central ESP32-S3 | Interfaz de usuario, envío de órdenes y presentación de estados | Ejemplo con monitor USB, dos buses I2C y un minirevolver remoto; también conserva el sensor y PWM locales |
| Minirevolver ESP32-S3 | Movimiento, búsqueda de origen y respuesta con posición/estado | Sketch I2C creado; pendiente de prueba en hardware |
| Módulo de flujo ESP32-S3 | Lectura del AFM3000 y PWM del ventilador, con órdenes y respuesta al central | Propuesto; la función conjunta está probada en el sketch independiente de ESP32-C3 |
| Módulos adicionales ESP32-S3 | Nuevos actuadores o mediciones, cada uno con su dirección | Por definir |

Usar la misma familia de microcontrolador facilita mantener una base común de firmware y herramientas. El modelo concreto de placa ESP32-S3, sus conectores y la asignación definitiva de pines aún deben elegirse. Usar S3 en todos los nodos no exige utilizar sus dos buses en cada módulo.

## Comunicación: I2C 0 e I2C 1

| Bus de la central | Papel | Implementación actual |
| --- | --- | --- |
| **I2C 0** | Periféricos locales: sensores y, si se elige una interfaz I2C, sus componentes | AFM3000 en `0x40`, código separado en `I2C0Sensores.h` |
| **I2C 1** | Comunicación bidireccional con módulos ESP32-S3 | Minirevolver en `0x30`, código separado en `I2C1Modulos.h` |

Varios módulos comparten SDA, SCL y GND con direcciones distintas. La central inicia las transacciones: manda una orden y consulta la respuesta. Cada módulo informa si está ocupado, su estado y el resultado de la orden. La posición reportada por el minirevolver es un conteo de pasos, no una medida de encoder.

Los buses 0 y 1 son independientes y sus líneas de señal no se unen. Los números identifican controladores dentro de cada microcontrolador: el minirevolver puede usar su I2C local 0 como dispositivo conectado al I2C 1 de la central.

La propuesta inicial usa **cables cortos**, I2C a **100 kHz** y señales de **3,3 V** entre microcontroladores. Un módulo puede estar fuera de la caja, pero la distancia, capacitancia y ruido del cableado deben comprobarse. Para enlaces de metros o mayor exposición al ruido de motores, considerar RS-485 o CAN con sus transceptores; ambos permiten una red de varios módulos. Esas alternativas no están implementadas.

El AFM3000 conserva su alimentación de 5 V y el conversor bidireccional I2C de 3,3 V a 5 V. Los detalles de conectores, pull-ups, mensajes y direcciones están en la [propuesta de comunicación](sistemasI2C/README.md).

## Organización del repositorio

### Nueva arquitectura ESP32-S3

| Carpeta | Contenido | Estado |
| --- | --- | --- |
| [centralI2C](centralI2C/README.md) | Central, USB serial, buses I2C 0/1 separados y órdenes remotas | Creado, pendiente de compilación y prueba por el usuario |
| [minirevolveri2c](minirevolveri2c/README.md) | Módulo de movimiento I2C, dirección `0x30` | Creado; seleccionar ESP32-S3 para la arquitectura propuesta |
| [sistemasI2C](sistemasI2C/README.md) | Diseño del enlace, cableado y protocolo versión 1 | Documentación de la primera integración |

El sketch del minirevolver conserva ramas de compatibilidad para ESP32 clásico y C3; **la plataforma elegida para los nuevos módulos es ESP32-S3**. Esas ramas no cambian la propuesta de hardware.

### Pruebas y referencias anteriores

| Carpeta | Contenido | Resultado o alcance |
| --- | --- | --- |
| [ventiladorFlujo](ventiladorFlujo/README.md) | PWM manual y lectura del AFM3000 en ESP32-C3 | Funcionamiento confirmado por el usuario |
| [airSensor](airSensor/README.md) | Lectura aislada del sensor y manual PDF | Lecturas confirmadas por el usuario |
| [pruebaPWM](pruebaPWM/README.md) | `analogWrite()` en GPIO9, ajustable por USB serial | Funcionamiento confirmado en otro hardware ESP32-C3 |
| [ventilador](ventilador/README.md) | Rampas automáticas del puente en ambos sentidos | El sentido izquierdo, LPWM, se reutilizó en el montaje de flujo |
| [minirevolver](minirevolver/README.md) | Control original del motor por serial | Pines 5/23/27 y limitaciones de comandos documentadas; no trasladar directamente al S3 |

Los sketches anteriores se conservan como referencias funcionales. La decisión de usar S3 en el nuevo sistema no significa que esas pruebas ya se hayan migrado o validado en S3.

## Primera integración disponible

El punto de entrada es [centralI2C.ino](centralI2C/centralI2C.ino), con un ESP32-S3 central y otro ESP32-S3 ejecutando [minirevolveri2c.ino](minirevolveri2c/minirevolveri2c.ino).

| Conexión | Central S3 | Minirevolver S3 |
| --- | --- | --- |
| I2C de módulos SDA | GPIO10 | GPIO10 |
| I2C de módulos SCL | GPIO11 | GPIO11 |
| Referencia eléctrica | GND | GND |
| Sensor AFM3000 local: SDA/SCL | GPIO8 / GPIO9, mediante conversor de nivel | No conectado en este módulo |
| Ventilador local: R_EN/L_EN/RPWM/LPWM | GPIO4 / GPIO5 / GPIO6 / GPIO7 | No conectado en este módulo |
| Motor: STEP/DIR/HOME | No conectado en la central | GPIO5 / GPIO6 / GPIO7 |

Son asignaciones propuestas para placas que expongan esos pines. **El cableado cambia respecto al C3 probado:** GPIO9 en la central S3 es SCL del sensor; el PWM del ventilador está en GPIO7. Verificar también el tipo de contacto del final de carrera y el recorrido mecánico antes de probar el movimiento.

La interfaz actual usa USB nativo, **USB CDC On Boot activado**, monitor a **115200** y terminación **Nueva línea**. Los ejemplos se prepararon para Arduino-ESP32 **2.0.17**. El módulo de movimiento requiere `ESP_FlexyStepper`.

| Comando en la central | Acción |
| --- | --- |
| `PWM 128` | Ajustar el PWM del ventilador local |
| `STOP` | Apagar solamente el ventilador |
| `REV HOME` | Buscar el origen del minirevolver |
| `REV POS 1` | Solicitar la posición predefinida 1 |
| `REV MOVE 160` | Solicitar posición absoluta de 160 pasos |
| `REV STOP` | Solicitar parada del minirevolver |
| `ESTADO` | Mostrar flujo, PWM y estado del módulo |

El minirevolver exige completar homing antes de aceptar posiciones. Los límites de recorrido del ejemplo son provisionales. Consultar los README de ambas placas para los demás comandos y el significado de las confirmaciones.

## Resultados del montaje de flujo

Las pruebas anteriores, comunicadas por el usuario, establecieron lo siguiente:

| Observación | Interpretación |
| --- | --- |
| Aproximadamente **2 a 35 slm** con el ventilador actual | Rango observado en ese montaje; no es el rango nominal del sensor |
| Puede no arrancar con **PWM 160**, pero mantenerse a ese valor una vez girando | El umbral de arranque es distinto del mínimo de marcha |
| Se detiene cerca de **PWM 60** al reducir la orden | Umbral aproximado observado, no aplicado como límite por software |
| Se cambió a un ventilador que responde al PWM | No se puede asumir la misma respuesta para cualquier ventilador brushless |
| Referencia en **1,8 slpm** y AFM3000 entre **1,90 y 2,30 slm** en una comparación | Diferencia pendiente de estudiar; no se aplicó corrección de calibración |

**slm** expresa litros estándar por minuto; el manual del AFM3000 usa **20 °C y 1013 mbar**. Para comparar con otro equipo hay que confirmar sus condiciones estándar y la estabilidad de ambas lecturas. El AFM3000 compensa internamente la temperatura, pero el manual disponible no documenta un comando para leerla.

Se mantiene el control manual de PWM y la lectura del caudal. No están implementados el control automático de caudal, un impulso de arranque, un mínimo automático de marcha ni una corrección de calibración. Las propuestas de PWM 255 durante 300 ms y mínimo 70 quedan como opciones no validadas, no como requisitos del sistema.

## Próximos pasos de la propuesta

1. Elegir una placa ESP32-S3 común y confirmar pines, conectores y alimentación de los módulos.
2. Probar central y minirevolver por I2C, incluida la recepción de estados durante un movimiento.
3. Separar ventilador y AFM3000 en un módulo de flujo ESP32-S3 con su protocolo de órdenes y respuesta.
4. Definir la interfaz de usuario de la central y construirla sobre las funciones de comunicación ya verificadas.
5. Evaluar RS-485 o CAN si el cableado externo supera lo adecuado para el bus I2C probado.

## Reglas de trabajo

Por indicación del usuario, el asistente **no debe compilar ni cargar firmware**. Las verificaciones realizadas por el asistente son revisiones de código, documentación y archivos; las pruebas físicas confirmadas son las comunicadas por el usuario.

Guardar sketches y headers como UTF-8 **sin BOM**. Mantener idénticas las copias de `Protocolo.h` de los dos nuevos sketches. Conservar las pruebas funcionales anteriores al desarrollar nuevas versiones.
