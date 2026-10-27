# Prueba rapida MPU-9250 remoto con dos ESP32-S3

Son dos sketches separados. Cada placa tiene sus controladores I2C locales; el bus de enlace existe porque se conectan fisicamente SDA, SCL y GND entre las placas. `Wire` y `Wire1` solo identifican controladores dentro de cada ESP32.

## Conexiones

| Senal | ESP32 principal | ESP32 sensor |
| --- | --- | --- |
| OLED SDA | D8 | - |
| OLED SCL | D9 | - |
| Enlace SDA | D6 | D6 |
| Enlace SCL | D7 | D7 |
| MPU-9250 SDA | - | D8 |
| MPU-9250 SCL | - | D9 |
| GND comun | GND | GND |

Usar logica de 3,3 V y pull-ups apropiados. Los dos sketches usan los alias `D6`/`D7`/`D8`/`D9` tal como aparecen en el ejemplo. Esos alias dependen de la definicion de placa: seleccionar el mismo modelo/paquete de placa para ambos ESP32-S3 y confirmar en su pinout que los alias correspondan a los pines fisicos cableados. En cada enlace, conectar D6 con D6, D7 con D7 y GND con GND.

## Sketches y buses

- `esp32_principal/esp32_principal.ino`: `Wire` maestro para OLED `0x3C` en D8/D9; `Wire1` maestro para consultar al nodo `0x12` en D6/D7.
- `esp32_sensor/esp32_sensor.ino`: `Wire` maestro para MPU-9250 `0x69` en D8/D9; `Wire1` esclavo `0x12` para responder al principal en D6/D7.

El nodo lee aceleracion aproximadamente cada 20 ms y guarda una respuesta fija. El callback I2C solo copia esos 9 bytes; no lee el IMU ni imprime. El principal consulta cada 100 ms y muestra los ejes en g. No hay calibracion, giroscopio ni magnetometro en esta prueba.

El paquete contiene estado, secuencia, tres aceleraciones signed de 16 bits en milli-g y XOR de comprobacion. Se asume que FastIMU entrega `accelX/Y/Z` en g y que la libreria usa `Wire` global, como en el ejemplo proporcionado.

## Preparacion

Instalar FastIMU y U8g2, seleccionar ESP32-S3 y cargar cada sketch en su carpeta a la placa correspondiente. El constructor OLED supone un SSD1306 128x64; si el modulo usa SH1106, cambiarlo por el constructor U8g2 correspondiente. El esclavo usa `Wire1.begin(direccion, SDA, SCL, frecuencia)` del core Arduino-ESP32; verificar que la version instalada admita callbacks de esclavo para `Wire1`.