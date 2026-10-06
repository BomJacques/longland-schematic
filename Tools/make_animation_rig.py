"""Create an editable animation file from the source COPY in background Blender."""
import bpy,math,argparse,sys
from pathlib import Path
from mathutils import Vector,Matrix
parser=argparse.ArgumentParser()
parser.add_argument('--output',default=str(Path(bpy.data.filepath).with_name('Longland-Schematic-UI-Animation-Rig.blend')))
options=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
path=Path(options.output).resolve()
assert path.suffix=='.blend' and path!=Path(bpy.data.filepath).resolve()
assert not path.exists(),'Animation rig exists; choose a new version instead of overwriting.'
s=bpy.context.scene
s.camera=bpy.data.objects['LS_Camera_Orthographic_UI'];s.frame_start=1;s.frame_end=49;s.render.fps=24
for o in [o for o in s.objects if o.name.startswith('LS_Control_')]:
    original=o.rotation_euler.z
    washer=next(c for c in o.children if c.name.endswith('washer'));washer_original=washer.rotation_euler.z
    for frame,angle in [(1,135),(49,-135)]:
        radians=math.radians(angle);o.rotation_euler.z=radians;o.keyframe_insert(data_path='rotation_euler',index=2,frame=frame)
        washer.rotation_euler.z=washer_original+original-radians;washer.keyframe_insert(data_path='rotation_euler',index=2,frame=frame)
needle=bpy.data.objects['LS_VU needle'];spline=needle.data.splines[0]
pivot=Vector(spline.points[0].co[:3]);tip=Vector(spline.points[-1].co[:3]);length=(tip-pivot).length
for frame in range(1,50):
    angle=math.radians(55-110*(frame-1)/48)
    point=pivot+Vector((-math.sin(angle)*length,math.cos(angle)*length,0))
    spline.points[-1].co=(*point,1);spline.points[-1].keyframe_insert(data_path='co',frame=frame)
for o in [o for o in s.objects if o.name.startswith(('LS_White key','LS_Black key'))]:
    rest=o.matrix_world.copy();hinge=rest@Vector((0,max(v[1] for v in o.bound_box),0))
    for frame,amount in [(1,0),(13,1),(25,0),(37,1),(49,0)]:
        angle=amount*(.055 if o.name.startswith('LS_Black') else .045)
        o.matrix_world=Matrix.Translation(hinge)@Matrix.Rotation(angle,4,'X')@Matrix.Translation(-hinge)@rest
        o.keyframe_insert(data_path='location',frame=frame);o.keyframe_insert(data_path='rotation_euler',frame=frame)
# New action slots in Blender 5.2 expose curves through channel bags.
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                bag=strip.channelbag(slot)
                if bag:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:key.interpolation='LINEAR'
s.frame_set(25)
notes=bpy.data.texts.new('VST animation export notes')
notes.write('Derived from the saved user-adjusted source, without modifying the live scene.\n49-frame clockwise control sweep; stationary washers; VU needle sweep.\nKeys pivot around their rear hinges. Their in-plugin five-pose animation is driven by MIDI.\nRuntime sprites are embedded in the VST3; Blender is not required to use it.\n')
bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
print('Saved animation rig:',path)
