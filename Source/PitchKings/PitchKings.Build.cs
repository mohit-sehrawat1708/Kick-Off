using UnrealBuildTool;

public class PitchKings : ModuleRules
{
	public PitchKings(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "AssetRegistry", "Slate", "SlateCore", "RenderCore", "RHI" });
	}
}
