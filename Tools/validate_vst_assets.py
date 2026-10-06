"""Validate packed sprite dimensions, identities and source provenance."""
import hashlib
import json
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
assets = root / 'Assets'
manifest = json.loads((assets / 'manifest.json').read_text(encoding='utf-8'))
assert (manifest['width'], manifest['height']) == (1600, 960)
assert manifest['knobFrames'] == 49 and manifest['keyFrames'] == 5
assert len(manifest['knobs']) == 30 and len(manifest['keys']) == 37
assert len({item['parameter'] for item in manifest['knobs']}) == 30
assert sorted(item['note'] for item in manifest['keys']) == list(range(48, 85))
source = Path(manifest['source'])
if source.is_file():
    fingerprint = hashlib.sha256(source.read_bytes()).hexdigest()
    assert manifest['sourceFingerprint'].startswith(fingerprint + '-'), 'Source fingerprint mismatch'

items = [(item, 49) for item in manifest['knobs'] + [manifest['vu']]]
items += [(item, 5) for item in manifest['keys']]
for item, frames in items:
    name = item['asset']
    assert Path(name).name == name and name.endswith('.png')
    x, y, width, height = item['rect']
    assert min(x, y) >= 0 and min(width, height) > 0
    assert x + width <= 1600 and y + height <= 960, name
    with Image.open(assets / name) as strip:
        strip.load()
        assert strip.size == (width, height * frames), name
        hashes = {
            hashlib.sha256(strip.crop((0, i * height, width, (i + 1) * height)).tobytes()).digest()
            for i in range(frames)
        }
        assert len(hashes) == frames, f'{name}: duplicated animation frames'
        if 'note' in item:
            assert strip.mode == 'RGBA' and strip.getchannel('A').getextrema()[0] == 0, name

with Image.open(assets / 'panel.png') as panel:
    panel.load()
    assert panel.size == (1600, 960)
assert len(list(assets.glob('*.png'))) == 69
print('Validated 30 unique 49-frame knobs, 49-frame VU, 37 unique five-pose keys, and panel.')
print('Source:', manifest['source'])
print('Packed PNG bytes:', sum(path.stat().st_size for path in assets.glob('*.png')))
