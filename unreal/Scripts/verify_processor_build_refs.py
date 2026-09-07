"""Read back the saved Blueprint and resolve preview references on an instance."""
import unreal

cls = unreal.EditorAssetLibrary.load_blueprint_class(
    "/Game/PalResourceFactory/Processor/BP_PRF_EggProcessor")
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector())
try:
    for field, expected, kind in (
        ("overlap_check_collision_ref", "PRF_BuildBounds", unreal.BoxComponent),
        ("main_mesh_ref", "PRF_Body", unreal.StaticMeshComponent),
    ):
        ref = actor.get_editor_property(field)
        prop = str(ref.get_editor_property("component_property"))
        component = actor.get_editor_property(prop)
        if prop != expected or not isinstance(component, kind):
            raise RuntimeError("Unresolved build reference: " + field)
        unreal.log("[PRF Verify] %s -> %s (%s)" % (
            field, component.get_name(), component.get_class().get_name()))
    projection = actor.get_editor_property("PRF_Projection")
    elements = projection.get_editor_property("elements")
    if not isinstance(projection, unreal.MaterialBillboardComponent) or len(elements) != 1:
        raise RuntimeError("Missing projection billboard")
    if elements[0].get_editor_property("material").get_name() != "M_PRF_Projection":
        raise RuntimeError("Wrong projection material")
    if (elements[0].get_editor_property("base_size_x"),
            elements[0].get_editor_property("base_size_y")) != (60.0, 120.0):
        raise RuntimeError("Projection must be twice as wide as tall (UE billboard axes)")
    unreal.log("[PRF Verify] Projection billboard and material resolved on saved instance")
    unreal.log("[PRF Verify] PASS saved Blueprint references; not game verification")
finally:
    unreal.EditorLevelLibrary.destroy_actor(actor)
