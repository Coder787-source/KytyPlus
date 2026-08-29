"""
gen_astro_pkg.py — Generate a synthetic PKG mimicking Astro's Playroom metadata.
Tests whether the launcher reads sce_sys/param.json from an installed package.
Fully synthetic — zero Sony content, zero encryption, zero keys.
"""
import struct
import os
import json

# ---- PFS constants ----
PFS_MAGIC = 20130315  # 0x01332A0B
PFS_VERSION_PS4 = 1
BLOCK_SIZE = 0x10000  # 64KB

# Inode modes
INODE_MODE_DIR = 0x4000
INODE_MODE_FILE = 0x8000

# Dirent types
DIRENT_TYPE_FILE = 2
DIRENT_TYPE_DIR = 3

INODE_D32_SIZE = 0xA8  # 168 bytes

def pad_block(data):
    return data + b'\x00' * (BLOCK_SIZE - len(data))

def make_inode(number, mode, nlink, size, db0):
    """Build a D32 inode at the correct field offsets."""
    inode = bytearray(INODE_D32_SIZE)
    # 0x00: number (uint32)
    struct.pack_into('<I', inode, 0x00, number)
    # 0x04: mode (uint16)
    struct.pack_into('<H', inode, 0x04, mode)
    # 0x06: nlink (uint16)
    struct.pack_into('<H', inode, 0x06, nlink)
    # 0x08: uid (uint32)
    struct.pack_into('<I', inode, 0x08, 0)
    # 0x0C: gid (uint32)
    struct.pack_into('<I', inode, 0x0C, 0)
    # 0x10-0x17: timestamps (zeros)
    # 0x18: size (uint32) — for files, actual size; for dirs, block_size
    struct.pack_into('<I', inode, 0x18, size)
    # 0x1C: padding/flags
    # 0x20: db[0] (first direct block pointer)
    struct.pack_into('<I', inode, 0x20, db0)
    # 0x24-0x4F: db[1..11] - unused direct pointers, terminated with -1
    # (matches real PFS images; the parser also treats 0 as unused)
    for i in range(1, 12):
        struct.pack_into('<i', inode, 0x20 + i * 4, -1)
    # 0x50-0x63: ib[0..4] - unused indirect pointers, terminated with -1
    for i in range(5):
        struct.pack_into('<i', inode, 0x50 + i * 4, -1)
    return bytes(inode)

def make_dirent(inode_num, dtype, name_bytes):
    """Build a directory entry (MkPFS-verified on-disk format).

    Header: u32 inode, i32 type, i32 name_len, i32 ent_size
    Then: name_len ASCII bytes, then zero padding up to ent_size,
    where ent_size = align8(name_len + 17).
    """
    name_len = len(name_bytes)
    ent_size = name_len + 17
    rem = ent_size % 8
    if rem:
        ent_size += 8 - rem
    dirent = struct.pack('<Iiii', inode_num, dtype, name_len, ent_size)
    dirent += name_bytes
    dirent += b'\x00' * (ent_size - 16 - name_len)
    return dirent

