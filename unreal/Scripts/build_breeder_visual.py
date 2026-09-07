"""Import the existing custom breeder body; no gameplay Blueprint changes."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(unreal.Paths.project_dir()) / "Scripts"))
import build_processor as palette
palette.ROOT = "/Game/PalResourceFactory/BreederVisual"
root = palette.ROOT
definitions = {
 "M_PRF_CeramicIvory": ((.63,.69,.66),.42,.32,0),
 "M_PRF_BurntOrange": ((.56,.105,.025),.38,.32,0),
 "M_PRF_Graphite": ((.022,.044,.054),.65,.3,0),
 "M_PRF_Rubber": ((.012,.019,.022),.05,.6,0),
 "M_PRF_BrushedSteel": ((.24,.33,.35),.85,.32,0),
 "M_PRF_ChampagneMetal": ((.49,.30,.105),.8,.3,0),
 "M_PRF_StatusCyan": ((.015,.67,.88),.25,.25,2),
 "M_PRF_StatusAmber": ((1,.28,.025),.2,.25,1.5),
 "M_PRF_Lettering": ((.82,.88,.81),.1,.45,0),
}
materials = {name: palette.material(name,*args) for name,args in definitions.items()}
task = unreal.AssetImportTask()
task.filename = str(palette.PROJECT / "art/blender/models/SM_PRF_ResourceBreedingFacility.fbx")
task.destination_path = root
task.destination_name = "SM_PRF_Breeder"
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False
options.static_mesh_import_data.import_uniform_scale = 100.0
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
meshes = [unreal.EditorAssetLibrary.load_asset(p) for p in task.imported_object_paths]
meshes = [m for m in meshes if isinstance(m,unreal.StaticMesh)]
assert len(meshes)==1, task.imported_object_paths
mesh=meshes[0]
for i,entry in enumerate(mesh.get_editor_property("static_materials")):
    name=str(entry.get_editor_property("imported_material_slot_name"))
    assert name in materials, name
    mesh.set_material(i, materials[name])
count=len(mesh.get_editor_property("body_setup").get_editor_property("agg_geom").get_editor_property("convex_elems"))
assert count==7, count
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.log("[PRF BreederVisual] DONE mesh=%s collision=%s bounds=%s" % (mesh.get_path_name(),count,mesh.get_bounding_box()))
