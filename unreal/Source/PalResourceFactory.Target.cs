using UnrealBuildTool;

public class PalResourceFactoryTarget : TargetRules
{
    public PalResourceFactoryTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultWarningLevel = WarningLevel.Warning;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        ExtraModuleNames.AddRange(new[] { "Pal", "PalModLoader" });
    }
}
