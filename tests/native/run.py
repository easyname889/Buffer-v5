#!/usr/bin/env python3
"""Actual-source audit. Exit 1 means a safety/goal assertion failed, not xfailed.
GPIO polarity is logical; no mechanical model, UART wire timing or real EEPROM.
Run from anywhere: python3 tests/native/run.py. Fresh process per scenario.
"""
import collections
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tests/native/artifacts'
COMMON = '''mdm_inputs pulse_direction pulse_wrap random_invariants basic low_override high_trim debounce hall_priority timeout_stop
 timeout_boundary timer_reset rollover pulse chain_edge bounce_key double_toggle
 long_jog_safety jog_chain_pulse stuck_reverse uart_failure timeout_lamp error_blink
 mdm_blink blockage_output blockage_blink serial_invalid serial_token serial_scale
 serial_speed zero_steps'''.split()
BACK = '''y_gate empty_receiver cooldown_repeat restart_occupied_y lost_token_reload
 no_token_stop runout duplicate_claim jog_timeout stop_enable'''.split()
FRONT = 'front_standalone front_lamp front_noise_restart'.split()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    compiler = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-O1', '-g',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT / 'tests/native'), '-DY_JUNCTION_SENSOR']
    if platform.system() == 'Darwin':
        sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
        compiler += ['-isystem', sdk + '/usr/include/c++/v1']
    results = []
    for role in ['back', 'front', 'back_debug', 'front_debug']:
        binary = OUT / role
        flags = ['-DFRONT_BUFFER_STANDALONE'] if role.startswith('front') else []
        if role.endswith('_debug'):
            flags += ['-DSENSOR_DEBUG']
        cmd = compiler + flags + [str(ROOT / 'tests/native/simulation.cpp'), '-o', str(binary)]
        build = subprocess.run(cmd, capture_output=True, text=True)
        (OUT / (role + '-compile.log')).write_text(build.stdout + build.stderr)
        if build.returncode:
            print(build.stderr)
            return 2
        cases = COMMON + (FRONT if role.startswith('front') else BACK)
        for case in cases:
            try:
                result = subprocess.run([str(binary), case], capture_output=True, text=True, timeout=8)
                entry = dict(role=role, case=case, returncode=result.returncode,
                             output=(result.stdout + result.stderr).strip())
            except subprocess.TimeoutExpired:
                entry = dict(role=role, case=case, returncode=124, output='HOST TIMEOUT after 8s')
            results.append(entry)
            print(role, entry['output'])
    counts = dict(collections.Counter('pass' if x['returncode'] == 0 else 'fail' for x in results))
    data = dict(timestamp_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                firmware_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                source_sha256=hashlib.sha256((ROOT / 'lib/buffer/buffer.cpp').read_bytes()).hexdigest(),
                simulation='Unchanged production C++; mocked GPIO, EEPROM, TMC2209, HAL and clock',
                counts=counts, results=results)
    (OUT / 'results.json').write_text(json.dumps(data) + '\n')
    print('TOTAL', len(results), counts)
    return int(bool(counts.get('fail')))


if __name__ == '__main__':
    sys.exit(main())
