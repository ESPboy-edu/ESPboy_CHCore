import os
import sys

P8_FILE = sys.argv[1] if len(sys.argv) > 1 else "game.p8"
OUT_FILE = "PicoAudioData.h"

def extract_audio():
    if not os.path.exists(P8_FILE):
        print(f"Ошибка: Файл {P8_FILE} не найден!")
        return

    with open(P8_FILE, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    sfx_lines = []
    music_lines = []
    mode = None

    for line in lines:
        clean = line.strip()
        if clean.startswith("__"):
            mode = clean
            continue
        if mode == "__sfx__":
            if clean: sfx_lines.append(clean)
        elif mode == "__music__":
            if clean: music_lines.append(clean)

    with open(OUT_FILE, "w", encoding="utf-8") as out:
        out.write("// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО ИЗ .p8\n")
        out.write("#pragma once\n#include <Arduino.h>\n#include <PicoSynth.h>\n\n")

        out.write("const P8Sfx PICO_SFX[64] PROGMEM = {\n")
        for idx in range(64):
            line = sfx_lines[idx] if idx < len(sfx_lines) else "00" * 86
            speed = int(line[2:4], 16)
            loop_start = int(line[4:6], 16)
            loop_end = int(line[6:8], 16)

            notes = []
            for n in range(32):
                pos = 8 + n * 5
                chunk = line[pos:pos+5] if pos+5 <= len(line) else "00000"
                pitch = int(chunk[0:2], 16)
                wave = int(chunk[2], 16)
                vol = int(chunk[3], 16)
                fx = int(chunk[4], 16)
                notes.append(f"{{{pitch},{wave},{vol},{fx}}}")

            out.write(f"  {{ {{ {', '.join(notes)} }}, {speed}, {loop_start}, {loop_end} }},\n")
        out.write("};\n\n")

        out.write("const P8Pattern PICO_MUSIC[64] PROGMEM = {\n")
        for idx in range(64):
            line = music_lines[idx] if idx < len(music_lines) else "00 40404040"
            parts = line.split()
            flags = int(parts[0], 16) if len(parts) > 0 else 0
            loop_begin = bool(flags & 1)
            loop_end = bool(flags & 2)
            stop = bool(flags & 4)

            chans = []
            # Правильное чтение 4-х каналов по 2 символа (с игнорированием пробелов)
            ch_str = "".join(parts[1:])
            if len(ch_str) < 8: ch_str = ch_str.ljust(8, '0')
            for c in range(4):
                val = int(ch_str[c*2:c*2+2], 16) & 0x7F
                chans.append(str(val))

            out.write(f"  {{ {{ {', '.join(chans)} }}, {str(loop_begin).lower()}, {str(loop_end).lower()}, {str(stop).lower()} }},\n")
        out.write("};\n")

    print(f"Готово: файл {OUT_FILE} успешно создан!")

if __name__ == "__main__":
    extract_audio()