"""Reader for PlayStation 4 ``.pkg`` packages.

Parses the PKG entry table, derives the PFS encryption keys, and walks the
inner PFS filesystem so its files can be streamed out decrypted and
decompressed.

The crypto and container details follow shadPS4's own implementation
(``src/core/file_format/pkg.cpp`` and ``src/core/crypto/crypto.cpp``, still
present in the shadPS4Plus fork after shadPS4 dropped PKG support).

Chain of keys::

    entry 0x10 -> key1[3]  --RSA(PkgDerivedKey3)-->  dk3
    entry 0x20 -> imgkey   --AES-CBC(sha256(entry||dk3))--> --RSA(Fake)--> ekpfs
    ekpfs + seed@pfs+0x370 --HMAC-SHA256--> tweak key || data key
    PFS image             --AES-128-XTS, 0x1000 sectors--> PFSC container
    PFSC blocks           --zlib--> file data
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from typing import Dict, Iterator, List, Optional, Tuple

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

import pkg_keys

PKG_MAGIC = 0x7F434E54
PFSC_MAGIC = b"PFSC"
PKG_HEADER_SIZE = 0x1000
PKG_ENTRY_SIZE = 32

XTS_SECTOR_SIZE = 0x1000
PFSC_BLOCK_SIZE = 0x10000
INODE_STRIDE = 0xA8
INODE_PARSED_SIZE = 104

PFS_FILE = 2
PFS_DIR = 3
PFS_CURRENT_DIR = 4
PFS_PARENT_DIR = 5

# PKG entries holding encrypted NP data, decrypted in place on extraction.
NP_ENTRY_IDS = (0x400, 0x401, 0x402, 0x403)

ENTRY_ID_ENTRY_KEYS = 0x10
ENTRY_ID_IMAGE_KEY = 0x20


class PkgError(Exception):
    """Raised when a PKG cannot be parsed or decrypted."""


# --------------------------------------------------------------------------
# small crypto helpers
# --------------------------------------------------------------------------

def _rsa_raw_decrypt(ciphertext: bytes, n: int, d: int) -> bytes:
    """RSAES-PKCS1-v1_5 decryption with the well-known private keys."""
    if len(ciphertext) != 256:
        raise PkgError(f"expected a 256-byte RSA block, got {len(ciphertext)}")
    plain = pow(int.from_bytes(ciphertext, "big"), d, n).to_bytes(256, "big")
    # Strip the PKCS#1 v1.5 type-2 padding: 00 02 <random, no zero> 00 <msg>
    if plain[0] == 0x00 and plain[1] == 0x02:
        sep = plain.find(b"\x00", 2)
        if sep >= 0:
            return plain[sep + 1:]
    # Some tools feed blocks whose padding does not validate; fall back to the
    # raw result rather than failing outright.
    return plain


def _aes_cbc_decrypt(key: bytes, iv: bytes, data: bytes) -> bytes:
    dec = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor()
    return dec.update(data) + dec.finalize()


class _XtsDecryptor:
    """AES-128-XTS over fixed 0x1000-byte sectors, as used by the PFS image."""

    def __init__(self, data_key: bytes, tweak_key: bytes) -> None:
        # XTS takes key1||key2; key1 enciphers the data, key2 the tweak.
        self._alg = algorithms.AES(data_key + tweak_key)

    def decrypt(self, data: bytes, first_sector: int) -> bytes:
        if len(data) % XTS_SECTOR_SIZE:
            raise PkgError("XTS input must be a multiple of the sector size")
        view = memoryview(data)
        parts = []
        for i in range(len(data) // XTS_SECTOR_SIZE):
            tweak = (first_sector + i).to_bytes(16, "little")
            dec = Cipher(self._alg, modes.XTS(tweak)).decryptor()
            parts.append(dec.update(view[i * XTS_SECTOR_SIZE:(i + 1) * XTS_SECTOR_SIZE]))
        return b"".join(parts)


# --------------------------------------------------------------------------
# structures
# --------------------------------------------------------------------------

@dataclass
class PkgEntry:
    id: int
    filename_offset: int
    flags1: int
    flags2: int
    offset: int
    size: int
    raw: bytes  # the 32 on-disk bytes, needed as IV key material

    @property
    def name(self) -> str:
        return pkg_keys.PKG_ENTRY_NAMES.get(self.id, str(self.id))


@dataclass
class Inode:
    mode: int
    nlink: int
    flags: int
    size: int
    size_compressed: int
    blocks: int
    loc: int

    @property
    def is_dir(self) -> bool:
        return bool(self.mode & 0o40000)

    @property
    def is_file(self) -> bool:
        return bool(self.mode & 0o100000)


@dataclass
class PfsFile:
    """A file inside the PKG, either from the PFS image or the PKG entry table."""

    path: str
    size: int
    inode: Optional[int] = None      # set for PFS files
    entry: Optional[PkgEntry] = None  # set for sce_sys metadata entries


@dataclass
class PkgInfo:
    content_id: str = ""
    title_id: str = ""
    title: str = ""
    app_version: str = ""
    category: str = ""
    drm_type: int = 0
    content_type: int = 0
    content_flags: List[str] = field(default_factory=list)
    pkg_size: int = 0


CONTENT_FLAGS = (
    (0x00100000, "FIRST_PATCH"),
    (0x00200000, "PATCHGO"),
    (0x00400000, "REMASTER"),
    (0x00800000, "PS_CLOUD"),
    (0x02000000, "GD_AC"),
    (0x04000000, "NON_GAME"),
    (0x08000000, "UNKNOWN_0x8000000"),
    (0x40000000, "SUBSEQUENT_PATCH"),
    (0x41000000, "DELTA_PATCH"),
    (0x60000000, "CUMULATIVE_PATCH"),
)


def parse_sfo(data: bytes) -> Dict[str, object]:
    """Parse a ``param.sfo`` into a plain dict."""
    if len(data) < 20 or data[:4] != b"\x00PSF":
        return {}
    key_table, data_table, count = struct.unpack_from("<III", data, 8)
    result: Dict[str, object] = {}
    for i in range(count):
        base = 20 + i * 16
        if base + 16 > len(data):
            break
        key_off, fmt, length, _max_len, data_off = struct.unpack_from("<HHIII", data, base)
        key_start = key_table + key_off
        key_end = data.find(b"\x00", key_start)
        if key_end < 0:
            continue
        key = data[key_start:key_end].decode("utf-8", "replace")
        raw = data[data_table + data_off:data_table + data_off + length]
        if fmt == 0x0404:  # integer
            if len(raw) >= 4:
                result[key] = struct.unpack("<I", raw[:4])[0]
        else:  # utf-8, possibly nul-terminated
            result[key] = raw.split(b"\x00", 1)[0].decode("utf-8", "replace")
    return result


# --------------------------------------------------------------------------
# reader
# --------------------------------------------------------------------------

class PkgReader:
    """Opens a PKG and exposes its contents as a flat list of files."""

    def __init__(self, path: str) -> None:
        self._path = path
        self._file = open(path, "rb")
        try:
            self._parse_header()
            self._read_entries()
            self._derive_keys()
            self._load_pfsc()
            self._read_inodes()
            self._build_file_list()
        except Exception:
            self._file.close()
            raise

    # -- lifecycle ---------------------------------------------------------

    def close(self) -> None:
        self._file.close()

    def __enter__(self) -> "PkgReader":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    # -- header ------------------------------------------------------------

    def _parse_header(self) -> None:
        self._file.seek(0, 2)
        self._file_size = self._file.tell()
        self._file.seek(0)
        hdr = self._file.read(PKG_HEADER_SIZE)
        if len(hdr) < PKG_HEADER_SIZE:
            raise PkgError("file is too small to be a PKG")

        magic, = struct.unpack_from(">I", hdr, 0x00)
        if magic != PKG_MAGIC:
            raise PkgError(f"bad PKG magic {magic:#010x} (expected {PKG_MAGIC:#010x})")

        self.entry_count, = struct.unpack_from(">I", hdr, 0x10)
        self.entry_table_offset, = struct.unpack_from(">I", hdr, 0x18)
        self.content_offset, self.content_size = struct.unpack_from(">QQ", hdr, 0x30)
        self.content_id = hdr[0x40:0x64].split(b"\x00", 1)[0].decode("utf-8", "replace")
        self.drm_type, self.content_type, self.content_flags = struct.unpack_from(">III", hdr, 0x70)
        self.pfs_image_offset, self.pfs_image_size = struct.unpack_from(">QQ", hdr, 0x410)
        self.pkg_size, = struct.unpack_from(">Q", hdr, 0x430)
        self.pfs_signed_size, self.pfs_cache_size = struct.unpack_from(">II", hdr, 0x438)

        # The title id is the 9 characters at offset 7 of the content id.
        self.title_id = self.content_id[7:16] if len(self.content_id) >= 16 else ""

        if self.pkg_size > self._file_size:
            raise PkgError(
                f"PKG claims to be {self.pkg_size} bytes but the file is "
                f"{self._file_size}; it is probably truncated or split"
            )
        if self.content_offset + self.content_size > self.pkg_size:
            raise PkgError("content extends past the end of the PKG")

    def _read_entries(self) -> None:
        self._file.seek(self.entry_table_offset)
        blob = self._file.read(self.entry_count * PKG_ENTRY_SIZE)
        if len(blob) < self.entry_count * PKG_ENTRY_SIZE:
            raise PkgError("PKG entry table is truncated")
        self.entries: List[PkgEntry] = []
        for i in range(self.entry_count):
            raw = blob[i * PKG_ENTRY_SIZE:(i + 1) * PKG_ENTRY_SIZE]
            eid, name_off, f1, f2, off, size = struct.unpack_from(">6I", raw, 0)
            self.entries.append(PkgEntry(eid, name_off, f1, f2, off, size, raw))

    # -- key derivation ----------------------------------------------------

    def _read_at(self, offset: int, size: int) -> bytes:
        self._file.seek(offset)
        data = self._file.read(size)
        if len(data) < size:
            raise PkgError(f"unexpected end of file reading {size} bytes at {offset:#x}")
        return data

    def _derive_keys(self) -> None:
        entry_keys = next((e for e in self.entries if e.id == ENTRY_ID_ENTRY_KEYS), None)
        image_key = next((e for e in self.entries if e.id == ENTRY_ID_IMAGE_KEY), None)
        if entry_keys is None or image_key is None:
            raise PkgError(
                "PKG is missing the ENTRY_KEYS (0x10) or IMAGE_KEY (0x20) entry; "
                "it cannot be decrypted"
            )

        # ENTRY_KEYS: seed digest (32), 7 digests (32 each), 7 RSA blocks (256 each).
        blob = self._read_at(entry_keys.offset, 32 + 7 * 32 + 7 * 256)
        key1_base = 32 + 7 * 32
        key1_3 = blob[key1_base + 3 * 256:key1_base + 4 * 256]
        dk3 = _rsa_raw_decrypt(
            key1_3, pkg_keys.PkgDerivedKey3Keyset_N, pkg_keys.PkgDerivedKey3Keyset_D
        )[:32]

        # IMAGE_KEY: AES-CBC over the 256-byte blob, keyed by sha256(entry||dk3).
        imgkeydata = self._read_at(image_key.offset, 256)
        iv_key = self._iv_key_for(image_key, dk3)
        img_key = _aes_cbc_decrypt(iv_key[16:32], iv_key[0:16], imgkeydata)
        self.ekpfs = _rsa_raw_decrypt(img_key, pkg_keys.FakeKeyset_N, pkg_keys.FakeKeyset_D)[:32]
        self._dk3 = dk3

        # The PFS data/tweak keys come from HMAC-SHA256(ekpfs, u32le(1) || seed).
        seed = self._read_at(self.pfs_image_offset + 0x370, 16)
        import hmac
        import hashlib

        digest = hmac.new(self.ekpfs, struct.pack("<I", 1) + seed, hashlib.sha256).digest()
        self.tweak_key = digest[0:16]
        self.data_key = digest[16:32]
        self._xts = _XtsDecryptor(self.data_key, self.tweak_key)

    @staticmethod
    def _iv_key_for(entry: PkgEntry, dk3: bytes) -> bytes:
        import hashlib

        return hashlib.sha256(entry.raw + dk3).digest()

    # -- PFS image access --------------------------------------------------

    def _read_pfs(self, offset: int, size: int) -> bytes:
        """Read ``size`` decrypted bytes at ``offset`` within the PFS image."""
        if size <= 0:
            return b""
        first_sector = offset // XTS_SECTOR_SIZE
        lead = offset - first_sector * XTS_SECTOR_SIZE
        span = lead + size
        sectors = (span + XTS_SECTOR_SIZE - 1) // XTS_SECTOR_SIZE
        want = sectors * XTS_SECTOR_SIZE
        self._file.seek(self.pfs_image_offset + first_sector * XTS_SECTOR_SIZE)
        raw = self._file.read(want)
        if len(raw) < want:
            # A trailing partial sector at the end of the image still has to be
            # a whole sector for XTS, so pad it out.
            raw += bytes(want - len(raw))
        return self._xts.decrypt(raw, first_sector)[lead:lead + size]

    def _load_pfsc(self) -> None:
        if not self.pfs_image_size:
            raise PkgError("PKG has no PFS image")

        # The PFSC container starts at a 0x10000 boundary somewhere after the
        # PFS superblock; scan for its magic the same way shadPS4 does.
        self.pfsc_offset = -1
        limit = min(self.pfs_image_size, max(self.pfs_cache_size * 2, 64 << 20))
        for probe in range(0x20000, limit, 0x10000):
            if self._read_pfs(probe, 4) == PFSC_MAGIC:
                self.pfsc_offset = probe
                break
        if self.pfsc_offset < 0:
            raise PkgError(
                "could not locate the PFSC header; the PKG may use keys this "
                "tool does not have, or is not a fake-signed/retail PKG"
            )

        hdr = self._read_pfs(self.pfsc_offset, 48)
        (_magic, _unk4, _unk8, _block_sz, self.pfsc_block_size,
         block_offsets, _data_start, data_length) = struct.unpack("<iiiiqqQq", hdr)

        if self.pfsc_block_size <= 0:
            raise PkgError(f"invalid PFSC block size {self.pfsc_block_size}")
        self.num_blocks = data_length // self.pfsc_block_size

        # The sector map holds num_blocks+1 offsets so each block's compressed
        # length is the difference between consecutive entries.
        map_bytes = self._read_pfs(self.pfsc_offset + block_offsets, (self.num_blocks + 1) * 8)
        self.sector_map = list(struct.unpack(f"<{self.num_blocks + 1}Q", map_bytes))

    def _read_pfsc_block(self, index: int) -> bytes:
        """Return one decompressed PFSC block (always ``pfsc_block_size`` long)."""
        if index < 0 or index + 1 >= len(self.sector_map):
            raise PkgError(f"PFSC block {index} is out of range")
        start = self.sector_map[index]
        size = self.sector_map[index + 1] - start
        block_size = self.pfsc_block_size
        if size == 0:
            # Sparse block.
            return bytes(block_size)
        if size < 0 or size > block_size:
            raise PkgError(f"PFSC block {index} has an invalid size {size}")
        data = self._read_pfs(self.pfsc_offset + start, size)
        if size == block_size:
            return data
        out = zlib.decompressobj().decompress(data, block_size)
        if len(out) < block_size:
            out += bytes(block_size - len(out))
        return out

    # -- inodes and directory tree ----------------------------------------

    def _read_inodes(self) -> None:
        superblock = self._read_pfsc_block(0)
        self.dinode_count, = struct.unpack_from("<q", superblock, 0x30)
        self.superroot_ino, = struct.unpack_from("<q", superblock, 0x48)

        occupied = -(-(self.dinode_count * INODE_STRIDE) // self.pfsc_block_size)
        self.inodes: List[Inode] = []
        for block_index in range(1, occupied + 1):
            block = self._read_pfsc_block(block_index)
            stop = False
            for pos in range(0, self.pfsc_block_size - INODE_PARSED_SIZE + 1, INODE_STRIDE):
                (mode, nlink, flags, size, size_c) = struct.unpack_from("<HHIqq", block, pos)
                if mode == 0:
                    stop = True
                    break
                blocks, loc = struct.unpack_from("<II", block, pos + 96)
                self.inodes.append(Inode(mode, nlink, flags, size, size_c, blocks, loc))
            if stop:
                break

    def _read_inode_data(self, inode: Inode) -> bytes:
        """Read a (small) inode's contents fully — used for directory blocks."""
        chunks = []
        for j in range(inode.blocks):
            chunks.append(self._read_pfsc_block(inode.loc + j))
        return b"".join(chunks)[:inode.size]

    def _read_dirents(self, ino: int) -> List[Tuple[int, int, str]]:
        """Return ``(inode, type, name)`` for each entry of a directory inode."""
        if ino >= len(self.inodes):
            raise PkgError(f"directory inode {ino} is out of range")
        data = self._read_inode_data(self.inodes[ino])
        entries: List[Tuple[int, int, str]] = []
        block_size = self.pfsc_block_size
        # Dirents never straddle a block boundary, so parse block by block.
        for block_start in range(0, len(data), block_size):
            pos = block_start
            end = min(block_start + block_size, len(data))
            while pos + 16 <= end:
                child, dtype, namelen, entsize = struct.unpack_from("<iiii", data, pos)
                if entsize <= 0:
                    break
                if child != 0 and 0 < namelen <= entsize - 16:
                    name = data[pos + 16:pos + 16 + namelen].decode("utf-8", "replace")
                    entries.append((child, dtype, name))
                pos += entsize
        return entries

    def _build_file_list(self) -> None:
        """Walk the PFS tree and the PKG entry table into a flat file list."""
        self.files: List[PfsFile] = []
        seen_paths: Dict[str, int] = {}

        root_ino = self._find_game_root()
        if root_ino is None:
            raise PkgError(
                "could not find the 'uroot' directory in the PFS image; the "
                "filesystem layout was not understood and no game files would "
                "be extracted"
            )
        self._walk(root_ino, "", seen_paths, set())

        # sce_sys metadata from the PKG entry table. PFS entries win on a
        # collision, matching the order shadPS4's extractor writes them in.
        for entry in self.entries:
            path = f"sce_sys/{entry.name}"
            if path in seen_paths:
                continue
            seen_paths[path] = len(self.files)
            self.files.append(PfsFile(path=path, size=entry.size, entry=entry))

    def _find_game_root(self) -> Optional[int]:
        """Locate the ``uroot`` directory, which maps to the game's /app0."""
        superroot = self.superroot_ino
        if superroot >= len(self.inodes):
            superroot = 0
        try:
            entries = self._read_dirents(superroot)
        except (PkgError, zlib.error):
            return None
        for child, dtype, name in entries:
            if name == "uroot" and dtype == PFS_DIR:
                return child
        # Fall back to any directory that is not ., .. or the path table.
        for child, dtype, name in entries:
            if dtype == PFS_DIR and name not in (".", "..", "flat_path_table"):
                return child
        return None

    def _walk(self, ino: int, prefix: str, seen: Dict[str, int], visited: set) -> None:
        if ino in visited:
            return
        visited.add(ino)
        for child, dtype, name in self._read_dirents(ino):
            if dtype in (PFS_CURRENT_DIR, PFS_PARENT_DIR) or name in (".", ".."):
                continue
            if name == "flat_path_table":
                continue
            # Names come from untrusted package data and are later joined onto
            # host paths, so reject anything that could escape the target dir.
            if "/" in name or "\\" in name or "\x00" in name or name.strip() in ("", "."):
                continue
            path = f"{prefix}{name}"
            if dtype == PFS_DIR:
                self._walk(child, path + "/", seen, visited)
            elif dtype == PFS_FILE:
                if child >= len(self.inodes):
                    continue
                if path in seen:
                    continue
                seen[path] = len(self.files)
                self.files.append(
                    PfsFile(path=path, size=self.inodes[child].size, inode=child)
                )

    # -- extraction --------------------------------------------------------

    def read_file(self, item: PfsFile) -> Iterator[bytes]:
        """Yield the decrypted, decompressed contents of a file in chunks."""
        if item.inode is not None:
            yield from self._read_pfs_file(self.inodes[item.inode])
        elif item.entry is not None:
            yield self._read_entry(item.entry)
        else:
            raise PkgError(f"file {item.path} has no source")

    def _read_pfs_file(self, inode: Inode) -> Iterator[bytes]:
        remaining = inode.size
        for j in range(inode.blocks):
            if remaining <= 0:
                break
            block = self._read_pfsc_block(inode.loc + j)
            if len(block) > remaining:
                block = block[:remaining]
            remaining -= len(block)
            yield block
        if remaining > 0:
            # Fewer blocks than the size claims; pad so the archive entry's
            # length still matches what we declared.
            yield bytes(remaining)

    def _read_entry(self, entry: PkgEntry) -> bytes:
        if entry.id not in NP_ENTRY_IDS:
            return self._read_at(entry.offset, entry.size)

        # The stored ciphertext is padded up to the AES block size even though
        # the entry records the logical length, so read the full final block
        # rather than zero-filling it (zero-filling corrupts the last block).
        padded_size = (entry.size + 15) & ~15
        self._file.seek(entry.offset)
        data = self._file.read(padded_size)
        if len(data) < padded_size:
            data += bytes(padded_size - len(data))
        iv_key = self._iv_key_for(entry, self._dk3)
        return _aes_cbc_decrypt(iv_key[16:32], iv_key[0:16], data)[:entry.size]

    # -- summary -----------------------------------------------------------

    def info(self) -> PkgInfo:
        sfo_data = b""
        for item in self.files:
            if item.path == "sce_sys/param.sfo":
                sfo_data = b"".join(self.read_file(item))
                break
        sfo = parse_sfo(sfo_data)
        return PkgInfo(
            content_id=self.content_id,
            title_id=str(sfo.get("TITLE_ID", self.title_id)),
            title=str(sfo.get("TITLE", "")),
            app_version=str(sfo.get("APP_VER", "")),
            category=str(sfo.get("CATEGORY", "")),
            drm_type=self.drm_type,
            content_type=self.content_type,
            content_flags=[n for bit, n in CONTENT_FLAGS if self.content_flags & bit],
            pkg_size=self.pkg_size,
        )
