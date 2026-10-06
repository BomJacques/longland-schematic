"""Run in a background Blender against the user-adjusted COPY, never the live file."""
import bpy, math, os, json, time, traceback, sys, argparse, hashlib
from pathlib import Path
from mathutils import Vector, Matrix
from bpy_extras.object_utils import world_to_camera_view

parser=argparse.ArgumentParser()
parser.add_argument('--output-root',default=str(Path(bpy.data.filepath).parent.parent))
parser.add_argument('--render-folder',default='render-source')
options=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
ROOT=Path(options.output_root)
OUT=ROOT/'Assets'/options.render_folder
OUT.mkdir(parents=True,exist_ok=True)
fingerprint=hashlib.sha256(Path(bpy.data.filepath).read_bytes()).hexdigest()+'-ui-export-v2-1600x960-16spp'
if (OUT/'manifest.json').exists() and any(OUT.glob('controls_*.png')):
    previous=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
    if previous.get('sourceFingerprint')!=fingerprint:
        raise RuntimeError('Source changed or frames lack a signature. Choose a fresh --render-folder; do not mix old frames with a new model.')
scene=bpy.context.scene
scene.camera=bpy.data.objects['LS_Camera_Orthographic_UI']
scene.render.engine='CYCLES'
scene.cycles.samples=16
scene.cycles.use_denoising=True
scene.render.use_persistent_data=True
scene.render.resolution_x=1600;scene.render.resolution_y=960;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.image_settings.color_depth='8';scene.render.image_settings.compression=30
prefs=bpy.context.preferences.addons['cycles'].preferences
prefs.compute_device_type='CUDA';prefs.get_devices()
for dev in prefs.devices:dev.use=dev.type=='CUDA'
scene.cycles.device='GPU'
WIDTH,HEIGHT=1600,960
FRAMES=49
KEYFRAMES=5
controls=sorted([o for o in scene.objects if o.name.startswith('LS_Control_')],key=lambda o:o.name)
keys=sorted([o for o in scene.objects if o.name.startswith(('LS_White key','LS_Black key'))],key=lambda o:o.name)
surfaces=[o for o in scene.objects if o.type in ['MESH','CURVE','FONT','SURFACE','META']]
rest={o.name:o.matrix_world.copy() for o in keys}
camera_visibility={o.name:o.visible_camera for o in surfaces}
needle=bpy.data.objects['LS_VU needle']
needle_rest=[p.co.copy() for p in needle.data.splines[0].points]
pivot=Vector(needle_rest[0][:3]);length=(Vector(needle_rest[-1][:3])-pivot).length
washers={o.name:(o.rotation_euler.z, next(c for c in o.children if c.name.endswith('washer')).rotation_euler.z) for o in controls}

MAP={
 'SOURCE_WAVE':'waveform','SOURCE_REGISTER':'register','SOURCE_PULSE':'pulse','SOURCE_BALANCE':'balance',
 'SOURCE_TONE':'tone','SOURCE_KEY TRACK':'keyTrack','MEMORY_DRIFT':'drift','MEMORY_AGE':'age',
 'MEMORY_SETTLE':'settle','MEMORY_WARMTH':'warmth','MEMORY_VOICE':'voice','MEMORY_SUPPLY':'supply',
 'CIRCUIT_CUTOFF':'cutoff','CIRCUIT_RESONANCE':'resonance','CIRCUIT_BODY':'body','CIRCUIT_BIAS':'bias',
 'CIRCUIT_NOISE':'noise','CIRCUIT_NOISE TYPE':'noiseType','CONTOUR_ATTACK':'attack','CONTOUR_DECAY':'decay',
 'CONTOUR_SUSTAIN':'sustain','CONTOUR_RELEASE':'release','CONTOUR_ENSEMBLE':'ensemble','CONTOUR_WIDTH':'width',
 'Master COMP':'compressor','Master DRIVE':'drive','Master VOLUME':'output',
 'Performance TUNE':'tune','Performance CHARACTER':'character','Performance EXPRESSION':'expression'}

def projected(objects,pad=0):
    points=[world_to_camera_view(scene,scene.camera,o.matrix_world@Vector(v)) for o in objects for v in o.bound_box]
    x0=max(0,math.floor(min(p.x for p in points)*WIDTH-pad));x1=min(WIDTH,math.ceil(max(p.x for p in points)*WIDTH+pad))
    y0=max(0,math.floor((1-max(p.y for p in points))*HEIGHT-pad));y1=min(HEIGHT,math.ceil((1-min(p.y for p in points))*HEIGHT+pad))
    return [x0,y0,x1-x0,y1-y0]

