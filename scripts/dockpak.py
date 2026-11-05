#!/usr/bin/env python3
"""Dockbar's private resource pack: several files in one, readable only by
src/pak.c. Licensed artwork ships in it instead of as loose files. Not
encryption: the bytes are scrambled so that the pack contains no recognisable
PNG files.

Format (little-endian): b"DKPAK1\\0\\0", u32 count, then per entry u32 name
length, name (UTF-8), u32 offset, u32 size; then the data. Each entry's bytes
are XORed with an xorshift32 stream seeded with 0x9E3779B9 ^ offset."""
import struct, sys, pathlib

def _stream(seed, n):
    x = (0x9E3779B9 ^ seed) & 0xffffffff or 1
    out = bytearray(n)
    for i in range(n):
        x ^= (x << 13) & 0xffffffff
        x ^= x >> 17
        x ^= (x << 5) & 0xffffffff
        out[i] = x & 0xff
    return out

def scramble(data, offset):
    key = _stream(offset, len(data))
    return bytes(a ^ b for a, b in zip(data, key))

def write(path, entries):
    """entries: list of (name, bytes); names unique, no '/', ':' or '#'."""
    names = [n for n, _ in entries]
    assert len(set(names)) == len(names)
    for n in names:
        assert n and not any(c in n for c in '/:#\\') and len(n.encode()) < 256
    head = 8 + 4 + sum(4 + len(n.encode()) + 8 for n in names)
    table, blobs, offset = [], [], head
    for name, data in entries:
        table.append(struct.pack('<I', len(name.encode())) + name.encode() + struct.pack('<II', offset, len(data)))
        blobs.append(scramble(data, offset))
        offset += len(data)
    pathlib.Path(path).write_bytes(b'DKPAK1\0\0' + struct.pack('<I', len(entries)) + b''.join(table) + b''.join(blobs))

if __name__ == '__main__':
    # dockpak.py OUT FILE...  (entry names are the file names)
    write(sys.argv[1], [(pathlib.Path(f).name, pathlib.Path(f).read_bytes()) for f in sys.argv[2:]])
