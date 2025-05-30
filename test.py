import struct
import os

fname = 'test3-2.bin'

# 파일이 제대로 열리는지 확인
if not os.path.isfile(fname):
    print(f"Error: '{fname}' not found")
    exit(1)

with open(fname, 'rb') as f:
    data = f.read(8)
    print(f"Read {len(data)} bytes: {' '.join(f'{b:02X}' for b in data)}")
    if len(data) >= 8:
        pid, ref_len = struct.unpack('<ii', data)
        print(f"PID = {pid}, ref_len = {ref_len}")
    else:
        print("File too short for header")
