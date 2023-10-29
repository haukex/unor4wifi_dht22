#!/usr/bin/env python3
"""``favicon.ico`` to ``favicon.h`` Converter

This script is a part of: Arduino Uno R4 WiFi with DHT22 Sensor, https://github.com/haukex/unor4wifi_dht22

This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
"""
from pathlib import Path
from itertools import batched
from more_itertools import mark_ends

BASEPATH = Path(__file__).parent
INFILE = BASEPATH/'webpage'/'favicon.ico'
OUTFILE = BASEPATH/'UnoR4WiFi_DHT22'/'favicon.h'
CHUNKSZ = 20

def main():
    with INFILE.open('rb') as fh: data = fh.read()
    with OUTFILE.open('wb') as fh:
        fh.write(f'// Generated from {INFILE.relative_to(BASEPATH).as_posix()} '
                 f'by {Path(__file__).relative_to(BASEPATH).as_posix()}, DO NOT EDIT\n'.encode('ASCII'))
        fh.write(b'#ifndef FAVICON_H\n#define FAVICON_H\n')
        fh.write(f'#define FAVICON_SIZE ({len(data)})\n'.encode('ASCII') )
        fh.write(b'const uint8_t favicon_data[FAVICON_SIZE] PROGMEM = {\n')
        for _is_first, is_last, batch in mark_ends(batched(data, CHUNKSZ)):
            fh.write(b'  ')
            fh.write( ', '.join( f"{b:#04x}" for b in batch ).encode('ASCII') )
            fh.write(b'\n};\n' if is_last else b',\n')
        fh.write(
                b'void write_favicon(Print& out) {\n'
                b'  for (size_t i=0; i<FAVICON_SIZE; i++)\n'
                b'    out.write(pgm_read_byte_near(favicon_data+i));\n'
                b'}\n#endif // #ifndef FAVICON_H\n'
            )
    print(f"Converted {INFILE.relative_to(BASEPATH)} to {OUTFILE.relative_to(BASEPATH)}, {len(data)} bytes")

if __name__=='__main__': main()
