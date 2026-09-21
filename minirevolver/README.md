# Motor paso a paso: posiciones y búsqueda de origen

[minirevolver.ino](minirevolver.ino) controla un driver de motor paso a paso mediante STEP/DIR usando la biblioteca **ESP_FlexyStepper**. Es independiente del sensor y del ventilador.

Hay un ejemplo nuevo separado en [minirevolveri2c](../minirevolveri2c/README.md) para recibir órdenes y devolver estado por I2C. Esa propuesta conserva este archivo original y todavía requiere compilación y prueba por el usuario.

## Placa y conexiones

| Señal | GPIO configurado |
| --- | --- |
| STEP del driver | 5 |
| DIR del driver | 23 |
| Final de carrera | 27, configurado como INPUT_PULLUP |

**No es compatible con el ESP32-C3 con esta asignación:** el C3 no tiene GPIO23 ni GPIO27. Seleccionar una placa que exponga esos GPIO o adaptar los pines antes de usarlo. El sketch no identifica el modelo exacto de placa ni configura USB CDC.

Instalar `ESP_FlexyStepper` en el entorno del usuario. La alimentación del motor y la configuración de corriente/micropasos corresponden al driver; el código no las configura. Verificar la conexión y polaridad del final de carrera según la biblioteca y el montaje.

## Comandos implementados

Monitor serial a **115200**. Los destinos son posiciones absolutas en **pasos**, no grados ni milímetros.

| Comando previsto | Acción |
| --- | --- |
| `1` | Ir a posición 150 |
| `2` | Ir a posición 350 |
| `3` | Ir a posición 550 |
| `H` o `h` | Ejecutar `moveToHomeInSteps(-1, 150, 2000, HOMING_SWITCH_PIN)` |
| Otro número | Interpretar un destino con `Serial.parseInt()` |

Para los movimientos de posición se fijan velocidad de 150 pasos/s, aceleración de 500 pasos/s² y desaceleración de 250 pasos/s². El programa no realiza homing al encender y no exige haberlo completado antes de mover. La referencia física requiere establecer el origen.

## Limitaciones encontradas en la revisión

- Se decide la acción por el **primer carácter**, no por una línea completa. Por ejemplo, `160` entra en el caso de `1` y selecciona 150; `250` selecciona 350 y `350` selecciona 550. Los bytes restantes se descartan si ya llegaron, o pueden procesarse después si llegan separados.
- Un salto de línea que llegue después del vaciado del búfer puede caer en `parseInt()`, esperar su timeout y acabar solicitando posición 0. Una entrada no numérica también puede terminar en 0. No hay validación completa de comandos.
- Para probar los comandos simples con el código actual, usar **Sin fin de línea** y enviar uno por vez. Eso no corrige la ambigüedad de los números que empiezan por 1, 2 o 3.
- Los movimientos se realizan mediante llamadas bloqueantes: no hay procesamiento de nuevos comandos durante el movimiento ni comando de parada implementado.
- La versión instalada de la biblioteca no se verificó en esta revisión; confirmar en ella el comportamiento de homing y su referencia de posición.

Estas limitaciones se documentan; no se cambió el comportamiento del sketch en esta revisión. No se compiló ni cargó firmware.
