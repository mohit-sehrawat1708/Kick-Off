#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Shared tuning values. All distances in cm, real-world football dimensions.
namespace PK
{
	// Full-size pitch: 105 m x 68 m
	constexpr float HalfLength = 5250.f;
	constexpr float HalfWidth = 3400.f;
	constexpr float GoalHalfWidth = 366.f; // 7.32 m goal
	constexpr float GoalHeight = 244.f;
	constexpr float GoalDepth = 200.f;
	constexpr float PostRadius = 6.f;
	constexpr float PenaltyDepth = 1650.f;
	constexpr float PenaltyHalfWidth = 2016.f;
	constexpr float SixDepth = 550.f;
	constexpr float SixHalfWidth = 916.f;
	constexpr float PenaltySpot = 1100.f;
	constexpr float CentreRadius = 915.f;

	// Ball: size 5 (22 cm). Drag/roll tuned for a dry, cut pitch.
	constexpr float BallRadius = 11.f;
	constexpr float Gravity = 980.f;
	constexpr float AirDrag = 0.12f;
	constexpr float RollDecel = 300.f;

	constexpr int32 PlayersPerTeam = 11;
	constexpr float MatchSeconds = 360.f;

	// Movement speeds (cm/s).
	constexpr float WalkSpeed = 300.f;
	constexpr float JogSpeed = 560.f;
	constexpr float SprintSpeed = 840.f;
	constexpr float ControlledSprintSpeed = 700.f; // FC-style controlled sprint: quicker than a jog, ball kept close
	constexpr float JockeySpeed = 330.f;
	constexpr float ShieldSpeed = 200.f;

	// Ends change at half time: Blue attacks +X in the first half and -X in the second.
	inline bool& SidesSwapped() { static bool bSwapped = false; return bSwapped; }
	// Shot speed: above ~130 km/h it rises only slowly (no swing sends a ball at 200 km/h).
	inline float CapShotSpeed(float Speed) { return Speed > 3600.f ? 3600.f + (Speed - 3600.f) * 0.3f : Speed; }
	inline float AttackDir(int32 Team) { return (Team == 0 ? 1.f : -1.f) * (SidesSwapped() ? -1.f : 1.f); }
	inline FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }

	// Project materials (built in the editor) with engine fallbacks.
	inline const TCHAR* SurfaceMaterial() { return TEXT("/Game/PitchKings/Materials/M_PK_Surface.M_PK_Surface"); }
	inline const TCHAR* GrassMaterial() { return TEXT("/Game/PitchKings/Materials/M_PK_Grass.M_PK_Grass"); }
	inline const TCHAR* CrowdMaterial() { return TEXT("/Game/PitchKings/Materials/M_PK_Crowd.M_PK_Crowd"); }
	inline const TCHAR* FallbackMaterial() { return TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"); }

	// Paints a component with M_PK_Surface (BaseColor/Roughness/Metallic/Emissive) or the engine shape material.
	inline UMaterialInstanceDynamic* Paint(UPrimitiveComponent* Comp, UMaterialInterface* Base, const FLinearColor& Color, float Roughness = 0.6f, float Emissive = 0.f)
	{
		if (!Comp || !Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0, Base);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetVectorParameterValue(TEXT("Color"), Color); // engine BasicShapeMaterial fallback
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			if (Emissive > 0.f)
			{
				MID->SetVectorParameterValue(TEXT("Emissive"), Color);
				MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Emissive);
			}
		}
		return MID;
	}

	inline FLinearColor TeamColor(int32 Team, bool bKeeper)
	{
		if (Team == 0)
		{
			return bKeeper ? FLinearColor(0.15f, 0.7f, 0.1f) : FLinearColor(0.02f, 0.09f, 0.55f);
		}
		return bKeeper ? FLinearColor(0.9f, 0.45f, 0.02f) : FLinearColor(0.6f, 0.02f, 0.02f);
	}

	inline FLinearColor ShortsColor(int32 Team, bool bKeeper)
	{
		if (bKeeper)
		{
			return FLinearColor(0.03f, 0.03f, 0.03f);
		}
		return Team == 0 ? FLinearColor(0.01f, 0.02f, 0.12f) : FLinearColor(0.03f, 0.03f, 0.03f);
	}

	inline const TCHAR* TeamName(int32 Team) { return Team == 0 ? TEXT("BLUE") : TEXT("RED"); }
}
