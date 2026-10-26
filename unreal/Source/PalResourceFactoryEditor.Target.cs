using UnrealBuildTool;

public class PalResourceFactoryEditorTarget : TargetRules
{
    public PalResourceFactoryEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultWarningLevel = WarningLevel.Warning;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        ExtraModuleNames.AddRange(new[] { "Pal", "PalModLoader" });
    }
}
