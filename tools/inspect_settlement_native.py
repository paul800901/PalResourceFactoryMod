"""Read-only inspection of the installed Palworld PE; never loads game code.

Uses the existing pefile/capstone packages. RVAs are specific to the inspected
executable, not offsets suitable for a released mod.
"""

import argparse
import bisect
import contextlib
import hashlib
import struct
from pathlib import Path

import capstone
import pefile


def locations(data, needle):
    start = 0
    while (found := data.find(needle, start)) >= 0:
        yield found
        start = found + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--name", action="append", default=[])
    parser.add_argument("--rva", action="append", default=[], type=lambda x: int(x, 0))
    parser.add_argument("--range", dest="ranges", action="append", default=[],
                        help="Explicit bounded RVA START:END, including split unwind continuations")
    parser.add_argument("--callers", action="append", default=[], type=lambda x: int(x, 0))
    parser.add_argument("--string-rva", action="append", default=[], type=lambda x: int(x, 0))
    parser.add_argument("--pointer-to", action="append", default=[], type=lambda x: int(x, 0))
    parser.add_argument("--limit", type=int, default=350)
    parser.add_argument("--output", type=Path, help="Write generated evidence to a NEW local file")
    args = parser.parse_args()
    if args.output:
        # Exclusive creation protects existing evidence and the input executable.
        with args.output.open('x', encoding='utf-8') as stream:
            with contextlib.redirect_stdout(stream):
                inspect(args)
    else:
        inspect(args)


def inspect(args):
    data = args.exe.read_bytes()
    pe = pefile.PE(data=data, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    print(f"EXE={args.exe} image_base={base:#x} timestamp={pe.FILE_HEADER.TimeDateStamp}")
    print(f"FILE_SIZE={len(data)} SHA256={hashlib.sha256(data).hexdigest()}")
    directory = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']]
    # Only boundaries are needed; do not parse every unwind program in the game.
    ranges = sorted((begin, end) for begin, end, _ in
                    struct.iter_unpack('<III', pe.get_data(directory.VirtualAddress, directory.Size)))
    starts = [pair[0] for pair in ranges]
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    decoder.detail = True

    for rva in args.string_rva:
        raw = pe.get_data(rva, 512)
        print(f"STRING rva={rva:#x} ascii={raw.split(bytes([0]), 1)[0]!r}")
        end = next((i for i in range(0, len(raw)-1, 2) if raw[i:i+2] == bytes(2)), len(raw))
        print(f"  utf16={ascii(raw[:end].decode('utf-16-le', errors='replace'))}")

    for target in args.pointer_to:
        for offset in locations(data, struct.pack('<Q', base + target)):
            if offset % 8 == 0:
                print(f"POINTER_TO target={target:#x} reference={pe.get_rva_from_offset(offset):#x}")

    for target in args.callers:
        for section in pe.sections:
            if not section.Characteristics & 0x20000000:
                continue
            code = section.get_data()
            for opcode in (b'\xe8', b'\xe9'):
                for position in locations(code, opcode):
                    if position + 5 > len(code):
                        continue
                    rva = section.VirtualAddress + position
                    if rva + 5 + struct.unpack_from('<i', code, position+1)[0] == target:
                        index = bisect.bisect_right(starts, rva) - 1
                        owner = starts[index] if index >= 0 and rva < ranges[index][1] else rva
                        print(f"CALLER_CANDIDATE target={target:#x} instruction={rva:#x} owner={owner:#x}")

    for name in args.name:
        for encoding, suffix in (("ascii", b'\0'), ("utf-16-le", b'\0\0')):
            for offset in locations(data, name.encode(encoding) + suffix):
                va = base + pe.get_rva_from_offset(offset)
                print(f"NAME {name} encoding={encoding} va={va:#x}")
                for reference in locations(data, struct.pack('<Q', va)):
                    # Native registration tables use adjacent name/function pointers.
                    if reference % 8:
                        continue
                    words = []
                    for delta in (-16, -8, 0, 8, 16):
                        if reference + delta < 0 or reference + delta + 8 > len(data):
                            continue
                        word = struct.unpack_from('<Q', data, reference + delta)[0]
                        words.append(f"{delta:+}: {word:#x}")
                    print(f"  POINTER_REF rva={pe.get_rva_from_offset(reference):#x} " + ', '.join(words))

    for rva in args.rva:
        index = bisect.bisect_right(starts, rva) - 1
        leaf = index < 0 or not (ranges[index][0] <= rva < ranges[index][1])
        if leaf:
            section = pe.get_section_by_rva(rva)
            if section is None or not section.Characteristics & 0x20000000:
                print(f"NOT_EXECUTABLE rva={rva:#x}")
                continue
            begin, end = rva, rva + 96
            print("LEAF_WITHOUT_UNWIND (bounded decode until return/tail jump)")
        else:
            begin, end = ranges[index]
        print(f"FUNCTION rva={begin:#x} end={end:#x} size={end-begin}")
        code = pe.get_data(begin, end-begin)
        for count, instruction in enumerate(decoder.disasm(code, base+begin)):
            if count >= args.limit:
                print("  TRUNCATED (increase --limit)")
                break
            notes = []
            for operand in instruction.operands:
                if operand.type == capstone.x86.X86_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                    reference = instruction.address + instruction.size + operand.mem.disp - base
                    notes.append(f"rip_rva={reference:#x}")
            print(f"  {instruction.address-base:08x}  {instruction.mnemonic:8} {instruction.op_str} {'; '.join(notes)}")
            if leaf and instruction.mnemonic in ('ret', 'jmp', 'int3'):
                break

    for selected in args.ranges:
        begin, end = (int(value, 0) for value in selected.split(':'))
        if not 0 < end - begin <= 65536:
            raise ValueError('Explicit range must contain 1..65536 bytes')
        section = pe.get_section_by_rva(begin)
        if section is None or not section.Characteristics & 0x20000000:
            raise ValueError('Explicit range must begin in executable code')
        print(f'EXPLICIT_RANGE rva={begin:#x} end={end:#x}; not an inferred function boundary')
        for count, instruction in enumerate(decoder.disasm(pe.get_data(begin, end-begin), base+begin)):
            if count >= args.limit:
                print('  TRUNCATED (increase --limit)')
                break
            print(f'  {instruction.address-base:08x}  {instruction.mnemonic:8} {instruction.op_str}')


if __name__ == '__main__':
    main()
