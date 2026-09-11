#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
c++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude src/game.cpp src/view.cpp src/lore.cpp src/events.cpp tests/native.cpp tests/features.cpp -o build/native-tests
./build/native-tests
python3 tests/journal_cli.py
