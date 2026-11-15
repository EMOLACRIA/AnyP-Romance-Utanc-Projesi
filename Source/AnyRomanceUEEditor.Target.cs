using UnrealBuildTool;

public class AnyRomanceUEEditorTarget : TargetRules
{
    public AnyRomanceUEEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("AnyRomanceUE");
    }
}
