# check_imports.py - Run on your PE (e.g. test.exe) to verify Import Directory and IAT.
# Usage: python check_imports.py test.exe
import struct
import sys

def read32(data, off):
    return struct.unpack('<I', data[off:off+4])[0]

def read64(data, off):
    return struct.unpack('<Q', data[off:off+8])[0]

def check_imports(path):
    with open(path, 'rb') as f:
        data = f.read()

    print(f"File size: {len(data)} bytes")

    # DOS Header
    pe_offset = read32(data, 0x3C)
    print(f"\nPE offset: {pe_offset:#x}")

    # COFF
    num_sections = read32(data, pe_offset + 6) & 0xFFFF
    opt_header_size = read32(data, pe_offset + 20)

    # Optional Header
    opt_start = pe_offset + 24
    magic = read32(data, opt_start) & 0xFFFF
    print(f"Magic: {magic:#x}")

    # Data Directory[1] - Import (PE32+ optional header: first dir at 112, second at 120)
    import_dir_rva = read32(data, opt_start + 120)
    import_dir_size = read32(data, opt_start + 124)
    print(f"\nImport Directory: RVA {import_dir_rva:#x}, Size {import_dir_size}")

    # Parse section table to find .idata
    sect_table = opt_start + opt_header_size
    print(f"\nSections:")
    idata_rva = idata_raw = None
    for i in range(num_sections):
        sec_off = sect_table + i * 40
        name = data[sec_off:sec_off+8].rstrip(b'\x00').decode('ascii', errors='ignore')
        virt_size = read32(data, sec_off + 8)
        virt_addr = read32(data, sec_off + 12)
        raw_addr = read32(data, sec_off + 20)
        print(f"  {name}: RVA {virt_addr:#x}, Raw {raw_addr:#x}, Size {virt_size}")

        if name == '.idata':
            idata_rva = virt_addr
            idata_raw = raw_addr

            # Verify Import Directory points to start of .idata
            if import_dir_rva != idata_rva:
                print(f"\n  ERROR: ImportDir RVA {import_dir_rva:#x} != .idata RVA {idata_rva:#x}")
                print("  The loader won't find imports!")
            else:
                print(f"\n  OK: Import Directory RVA matches .idata RVA")

            # Parse IDT (at import_dir_rva / idata_rva)
            idt_off = idata_raw  # File offset of IDT
            ilt_rva = read32(data, idt_off)
            name_rva = read32(data, idt_off + 12)
            iat_rva = read32(data, idt_off + 16)

            print(f"\n  IDT:")
            print(f"    ILT RVA:  {ilt_rva:#x} (expected {idata_rva + 40:#x})")
            print(f"    Name RVA: {name_rva:#x} (expected {idata_rva + 88:#x})")
            print(f"    IAT RVA:  {iat_rva:#x} (expected {idata_rva + 56:#x})")

            # Check ILT[0] (64-bit)
            ilt_file_off = idata_raw + (ilt_rva - idata_rva)
            ilt_entry = read64(data, ilt_file_off)
            print(f"\n  ILT[0] at file {ilt_file_off:#x}: {ilt_entry:#016x}")

            # Check IAT[0] (64-bit) - THIS IS THE CRITICAL CHECK
            iat_file_off = idata_raw + (iat_rva - idata_rva)
            iat_entry = read64(data, iat_file_off)
            hint_name_expected = idata_rva + 72
            print(f"  IAT[0] at file {iat_file_off:#x}: {iat_entry:#016x}")
            print(f"  Expected:   {hint_name_expected:#016x} (Hint/Name RVA)")

            if iat_entry == 0:
                print("\n  *** ERROR: IAT[0] is zero! ***")
                print("  The loader won't resolve ExitProcess.")
                print("  Fix: Write hint_name_rva to IAT[0] in pe_writer.c")
            elif iat_entry != hint_name_expected:
                print(f"\n  *** ERROR: IAT[0] value wrong ***")
            else:
                print("\n  OK: IAT initialized correctly")

            # Check Hint/Name
            hint_off = idata_raw + 72
            hint = read32(data, hint_off) & 0xFFFF
            name_str = data[hint_off+2:hint_off+32].split(b'\x00')[0].decode('ascii', errors='replace')
            print(f"\n  Hint/Name: Hint={hint}, Name='{name_str}'")

    if idata_rva is None:
        print("\n  No .idata section found.")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python check_imports.py <pe_file>")
        sys.exit(1)
    check_imports(sys.argv[1])
