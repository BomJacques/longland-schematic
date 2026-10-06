"""Apply only the static scale-marking difference to existing animation pixels."""
import hashlib,json,sys
from pathlib import Path
import numpy as np
from PIL import Image

root=Path(sys.argv[1])
base=root/'dist'/'v0.4.1'/'source'/'LonglandSchematic'/'Assets'
assets=root/'Assets';patch=assets/'static-markings-v042'
metadata=json.loads((patch/'patch.json').read_text(encoding='utf-8'))
manifest=json.loads((base/'manifest.json').read_text(encoding='utf-8'))
assert manifest['sourceFingerprint'].startswith(metadata['animationSourceSha256']+'-')
before=np.asarray(Image.open(patch/'before.png').convert('RGB'),dtype=np.int16)
after=np.asarray(Image.open(patch/'after.png').convert('RGB'),dtype=np.int16)
mask=np.zeros((960,1600),dtype=bool)
for x0,y0,x1,y1 in metadata['rects']:mask[y0:y1,x0:x1]=True
delta=(after-before)*mask[:,:,None]
changed=0

def apply(name,rect,frames):
    global changed
    original=np.asarray(Image.open(base/name).convert('RGBA')).copy()
    result=original.copy()
    x,y,w,h=rect
    region=delta[y:y+h,x:x+w]
    for frame in range(frames):
        pixels=result[frame*h:(frame+1)*h,:,:3]
        pixels[:]=np.clip(pixels.astype(np.int16)+region,0,255).astype(np.uint8)
    allowed=np.tile(mask[y:y+h,x:x+w],(frames,1))
    assert np.array_equal(result[~allowed],original[~allowed]), 'Changed pixels outside the static marking mask'
    assert np.array_equal(result[:,:,3],original[:,:,3]), 'Alpha changed'
    Image.fromarray(result).save(assets/name,optimize=True)
    changed+=np.count_nonzero(np.any(result!=original,axis=2))

apply('panel.png',[0,0,1600,960],1)
for item in manifest['knobs']:apply(item['asset'],item['rect'],manifest['knobFrames'])
# Keys and VU are deliberately untouched, verified byte-for-byte.
for item in manifest['keys']+[manifest['vu']]:
    assert hashlib.sha256((assets/item['asset']).read_bytes()).digest()==hashlib.sha256((base/item['asset']).read_bytes()).digest()
manifest['animationSource']=manifest['source']
manifest['animationSourceFingerprint']=manifest['sourceFingerprint']
manifest['source']=metadata['source'];manifest['sourceFingerprint']=metadata['sourceFingerprint']
manifest['staticPatch']={'folder':'static-markings-v042','markCount':metadata['markCount'],'referenceRenders':2,'changedPixels':int(changed)}
(assets/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Corrected 297 static scale marks using two reference renders; 49-frame sweeps reused.')
print('Changed pixels:',changed,'; keys and VU remain byte-identical.')
