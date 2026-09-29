"""Strip reload resource payloads from a locally built RFX DLL for distribution.
Preserves original resource-section address space so Setup can insert local cues.
Usage: python tools/prepare_runtime.py path/to/local/RFX.dll payload/RFX.dll
"""
from pathlib import Path
import struct,sys
b=bytearray(Path(sys.argv[1]).read_bytes());pe=struct.unpack_from('<I',b,0x3c)[0];opt=pe+24
assert b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,opt)[0]==0x10b
n=struct.unpack_from('<H',b,pe+6)[0];s=opt+struct.unpack_from('<H',b,pe+20)[0]
for i in range(n):
 p=s+i*40
 if b[p:p+8].rstrip(b'\0')==b'.rsrc':
  size,raw=struct.unpack_from('<II',b,p+16);b[raw:raw+size]=bytes(size);struct.pack_into('<I',b,opt+116,16)
# Remove the local linker PDB path only; keep GUID/age and all runtime code.
pos=b.find(b'RSDS')
if pos>=0:
 end=b.find(0,pos+24)
 if end>pos+24 and end-pos<2048:
  b[pos+24:end]=bytes(end-pos-24);b[pos+24:pos+31]=b'RFX.pdb'
Path(sys.argv[2]).write_bytes(b)
