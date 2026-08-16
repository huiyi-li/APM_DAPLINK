#!/usr/bin/env python3
"""
flm2bin.py - extract a flash algorithm binary + cfg from a Keil FLM file.

The offline programmer (mcu_port/backends/app_flash_dap.c) loads the
algo bin into the target RAM at ram_addr and calls the exported
functions by offset.

Usage:
    python3 flm2bin.py <chip.flm> <out_dir> [--ram-addr 0x20000000]
"""
import argparse
import os
import struct
import subprocess
import sys

SYMBOLS = ["Init", "UnInit", "EraseChip", "EraseSector", "ProgramPage"]

FLASH_DEVICE_FMT = "<H128sH7L"
# Vers u16, DevName[128], DevType u16, DevAdr u32, DevSz u32, PageSz u32,
# EraseVal u32, Reserved u32, TimeOutProg u32, TimeOutErase u32,
# then SectorSz[16] of (u16 Sectors, u16 AddrAdr)

SECTOR_FMT = "<16HH"


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("cmd failed: %s\n%s" % (" ".join(cmd), r.stderr))
    return r.stdout


def parse_flash_device(flm, sym_addr):
    """FlashDevice struct is placed in the file at segment offset of its
    virtual address. Find the LOAD segment that contains sym_addr."""
    out = run(["arm-none-eabi-readelf", "-l", flm])
    segs = []
    for line in out.splitlines():
        t = line.split()
        if len(t) >= 5 and t[0] == "LOAD":
            off, va, pa, filesz, memsz = int(t[1], 16), int(t[2], 16), int(t[3], 16), int(t[4], 16), int(t[5], 16)
            segs.append((off, va, filesz))
    for off, va, sz in segs:
        if va <= sym_addr < va + sz:
            file_pos = off + (sym_addr - va)
            with open(flm, "rb") as f:
                f.seek(file_pos)
                data = f.read(4 + 128 + 2 + 8 * 4 + 16 * 4)
            vers, name, dtype, devadr, devsz, pagesz, eraseval, resv, tp, te = struct.unpack_from(FLASH_DEVICE_FMT, data, 0)
            sectors = struct.unpack_from(SECTOR_FMT, data, 4 + 128 + 2 + 7 * 4)
            return {
                "vers": vers, "name": name.rstrip(b"\0").decode("latin1"),
                "dev_type": dtype, "dev_adr": devadr, "dev_sz": devsz,
                "page_sz": pagesz, "erase_val": eraseval,
                "timeout_prog": tp, "timeout_erase": te,
                "sectors": [(sectors[i], sectors[i + 1]) for i in range(0, len(sectors) - 1, 2) if sectors[i] != 0xFFFF],
            }
    sys.exit("FlashDevice symbol not inside any LOAD segment")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("flm")
    ap.add_argument("out_dir")
    ap.add_argument("--ram-addr", default="0x20000000")
    ap.add_argument("--name", default=None)
    ap.add_argument("--sectors", default=None,
                    help="comma list size@addr, e.g. 0x4000@0x0,... "
                         "(default: GD32F470 512KB layout)")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    base = os.path.splitext(os.path.basename(args.flm))[0]
    name = args.name or base.replace("_512KB", "").replace("GD32F4xx", "GD32F470")

    # 1. symbols (offsets from segment base, load address is 0)
    syms = {}
    for line in run(["arm-none-eabi-nm", "-n", args.flm]).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in SYMBOLS:
            syms[parts[2]] = int(parts[0], 16)
    missing = [s for s in SYMBOLS if s not in syms]
    if missing:
        sys.exit("missing symbols: %s" % missing)

    # 2. algo binary = PrgCode + PrgData
    algo_bin = os.path.join(args.out_dir, "%s_algo.bin" % name)
    run(["arm-none-eabi-objcopy", "-O", "binary",
         "--only-section=PrgCode", "--only-section=PrgData",
         args.flm, algo_bin])
    algo_size = os.path.getsize(algo_bin)

    # 3. FlashDevice descriptor
    dev = parse_flash_device(args.flm, syms.get("FlashDevice", 0x314))

    ram_addr = int(args.ram_addr, 0)

    if args.sectors:
        sec_list = args.sectors
    else:
        # GD32F470 512KB: 4x16KB + 1x64KB + 3x128KB
        sec_list = "0x4000@0x0,0x4000@0x4000,0x4000@0x8000,0x4000@0xC000," \
                   "0x10000@0x10000,0x20000@0x20000,0x20000@0x40000,0x20000@0x60000"
    sector_size = int(sec_list.split(",")[0].split("@")[0], 0)

    cfg = [
        "name=%s" % name,
        "chip=GD32F470",
        "flash_size_kb=%d" % (dev["dev_sz"] // 1024),
        "flash_base=0x%08X" % dev["dev_adr"],
        "ram_addr=0x%08X" % ram_addr,
        "algo_bin=/algo/%s_algo.bin" % name,
        "algo_size=%d" % algo_size,
        "fn_init=0x%X" % syms["Init"],
        "fn_uninit=0x%X" % syms["UnInit"],
        "fn_erase_chip=0x%X" % syms["EraseChip"],
        "fn_erase_sector=0x%X" % syms["EraseSector"],
        "fn_program_page=0x%X" % syms["ProgramPage"],
        "page_size=0x%X" % dev["page_sz"],
        "sector_size=0x%X" % sector_size,
        "sectors=%s" % sec_list,
        "is_default=1",
    ]
    cfg_path = os.path.join(args.out_dir, "%s.cfg" % name)
    with open(cfg_path, "w") as f:
        f.write("\n".join(cfg) + "\n")

    print("algo bin : %s (%d bytes)" % (algo_bin, algo_size))
    print("cfg      : %s" % cfg_path)
    for line in cfg:
        print("  " + line)


if __name__ == "__main__":
    main()
