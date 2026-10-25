import os

files = ["CUBE.STL", "BOWL.STL", "GEAR.STL", "ICOSPH.STL", "KNOT.STL", "TORUS.STL"]

with open("ModelsData.cpp", "w") as out_cpp, open("ModelsData.h", "w") as out_h:
    out_h.write("#pragma once\n#include <stdint.h>\n#include <pgmspace.h>\n\n")
    out_cpp.write('#include "ModelsData.h"\n\n')

    for f in files:
        if not os.path.exists(f):
            print(f"Файл {f} не найден, пропускаем...")
            continue
            
        name = f.split('.')[0]
        with open(f, "rb") as stl:
            data = stl.read()

        size = len(data)
        print(f"Обработан {f}: {size} байт")

        # Только объявление массива (с размером, чтобы sizeof работал)
        out_h.write(f"extern const uint8_t MODEL_{name}[{size}] PROGMEM;\n")

        # Сами данные
        out_cpp.write(f"const uint8_t MODEL_{name}[{size}] PROGMEM = {{\n")
        hex_data = [f"0x{b:02X}" for b in data]
        for i in range(0, len(hex_data), 16):
            out_cpp.write("    " + ", ".join(hex_data[i:i+16]) + ",\n")
        out_cpp.write("};\n\n")

print("Готово! Файлы ModelsData.h и ModelsData.cpp созданы.")