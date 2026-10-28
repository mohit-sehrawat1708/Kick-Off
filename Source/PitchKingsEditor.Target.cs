using UnrealBuildTool;

public class PitchKingsEditorTarget : TargetRules
{
	public PitchKingsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PitchKings");
	}
}
