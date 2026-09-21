# Prueba aislada de PWM por serial

Abrir [pruebaPWM.ino](pruebaPWM.ino). Este sketch usa `analogWrite()` en **GPIO9**, con la misma forma de control que el ejemplo Fade. El usuario confirmó que funciona en otro hardware ESP32-C3. Sirvió como base para [ventiladorFlujo](../ventiladorFlujo/README.md).

## Configuración

ESP32-C3, Arduino-ESP32 **2.0.17**, USB nativo y **USB CDC On Boot = Enabled**. Monitor a **115200** con **Nueva línea** o **Ambos NL y CR**.

Solo controla GPIO9. No lee el sensor ni configura los enables del puente H. Para observarlo con un LED externo, usar resistencia en serie. GPIO9 también participa en el arranque; identificar el GPIO real en la placa y evitar pulsar BOOT durante la prueba.

## Comportamiento y comandos

Después de la espera inicial de un segundo, arranca automáticamente con **PWM 128**, aproximadamente 50 %. Usa la configuración predeterminada de `analogWrite` del core 2.0.17: **8 bits y 1 kHz**. No configura directamente canales LEDC.

| Entrada | Resultado |
| --- | --- |
| `32` | Aplica 32 de 255 |
| `128` o `PWM 128` | Aplica aproximadamente 50 % |
| `255` | Salida al máximo |
| `0` o `STOP` | Apaga la salida PWM |

Se aceptan enteros de 0 a 255, espacios exteriores y comandos en minúsculas. Una entrada inválida o demasiado larga se descarta sin modificar el PWM. El monitor confirma cada cambio con `PWM GPIO9 = valor/255`; no imprime continuamente ni acepta `ESTADO` o `AYUDA`.

Cerrar el monitor no cambia el PWM. Un reinicio vuelve al valor inicial 128. Esto difiere del sketch integrado, que arranca apagado.

Revisión de código y documentación sin compilar ni cargar por el asistente. Guardar el archivo sin BOM.
