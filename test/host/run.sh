#!/bin/sh
# Host-side test: renders docs/demo.wav and checks pitch + MIDI parser (needs g++)
cd "$(dirname "$0")" && g++ -std=c++17 -O1 -fsanitize=address,undefined -Istubs -I../../src \
  test_host.cpp ../../src/synth.cpp ../../src/player.cpp -o /tmp/hurdy_test_host && \
  /tmp/hurdy_test_host && mv -f demo.wav ../../docs/demo.wav
