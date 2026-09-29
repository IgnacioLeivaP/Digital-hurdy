# Digital Hurdy-Gurdy

Zanfona digital MIDI con ESP32-S3 que también puede sonar sola. Ver **[docs/PLAN.md](docs/PLAN.md)** para el plan, cableado y pasos de prueba.

```
src/config.h   pines y parámetros    src/synth.*   sintetizador (rueda, melodía, bordones, trompette)
src/crank.*    encoder / manivela    src/player.*  melodías incluidas + .mid desde SD
src/keys.*     teclas y botón        src/main.cpp  modos PLAY/AUTO y MIDI USB
```
Compilar: `pio run` · Cargar: `pio run -t upload` · Escuchar la síntesis sin hardware: `test/host/run.sh` → `docs/demo.wav`.
