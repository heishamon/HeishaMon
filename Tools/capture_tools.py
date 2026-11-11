#!/usr/bin/env python3
"""Helpers to find which byte of the heat pump datagrams belongs to a setting.

Input is the HeishaMon log with "Hexdump" enabled (console page, or the MQTT log
topic saved to a text file). Relevant log lines:

    Received <n> bytes data         start of a datagram from the heat pump
    PROXY Received <n> bytes        start of a command from the Service Cloud
    data: XX XX ...                 the bytes, up to 32 per line

Anything in front of the log text (time stamp, phase marker) is ignored.

  capture_tools.py diff A.log B.log [C.log ...]
      Compares the main data block (203 bytes, 71 C8 01 10) of several
      captures, for example "before", "setting changed", "changed back".
      Prints the bytes whose value is stable inside each capture but differs
      between captures, with the changed bits (bit 7 = MSB).

  capture_tools.py proxy A.log [B.log ...]
      Lists the commands (F1 ..., 111 bytes) that the Service Cloud sent through
      the proxy port. A zero byte means "no change", so the non-zero bytes are
      what the cloud changed.

  capture_tools.py rawcmd 22=01 60=56
      Builds a 110-byte command with the given bytes (position=hex value) for
      the SendRawValue MQTT topic. HeishaMon adds the checksum.
"""
import re
import sys
from collections import Counter

START = re.compile(r'(PROXY )?Received (\d+) bytes( data)?\s*$')
DOMINANT = 0.9   # a byte counts as stable if one value is in >= 90 % of the blocks


def frames(path):
    """Yield (is_proxy, bytes) for every complete datagram with a valid checksum."""
    cur = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if 'data: ' in line:
                if cur is None:
                    continue
                try:
                    cur['b'] += [int(x, 16) for x in line.split('data: ', 1)[1].split()]
                except ValueError:
                    cur = None
                    continue
                if len(cur['b']) >= cur['n']:
                    b = cur['b'][:cur['n']]
                    if sum(b) & 0xFF == 0:
                        yield cur['proxy'], b
                    cur = None
                continue
            m = START.search(line)
            cur = {'proxy': bool(m.group(1)), 'n': int(m.group(2)), 'b': []} if m else None


def stable_values(path):
    """Most common value of every byte of the main data block, None if not stable."""
    blocks = [b for proxy, b in frames(path)
              if not proxy and len(b) == 203 and b[0] == 0x71 and b[3] == 0x10]
    if not blocks:
        sys.exit(f'{path}: no valid 203-byte data block found')
    out = []
    for i in range(203):
        value, n = Counter(b[i] for b in blocks).most_common(1)[0]
        out.append(value if n / len(blocks) >= DOMINANT else None)
    return len(blocks), out


def cmd_diff(paths):
    if len(paths) < 2:
        sys.exit('diff needs at least two captures')
    caps = [stable_values(p) for p in paths]
    print('blocks per capture: ' + ', '.join(f'{p}={n}' for p, (n, _) in zip(paths, caps)))
    hits = 0
    for i in range(202):   # byte 202 is the checksum
        vals = [v[i] for _, v in caps]
        if None in vals or len(set(vals)) == 1:
            continue
        changed = 0
        for v in vals:
            changed |= v ^ vals[0]
        bits = ', '.join(f'bit {k}' for k in range(7, -1, -1) if changed & (1 << k))
        print(f'byte {i:3d}: ' + '  '.join(f'0x{v:02X}/{v:08b}' for v in vals) + f'   ({bits})')
        hits += 1
    print(f'{hits} byte(s) differ between the captures')
    unstable = [i for i in range(202) if any(v[i] is None for _, v in caps)]
    print(f'unstable bytes (ignored): {unstable}')


def cmd_proxy(paths):
    kinds = Counter()
    for path in paths:
        for proxy, b in frames(path):
            if not proxy:
                continue
            kinds[(f'0x{b[0]:02X}', len(b))] += 1
            if b[0] == 0xF1:
                payload = [(i, v) for i, v in enumerate(b[:-1]) if i >= 4 and v]
                print(', '.join(f'byte {i}=0x{v:02X}' for i, v in payload) or 'no non-zero bytes')
    print('proxy datagrams (header, length):', dict(kinds))


def raw_command(items):
    """Return the 110 command bytes (without checksum) for position=hex items."""
    b = bytearray(110)
    b[0:4] = bytes([0xF1, 0x6C, 0x01, 0x10])
    for item in items:
        pos, val = item.split('=')
        if not 4 <= int(pos) < 110:
            sys.exit(f'position {pos} outside 4..109')
        b[int(pos)] = int(val, 16)
    return bytes(b)


def cmd_rawcmd(items):
    b = raw_command(items)
    print(' '.join("b'" + ''.join('\\x%02X' % x for x in b[k:k + 10]) + "'"
                   for k in range(0, 110, 10)))


if __name__ == '__main__':
    commands = {'diff': cmd_diff, 'proxy': cmd_proxy, 'rawcmd': cmd_rawcmd}
    if len(sys.argv) < 3 or sys.argv[1] not in commands:
        sys.exit(__doc__)
    commands[sys.argv[1]](sys.argv[2:])