def build_pfs():
    """
    Build a PFS with nested directories:
    
    Block 0:  Superblock
    Block 1:  Inode 1 (root dir)     -> db[0] = block 6
    Block 2:  Inode 2 (sce_sys dir)  -> db[0] = block 7
    Block 3:  Inode 3 (eboot.bin)     -> db[0] = block 8
    Block 4:  Inode 4 (param.json)   -> db[0] = block 9
    Block 5:  Inode 5 (icon0.png)    -> db[0] = block 10
    Block 6:  Root dir entries (eboot.bin -> inode 3, sce_sys -> inode 2)
    Block 7:  sce_sys dir entries (param.json -> inode 4, icon0.png -> inode 5)
    Block 8:  eboot.bin data (fake ELF)
    Block 9:  param.json data
    Block 10: icon0.png data (minimal PNG)
    """
    
    # ---- Content ----
    
    # Fake eboot.bin (minimal PS5 ELF header)
    eboot_data = bytearray(136)
    eboot_data[0:4] = b'\x7fELF'           # ELF magic
    eboot_data[4] = 2                       # ELFCLASS64
    eboot_data[5] = 1                       # ELFDATA2LSB
    eboot_data[6] = 1                       # EV_CURRENT
    eboot_data[7] = 9                       # ELFOSABI_FREEBSD (PS5)
    struct.pack_into('<H', eboot_data, 16, 2)  # e_type = ET_EXEC
    struct.pack_into('<H', eboot_data, 18, 0x3E)  # e_machine = EM_X86_64
    struct.pack_into('<Q', eboot_data, 24, 0x400000)  # e_entry
    struct.pack_into('<H', eboot_data, 52, 64)  # e_phoff
    struct.pack_into('<H', eboot_data, 54, 0)  # e_phentsize
    struct.pack_into('<H', eboot_data, 56, 0)  # e_phnum
    # Fill rest with HLT (0xF4) to simulate code
    for i in range(64, 136):
        eboot_data[i] = 0xF4
    eboot_data = bytes(eboot_data)
    
    # param.json (Astro's Playroom-style metadata — synthetic, public title ID)
    param_json = {
        "title": "Astro's Playroom",
        "title_id": "PPSA01325",
        "title_subname": "ASTRO BOT",
        "version": "01.000.000",
        "app_type": 0,
        "app_version": "01.000.000",
        "category": "gd",
        "content_id": "UP1477-PPSA01325_00-ASTROPLAYROOM00",
        "parental_level": 1,
        "kernel": "Prospero",
        "system_ver": "0x01800001"
    }
    param_data = json.dumps(param_json, indent=2).encode('utf-8')
    
    # icon0.png (minimal 1x1 transparent PNG)
    # This is a standard minimal PNG — not Sony content
    icon_data = (
        b'\x89PNG\r\n\x1a\n'  # PNG signature
        b'\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89'
        b'\x00\x00\x00\nIDATx\x9cc\x00\x01\x00\x00\x05\x00\x01\r\n-\xb4'
        b'\x00\x00\x00\x00IEND\xaeB`\x82'
    )
    
    # ---- Build blocks ----
    
    # Block 0: Superblock
    superblock = bytearray(BLOCK_SIZE)
    struct.pack_into('<Q', superblock, 0x00, PFS_VERSION_PS4)   # version
    struct.pack_into('<Q', superblock, 0x08, PFS_MAGIC)         # format (magic)
    struct.pack_into('<Q', superblock, 0x10, 0)                  # id
    struct.pack_into('<I', superblock, 0x18, 0)                  # flags
    struct.pack_into('<H', superblock, 0x1C, 0)                 # mode (unencrypted)
    struct.pack_into('<H', superblock, 0x1E, 0)                 # unknown
    struct.pack_into('<I', superblock, 0x20, BLOCK_SIZE)        # block_size
    struct.pack_into('<I', superblock, 0x24, 0)                 # nbackup
    struct.pack_into('<Q', superblock, 0x28, 11)               # nblocks (11 blocks)
    struct.pack_into('<Q', superblock, 0x30, 6)                # ndinode (6 inodes: 0-5)
    struct.pack_into('<Q', superblock, 0x38, 5)                # ndblock (5 data blocks)
    struct.pack_into('<Q', superblock, 0x40, 5)                # ndinodeblock (5 inode blocks)
    struct.pack_into('<Q', superblock, 0x48, 1)                # superroot_ino (inode 1)
    
    # Block 1: Root inode (inode 1, directory)
    root_inode = make_inode(1, INODE_MODE_DIR, 2, BLOCK_SIZE, 6)  # db[0] = block 6
    
    # Block 2: sce_sys inode (inode 2, directory)
    scesys_inode = make_inode(2, INODE_MODE_DIR, 2, BLOCK_SIZE, 7)  # db[0] = block 7
    
    # Block 3: eboot.bin inode (inode 3, file)
    eboot_inode = make_inode(3, INODE_MODE_FILE, 1, len(eboot_data), 8)  # db[0] = block 8
    
    # Block 4: param.json inode (inode 4, file)
    param_inode = make_inode(4, INODE_MODE_FILE, 1, len(param_data), 9)  # db[0] = block 9
    
    # Block 5: icon0.png inode (inode 5, file)
    icon_inode = make_inode(5, INODE_MODE_FILE, 1, len(icon_data), 10)  # db[0] = block 10
    
    # Block 6: Root directory entries
    root_dirents = b''
    root_dirents += make_dirent(3, DIRENT_TYPE_FILE, b'eboot.bin')      # eboot.bin -> inode 3
    root_dirents += make_dirent(2, DIRENT_TYPE_DIR, b'sce_sys')         # sce_sys -> inode 2 (dir)
    
    # Block 7: sce_sys directory entries
    scesys_dirents = b''
    scesys_dirents += make_dirent(4, DIRENT_TYPE_FILE, b'param.json')   # param.json -> inode 4
    scesys_dirents += make_dirent(5, DIRENT_TYPE_FILE, b'icon0.png')    # icon0.png -> inode 5
    
    # Block 8: eboot.bin data
    # Block 9: param.json data
    # Block 10: icon0.png data
    
    # ---- Assemble ----
    pfs = b''
    pfs += pad_block(bytes(superblock))      # Block 0
    pfs += pad_block(root_inode)              # Block 1
    pfs += pad_block(scesys_inode)            # Block 2
    pfs += pad_block(eboot_inode)             # Block 3
    pfs += pad_block(param_inode)             # Block 4
    pfs += pad_block(icon_inode)              # Block 5
    pfs += pad_block(root_dirents)            # Block 6
    pfs += pad_block(scesys_dirents)          # Block 7
    pfs += pad_block(eboot_data)              # Block 8
    pfs += pad_block(param_data)              # Block 9
    pfs += pad_block(icon_data)               # Block 10
    
    return pfs



