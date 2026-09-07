"""Finalize the generated asset libraries as regular Blender projects; round-trip FBX."""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

root=Path(__file__).resolve().parent
expected=json.loads((root/'asset_report.json').read_text(encoding='utf-8'))
results={}
for key,stem,hulls in (
    ('PRF_26_EggResourceProcessor','SM_PRF_EggResourceProcessor',4),
    ('PRF_36_ResourceBreedingFacility','SM_PRF_ResourceBreedingFacility',7),
):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/'models'/f'{stem}.fbx'))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_')]
    collision=[o for o in bpy.context.scene.objects if o.name.startswith('UCX_')]
    assert {o.name for o in meshes}=={stem,stem+'_Status'}
    assert len(collision)==hulls
    points=[o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo=[min(v[i] for v in points) for i in range(3)]
    hi=[max(v[i] for v in points) for i in range(3)]
    size=[hi[i]-lo[i] for i in range(3)]
    assert max(abs(a-b) for a,b in zip(size,expected[key]['bounds']['size_m']))<.001
    triangles=0
    for o in meshes:
        assert len(o.data.uv_layers)>0
        assert not o.data.validate(verbose=True)
        assert all(math.isfinite(c) for v in o.data.vertices for c in v.co)
        assert o.location.length<.0001
        o.data.calc_loop_triangles()
        triangles+=len(o.data.loop_triangles)
    assert triangles==expected[key]['triangles'], (triangles,expected[key]['triangles'])
    materials=sorted({m.name for o in meshes for m in o.data.materials})
    assert 'M_PRF_StatusCyan' in materials
    results[stem]={'fbx_roundtrip':'PASS','size_m':size,'triangles':triangles,
                   'collision_hulls':len(collision),'separate_status_mesh':True,
                   'uv0':True,'materials':materials}
    asset_path=str(root/'models'/f'{stem}.blend')
    # Library writer omits the UI. Put the authored collections in a regular project
    # with a useful opening view, then check that ordinary project by reopening it.
    bpy.ops.wm.open_mainfile(filepath=asset_path)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=1.0
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active
                space.shading.type='MATERIAL'
                space.region_3d.view_location=Vector((0,0,1.4))
                space.region_3d.view_distance=7.5
                direction=Vector((6,-8,5))-Vector((0,0,1.4))
                space.region_3d.view_rotation=direction.to_track_quat('Z','Y')
    bpy.ops.wm.save_as_mainfile(filepath=asset_path)
    bpy.ops.wm.open_mainfile(filepath=asset_path)
    assert len(bpy.context.scene.objects)>10
    assert any(c.name==key for c in bpy.context.scene.collection.children)
    results[stem]['standalone_blend_open']='PASS'

(root/'export_verification.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print('PRF_EXPORT_VERIFIED',json.dumps(results),flush=True)
