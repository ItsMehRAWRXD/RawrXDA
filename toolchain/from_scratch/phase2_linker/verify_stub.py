#!/usr/bin/env python3
"""
Verify stub bytes in a linked PE.
With FILE_ALIGN=0x400, .text starts at file offset 0x400. Stub is 24 bytes.
Usage: python verify_stub.py test.exe
"""
import sys

STUB_SIZE = 24
REL32_MAIN_OFFSET = 5
DISP32_IAT_OFFSET = 13
CALL_IAT_NEXT_RIP = 17  # 0x11
CALL_MAIN_NEXT_RIP = 9  # 0x09
TEXT_RVA = 0x1000

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "test.exe"
    # FILE_ALIGN 0x400: headers aligned to 0x400, .text at file offset 0x400
    text_file_offset = 0x400
    with open(path, "rb") as f:
        f.seek(text_file_offset)
        data = f.read(STUB_SIZE)
    print("Stub bytes (first {} at file offset 0x{:X}):".format(STUB_SIZE, text_file_offset))
    print("  " + " ".join("{:02X}".format(b) for b in data))

    # IAT disp32: next RIP = stub_base + 17
    next_rip = TEXT_RVA + CALL_IAT_NEXT_RIP
    disp = int.from_bytes(data[DISP32_IAT_OFFSET:DISP32_IAT_OFFSET+4], "little", signed=True)
    target_rva = (next_rip + disp) & 0xFFFFFFFF
    print("disp32 at 0x{:02X}: 0x{:08X} ({})".format(DISP32_IAT_OFFSET, disp & 0xFFFFFFFF, disp))
    print("  Next RIP 0x{:04X} + disp32 => Target RVA: 0x{:04X} (expected IAT, e.g. 0x2038)".format(next_rip, target_rva))

    rel32 = int.from_bytes(data[REL32_MAIN_OFFSET:REL32_MAIN_OFFSET+4], "little", signed=True)
    main_rva = (TEXT_RVA + CALL_MAIN_NEXT_RIP + rel32) & 0xFFFFFFFF
    print("rel32 at 0x{:02X}: 0x{:08X} => main at 0x{:04X} + rel32 = 0x{:04X}".format(
        REL32_MAIN_OFFSET, rel32 & 0xFFFFFFFF, TEXT_RVA + CALL_MAIN_NEXT_RIP, main_rva))

    # main must be after stub (RVA >= TEXT_RVA + STUB_SIZE = 0x1018)
    main_ok = main_rva >= TEXT_RVA + STUB_SIZE
    if not main_ok:
        print("[FAIL] main RVA 0x{:04X} is inside stub (stub ends at 0x{:04X}); use 24-byte stub.".format(
            main_rva, TEXT_RVA + STUB_SIZE))
        return 1
    print("[OK] main is after stub at 0x{:04X}.".format(main_rva))
    return 0

if __name__ == "__main__":
    sys.exit(main())
