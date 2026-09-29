# Hurdy-Gurdy digital MIDI que suena sola — Plan

## Idea
Una zanfona (hurdy-gurdy) digital:
- **Manivela** = encoder de 600 PPR. Su velocidad es la presión del arco: más rápido = más fuerte y más brillante. Un tirón rápido activa la *trompette* (zumbido rítmico), como en la zanfona real.
- **Teclas** eligen la nota de la cuerda melódica. Sin tecla suena la cuerda al aire. Dos **bordones** (tónica + quinta) suenan siempre que gira la rueda.
- **Suena sola** (modo AUTO): una manivela virtual gira sola y un secuenciador toca melodías incluidas o archivos `.mid` de la tarjeta SD.
- **MIDI USB**: sale lo que tocas (notas + CC11 con la velocidad de manivela) y entra lo que le mandes (notas, CC1 = "girar la manivela", CC7 volumen, CC20 tónica).
- Todo se sintetiza en el ESP32-S3 → I2S → PCM5102A → PAM8403 → parlante. Con batería 18650 es portátil.

## Componentes (según tus compras)
| Pieza | Uso |
|---|---|
| ESP32-S3 N16R8 | cerebro, síntesis, USB-MIDI nativo |
| Encoder E38S6G5-600B (5–24 V) | manivela |
| PCM5102A (I2S DAC) | audio de calidad, sin hardware extra |
| PAM8403 | amplificador para el parlante |
| Módulo microSD (SPI) | canciones `.mid` |
| TP4056 + 18650 | carga y batería |
| MT3608 | sube 3.7 V → 5 V |

**Falta comprar/conseguir:** 12 pulsadores (microswitch/teclas), parlante 3 W 4–8 Ω, interruptor, resistencias 4.7 kΩ ×3 (pull-ups del encoder) y 2×100 kΩ (medir batería), condensadores 100–470 µF para el riel de 5 V, protoboard/perfboard, una celda 18650 **protegida y de alta corriente**, y el acople mecánico (manivela → eje de 6 mm del encoder).

## Cableado
Pines definidos en `src/config.h` (cámbialos ahí si quieres).

**Alimentación**
```
18650 ── TP4056 (B+/B-)      OUT+ ── interruptor ── MT3608 IN+      OUT- ── MT3608 IN-
MT3608 OUT (ajustado a 5.0 V ANTES de conectar nada) ── riel 5V ── ESP32 "5V", PAM8403, encoder, módulo SD
GND común para todo
```
- Ajusta el trimmer del MT3608 midiendo con multímetro, **sin nada conectado**, hasta 5.0 V.
- No conectes USB y batería al mismo tiempo sin un diodo Schottky en serie con el riel de batería. Para programar: apaga el interruptor.
- Pon un condensador grande (≥470 µF) cerca del PAM8403.

**Audio**
| PCM5102A | ESP32-S3 |
|---|---|
| VIN | 3V3 (o 5V) |
| GND | GND |
| BCK | GPIO 5 |
| LCK (WS) | GPIO 6 |
| DIN | GPIO 7 |
| SCK | **GND** (usa el PLL interno) |

En el reverso del módulo (puentes de soldadura), la configuración típica es: FLT→L, DEMP→L, XSMT→H, FMT→L (I2S). Revisa el tuyo.
`LOUT` → PAM8403 `L-in` (y `ROUT` → `R-in`, o une ambos). Salida de 2 Vrms es mucha para el PAM: usa un potenciómetro de 10 kΩ como volumen. El parlante va entre `L+` y `L-` del PAM (ninguno a GND).

**MicroSD** — VCC→5V (tiene regulador y adaptador de nivel), GND, CS→GPIO 10, MOSI→11, SCK→12, MISO→13. Formato FAT32, carpeta `/songs`.

**Encoder** — hilos típicos: rojo = V+, negro = GND, verde = A, blanco = B, amarillo = Z (**verifícalo con el datasheet/vendedor**).
- Alimenta a 5 V. A→GPIO 1, B→GPIO 2, Z→GPIO 42.
- Si las salidas son NPN colector abierto (lo habitual con sufijo `N`): pull-up de 4.7 kΩ de cada salida a **3.3 V**.
- Si fueran push-pull (salen 5 V): **no** las conectes directo; usa divisor 1.8 kΩ (serie) + 3.3 kΩ a GND por línea.
- Prueba antes con un multímetro: con la salida en alto, ¿mide 5 V o queda "flotando"?

**Teclas** — cada pulsador entre el GPIO y GND (pull-up interno): GPIO 8, 9, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41 (de la tecla más grave a la más aguda).
**Botón de modo** — GPIO 47 a GND. **Batería (opcional)** — divisor 100k/100k desde OUT+ a GPIO 4.

Evitados a propósito: GPIO 26–37 (flash/PSRAM octal), 19/20 (USB), 0/3/45/46 (strapping).

## Uso
- Corto en el botón de modo: **PLAY ↔ AUTO**.
- Largo en PLAY: cambia la tónica (Do → Sol → Re → La…). Largo en AUTO: siguiente canción.
- PLAY: gira la manivela y presiona teclas. AUTO: se toca sola; la tónica y los bordones se ajustan a la canción.

## Compilar y cargar
```
pip install platformio
pio run -t upload        # por el puerto USB-C marcado "UART"
pio device monitor       # logs por el mismo puerto
```
El puerto USB-C marcado "USB" (nativo) queda como dispositivo MIDI: conéctalo al PC/celular y aparece "TinyUSB MIDI".
Para probar el sonido sin hardware: `test/host/run.sh` genera `docs/demo.wav`.

## Estado
- [x] Firmware completo que compila (ESP32-S3, Arduino core 3.3.12).
- [x] Pruebas en PC del sintetizador (afinación, silencio sin arco) y del parser MIDI.
- [ ] **Nada probado en hardware todavía.** Orden sugerido:
  1. Solo ESP32 + PCM5102A: ¿suena en modo AUTO? (pulsa el botón de modo; o une el GPIO 47 a GND un instante).
  2. Encoder: mira en `pio device monitor`; gira y observa que el arco responde.
  3. Teclas y MIDI USB.
  4. SD con `.mid` en `/songs`.
  5. Batería + PAM8403 + parlante.

## Ideas siguientes
- Resonancia de caja (filtros formantes) y reverb corto para un timbre más "madera".
- Dos cuerdas melódicas independientes / cuerdas simpáticas.
- Motor + control para que la manivela gire sola de verdad (el encoder daría la velocidad real).
- Más de 12 teclas con un expansor I2C MCP23017.
- Pantalla OLED con nombre de canción y tónica.
- Mapear el sentido de giro (adelante/atrás) a timbres distintos.
