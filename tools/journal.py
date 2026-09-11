"""Inspect/export PFJ1 journals; never edits the input. Python standard library only."""
import argparse
import json
import struct
import zlib
from pathlib import Path

def inspect(path):
    raw = Path(path).read_bytes()
    catches = []
    error = None
    for off in range(0, len(raw), 32):
        block = raw[off:off+32]
        if len(block) != 32:
            error = f'truncated record at byte {off}'
            break
        magic, seq, form, seed, mm, spot, version, crc = struct.unpack('<8I', block)
        if magic != 0x314a4650 or seq != len(catches)+1 or version not in (1, 2, 3) or zlib.crc32(block[:28]) != crc:
            error = f'invalid magic/sequence/version/checksum at byte {off}'
            break
        object_limit = 256 if version == 1 else 768
        if not (form < 131072 or 1048576 <= form < 1048576 + object_limit) or not 1 <= mm <= 2000 or spot > 2:
            error = f'invalid catch fields at byte {off}'
            break
        species = (form & 7) | ((form >> 12) & 8)
        canonical = (species&7) | ((species%4)<<3) | (((species//2)%4)<<5) | ((species%8)<<7) | (((species*3)%8)<<10) | (((species//4)%4)<<13) | ((1 if species>=8 else 0)<<15)
        if version == 3 and form < 1048576 and form != canonical:
            error = f'invalid v3 species form at byte {off}'
            break
        catches.append({'sequence': seq, 'appearance_id': form, 'kind': 'object' if form & 1048576 else 'fish',
                        'seed': seed, 'length_mm': mm, 'spot': spot, 'generator_version': version, 'catalogue_id': 16 + (form & 1048575) if form & 1048576 else species})
    return raw, {'status': 'partial' if error else 'valid', 'error': error, 'bytes': len(raw),
                 'valid_prefix_bytes': len(catches)*32, 'catches': len(catches),
                 'discoveries': len({c['catalogue_id'] for c in catches}), 'records': catches}

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('--export', type=Path, help='new JSON file, must not exist')
    p.add_argument('--recover-prefix', type=Path, help='copy valid prefix to a NEW file; original unchanged')
    args = p.parse_args()
    raw, result = inspect(args.input)
    if args.export:
        with args.export.open('x') as out:
            json.dump(result, out, ensure_ascii=False, indent=2)
    if args.recover_prefix:
        with args.recover_prefix.open('xb') as out:
            out.write(raw[:result['valid_prefix_bytes']])
    print(json.dumps({k:v for k,v in result.items() if k != 'records'}, ensure_ascii=False, indent=2))
    raise SystemExit(2 if result['status'] == 'partial' else 0)
