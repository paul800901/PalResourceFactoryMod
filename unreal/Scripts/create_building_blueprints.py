import unreal


BUILD_OBJECT_ROOT = "/Game/Pal/Blueprint/MapObject/BuildObject"
CUSTOM_ROOT = "/Game/PalResourceFactory/Buildings"


def ensure_blueprint(asset_name: str, package_path: str, parent_class: unreal.Class) -> unreal.Blueprint:
    asset_path = f"{package_path}/{asset_name}"
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        return existing

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        package_path,
        unreal.Blueprint,
        factory,
    )
    if not asset:
        raise RuntimeError(f"Failed to create Blueprint: {asset_path}")
    return asset


def load_blueprint_class(asset_path: str) -> unreal.Class:
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if not generated_class:
        raise RuntimeError(f"Failed to load Blueprint generated class: {asset_path}")
    return generated_class


def main() -> None:
    base_build_object_class = unreal.load_class(None, "/Script/Pal.PalBuildObject")
    if not base_build_object_class:
        raise RuntimeError("PalBuildObject native class is unavailable")

    # The official dummy pack predates the breeding incubator.  This local-only
    # dummy gives the child Blueprint the correct runtime parent import path.
    # It is never included in the released pak.
    breeder_dummy = ensure_blueprint(
        "BP_BuildObject_MultiElectricHatchingPalEggWithBreed",
        BUILD_OBJECT_ROOT,
        base_build_object_class,
    )
    unreal.EditorAssetLibrary.save_loaded_asset(breeder_dummy, only_if_is_dirty=False)

    processor_parent = load_blueprint_class(
        f"{BUILD_OBJECT_ROOT}/BP_BuildObject_MultiElectricHatchingPalEgg"
    )
    breeder_parent = load_blueprint_class(
        f"{BUILD_OBJECT_ROOT}/BP_BuildObject_MultiElectricHatchingPalEggWithBreed"
    )

    processor = ensure_blueprint(
        "BP_PRF_EggResourceProcessor",
        CUSTOM_ROOT,
        processor_parent,
    )
    breeder = ensure_blueprint(
        "BP_PRF_ResourceBreedingFacility",
        CUSTOM_ROOT,
        breeder_parent,
    )

    blueprint_editor = getattr(unreal, "BlueprintEditorLibrary", None)
    for asset in (processor, breeder):
        if blueprint_editor:
            blueprint_editor.compile_blueprint(asset)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

    unreal.EditorAssetLibrary.save_directory(CUSTOM_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log("[PalResourceFactory] Created independent building Blueprint assets")


main()
