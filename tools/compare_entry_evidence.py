"""Compare recorded disassembly, ignoring relocated code/data addresses only."""
import re
import difflib
from pathlib import Path

def body(path, rva):
    text = Path(path).read_text(encoding='utf-8')
    match = re.search(rf'FUNCTION rva={rva} end=(0x[0-9a-f]+) size=\d+\n(.*?)(?=\n\S|\Z)', text, re.S)
    if not match:
        raise ValueError((path, rva))
    start, end = int(rva, 16), int(match[1], 16)
    result = []
    for line in match[2].splitlines():
        m = re.match(r'\s+[0-9a-f]+\s+(\w+)\s*(.*?)\s*$', line)
        if not m:
            continue
        op, args = m.groups()
        args = re.sub(r' rip_rva=.*', '', args)
        args = re.sub(r'\[rip [+-] 0x[0-9a-f]+\]', '[rip DATA]', args)
        if op == 'call' or op.startswith('j'):
            def target(m):
                address = int(m[0], 16) - 0x140000000
                return f'LOCAL+{address-start:x}' if start <= address < end else 'EXTERNAL'
            args = re.sub(r'0x14[0-9a-f]+', target, args)
        result.append(op + ' ' + args)
    return result

pairs = [
 ('plan','native-plan-layout-evidence.txt','0x2fb44c0','v104-consume-plan.txt','0x2fc0ff0'),
 ('apply','native-commit-final-evidence.txt','0x2fa8ef0','v104-apply-entry.txt','0x2fb5680'),
 ('access','native-plan-validation-evidence.txt','0x2fa3460','v104-final-entry-bodies.txt','0x2fafbf0'),
 ('dispose','native-egg-cleanup-evidence.txt','0x2ea74d0','v104-cleanup-candidates.txt','0x2ed8c80'),
 ('hatch','native-evidence.txt','0x3026700','v104-final-entry-bodies.txt','0x3035620'),
 ('butcher','native-evidence.txt','0x2e733b0','v104-final-entry-bodies.txt','0x2e7a830'),
 ('ground','ground-drop-native-20260906.txt','0x2f99740','v104-final-entry-bodies.txt','0x2fa5ed0'),
 ('consume','container-operation-evidence.txt','0x2facf40','v104-final-entry-bodies.txt','0x2fb98f0'),
]
for name, old, old_rva, new, new_rva in pairs:
    a = body('build/direct-settlement-20260906/' + old, old_rva)
    b = body('build/' + new, new_rva)
    print(name, old_rva, '->', new_rva, len(a), len(b), 'MATCH' if a == b else 'DIFF')
    if a != b:
        print('\n'.join(difflib.unified_diff(a,b,fromfile='old',tofile='new')))
