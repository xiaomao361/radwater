"""The repair/export command must preserve the original, including on partial input."""
from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    p = Path(td)
    source = p / 'original.pfj'
    header = struct.pack('<7I', 0x314a4650, 1, 0, 42, 123, 0, 1)
    valid = header + struct.pack('<I', zlib.crc32(header))
    source.write_bytes(valid)
    command = [sys.executable, str(root/'tools/journal.py'), str(source)]
    result = subprocess.run(command, capture_output=True, check=True)
    assert json.loads(result.stdout)['status'] == 'valid'
    damaged = valid + b'partial'
    source.write_bytes(damaged)
    output = p/'recovered.pfj'
    result = subprocess.run(command+['--export',str(p/'export.json'),'--recover-prefix',str(output)],capture_output=True)
    assert result.returncode == 2 and json.loads(result.stdout)['status'] == 'partial'
    assert source.read_bytes() == damaged and output.read_bytes() == valid
    assert json.loads((p/'export.json').read_text())['valid_prefix_bytes'] == 32
    result = subprocess.run(command+['--recover-prefix',str(output)],capture_output=True)
    assert result.returncode != 0 and source.read_bytes() == damaged and output.read_bytes() == valid
    # Cross-language fixture produced by the current C++ Journal, including v1/v2 records.
    mixed = root/'build/mixed-v1-v2.pfj'
    mixed_bytes = mixed.read_bytes()
    result = subprocess.run([sys.executable,str(root/'tools/journal.py'),str(mixed),'--export',str(p/'mixed.json')],capture_output=True,check=True)
    exported = json.loads((p/'mixed.json').read_text())
    assert exported['status']=='valid' and exported['catches']==35
    assert [r['generator_version'] for r in exported['records'][-3:]]==[1,2,2]
    assert [r['appearance_id'] for r in exported['records'][-3:]]==[1048576,1048832,1049088]
    assert mixed.read_bytes()==mixed_bytes
    # Valid CRC cannot turn unknown versions or out-of-range new IDs into healthy data.
    for form,version in [(1048832,1),(1049344,2),(1048576,3)]:
        invalid=struct.pack('<7I',0x314a4650,2,form,42,123,0,version)
        raw=valid+invalid+struct.pack('<I',zlib.crc32(invalid))
        source.write_bytes(raw)
        result=subprocess.run(command,capture_output=True)
        report=json.loads(result.stdout)
        assert result.returncode==2 and report['status']=='partial' and report['valid_prefix_bytes']==32
        assert source.read_bytes()==raw
print('journal CLI: valid/partial, export, recovery to new file, overwrite refusal PASS')
print('journal CLI: C++ mixed v1/v2 fixture and invalid version/ID boundaries PASS')