# ---- PFSC (compressed PFS) builder ----
# Mirrors MkPFS encode_pfsc_payload: 0x30-byte header, offset table at 0x400,
# stored blocks (compressed when strictly smaller than the logical block size,
# raw otherwise), offset table has (block_count + 1) entries with a final
# sentinel equal to the end of the data region.

PFSC_MAGIC = 0x43534650          # "PFSC" little-endian
PFSC_LBS = 0x10000               # logical block size (64KB)
PFSC_HEADER_SIZE = 0x30
PFSC_OFFSET_ENTRY_SIZE = 8
PFSC_BLOCK_OFFSETS_OFFSET = 0x400
PFSC_INITIAL_DATA_OFFSET = 0x10000

def build_pfsc(raw, threshold_gain=0, level=9):
    """Compress raw bytes into a PFSC container (zlib per 64KB logical block)."""
    import zlib
    # Split into 64KB logical blocks (last padded with zeros)
    blocks = []
    for off in range(0, len(raw), PFSC_LBS):
        blk = raw[off:off + PFSC_LBS]
        blk = blk + b'\\x00' * (PFSC_LBS - len(blk))
        blocks.append(blk)
    if not blocks:
        blocks = [b'\\x00' * PFSC_LBS]
    block_count = len(blocks)

    # Encode each block: compress; keep compressed only if strictly smaller
    stored = []
    for blk in blocks:
        comp = zlib.compress(blk, level)
        gain_pct = (1.0 - len(comp) / PFSC_LBS) * 100.0
        if len(comp) < PFSC_LBS and gain_pct >= threshold_gain:
            stored.append(comp)
        else:
            stored.append(blk)

    # Header size: 0x10000 unless the offset table outgrows the initial
    # capacity (0x10000 - 0x400 bytes = 508 entries), then it grows in
    # logical-block steps.
    table_bytes = (block_count + 1) * PFSC_OFFSET_ENTRY_SIZE
    initial_capacity = PFSC_INITIAL_DATA_OFFSET - PFSC_BLOCK_OFFSETS_OFFSET
    extra = max(0, table_bytes - initial_capacity)
    extra_blocks = (extra + PFSC_LBS - 1) // PFSC_LBS if extra > 0 else 0
    data_offset = PFSC_INITIAL_DATA_OFFSET + extra_blocks * PFSC_LBS

    offsets = [data_offset]
    for s in stored:
        offsets.append(offsets[-1] + len(s))

    header = bytearray(data_offset)
    # struct '<iiiiqqQq': magic, unk4=0, unk8=6, lbs, lbs, off_off, data_off, logical_size
    struct.pack_into('<iiiiqqQq', header, 0,
                     PFSC_MAGIC, 0, 6, PFSC_LBS, PFSC_LBS,
                     PFSC_BLOCK_OFFSETS_OFFSET, data_offset,
                     block_count * PFSC_LBS)
    # Offset table at 0x400: (block_count + 1) x u64 LE
    for i, off in enumerate(offsets):
        struct.pack_into('<Q', header, PFSC_BLOCK_OFFSETS_OFFSET + i * 8, off)

    payload = bytes(header) + b''.join(stored)
    return payload

