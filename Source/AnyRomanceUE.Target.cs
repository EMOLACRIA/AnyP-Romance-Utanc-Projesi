using UnrealBuildTool;

public class AnyRomanceUETarget : TargetRules
{
    public AnyRomanceUETarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("AnyRomanceUE");
    }
}
