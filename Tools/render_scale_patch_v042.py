"""Two static marking-reference renders; never re-render the control sweep."""
import bpy, math, json, hashlib
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

source=Path(bpy.data.filepath)
root=source.parent.parent
target=source.with_name('Longland-Schematic-Corrected-Scales-v007.blend')
out=root/'Assets'/'static-markings-v042'
assert not target.exists() and not out.exists(), 'Use fresh output paths'
out.mkdir(parents=True)
scene=bpy.context.scene
marks=sorted([o for o in scene.objects if ' scale ' in o.name],key=lambda o:o.name)
assert len(marks)==297
old={o.name:[p.co.copy() for p in o.data.splines[0].points] for o in marks}
new={}
for obj in marks:
    stem,index=obj.name.rsplit(' scale ',1)
    control=bpy.data.objects['LS_Control_'+stem.removeprefix('LS_')]
    assert obj.parent==control.parent
    angle=math.radians(225-27*int(index))
    points=[]
    for point in old[obj.name]:
        radius=math.hypot(point.x-control.location.x,point.y-control.location.y)
        points.append(Vector((control.location.x+radius*math.cos(angle),control.location.y+radius*math.sin(angle),point.z,point.w)))
    new[obj.name]=points

def pose(values):
    for obj in marks:
        if obj.data.users>1:obj.data=obj.data.copy()
        for point,co in zip(obj.data.splines[0].points,values[obj.name]):point.co=co
    bpy.context.view_layer.update()

pose(new)
scene['knob_scale_sweep_degrees']='225 to -45, clockwise, gap at bottom'
bpy.ops.wm.save_as_mainfile(filepath=str(target),copy=True,compress=True)
scene.camera=bpy.data.objects['LS_Camera_Orthographic_UI']
assert scene.camera.data.type=='ORTHO'
scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.use_persistent_data=True
scene.render.resolution_x=1600;scene.render.resolution_y=960;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.image_settings.color_depth='8';scene.render.image_settings.compression=30
prefs=bpy.context.preferences.addons['cycles'].preferences
prefs.compute_device_type='CUDA';prefs.get_devices()
for device in prefs.devices:device.use=device.type=='CUDA'
scene.cycles.device='GPU'
for control in [o for o in scene.objects if o.name.startswith('LS_Control_')]:
    original=control.rotation_euler.z
    washer=next(c for c in control.children if c.name.endswith('washer'))
    control.rotation_euler.z=math.radians(135)
    washer.rotation_euler.z+=original-control.rotation_euler.z
for obj in scene.objects:
    if obj.name.startswith(('LS_White key','LS_Black key')):obj.visible_camera=False
needle=bpy.data.objects['LS_VU needle'].data.splines[0]
pivot=Vector(needle.points[0].co[:3]);length=(Vector(needle.points[-1].co[:3])-pivot).length
angle=math.radians(55)
needle.points[-1].co=(*(pivot+Vector((-math.sin(angle)*length,math.cos(angle)*length,0))),1)

def rect(obj):
    points=[world_to_camera_view(scene,scene.camera,obj.matrix_world@Vector(v)) for v in obj.bound_box]
    return [max(0,math.floor(min(p.x for p in points)*1600-3)),max(0,math.floor((1-max(p.y for p in points))*960-3)),
            min(1600,math.ceil(max(p.x for p in points)*1600+3)),min(960,math.ceil((1-min(p.y for p in points))*960+3))]

rects=[]
for name,values in [('before',old),('after',new)]:
    pose(values)
    rects.extend(rect(obj) for obj in marks)
    scene.render.filepath=str(out/(name+'.png'))
    bpy.ops.render.render(write_still=True)
    print('STATIC_SCALE_RENDER',name,flush=True)
metadata={'source':str(target),'sourceFingerprint':hashlib.sha256(target.read_bytes()).hexdigest()+'-static-scale-patch-v1',
          'animationSource':str(source),'animationSourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),
          'markCount':len(marks),'rects':rects,'referenceRenders':2}
(out/'patch.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print('STATIC_SCALE_PATCH_READY',str(out),flush=True)