def build_pkg(pfs_body):
    """Wrap a PFS image in a PKG container with a real-format entry table."""
    body_offset = 0x200
    body_size = len(pfs_body)
    
    # ---- Entry table (real PKG format, matches pkgParser.cpp) ----
    # Layout: header (0x200) | entry table | name table | body (PFS image)
    # The entry table has one code-0 descriptor record (pointing at the name
    # table) plus one 32-byte record per file. File records use code
    # 0x200 + (name offset in name table); offset/size locate the data.
    entry_count = 2  # 1 descriptor + 1 file (the PFS body)
    entry_table_size = entry_count * 32
    name_table_size = len("body.pfs") + 1  # null-terminated
    
    entry_table_offset = body_offset
    name_table_offset = entry_table_offset + entry_table_size
    actual_body_offset = name_table_offset + name_table_size
    
    # Name table: null-terminated names
    name_table = b"body.pfs\x00"
    
    # Descriptor record (code 0): offset -> name table location, size -> name table size
    descriptor = struct.pack('>IIQQII',
        0,                      # code: name-table descriptor
        0,                      # unknown1
        name_table_offset,      # offset of the name table
        name_table_size,        # size of the name table
        0,                      # unknown2
        0)                      # encrypted flag
    
    # File record for the PFS body
    file_record = struct.pack('>IIQQII',
        0x200 + 0,              # code: file whose name starts at name-table offset 0
        0,                      # unknown1
        actual_body_offset,     # offset of the file data
        body_size,              # size of the file data
        0,                      # unknown2
        0)                      # encrypted flag
    
    entry_table = descriptor + file_record
    assert len(entry_table) == entry_table_size
    
    header = bytearray(0x200)
    struct.pack_into('>I', header, 0x00, 0x7F504B47)  # PKG magic
    struct.pack_into('>I', header, 0x04, 1)            # version
    struct.pack_into('>I', header, 0x0C, 1)            # file_count
    struct.pack_into('>I', header, 0x10, entry_count) # table_entries
    struct.pack_into('>I', header, 0x18, entry_table_offset)  # table_offset
    struct.pack_into('>I', header, 0x24, actual_body_offset)  # body_offset
    struct.pack_into('>I', header, 0x2C, body_size)    # body_size
    
    content_id = b"UP1477-PPSA01325_00-ASTROPLAYROOM00"
    header[0x40:0x40+len(content_id)] = content_id
    
    return bytes(header) + entry_table + name_table + pfs_body

if __name__ == "__main__":
    print("Building PFS image with nested directories (eboot.bin + sce_sys/)...")
    pfs = build_pfs()
    print(f"PFS image size: {len(pfs)} bytes ({len(pfs)/1024:.1f} KB)")
    print(f"  PFS magic at offset 0x08: {struct.unpack_from('<Q', pfs, 0x08)[0]} (expected {PFS_MAGIC})")
    print(f"  Blocks: 11, Inodes: 6, Root inode: 1")
    print(f"  Structure: /eboot.bin + /sce_sys/param.json + /sce_sys/icon0.png")
    
    print("\nBuilding PKG container...")
    pkg = build_pkg(pfs)
    print(f"PKG file size: {len(pkg)} bytes ({len(pkg)/1024:.1f} KB)")
    print(f"  PKG magic: 0x{struct.unpack_from('>I', pkg, 0x00)[0]:08X}")
    print(f"  Content ID: {pkg[0x40:0x64].decode('ascii', errors='replace').rstrip(chr(0))}")
    print(f"  Body offset: 0x{struct.unpack_from('>I', pkg, 0x24)[0]:X}")
    print(f"  Body size: {struct.unpack_from('>I', pkg, 0x2C)[0]} bytes")
    
    out_path = "mock_astro_pkg.pkg"
    with open(out_path, "wb") as f:
        f.write(pkg)
    
    print(f"\nWritten to: {out_path}")

    # Also emit a PFSC-compressed variant to exercise the compressed-PFS path
    pfsc = build_pfsc(pfs)
    pfsc_path = "mock_astro_pkg_pfsc.pkg"
    with open(pfsc_path, "wb") as f:
        f.write(build_pkg(pfsc))
    print(f"\nPFSC variant written to: {pfsc_path}")
    print(f"  PFSC magic: 0x{struct.unpack_from('<I', pfsc, 0)[0]:08X}")
    print(f"  Compressed: {len(pfsc)} bytes vs raw {len(pfs)} bytes")

    print(f"\nMetadata in sce_sys/param.json:")
    print(f'  title: "Astro\'s Playroom"')
    print(f'  title_id: "PPSA01325"')
    print(f'\nNow: launch launcher.exe -> Install Package (.pkg) -> select {out_path}')
    print(f'Expected: game appears in main window with "Astro\'s Playroom" title')