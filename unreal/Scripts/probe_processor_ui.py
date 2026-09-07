"""Read-only feasibility probe; never saves or replaces game UI assets."""
import unreal

path = "/Game/PRFUIProbe/WBP_IngameMenu_Chest"
unreal.log("PRF_UI_PROBE_BEGIN")
for name in ("WidgetBlueprintFactory", "WidgetBlueprint", "load_package", "load_class"):
    unreal.log("PRF_UI_API {}={}".format(name, hasattr(unreal, name)))
try:
    # Session-only diagnostic flags documented in the local engine source.
    # No project settings or game files are changed, and loaded assets are never saved.
    if "-PRFAllowCookedUIProbe" in unreal.SystemLibrary.get_command_line():
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        for setting in ("s.AllowUnversionedContentInEditor 1", "cook.AllowCookedDataInEditorBuilds 1"):
            unreal.SystemLibrary.execute_console_command(world, setting)
    package = unreal.load_package(path)
    unreal.log("PRF_UI_PACKAGE {}".format(package))
    if package:
        unreal.log("PRF_UI_PACKAGE_PATH {}".format(package.get_path_name()))
except Exception as error:
    unreal.log_warning("PRF_UI_PACKAGE_FAILED {}".format(error))
unreal.log("PRF_UI_PROBE_END")
