#!/usr/bin/env python3
"""Read-only USB CDC capture: no writes/commands, bounded duration, DTR/RTS false.
Opening a serial port may still affect some USB devices; use an idle safe rig.
Usage: uv run --with pyserial python tests/native/listen.py [seconds]
"""
import concurrent.futures
import datetime
import json
from pathlib import Path
import sys
import time
import serial
from serial.tools import list_ports

OUT = Path(__file__).resolve().parent / 'artifacts'


def listen(device, duration, stamp):
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = device
    chunks = []
    result = {'device': device, 'baud': 115200, 'writes': 0, 'received_bytes': 0}
    try:
        port.open()
        start = time.monotonic()
        while time.monotonic() - start < duration:
            data = port.read(min(max(port.in_waiting, 1), 8192))
            if data:
                result['received_bytes'] += len(data)
                chunks.append({'elapsed': round(time.monotonic() - start, 3),
                               'text': data.decode('utf-8', errors='replace')})
    except Exception as exc:
        result['error'] = str(exc)
    finally:
        port.close()
    result['chunks'] = chunks
    path = OUT / (stamp + '-' + Path(device).name + '.json')
    path.write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 8
    if not 0 < duration <= 60:
        raise ValueError('capture duration must be >0 and <=60 seconds')
    ports = [p for p in list_ports.comports() if p.device.startswith('/dev/cu.usbmodem')]
    print(json.dumps([{'device': p.device, 'description': p.description,
                       'serial': p.serial_number, 'location': p.location,
                       'vid': p.vid, 'pid': p.pid} for p in ports], indent=2), flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('live-%Y%m%dT%H%M%SZ')
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, len(ports))) as pool:
        results = list(pool.map(lambda p: listen(p.device, duration, stamp), ports))
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
