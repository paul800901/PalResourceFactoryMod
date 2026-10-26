"""Find current processor prologue candidates in a PE; never executes game code.

Matches are research candidates only, not permission to update runtime addresses.
"""
import argparse
import hashlib
import re
import struct
from pathlib import Path
import pefile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('exe', type=Path)
a = p.parse_args()
data = a.exe.read_bytes()
pe = pefile.PE(data=data, fast_load=True)
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
starts = {x[0] for x in struct.iter_unpack('<III', pe.get_data(d.VirtualAddress, d.Size))}
print('SHA256', hashlib.sha256(data).hexdigest())
print('TIMESTAMP', pe.FILE_HEADER.TimeDateStamp, 'IMAGE_SIZE', hex(pe.OPTIONAL_HEADER.SizeOfImage))
root = Path(__file__).resolve().parents[1]
manual = (root / 'native/src/ManualSettlement.hpp').read_text(encoding='utf-8')
patterns = [(m[0], m[1]) for m in re.findall(r'entry\((0x[0-9a-f]+),\s*\{([^}]+)\}', manual)]
for file, name in [('SettlementRuntime.hpp', 'butcher'), ('ProcessorNative.hpp', 'hatch')]:
    src = (root / 'native/src' / file).read_text(encoding='utf-8')
    match = re.search(r'constexpr std::array<uint8_t, (?:32|16)> (?:signature|prefix)\{([^}]+)\}', src)
    patterns.append((name, match[1]))
for name, raw in patterns:
    sig = bytes(int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', raw))
    hits = []
    for s in pe.sections:
        if not s.Characteristics & 0x20000000:
            continue
        code = s.get_data()
        offset = code.find(sig)
        while offset >= 0:
            rva = s.VirtualAddress + offset
            if rva in starts:
                hits.append(hex(rva))
            offset = code.find(sig, offset + 1)
    old = {'butcher': 0x2e733b0, 'hatch': 0x3026700}.get(name)
    if old is None:
        old = int(name, 16)
    ranked = sorted(hits, key=lambda x: abs(int(x, 16)-old))
    print(name, 'TOTAL_MATCHES', len(hits), 'NEAREST_CANDIDATES_NOT_VERIFIED', ','.join(ranked[:8]))
