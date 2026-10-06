"""Lossless sprite extraction from Blender renders."""
import json, shutil, sys
from pathlib import Path
from PIL import Image
root=(Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'outputs'/'longland-schematic')/'Assets'
source=root/(sys.argv[2] if len(sys.argv)>2 else 'render-source')
manifest=json.loads((source/'manifest.json').read_text(encoding='utf-8'))
assert json.loads((source/'progress.json').read_text())['status']=='complete'
def filmstrip(items,prefix,count):
    strips={item['asset']:Image.new('RGBA',(item['rect'][2],item['rect'][3]*count)) for item in items}
    for frame in range(count):
        with Image.open(source/(prefix+'_%03d.png'%frame)) as rendered:
            for item in items:
                x,y,w,h=item['rect']
                strips[item['asset']].paste(rendered.crop((x,y,x+w,y+h)),(0,h*frame))
    for filename,strip in strips.items():strip.save(root/filename,optimize=True)
filmstrip(manifest['knobs']+[manifest['vu']],'controls',manifest['knobFrames'])
for kind in ['white','black']:
    filmstrip([k for k in manifest['keys'] if k['black']==(kind=='black')],kind,manifest['keyFrames'])
shutil.copy2(source/'panel.png',root/'panel.png')
(root/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
assert len(list(root.glob('knob_*.png')))==30
assert len(list(root.glob('key_*.png')))==37
print('Packed 30 knob filmstrips, 37 key filmstrips, VU filmstrip and panel.')
print('Total packed PNG bytes:',sum(p.stat().st_size for p in root.glob('*.png')))
