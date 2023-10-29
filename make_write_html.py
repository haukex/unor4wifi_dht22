#!/usr/bin/env python3
"""``index.html`` to ``write_html.h`` Converter

This script is a part of: Arduino Uno R4 WiFi with DHT22 Sensor, https://github.com/haukex/unor4wifi_dht22

This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
"""
from pathlib import Path

BASEPATH = Path(__file__).parent
INFILE = BASEPATH/'webpage'/'dist'/'index.html'
OUTFILE = BASEPATH/'UnoR4WiFi_DHT22'/'write_html.h'
CHUNKSZ = 256

def main():
    with INFILE.open('rb') as fh: data = fh.read()
    assert b'I_am_unique' not in data
    with OUTFILE.open('wb') as fh:
        fh.write(f'// Generated from {INFILE.relative_to(BASEPATH).as_posix()} '
                 f'by {Path(__file__).relative_to(BASEPATH).as_posix()}, DO NOT EDIT\n'.encode('ASCII'))
        fh.write(b'#ifndef WRITE_HTML_H\n#define WRITE_HTML_H\n')
        fh.write(f'#define WRITE_HTML_SIZE ({len(data)})\n'.encode('ASCII') )
        fh.write(b'void write_html(Print& out) {\n')
        for chunknum, offset in enumerate(range(0, len(data), CHUNKSZ), start=1):
            fh.write(b'  out.print(F(R"I_am_unique(')
            fh.write(data[offset:offset+CHUNKSZ])
            fh.write(b')I_am_unique"));\n')
        fh.write(b'}\n#endif // #ifndef WRITE_HTML_H\n')
    print(f"Converted {INFILE.relative_to(BASEPATH)} to {OUTFILE.relative_to(BASEPATH)}, "
          f"{len(data)} bytes split into {chunknum} chunks of {CHUNKSZ} bytes")

if __name__=='__main__': main()
