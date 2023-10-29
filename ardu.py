#!/usr/bin/env python3
"""Arduino Upload/Compliation Script

This script is a part of: Arduino Uno R4 WiFi with DHT22 Sensor, https://github.com/haukex/unor4wifi_dht22

This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
"""
import argparse
from pathlib import Path
import subprocess
import json

WORKDIR = Path(__file__).parent
TARGET = 'UnoR4WiFi_DHT22'
BOARD_FQBN = 'arduino:renesas_uno:unor4wifi'
BAUDRATE = 115200

def get_ardu_port() -> str|None:
    rv = subprocess.run(['arduino-cli','board','list','--fqbn',BOARD_FQBN,'--format','json'],
                        capture_output=True, check=True, encoding='ASCII')
    assert rv.stderr == ''
    boards = json.loads(rv.stdout)
    assert isinstance(boards, list)
    return boards[0]['port']['address'] if len(boards)==1 else None

def monitor_port(port :str):
    try: subprocess.run(['arduino-cli','monitor','-p',port,'--config',f'baudrate={BAUDRATE}'], check=False)
    except KeyboardInterrupt: return

def main():
    parser = argparse.ArgumentParser(description='My Arduino Script')
    parser.add_argument('command', choices=['build', 'upload', 'monitor'])
    args = parser.parse_args()
    port = get_ardu_port()
    if args.command == 'monitor':
        if port: monitor_port(port)
        else: raise RuntimeError("Can't monitor, no port found")
    else:
        assert args.command in ('build','upload')
        print(f"Compiling {TARGET} for {BOARD_FQBN}")
        subprocess.run(['arduino-cli','compile','--fqbn',BOARD_FQBN,TARGET], check=True)
        if args.command == 'upload':
            if port:
                print(f"Found one board at {port}, uploading")
                subprocess.run(['arduino-cli','upload','-p',port,'--fqbn',BOARD_FQBN,TARGET], check=True)
                monitor_port(port)
            else:
                print(f"Not uploading because I didn't find exactly one board")

if __name__=='__main__': main()
