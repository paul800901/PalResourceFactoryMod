"""Read local dummy Blueprints; do not save or change source game assets."""
import unreal

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
for name in ("BP_BuildObject_ItemChest", "BP_BuildObject_MultiElectricHatchingPalEgg"):
    path = "/Game/Pal/Blueprint/MapObject/BuildObject/" + name
    bp = unreal.load_asset(path)
    unreal.log("[PRF Inspect] " + path)
    if not bp:
        raise RuntimeError("Missing local dummy: " + path)
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    for prop in ("concrete_model_class", "indicator_widget_class", "mesh_visible_data_set"):
        unreal.log("[PRF Inspect] CDO %s=%s" % (prop, cdo.get_editor_property(prop)))
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = library.get_data(handle)
        obj = library.get_object(data)
        unreal.log("[PRF Inspect] COMPONENT %s %s" % (obj.get_name(), obj.get_class().get_name()))
        for prop in ("slot_num", "auto_work_amount_by_sec", "menu_ui_widget_class", "target_types_a",
                     "target_types_b", "static_mesh", "relative_location", "relative_scale3d",
                     "operation_restrict_type", "consumption_energy_speed", "interact_type"):
            try:
                value = obj.get_editor_property(prop)
            except Exception:
                continue
            unreal.log("[PRF Inspect]   %s=%s" % (prop, value))
unreal.log("[PRF Inspect] DONE read-only")