def union(a,b):
    x=min(a[0],b[0]);y=min(a[1],b[1]);r=max(a[0]+a[2],b[0]+b[2]);bot=max(a[1]+a[3],b[1]+b[3])
    return [x,y,r-x,bot-y]

def pose_controls(t):
    angle=math.radians(135-270*t)
    for o in controls:
        o.rotation_euler.z=angle
        original,washer_original=washers[o.name]
        washer=next(c for c in o.children if c.name.endswith('washer'))
        washer.rotation_euler.z=washer_original+original-angle
    angle=math.radians(55-110*t)
    tip=pivot+Vector((-math.sin(angle)*length,math.cos(angle)*length,0))
    needle.data.splines[0].points[-1].co=(*tip,1)
    bpy.context.view_layer.update()

def pose_keys(t,kind=None):
    for o in keys:
        original=rest[o.name]
        is_black=o.name.startswith('LS_Black')
        amount=t if kind is None or (kind=='black')==is_black else 0
        local_back=max(v[1] for v in o.bound_box)
        hinge=original@Vector((0,local_back,0))
        o.matrix_world=Matrix.Translation(hinge)@Matrix.Rotation(amount*(.055 if is_black else .045),4,'X')@Matrix.Translation(-hinge)@original
    bpy.context.view_layer.update()

knob_bounds={o.name:None for o in controls}
key_bounds={o.name:None for o in keys}
for t in [0,.25,.5,.75,1]:
    pose_controls(t);pose_keys(t)
    for o in controls:
        b=projected(list(o.children),9)
        knob_bounds[o.name]=union(knob_bounds[o.name],b) if knob_bounds[o.name] else b
    for o in keys:
        b=projected([o],0)
        key_bounds[o.name]=union(key_bounds[o.name],b) if key_bounds[o.name] else b

white_semitones=[0,2,4,5,7,9,11]
def key_note(o):
    i=int(o.name[-2:]);return 48+(i//7)*12+white_semitones[i%7]+(1 if o.name.startswith('LS_Black') else 0)
manifest={'width':WIDTH,'height':HEIGHT,'knobFrames':FRAMES,'keyFrames':KEYFRAMES,
          'source':bpy.data.filepath,'sourceFingerprint':fingerprint,'camera':scene.camera.name,
          'knobs':[{'object':o.name,'parameter':MAP[o.name.removeprefix('LS_Control_')],'rect':knob_bounds[o.name],
                    'asset':'knob_'+MAP[o.name.removeprefix('LS_Control_')]+'.png'} for o in controls],
          'keys':[{'object':o.name,'note':key_note(o),'black':o.name.startswith('LS_Black'),'rect':key_bounds[o.name],
                   'asset':'key_'+str(key_note(o))+'.png'} for o in keys],
          'vu':{'rect':projected([bpy.data.objects['LS_Meter housing']],3),'asset':'vu.png','frames':FRAMES,'minDb':-20,'maxDb':3,'referenceDbFS':-18}}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
start=time.time();done=0;total=FRAMES+2*KEYFRAMES+1

def render(name):
    global done
    path=OUT/(name+'.png')
    if not path.exists():
        scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
    done+=1
    (OUT/'progress.json').write_text(json.dumps({'done':done,'total':total,'last':name,'elapsed':round(time.time()-start,1),'status':'rendering'}),encoding='utf-8')
    print('LONGLAND_ASSET',done,total,name,flush=True)

try:
    # Opaque control patches retain the actual moving shadows and cap highlights.
    pose_keys(0)
    for frame in range(FRAMES):
        pose_controls(frame/(FRAMES-1));render('controls_%03d'%frame)
    # Cabinet plate with all keys removed only from camera rays; lighting is retained.
    pose_controls(0);pose_keys(0)
    for o in keys:o.visible_camera=False
    render('panel')
    # Transparent key layers: white keys below black keys in the editor.
    scene.render.film_transparent=True
    for kind in ['white','black']:
        for o in surfaces:o.visible_camera=False
        for o in keys:o.visible_camera=(o.name.startswith('LS_Black'))==(kind=='black')
        for frame in range(KEYFRAMES):
            pose_keys(frame/(KEYFRAMES-1),kind);render(kind+'_%03d'%frame)
    (OUT/'progress.json').write_text(json.dumps({'done':total,'total':total,'status':'complete','elapsed':round(time.time()-start,1)}),encoding='utf-8')
except Exception:
    (OUT/'progress.json').write_text(json.dumps({'done':done,'total':total,'status':'failed','error':traceback.format_exc()}),encoding='utf-8')
    raise
