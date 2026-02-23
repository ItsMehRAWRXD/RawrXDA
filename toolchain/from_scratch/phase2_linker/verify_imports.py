#!/usr/bin/env python3
"""
Verify import directory layout in a PE32+ file.
Usage: python verify_imports.py test.exe
"""
import struct
import sys

def verify_imports(pe_file):
    with open(pe_file, "rb") as f:
        data = f.read()

    e_lfanew = struct.unpack("<I", data[0x3C:0x40])[0]
    print("PE offset: {:#x}".format(e_lfanew))

    num_sections = struct.unpack("<H", data[e_lfanew + 6 : e_lfanew + 8])[0]
    opt_header_size = struct.unpack("<H", data[e_lfanew + 20 : e_lfanew + 22])[0]
    print("Sections: {}, OptHeader: {}".format(num_sections, opt_header_size))

    opt_start = e_lfanew + 24
    # DataDirectory[1] (Import) is at offset 112 + 8 in optional header
    import_dir_rva = struct.unpack("<I", data[opt_start + 120 : opt_start + 124])[0]
    import_dir_size = struct.unpack("<I", data[opt_start + 124 : opt_start + 128])[0]
    print("Import Dir: RVA {:#x}, Size {}".format(import_dir_rva, import_dir_size))

    if import_dir_rva != 0x2000:
        print("ERROR: Expected IDT at 0x2000, got {:#x}".format(import_dir_rva))
        return False

    idt_offset = 0x400  # .idata file offset (after 512 header + 512 .text)

    ilt_rva, ts, fc, name_rva, iat_rva = struct.unpack(
        "<IIIII", data[idt_offset : idt_offset + 20]
    )
    print("\nIDT Entry:")
    print("  ILT RVA:  {:#x} (expected 0x2028)".format(ilt_rva))
    print("  Name RVA: {:#x} (expected 0x2058)".format(name_rva))
    print("  IAT RVA:  {:#x} (expected 0x2038)".format(iat_rva))

    ilt_file = idt_offset + 0x28
    ilt_entry = struct.unpack("<Q", data[ilt_file : ilt_file + 8])[0]
    print("\nILT[0]: {:#x} (expected 0x2048 - hint/name RVA)".format(ilt_entry))

    iat_file = idt_offset + 0x38
    iat_entry = struct.unpack("<Q", data[iat_file : iat_file + 8])[0]
    print("IAT[0]: {:#x} (pre-load, should be 0x2048 for loader to resolve)".format(iat_entry))

    hint_file = idt_offset + 0x48
    hint = struct.unpack("<H", data[hint_file : hint_file + 2])[0]
    name = data[hint_file + 2 : hint_file + 20].split(b"\0")[0]
    print("\nHint/Name: Hint={}, Name='{}'".format(hint, name.decode()))

    dll_file = idt_offset + 0x58
    dll_name = data[dll_file : dll_file + 20].split(b"\0")[0]
    print("DLL Name: '{}'".format(dll_name.decode()))

    ok = (
        ilt_rva == 0x2028
        and name_rva == 0x2058
        and iat_rva == 0x2038
        and ilt_entry == 0x2048
    )
    if ok:
        print("\n[OK] Import layout matches; IAT[0]=0x2048 lets loader overwrite with address.")
    else:
        print("\n[FAIL] RVA or ILT/IAT value mismatch.")
    return ok

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "test.exe"
    try:
        ok = verify_imports(path)
    except Exception as e:
        print("Error: {}".format(e))
        return 1
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
