"""Author only the new egg processor assets in this project's own Content.

No original incubator/breeder Blueprint is inherited or packaged.
The native storage model owns the saved input; PRF's DLL owns processing.
"""
from pathlib import Path
import unreal

ROOT = "/Game/PalResourceFactory/Processor"
PROJECT = Path(unreal.Paths.project_dir()).resolve().parent
assets = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = unreal.SubobjectDataBlueprintFunctionLibrary


def material(name, rgb, metal=0.4, rough=0.32, emission=0.0):
    path = ROOT + "/Materials/" + name
    mat = library.load_asset(path) if library.does_asset_exist(path) else None
    edit = unreal.MaterialEditingLibrary
    if mat:
        edit.delete_all_material_expressions(mat)
    else:
        mat = assets.create_asset(name, ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    color = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property("parameter_name", "Tint")
    color.set_editor_property("default_value", unreal.LinearColor(*rgb, 1.0))
    edit.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in ((metal, unreal.MaterialProperty.MP_METALLIC), (rough, unreal.MaterialProperty.MP_ROUGHNESS)):
        scalar = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
        scalar.set_editor_property("r", value)
        edit.connect_material_property(scalar, "", prop)
    if emission:
        glow = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
        glow.set_editor_property("parameter_name", "Glow")
        # Alpha carries pulse speed; zero keeps un-driven/body lights steady.
        glow.set_editor_property("default_value", unreal.LinearColor(*(v * emission for v in rgb), 0.0))
        glow.set_editor_property("use_custom_primitive_data", True)
        glow.set_editor_property("primitive_data_index", 0)
        clock = edit.create_material_expression(mat, unreal.MaterialExpressionTime)
        def wire(source, output_name, target, input_name):
            if not edit.connect_material_expressions(source, output_name, target, input_name):
                raise RuntimeError("Material connection failed: %s.%s -> %s.%s" % (
                    source.get_class().get_name(), output_name, target.get_class().get_name(), input_name))
        phase = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply)
        wire(clock, "", phase, "A")
        wire(glow, "A", phase, "B")
        wave = edit.create_material_expression(mat, unreal.MaterialExpressionSine)
        wave.set_editor_property("period", 1.0)
        wire(phase, "", wave, "")
        amplitude = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply)
        amplitude.set_editor_property("const_b", 0.2)
        wire(wave, "", amplitude, "A")
        baseline = edit.create_material_expression(mat, unreal.MaterialExpressionAdd)
        baseline.set_editor_property("const_b", 0.8)
        wire(amplitude, "", baseline, "A")
        output = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply)
        wire(glow, "", output, "A")
        wire(baseline, "", output, "B")
        edit.connect_material_property(output, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    library.save_loaded_asset(mat)
    return mat


def main():
    definitions = {
        "M_PRF_CeramicIvory": ((.63, .69, .66), .42, .32, 0),
        "M_PRF_BurntOrange": ((.56, .105, .025), .38, .32, 0),
        "M_PRF_Graphite": ((.022, .044, .054), .65, .3, 0),
        "M_PRF_Rubber": ((.012, .019, .022), .05, .6, 0),
        "M_PRF_BrushedSteel": ((.24, .33, .35), .85, .32, 0),
        "M_PRF_ChampagneMetal": ((.49, .30, .105), .8, .3, 0),
        "M_PRF_StatusCyan": ((.015, .67, .88), .25, .25, 2),
        "M_PRF_StatusAmber": ((1, .28, .025), .2, .25, 1.5),
        "M_PRF_Lettering": ((.82, .88, .81), .1, .45, 0),
    }
    materials = {name: material(name, *args) for name, args in definitions.items()}
    material("M_PRF_StatusProcessing", (.9, .015, .008), .2, .3, 2)
    task = unreal.AssetImportTask()
    task.filename = str(PROJECT / "art/blender/models/SM_PRF_EggResourceProcessor.fbx")
    task.destination_path = ROOT + "/Meshes"
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.combine_meshes = False
    data.auto_generate_collision = False
    data.import_uniform_scale = 100.0  # Blender meter geometry -> Unreal centimeters.
    data.generate_lightmap_u_vs = True
    task.options = options
    assets.import_asset_tasks([task])
    meshes = []
    for path in task.imported_object_paths:
        mesh = library.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        for index, entry in enumerate(mesh.get_editor_property("static_materials")):
            name = str(entry.get_editor_property("imported_material_slot_name"))
            if name not in materials:
                raise RuntimeError("Unexpected processor material slot: " + name)
            mesh.set_material(index, materials[name])
        library.save_loaded_asset(mesh)
        if "Status" not in mesh.get_name():
            collision_count = len(mesh.get_editor_property("body_setup").get_editor_property("agg_geom").get_editor_property("convex_elems"))
            if collision_count != 4:
                raise RuntimeError("Expected 4 authored convex collision hulls, got %s" % collision_count)
            unreal.log("[PRF Build] COLLISION_READBACK convex=4")
        meshes.append(mesh)
        unreal.log("[PRF Build] Imported mesh " + mesh.get_path_name())
    if len(meshes) != 2:
        raise RuntimeError("Expected body and status meshes, found %s" % len(meshes))

    name = "BP_PRF_EggProcessor"
    bp_path = ROOT + "/" + name
    bp = library.load_asset(bp_path) if library.does_asset_exist(bp_path) else None
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/Pal.PalBuildObject"))
        bp = assets.create_asset(name, ROOT, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    gathered = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    actor_handle = gathered[0]
    for h in gathered:
        obj = handles.get_object(handles.get_data(h))
        unreal.log("[PRF Build] Subobject %s %s" % (obj.get_name(), obj.get_class().get_name()))
    root_handle = next((h for h in gathered if handles.get_object(handles.get_data(h)).get_name() == "PRF_Root_GEN_VARIABLE"), None)
    if root_handle is None:
        root_handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=actor_handle, new_class=unreal.SceneComponent, blueprint_context=bp))
        if str(reason):
            raise RuntimeError(str(reason))
        subsystem.rename_subobject(root_handle, unreal.Text("PRF_Root"))
        subsystem.make_new_scene_root(actor_handle, root_handle, bp)

    def component(name, cls, scene=False):
        for existing in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            obj = handles.get_object(handles.get_data(existing))
            if obj.get_name() == name or obj.get_name() == name + "_GEN_VARIABLE":
                if scene:
                    subsystem.attach_subobject(root_handle, existing)
                return obj
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root_handle if scene else actor_handle,
            new_class=cls, blueprint_context=bp))
        if str(reason):
            raise RuntimeError(str(reason))
        subsystem.rename_subobject(handle, unreal.Text(name))
        return handles.get_object(handles.get_data(handle))

    for mesh in meshes:
        status = "Status" in mesh.get_name()
        comp = component("PRF_Status" if status else "PRF_Body", unreal.StaticMeshComponent, True)
        comp.set_editor_property("static_mesh", mesh)
        # Blender front is -Y; FBX axis conversion is inspected in the exported
        # bounds. The local outlet is +X (200 cm forward, 60 cm above origin).
        # Import readback: status/front is now +Y. Rotate -90 degrees to +X.
        comp.set_editor_property("relative_rotation", unreal.Rotator(0, 0, -90))
        comp.set_collision_profile_name("NoCollision" if status else "BlockAll")

    params = component("PRF_Input54", unreal.load_class(None, "/Script/Pal.PalMapObjectItemChestParameterComponent"))
    params.set_editor_property("slot_num", 54)
    params.set_editor_property("target_types_a", [])
    params.set_editor_property("target_types_b", [unreal.PalItemTypeB.MATERIAL_PAL_EGG])
    params.set_editor_property("auto_destroy_if_empty", False)
    params.set_editor_property("corruption_multiplier", 0.0)

    # The game's build-preview path duplicates OverlapCheckCollision without
    # accepting null (2026-09-06 crash: 2dcead1 -> 50dd4e9). Body mesh collision
    # alone does not populate this native component reference.
    bounds = component("PRF_BuildBounds", unreal.BoxComponent, True)
    bounds.set_editor_property("box_extent", unreal.Vector(120, 145, 139))
    bounds.set_editor_property("relative_location", unreal.Vector(0, 6, 139))
    bounds.set_collision_profile_name("NoCollision")
    bounds.set_editor_property("generate_overlap_events", False)

    interact = component("PRF_Interact", unreal.load_class(None, "/Script/Pal.PalInteractiveObjectBoxComponent"), True)
    interact.set_editor_property("box_extent", unreal.Vector(170, 165, 150))
    interact.set_editor_property("relative_location", unreal.Vector(30, 0, 150))
    interact.set_editor_property("is_enable_trigger_interact", True)
    interact.set_editor_property("is_implemented_trigger_interact", True)
    interact.set_editor_property("is_enable_interacting_tick", False)
    interact.set_editor_property("generate_overlap_events", True)
    interact.set_collision_profile_name("OverlapAllDynamic")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(library.load_blueprint_class(ROOT + "/" + name))
    cdo.set_editor_property("concrete_model_class", unreal.load_class(None, "/Script/Pal.PalMapObjectItemStorageModel"))
    cdo.set_editor_property("replicates", True)
    bounds_ref = unreal.ComponentReference()
    bounds_ref.set_editor_property("component_property", "PRF_BuildBounds")
    cdo.set_editor_property("overlap_check_collision_ref", bounds_ref)
    mesh_ref = unreal.ComponentReference()
    mesh_ref.set_editor_property("component_property", "PRF_Body")
    cdo.set_editor_property("main_mesh_ref", mesh_ref)
    library.save_loaded_asset(bp, only_if_is_dirty=False)
    for comp_handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = handles.get_object(handles.get_data(comp_handle))
        if isinstance(obj, unreal.StaticMeshComponent) and obj.get_editor_property("static_mesh"):
            mesh = obj.get_editor_property("static_mesh")
            unreal.log("[PRF Build] MESH_READBACK %s mesh=%s rotation=%s bounds=%s" % (
                obj.get_name(), mesh.get_name(), obj.get_editor_property("relative_rotation"), mesh.get_bounding_box()))
        if obj.get_class().get_name() == "PalMapObjectItemChestParameterComponent":
            if obj.get_editor_property("slot_num") != 54 or list(obj.get_editor_property("target_types_b")) != [unreal.PalItemTypeB.MATERIAL_PAL_EGG]:
                raise RuntimeError("Processor input readback differs")
            unreal.log("[PRF Build] INPUT_READBACK slots=54 types=MaterialPalEgg")
    unreal.log("[PRF Build] DONE processor only; 54 egg-only slots; no energy/work AI components")


if __name__ == "__main__":
    main()
