# AFM3000 con ESP32-C3

Abrir `airSensor.ino` en Arduino IDE. Solo necesita `Wire`, incluida en el paquete de placas ESP32.

En este montaje se usa el **USB nativo del ESP32-C3**, confirmado para esta prueba. El objetivo actual es únicamente obtener el valor crudo y el caudal del sensor por el monitor serie.

## Conexiones

El manual especifica alimentación de 4,75 a 5,25 V, nivel alto I2C mínimo de 4 V y resistencias internas de 4,7 kΩ hacia VDD. Usar un conversor bidireccional para I2C de 3,3 V a 5 V (por ejemplo, un módulo BSS138 con pull-ups). No conectar SDA/SCL del sensor directamente al ESP32-C3 ni alimentar el sensor con 3,3 V.

| Señal | Conexión |
| --- | --- |
| VDD del AFM3000 | 5 V |
| GND del AFM3000 | GND común con ESP32 y conversor |
| LV del conversor | 3,3 V del ESP32 |
| HV del conversor | 5 V |
| SDA del AFM3000 | Canal 1, lado HV del conversor |
| GPIO20 del ESP32 | Canal 1, lado LV del conversor |
| SCL del AFM3000 | Canal 2, lado HV del conversor |
| GPIO21 del ESP32 | Canal 2, lado LV del conversor |

Colocar 100 nF entre VDD y GND cerca del sensor, según el manual. Identificar las señales por la figura 2 del manual y las marcas del conector: la posición de izquierda a derecha en el dibujo no sigue el orden numérico de la tabla 9.

## Carga y monitor

1. Seleccionar la placa ESP32-C3 correspondiente (por ejemplo, `ESP32C3 Dev Module`).
2. Activar **Herramientas > USB CDC On Boot > Enabled**.
3. Conectar al USB nativo de la placa, cargar y abrir el monitor serie a **115200**.

GPIO20 y GPIO21 son también RX/TX de UART0. Este ejemplo usa `Serial` por USB CDC para dejarlos disponibles para I2C. Si la placa lleva un puente USB-UART conectado físicamente a esos pines, hay que aislarlo para usarlos con el sensor. Activar CDC no desconecta ese circuito. Una placa que solo expone USB mediante un puente USB-UART requiere adaptar la conexión de depuración o elegir otros GPIO para I2C.

## Lectura

- Dirección I2C de 7 bits: `0x40` (no usar `0x80`/`0x81` con Wire).
- Comando por lectura: `0x10`, `0x00`; respuesta: byte alto, byte bajo y CRC8.
- CRC8 inicial `0x00`, polinomio `0x131` (implementado con `0x31` en 8 bits).
- Caudal de aire/N2: `(RAW - 32000) / 140`, en slm. Para O2 cambiar `FLOW_SCALE` a `142.8f`.
- Se descarta la primera lectura y se informa si hay errores de comunicación o CRC.
- Espera de 20 ms tras el comando como margen de implementación sobre el ciclo de muestreo de 10 ms. El usuario confirmó que este ejemplo entrega lecturas en su montaje.

Ejemplo: `RAW: 33400 | Flujo: 10.00 slm`. La unidad slm corresponde a litros estándar por minuto a 20 °C y 1013 mbar según el manual.

Fuente del protocolo: `Mass Flow Meter AFM3000 Product Manual.pdf`, secciones 2, 3 y 4.

## Comportamiento del sketch

No recibe comandos seriales. Tras la espera inicial de 1,5 s, consulta el sensor y muestra cada lectura válida; entre ciclos normales hay 100 ms de pausa, además de los 20 ms de espera I2C. El tiempo total entre lecturas es aproximadamente 120 ms más la comunicación.

Si falla una lectura, informa el error y espera 1 s antes de reintentar. Descarta la primera lectura válida al iniciar y al recuperar la comunicación. Si falla `Wire.begin`, este ejemplo aislado permanece en una espera indefinida; es distinto del comportamiento del sketch integrado.

Para ajustar el ventilador y leer el caudal simultáneamente usar [ventiladorFlujo](../ventiladorFlujo/README.md). Los 2 a 35 slm observados allí pertenecen al montaje, no definen el rango nominal del AFM3000.

Revisión actual de código y documentación, sin compilar ni cargar por el asistente. El usuario realiza esas tareas. Guardar los sketches sin BOM.

## Referencias

API I2C: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html
USB CDC: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html
