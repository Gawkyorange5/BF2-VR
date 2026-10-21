"""Create silent build inputs, never extracted game audio. Run before building src/RFX.sln."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
for name,meta in json.loads((root/'installer/audio-resource-sizes.json').read_text()).items():
    base=(root/'src/RFX').resolve();dest=(base/name).resolve()
    if not dest.is_relative_to(base) or dest.suffix!='.pcm':raise ValueError(name)
    dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.exists():raise FileExistsError('Preserve existing local audio: '+str(dest))
    dest.write_bytes(bytes(meta['bytes']))
print('Created 822 zero-filled resource inputs. Setup supplies local game audio during installation.')
