#!/usr/bin/env python3
"""2/3/4 rear MCUs + one front MCU, each running production C++ in its own process.
Shared Y is a scripted sensor trace, not simulated filament mechanics. Loop 5ms.
Run run.py first to build binaries. Exit 1 on violated topology goals.
"""
import json
from pathlib import Path
import selectors
import subprocess
import sys

OUT = Path(__file__).resolve().parent / 'artifacts'


class Board:
    def __init__(self, role):
        self.p = subprocess.Popen([str(OUT / role), 'board'], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.p.stdout, selectors.EVENT_READ)
        self.state = [16, 0, 1, 1, 0, 0, 1, 0]
        self.role = role

    def step(self, fil=0, y=1, halls=0, chain=1, token=-1, dt=5):
        self.p.stdin.write(f'{dt} {fil} {y} {halls} {chain} {token}\n')
        self.p.stdin.flush()
        if not self.selector.select(3):
            raise TimeoutError(f'{self.role} MCU simulation blocked')
        line = self.p.stdout.readline()
        if not line:
            raise RuntimeError(self.p.stderr.read())
        self.state = list(map(int, line.split()))
        return self.state

    def close(self):
        self.p.stdin.close()
        try:
            self.p.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.selector.close()
        self.p.stdout.close()
        self.p.stderr.close()


def flow(n, scenario):
    rear = [Board('back') for _ in range(n)]
    front = Board('front')
    changes = []
    previous = None
    flags = {'occupied_wait': False, 'second_feeding': False,
             'front_runout_stop': False, 'front_resume': False,
             'two_forward': False}
    try:
        for t in range(0, 1800, 5):
            # A runs out at 300ms. Its tail occupies Y until 700ms.
            # New filament reaches Y at 1000ms; front loop empties at 750ms.
            y = int(t < 100 or 700 <= t < 1000)
            if scenario == 'duplicate_token':
                y = 1 if t < 100 else 0
            old = [b.state[3] for b in rear]
            for i, b in enumerate(rear):
                fil = int(i == 0 and t >= 300)
                if scenario == 'skip_empty' and i == 1:
                    fil = 1
                if scenario == 'duplicate_token':
                    fil = 0
                claim = int(i == 0) if t == 0 else -1
                if scenario == 'duplicate_token' and t == 0 and i == 1:
                    claim = 1
                b.step(fil=fil, y=y, chain=old[i - 1], token=claim)
            front.step(y=y, halls=1 if 750 <= t < 1100 else 0)
            state = [b.state[1:] for b in rear] + [front.state[1:]]
            if state != previous:
                changes.append(dict(ms=t, rear=state[:-1], front=state[-1]))
                previous = state
            if 400 <= t < 650 and rear[1].state[1] and rear[1].state[2] == 1:
                flags['occupied_wait'] = True
            if 800 <= t < 950 and rear[1].state[2] == 0:
                flags['second_feeding'] = True
            if 800 <= t < 950 and front.state[2] == 1:
                flags['front_runout_stop'] = True
            if 1030 <= t < 1100 and front.state[2] == 0:
                flags['front_resume'] = True
            if sum(b.state[2] == 0 for b in rear) > 1:
                flags['two_forward'] = True
        if scenario == 'normal':
            passed = all(flags[k] for k in ['occupied_wait', 'second_feeding',
                                           'front_runout_stop', 'front_resume']) and not flags['two_forward']
        elif scenario == 'skip_empty':
            passed = rear[2].state[1] == 1 and rear[2].state[2] == 0
        else:
            passed = not flags['two_forward']
        return dict(rear_count=n, scenario=scenario, passed=passed, flags=flags,
                    columns=['token', 'motor(0=forward,1=stop,2=back)', 'chain_out',
                             'start_led', 'error', 'status_PB15', 'motor_enable(active_low)'], trace=changes)
    finally:
        for b in rear + [front]:
            b.close()


def main():
    results = [flow(n, s) for n in (2, 3, 4)
               for s in ('normal', 'skip_empty', 'duplicate_token')
               if not (n == 2 and s == 'skip_empty')]
    (OUT / 'chain-results.json').write_text(json.dumps(results) + '\n')
    for r in results:
        print('PASS' if r['passed'] else 'FAIL', r['rear_count'], r['scenario'], r['flags'])
    print('TOTAL', len(results), 'PASS', sum(r['passed'] for r in results))
    return int(any(not r['passed'] for r in results))


if __name__ == '__main__':
    sys.exit(main())
