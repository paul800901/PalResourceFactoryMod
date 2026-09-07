from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve().parent
for name in ['PRF_EggResourceProcessor', 'PRF_ResourceBreedingFacility']:
    task = unreal.AssetImportTask()
    task.filename = str(root / 'art/blender/previews' / (name + '.png'))
    task.destination_path = '/Game/PalResourceFactory/Icons'
    task.destination_name = 'T_' + name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(task.imported_object_paths[0])
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log('PRF_ICON_IMPORTED ' + texture.get_path_name())
