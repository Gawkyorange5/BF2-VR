"""Zero only game-audio resource bytes from locally built r134 for distribution.
Preserve resource directory, Windows manifest and section address space exactly.
Usage: python tools/prepare_runtime.py path/to/local/RFX.dll payload/RFX.dll
"""
from pathlib import Path
import struct,sys
b=bytearray(Path(sys.argv[1]).read_bytes());pe=struct.unpack_from('<I',b,0x3c)[0];opt=pe+24
assert b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,opt)[0]==0x10b
n=struct.unpack_from('<H',b,pe+6)[0];s=opt+struct.unpack_from('<H',b,pe+20)[0]
def offset(rva):
 for i in range(n):
  p=s+i*40;va,size,raw=struct.unpack_from('<III',b,p+12)
  if va<=rva<va+size:return raw+rva-va
 raise ValueError('Invalid resource address')
resource=offset(struct.unpack_from('<I',b,opt+112)[0]);count=0
def visit(relative,level,resource_type=None):
 global count
 p=resource+relative;named,ids=struct.unpack_from('<HH',b,p+12)
 for i in range(named+ids):
  name,target=struct.unpack_from('<II',b,p+16+i*8);kind=name if level==0 else resource_type
  if target&0x80000000:visit(target&0x7fffffff,level+1,kind)
  elif kind==10:
   rva,size=struct.unpack_from('<II',b,resource+target);raw=offset(rva);b[raw:raw+size]=bytes(size);count+=1
visit(0,0)
assert count==822,('Unexpected r134 audio count',count)
# Remove the local linker PDB path only; keep GUID/age and all runtime code.
pos=b.find(b'RSDS')
if pos>=0:
 end=b.find(0,pos+24)
 if end>pos+24 and end-pos<2048:
  b[pos+24:end]=bytes(end-pos-24);b[pos+24:pos+31]=b'RFX.pdb'
Path(sys.argv[2]).write_bytes(b)
