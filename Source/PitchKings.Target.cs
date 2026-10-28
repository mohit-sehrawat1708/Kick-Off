using UnrealBuildTool;

public class PitchKingsTarget : TargetRules
{
	public PitchKingsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PitchKings");
	}
}
