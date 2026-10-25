"""Verify static evidence against a user-supplied executable. Requires pefile.
Usage: python research/verify_windows_profile.py /path/to/amtrucks.exe
No executable content is copied to the repository.
"""
import hashlib
from pathlib import Path
import struct
import sys
import pefile

EXPECTED = 'c60e8ee02cc97b3cc6ef8c6298bb5c3c23c4ffd330b8185b646fb1b608a3c7b4'
data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() == EXPECTED, 'Wrong executable SHA-256'
pe = pefile.PE(data=data)
assert pe.FILE_HEADER.Machine == 0x8664
assert pe.OPTIONAL_HEADER.SizeOfImage == 0x3975000
image = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
signature = bytes.fromhex('48 83 C1 28 E9 F7 0C 00 00 CC CC CC CC CC CC CC')
text = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
assert image[0x1f1520:0x1f1530] == signature
assert image[text.VirtualAddress:text.VirtualAddress+text.Misc_VirtualSize].count(signature) == 1
assert struct.unpack_from('<Q', image, 0x2435ae0)[0] == base+0x1f1520
assert image[0x1584425:0x1584428] == bytes.fromhex('FF 50 08')
assert image[0x1584457:0x158445c] == bytes.fromhex('48 85 C0 74 4A')
for rva, hexbytes in {
    0x1583c38: '8B 87 38 06 00 00',
    0x1583c53: 'F3 0F 10 4F 54',
    0x1583c5b: 'F3 0F 10 57 58',
    0x1584015: 'F3 0F 10 8F 34 06 00 00',
    0x15842f8: '48 8D B7 50 06 00 00',
}.items():
    expected=bytes.fromhex(hexbytes)
    assert image[rva:rva+len(expected)] == expected, f'Field evidence mismatch at {rva:x}'
print('Verified Windows ATS 1.61.3.18 static profile. Runtime validation still required.')
