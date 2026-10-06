"""Apply once in the live user-adjusted scene, after a recoverable snapshot."""
import bpy
from pathlib import Path

scene = bpy.context.scene
assert scene.camera.data.type == 'ORTHO'
assert not scene.get('longland_large_labels_v041', False), 'Already enlarged'
for obj in scene.objects:
    if obj.type != 'FONT':
        continue
    if obj.data.users > 1:
        obj.data = obj.data.copy()
    obj['label_size_before_v041'] = obj.data.size
    if obj.name.endswith(' label'):
        factor = 2.3
        # Keep the enlarged letters below the control's washer.
        obj.location.y -= 0.006
    elif obj.name.endswith(' header'):
        factor = 2.0
    elif obj.name == 'LS_Brand':
        factor = 1.7
    elif obj.name == 'LS_Descriptor':
        factor = 2.0
    elif obj.name == 'LS_VU units':
        obj.data.body = '-20  -10   0  +3'
        factor = 2.5
        obj.location.y = 0.0812
    elif obj.name == 'LS_VU':
        factor = 2.3333333333
        obj.location.y = 0.0528
    elif obj.name.endswith('front legend'):
        factor = 3.0
        obj.location.y -= 0.006
        if obj.name == 'LS_EXPRESSION front legend':
            obj.location.y += 0.005
            obj.data.body = 'EXP'
        obj.data.materials.clear()
        obj.data.materials.append(bpy.data.materials['LS | Faded umber silkscreen'])
    elif obj.name == 'LS_Serial legend':
        factor = 1.7
    else:
        factor = 2.5
        if obj.name in ('LS_Power legend', 'LS_Ready legend'):
            obj.data.materials.clear()
            obj.data.materials.append(bpy.data.materials['LS | Faded umber silkscreen'])
    obj.data.size *= factor
scene['longland_large_labels_v041'] = True
bpy.context.view_layer.update()
