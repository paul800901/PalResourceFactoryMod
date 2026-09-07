"""Render the authored geometry as square transparent game icons."""
from pathlib import Path
import bpy
from mathutils import Vector

root = Path(__file__).resolve().parent
for stem, icon in [('SM_PRF_EggResourceProcessor', 'PRF_EggResourceProcessor'),
                   ('SM_PRF_ResourceBreedingFacility', 'PRF_ResourceBreedingFacility')]:
    bpy.ops.wm.open_mainfile(filepath=str(root / 'models' / (stem + '.blend')))
    scene = bpy.context.scene
    for obj in scene.objects:
        if obj.name.startswith('UCX_'):
            obj.hide_render = True
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    camera = bpy.data.objects.new('IconCamera', bpy.data.cameras.new('IconCamera'))
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 4.7 if 'EggResource' in stem else 5.5
    target = Vector((0, 0, 1.4))
    camera.location = (6, -9, 5.5)
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
    for name, pos, energy, size in [('Key',(1,-5,7),1700,5), ('Fill',(-5,-1,4),1200,4), ('Rim',(2,5,6),2100,3)]:
        data = bpy.data.lights.new(name, 'AREA')
        data.energy, data.size = energy, size
        obj = bpy.data.objects.new(name, data)
        scene.collection.objects.link(obj)
        obj.location = pos
        obj.rotation_euler = (target-obj.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath = str(root / 'previews' / (icon + '.png'))
    bpy.ops.render.render(write_still=True)
