#include "PKFootballer.h"
#include "PKAnimInstance.h"
#include "PKCopyPoseAnimInstance.h"
#include "PKHeadSet.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PKTypes.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CapsuleRadius = 34.f;
	constexpr float CapsuleHalfHeight = 90.f;
	constexpr float FallbackKickDuration = 0.5f;
	constexpr float FallbackKickRelease = 0.21f;
}

APKFootballer::APKFootballer()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = PK::JogSpeed;
	Move->MaxAcceleration = 1700.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GroundFriction = 6.f;
	Move->bUseSeparateBrakingFriction = true;
	Move->BrakingFriction = 4.f;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	// The mannequin with its plate grooves and joint rings smoothed away (Tools/editor_scripts/make_player_body.py).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlayerBody(TEXT("/Game/PitchKings/Characters/SKM_PK_Body.SKM_PK_Body"));
	USkeletalMesh* BodyMesh = PlayerBody.Object ? PlayerBody.Object : MannyMesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MannyMaterial(TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/PitchKings/Materials/M_PK_Surface.M_PK_Surface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> KitBody(TEXT("/Game/PitchKings/Materials/M_PK_Kit.M_PK_Kit"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> KitShellMat(TEXT("/Game/PitchKings/Materials/M_PK_KitShell.M_PK_KitShell"));
	KitBodyMaterial = KitBody.Object;
	KitShellMaterial = KitShellMat.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMesh(TEXT("/Game/PitchKings/Stadium/SM_PK_Ring.SM_PK_Ring"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChevronMesh(TEXT("/Game/PitchKings/Stadium/SM_PK_Chevron.SM_PK_Chevron"));
	KitMaterial = MannyMaterial.Object;
	SurfaceMaterial = Surface.Object;

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(BodyMesh);
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), FRotator(0.f, -90.f, 0.f));
	Body->SetAnimInstanceClass(UPKAnimInstance::StaticClass());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	KitShell = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("KitShell"));
	KitShell->SetupAttachment(Body);
	KitShell->SetSkeletalMesh(BodyMesh);
	KitShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	KitShell->SetVisibility(KitShellMaterial != nullptr);
	KitShell->SetCastShadow(false); // the body's shadow is enough: saves a skinned shadow pass per player

	// MetaHuman head, shirt and shorts (see the header). Heads m_001..m_006 of City Sample Crowds.
	static const TCHAR* CrowdChar = TEXT("/Game/CitySampleCrowd/Character");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head1(TEXT("/Game/CitySampleCrowd/Character/Male/m_001/Face/m_001_nrw_FaceMesh.m_001_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head2(TEXT("/Game/CitySampleCrowd/Character/Male/m_002/Face/m_002_nrw_FaceMesh.m_002_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head3(TEXT("/Game/CitySampleCrowd/Character/Male/m_003/Face/m_003_nrw_FaceMesh.m_003_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head4(TEXT("/Game/CitySampleCrowd/Character/Male/m_004/Face/m_004_nrw_FaceMesh.m_004_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head5(TEXT("/Game/CitySampleCrowd/Character/Male/m_005/Face/m_005_nrw_FaceMesh.m_005_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Head6(TEXT("/Game/CitySampleCrowd/Character/Male/m_006/Face/m_006_nrw_FaceMesh.m_006_nrw_FaceMesh"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CrewNeck(TEXT("/Game/CitySampleCrowd/Character/Male/NormalWeight/Meshes/m_tal_nrw_crewneck.m_tal_nrw_crewneck"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Slacks(TEXT("/Game/CitySampleCrowd/Character/Male/NormalWeight/Meshes/m_tal_nrw_slacks.m_tal_nrw_slacks"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkinMat(TEXT("/Game/PitchKings/Materials/Player/M_PK_PlayerSkin.M_PK_PlayerSkin"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShirtMat(TEXT("/Game/PitchKings/Materials/Player/MI_PK_Shirt.MI_PK_Shirt"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShortsMat(TEXT("/Game/PitchKings/Materials/Player/MI_PK_Shorts.MI_PK_Shorts"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EyeMat(TEXT("/Game/PitchKings/Materials/Player/M_PK_PlayerEye.M_PK_PlayerEye"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeethMat(TEXT("/Game/PitchKings/Materials/Player/M_PK_PlayerTeeth.M_PK_PlayerTeeth"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HiddenMat(TEXT("/Game/PitchKings/Materials/Player/M_PK_PlayerHidden.M_PK_PlayerHidden"));
	for (USkeletalMesh* HeadMesh : { Head1.Object, Head2.Object, Head3.Object, Head4.Object, Head5.Object, Head6.Object })
	{
		if (HeadMesh)
		{
			MHHeadMeshes.Add(HeadMesh);
		}
	}
	auto LoadMaps = [](std::initializer_list<const TCHAR*> Names, TArray<TObjectPtr<UTexture>>& Out)
	{
		for (const TCHAR* Name : Names)
		{
			Out.Add(LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("%s/Shared/Textures/Hair/%s/%s.%s"), CrowdChar, Name, Name, Name)));
		}
	};
	// Hair (EPKHairCut order): the scalp map painted on the head, plus groom cards for the longer cuts.
	// (the hair-loss map is sRGB, which the skin material's linear sampler cannot take: it uses the pulled-back map)
	LoadMaps({ TEXT("Hair_S_BuzzCut_baked"), TEXT("Hair_S_AfroFade_baked"), TEXT("Hair_S_Coil_baked"), TEXT("Hair_S_PulledBack_baked"), TEXT("Hair_S_PulledBack_baked"), TEXT("Hair_S_PulledBack_baked"), TEXT("Hair_S_Coil_baked") }, MHHairMaps);
	for (const TCHAR* Style : { TEXT("AfroFade"), TEXT("CurlyFade"), TEXT("SideSweptFringe"), TEXT("PulledBack"), TEXT("HairLoss"), TEXT("Coil"), TEXT("BeardFull"), TEXT("ChinStrap") })
	{
		// LOD-chained card meshes made by Tools/editor_scripts/build_player_hair.py
		MHHairMeshes.Add(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/PitchKings/Characters/Hair/SM_PK_Hair_%s.SM_PK_Hair_%s"), Style, Style)));
	}
	LoadMaps({ TEXT("Eyebrows_M_Full_baked"), TEXT("Eyebrows_M_Dense_baked"), TEXT("Eyebrows_M_Thick_baked"), TEXT("Eyebrows_M_Natural_baked"), TEXT("Eyebrows_M_Wide_baked"), TEXT("Eyebrows_M_Unruly_baked") }, MHBrowMaps);
	LoadMaps({ TEXT("Beard_S_Stubble_baked"), TEXT("Goatee_S_ChinStrap_baked") }, MHBeardMaps);
	for (int32 i = 1; i <= 6; ++i)
	{
		// each scanned head's own normal map (sculpt detail the synthesized crowd materials would bake in)
		MHHeadNormals.Add(LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("%s/Male/m_00%d/Textures/m_00%d_nrw_LODnormals.m_00%d_nrw_LODnormals"), CrowdChar, i, i, i)));
	}
	MHSkinMaterial = SkinMat.Object;
	MHShirtMaterial = ShirtMat.Object;
	MHShortsMaterial = ShortsMat.Object;
	MHEyeMaterial = EyeMat.Object;
	MHTeethMaterial = TeethMat.Object;
	MHHiddenMaterial = HiddenMat.Object;
	bMetaHumanParts = MHHeadMeshes.Num() > 0 && CrewNeck.Object && Slacks.Object && MHSkinMaterial && MHShirtMaterial
		&& MHShortsMaterial && MHHiddenMaterial && !FParse::Param(FCommandLine::Get(), TEXT("PKMannyKit"));
	auto MakePart = [this, Body](const TCHAR* Name, USkeletalMesh* PartMesh) -> USkeletalMeshComponent*
	{
		USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Part->SetupAttachment(Body);
		Part->SetSkeletalMesh(PartMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Part->bUseAttachParentBound = true; // culled with the body: a part culled on its own stops copying the pose
		Part->SetVisibility(bMetaHumanParts);
		Part->SetAnimInstanceClass(UPKCopyPoseAnimInstance::StaticClass());
		return Part;
	};
	MHHead = MakePart(TEXT("MHHead"), MHHeadMeshes.Num() ? MHHeadMeshes[0].Get() : nullptr);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BootMeshL(TEXT("/Game/PitchKings/Characters/Boots/SM_PK_Boot_L.SM_PK_Boot_L"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BootMeshR(TEXT("/Game/PitchKings/Characters/Boots/SM_PK_Boot_R.SM_PK_Boot_R"));
	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* Boot = CreateDefaultSubobject<UStaticMeshComponent>(i == 0 ? TEXT("BootL") : TEXT("BootR"));
		Boot->SetupAttachment(Body, i == 0 ? TEXT("foot_l") : TEXT("foot_r"));
		Boot->SetStaticMesh(i == 0 ? BootMeshL.Object : BootMeshR.Object);
		Boot->SetRelativeScale3D(FVector(1.03f)); // a touch roomier: the collar covers the sock's cut edge as the ankle turns
		Boot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Boot->SetGenerateOverlapEvents(false);
		Boot->SetVisibility(bMetaHumanParts && BootMeshL.Object && BootMeshR.Object);
		(i == 0 ? BootL : BootR) = Boot;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* Groom = CreateDefaultSubobject<UStaticMeshComponent>(i == 0 ? TEXT("MHHair") : TEXT("MHBeard"));
		Groom->SetupAttachment(MHHead, TEXT("head"));
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Groom->SetGenerateOverlapEvents(false);
		Groom->SetVisibility(false);
		(i == 0 ? MHHair : MHBeard) = Groom;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* Groom = CreateDefaultSubobject<UStaticMeshComponent>(i == 0 ? TEXT("MHMustache") : TEXT("MHBrows"));
		Groom->SetupAttachment(MHHead, TEXT("head"));
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Groom->SetGenerateOverlapEvents(false);
		Groom->SetCastShadow(false); // a few thin cards: their shadow is noise
		Groom->SetVisibility(false);
		(i == 0 ? MHMustache : MHBrows) = Groom;
	}
	static ConstructorHelpers::FObjectFinder<UPKHeadSet> Heads(TEXT("/Game/PitchKings/Data/DA_PK_HeadSet.DA_PK_HeadSet"));
	if (Heads.Object && Heads.Object->Heads.Num() && !FParse::Param(FCommandLine::Get(), TEXT("PKCrowdHeads")))
	{
		HeadSet = Heads.Object;
	}
	MHHairBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MHHairBase"));
	MHHairBase->SetupAttachment(MHHead, TEXT("head"));
	MHHairBase->SetRelativeScale3D(FVector(0.985f));
	MHHairBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MHHairBase->SetGenerateOverlapEvents(false);
	MHHairBase->SetCastShadow(false);
	MHHairBase->SetVisibility(false);
	MHShirt = MakePart(TEXT("MHShirt"), CrewNeck.Object);
	MHShorts = MakePart(TEXT("MHShorts"), Slacks.Object);
	// The pack's head blueprint (facial idles + RigLogic post-process) stalls when the head is culled and costs
	// a lot per player; the plain copy pose keeps the face locked to the body.
	MHHead->SetDisablePostProcessBlueprint(true);
	if (bMetaHumanParts)
	{
		KitShell->SetVisibility(false);
	}

	// Selection ring for the human-controlled player.
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(GetCapsuleComponent());
	// A thin glowing ring on the grass (falls back to a flat disc if the stadium meshes are missing).
	Marker->SetStaticMesh(RingMesh.Object ? RingMesh.Object : CylinderMesh.Object);
	Marker->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight + 0.8f));
	Marker->SetRelativeScale3D(RingMesh.Object ? FVector(1.f) : FVector(1.2f, 1.2f, 0.01f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	Marker->SetVisibility(false);

	// Downward arrow above the head marking the next player on the ball.
	BallArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallArrow"));
	BallArrow->SetupAttachment(GetCapsuleComponent());
	// A small chevron above the head (the cone is the fallback).
	BallArrow->SetStaticMesh(ChevronMesh.Object ? ChevronMesh.Object : ConeMesh.Object);
	BallArrow->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 135.f), ChevronMesh.Object ? FRotator::ZeroRotator : FRotator(180.f, 0.f, 0.f));
	BallArrow->SetRelativeScale3D(ChevronMesh.Object ? FVector(1.f) : FVector(0.42f, 0.42f, 0.5f));
	BallArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallArrow->SetCastShadow(false);
	BallArrow->SetVisibility(false);
}

void APKFootballer::BeginPlay()
{
	Super::BeginPlay();
	static bool bDumped = false;
	if (!bDumped && FParse::Param(FCommandLine::Get(), TEXT("PKBoneDump")) && GetMesh()->GetSkeletalMeshAsset())
	{
		// Reference pose (mesh space) of the bones the kit material uses, and the mesh bounds.
		bDumped = true;
		const FReferenceSkeleton& Ref = GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();
		TArray<FTransform> CS;
		CS.SetNum(Ref.GetNum());
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const int32 Parent = Ref.GetParentIndex(i);
			CS[i] = Parent >= 0 ? Ref.GetRefBonePose()[i] * CS[Parent] : Ref.GetRefBonePose()[i];
			const FVector L = CS[i].GetLocation();
			const FRotator R = CS[i].Rotator();
			UE_LOG(LogTemp, Log, TEXT("[BoneDump] %s (%.1f, %.1f, %.1f) rot (%.0f, %.0f, %.0f) parent %s"), *Ref.GetBoneName(i).ToString(), L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, Parent >= 0 ? *Ref.GetBoneName(Parent).ToString() : TEXT("-"));
		}
		for (USkeletalMeshComponent* Part : { MHShirt.Get(), MHShorts.Get(), MHHead.Get() })
		{
			const USkeletalMesh* PartMesh = Part ? Part->GetSkeletalMeshAsset() : nullptr;
			if (!PartMesh)
			{
				continue;
			}
			const FReferenceSkeleton& PRef = PartMesh->GetRefSkeleton();
			TArray<FTransform> PCS;
			PCS.SetNum(PRef.GetNum());
			for (int32 i = 0; i < PRef.GetNum(); ++i)
			{
				const int32 Parent = PRef.GetParentIndex(i);
				PCS[i] = Parent >= 0 ? PRef.GetRefBonePose()[i] * PCS[Parent] : PRef.GetRefBonePose()[i];
				const FString Name = PRef.GetBoneName(i).ToString();
				if (Ref.FindBoneIndex(PRef.GetBoneName(i)) != INDEX_NONE)
				{
					const FVector L = PCS[i].GetLocation();
					const FRotator R = PCS[i].Rotator();
					UE_LOG(LogTemp, Log, TEXT("[BoneDumpMH] %s %s (%.1f, %.1f, %.1f) rot (%.0f, %.0f, %.0f)"), *PartMesh->GetName(), *Name, L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll);
				}
			}
			const FBoxSphereBounds PB = PartMesh->GetImportedBounds();
			UE_LOG(LogTemp, Log, TEXT("[BoneDumpMH] %s bounds origin (%.1f, %.1f, %.1f) extent (%.1f, %.1f, %.1f)"), *PartMesh->GetName(), PB.Origin.X, PB.Origin.Y, PB.Origin.Z, PB.BoxExtent.X, PB.BoxExtent.Y, PB.BoxExtent.Z);
		}
		const FBoxSphereBounds B = GetMesh()->GetSkeletalMeshAsset()->GetImportedBounds();
		UE_LOG(LogTemp, Log, TEXT("[BoneDump] bounds origin (%.1f, %.1f, %.1f) extent (%.1f, %.1f, %.1f)"), B.Origin.X, B.Origin.Y, B.Origin.Z, B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z);
	}
	LookTarget = GetActorLocation() + GetActorForwardVector() * 500.f;
	if (KitShell)
	{
		KitShell->SetLeaderPoseComponent(GetMesh()); // follows the body's pose, no animation of its own
	}
	for (USkeletalMeshComponent* Part : { MHHead.Get(), MHShirt.Get(), MHShorts.Get() })
	{
		if (Part && bMetaHumanParts)
		{
			Part->AddTickPrerequisiteComponent(GetMesh()); // they copy the body's evaluated pose
		}
	}
	// Bodies do not block each other rigidly (players got wedged between two opponents);
	// the game mode resolves contact softly and turns it into pushing and shoulder challenges.
	if (!FParse::Param(FCommandLine::Get(), TEXT("PKHardCollision")))
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void APKFootballer::InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome)
{
	Team = InTeam;
	Role = InRole;
	Index = InIndex;
	Home = InHome;
	ShirtNumber = InIndex + 1;
	// Ratings by role with individual spread, plus one or two traits.
	auto Rate = [](float Base) { return FMath::Clamp(Base + FMath::FRandRange(-0.08f, 0.08f), 0.35f, 0.97f); };
	auto Set = [this](float Pa, float Co, float Ps, float Sh, float De, float Ph)
	{
		Attr.Pace = Pa; Attr.Control = Co; Attr.Passing = Ps; Attr.Shooting = Sh; Attr.Defending = De; Attr.Physical = Ph;
	};
	switch (Role)
	{
	case EPKRole::Keeper:     Set(Rate(0.55f), Rate(0.55f), Rate(0.6f), Rate(0.35f), Rate(0.8f), Rate(0.75f)); break;
	case EPKRole::Defender:   Set(Rate(0.68f), Rate(0.62f), Rate(0.66f), Rate(0.45f), Rate(0.82f), Rate(0.8f)); break;
	case EPKRole::Midfielder: Set(Rate(0.72f), Rate(0.78f), Rate(0.82f), Rate(0.66f), Rate(0.64f), Rate(0.7f)); break;
	default:                  Set(Rate(0.84f), Rate(0.8f), Rate(0.7f), Rate(0.84f), Rate(0.4f), Rate(0.72f)); break;
	}
	if (!IsKeeper())
	{
		const uint8 Pools[3][3] = {
			{ PKTrait::PressProven, PKTrait::Anticipate, PKTrait::TikiTaka },
			{ PKTrait::TikiTaka, PKTrait::Technical, PKTrait::Anticipate },
			{ PKTrait::Finesse, PKTrait::PowerShot, PKTrait::Rapid },
		};
		const int32 Pool = Role == EPKRole::Defender ? 0 : (Role == EPKRole::Midfielder ? 1 : 2);
		Traits |= Pools[Pool][FMath::RandRange(0, 2)];
		if (FMath::FRand() < 0.5f)
		{
			Traits |= Pools[FMath::RandRange(0, 2)][FMath::RandRange(0, 2)];
		}
	}
	PaceScale = FMath::Lerp(0.9f, 1.06f, Attr.Pace) * (HasTrait(PKTrait::Rapid) ? 1.04f : 1.f) * (IsKeeper() ? 0.92f : 1.f);

	// Kit: a plain team kit until the team sheet dresses the player (ApplyKit).
	USkeletalMeshComponent* Body = GetMesh();
	const int32 TorsoSlot = Body->GetMaterialIndex(TEXT("M_Torso"));
	const int32 LegsSlot = Body->GetMaterialIndex(TEXT("M_HeadLegs"));
	if (KitBodyMaterial)
	{
		FPKKit Plain;
		Plain.Primary = PK::TeamColor(Team, false);
		Plain.Secondary = Plain.Primary * 0.5f;
		Plain.Socks = Plain.Primary;
		Plain.Shorts = PK::ShortsColor(Team, false);
		ApplyKit(Plain, Look);
	}
	else if (KitMaterial)
	{
		if (UMaterialInstanceDynamic* Shirt = Body->CreateDynamicMaterialInstance(TorsoSlot == INDEX_NONE ? 1 : TorsoSlot, KitMaterial))
		{
			Shirt->SetVectorParameterValue(TEXT("Paint Tint"), PK::TeamColor(Team, IsKeeper()));
		}
		if (UMaterialInstanceDynamic* Legs = Body->CreateDynamicMaterialInstance(LegsSlot == INDEX_NONE ? 0 : LegsSlot, KitMaterial))
		{
			Legs->SetVectorParameterValue(TEXT("Paint Tint"), PK::ShortsColor(Team, IsKeeper()));
		}
	}
	PK::Paint(Marker, SurfaceMaterial, Team == 0 ? FLinearColor(1.f, 0.8f, 0.f) : FLinearColor(0.2f, 0.85f, 1.f), 0.5f, 2500.f); // emissive in nits (physical sun)
	PK::Paint(BallArrow, SurfaceMaterial, Team == 0 ? FLinearColor(0.15f, 0.55f, 1.f) : FLinearColor(1.f, 0.3f, 0.15f), 0.4f, 3000.f);
}

void APKFootballer::ApplyKit(const FPKKit& Kit, const FPKLook& InLook)
{
	Look = InLook;
	if (const FPKHead* Head = bMetaHumanParts ? PickHead() : nullptr)
	{
		// limbs take the face's own (baked) skin tone; the kit material's limb shading warms and darkens it, so it
		// goes in a little paler and less saturated than the albedo
		const float HeadLum = Head->Skin.GetLuminance();
		Look.Skin = FMath::Lerp(Head->Skin, FLinearColor(HeadLum, HeadLum, HeadLum), 0.35f) * 1.1f;
	}
	else
	{
	// the stadium sun and the filmic curve push tan skin to orange: take a quarter of the saturation out (face and limbs alike)
	const float SkinLum = Look.Skin.GetLuminance();
	Look.Skin = FMath::Lerp(Look.Skin, FLinearColor(SkinLum, SkinLum, SkinLum), 0.25f) * 0.85f; // and a little darker: pale faces flared
	// cap the albedo of the palest tones (they bleached to white eggs in full sun at broadcast distance)
	const float Capped = Look.Skin.GetLuminance();
	if (Capped > 0.22f)
	{
		Look.Skin *= 0.22f / Capped;
	}
	}
	USkeletalMeshComponent* Body = GetMesh();
	if (!KitBodyMaterial || !Body)
	{
		return;
	}
	if (!KitBodyMID)
	{
		KitBodyMID = UMaterialInstanceDynamic::Create(KitBodyMaterial, this);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, KitBodyMID);
		}
	}
	if (!KitShellMID && KitShellMaterial && KitShell)
	{
		KitShellMID = UMaterialInstanceDynamic::Create(KitShellMaterial, this);
		for (int32 Slot = 0; Slot < KitShell->GetNumMaterials(); ++Slot)
		{
			KitShell->SetMaterial(Slot, KitShellMID);
		}
	}
	const bool bKeeper = IsKeeper();
	const float Lum = (bKeeper ? Kit.KeeperPrimary : Kit.Primary).GetLuminance();
	for (UMaterialInstanceDynamic* MID : { KitBodyMID.Get(), KitShellMID.Get() })
	{
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(TEXT("Primary"), bKeeper ? Kit.KeeperPrimary : Kit.Primary);
		MID->SetVectorParameterValue(TEXT("Secondary"), bKeeper ? Kit.KeeperSecondary : Kit.Secondary);
		MID->SetVectorParameterValue(TEXT("Trim"), Kit.Trim);
		MID->SetVectorParameterValue(TEXT("Shorts"), bKeeper ? Kit.KeeperShorts : Kit.Shorts);
		MID->SetVectorParameterValue(TEXT("Socks"), bKeeper ? Kit.KeeperPrimary : Kit.Socks);
		MID->SetVectorParameterValue(TEXT("NumberColor"), bKeeper ? (Lum > 0.25f ? FLinearColor(0.02f, 0.02f, 0.02f) : FLinearColor(0.9f, 0.9f, 0.9f)) : Kit.Number);
		MID->SetVectorParameterValue(TEXT("Boots"), Look.Boots);
		MID->SetVectorParameterValue(TEXT("Skin"), Look.Skin);
		MID->SetVectorParameterValue(TEXT("Hair"), Look.Hair);
		MID->SetVectorParameterValue(TEXT("Gloves"), Kit.KeeperGloves);
		// KitA = (pattern, number, sleeve length, shorts length), KitB = (hair style, beard, keeper, boot flash)
		MID->SetVectorParameterValue(TEXT("KitA"), FLinearColor(bKeeper ? 2.f : (float)Kit.Pattern, (float)ShirtNumber, bKeeper ? 1.f : 0.52f, 0.46f));
		MID->SetVectorParameterValue(TEXT("KitB"), FLinearColor((float)Look.HairStyle, Look.Beard, bKeeper ? 1.f : 0.f, Look.bBootFlash ? 1.f : 0.f));
	}
	if (KitBodyMID)
	{
		KitBodyMID->SetScalarParameterValue(TEXT("MHParts"), bMetaHumanParts ? 1.f : 0.f);
	}
	if (bMetaHumanParts)
	{
		DressMetaHuman(Kit);
	}
	if (BootL && BootL->IsVisible())
	{
		if (!BootMID)
		{
			BootMID = UMaterialInstanceDynamic::Create(BootL->GetMaterial(0), this);
			BootL->SetMaterial(0, BootMID);
			BootR->SetMaterial(0, BootMID);
		}
		// boots that match the socks vanish into them: swap to white (or black on light socks)
		const FLinearColor Socks = IsKeeper() ? Kit.KeeperPrimary : Kit.Socks;
		FLinearColor Upper = Look.Boots;
		const FVector3f D(Upper.R - Socks.R, Upper.G - Socks.G, Upper.B - Socks.B);
		if (D.Size() < 0.35f)
		{
			Upper = Socks.GetLuminance() > 0.4f ? FLinearColor(0.03f, 0.03f, 0.035f) : FLinearColor(0.88f, 0.88f, 0.86f);
		}
		BootMID->SetVectorParameterValue(TEXT("Upper"), Upper);
		// two-tone: a contrasting sole plate and studs
		const bool bLight = Upper.GetLuminance() > 0.3f;
		BootMID->SetVectorParameterValue(TEXT("Sole"), bLight ? FLinearColor(0.03f, 0.03f, 0.035f) : FLinearColor(0.85f, 0.85f, 0.83f));
		BootMID->SetVectorParameterValue(TEXT("Trim"), bLight ? FLinearColor(0.02f, 0.02f, 0.025f) : FLinearColor(0.9f, 0.9f, 0.9f));
		BootMID->SetScalarParameterValue(TEXT("Flash"), 0.f); // plain uppers: a curved flash on this boot read like a brand logo
		if (KitBodyMID)
		{
			KitBodyMID->SetScalarParameterValue(TEXT("BootMeshes"), 1.f); // the mannequin's own boots hide under them
		}
	}
	// Body build: taller / broader players (feet stay on the ground: the mesh origin is at the feet).
	Body->SetRelativeScale3D(FVector(Look.Build, Look.Build * 0.97f + 0.03f, Look.Height));
}

void APKFootballer::DressMetaHuman(const FPKKit& Kit)
{
	// Shirt and shorts: the same kit parameters as the body material.
	auto Cloth = [this](USkeletalMeshComponent* Part, UMaterialInterface* Material, TObjectPtr<UMaterialInstanceDynamic>& MID)
	{
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(Material, this);
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				const bool bSocks = Part->GetMaterialSlotNames().IsValidIndex(Slot) && Part->GetMaterialSlotNames()[Slot].ToString().Contains(TEXT("Socks"));
				Part->SetMaterial(Slot, bSocks ? MHHiddenMaterial.Get() : MID.Get());
			}
		}
		for (const FName Param : { FName("Primary"), FName("Secondary"), FName("Trim"), FName("Shorts"), FName("NumberColor"), FName("KitA"), FName("KitB") })
		{
			FLinearColor Value;
			if (KitBodyMID && KitBodyMID->GetVectorParameterValue(FHashedMaterialParameterInfo(Param), Value))
			{
				MID->SetVectorParameterValue(Param, Value);
			}
		}
	};
	Cloth(MHShirt, MHShirtMaterial, MHShirtMID);
	Cloth(MHShorts, MHShortsMaterial, MHShortsMID);
	// a chest sponsor per club (rows 0-5 of the sponsor atlas), picked from the kit colours so both teams differ
	const FLinearColor& Main = Kit.Primary;
	const int32 Sponsor = (FMath::Abs(FMath::RoundToInt(Main.R * 7.f + Main.G * 13.f + Main.B * 29.f + Kit.Pattern * 3.f)) + Team * 3) % 6; // the sides never share one
	MHShirtMID->SetScalarParameterValue(TEXT("Sponsor"), (float)Sponsor);
	SetShirtName();
	FString Hide;
	if (FParse::Value(FCommandLine::Get(), TEXT("PKHidePart="), Hide)) // debugging: Shirt / Shorts / Head
	{
		for (USkeletalMeshComponent* Part : { MHShirt.Get(), MHShorts.Get(), MHHead.Get() })
		{
			Part->SetVisibility(!Part->GetName().Contains(Hide), true);
		}
	}

	if (const FPKHead* Head = PickHead())
	{
		DressHeadFromSet(*Head);
		return;
	}
	// Haircut from the look; the head is then the scanned face that groom was made for (a groom on another skull
	// floated off the forehead or showed scalp through it). Buzz cuts are painted and go on any head.
	const bool bDark = Look.Skin.GetLuminance() < 0.1f;
	EPKHairCut Cut = EPKHairCut::Buzz;
	switch (Look.HairStyle)
	{
	case 0:
	case 1: // short crops get cards too: painted scalps read as bald white eggs from the broadcast camera
		Cut = bDark ? EPKHairCut::AfroFade : EPKHairCut::PulledBack;
		break;
	case 2: Cut = bDark ? EPKHairCut::AfroFade : (ShirtNumber % 4 == 0 ? EPKHairCut::HairLoss : EPKHairCut::PulledBack); break;
	case 3: Cut = bDark ? EPKHairCut::Coil : EPKHairCut::CurlyFade; break;
	case 4: Cut = bDark ? EPKHairCut::CurlyFade : EPKHairCut::PulledBack; break; // (no long fringe: it hid an eye)
	default: break;
	}
	// groom owners: AfroFade m_001, CurlyFade m_002, SideSwept m_003, PulledBack m_004, HairLoss m_006 (Coil is from a
	// female head, refitted in build_player_hair.py: it goes on m_005)
	// (tying the head to the haircut gave five players one face: heads are spread evenly over each team instead,
	// a different order for each side)
	const int32 HeadIndex = (Index + Team * 3 + (Index / MHHeadMeshes.Num()) * 2) % MHHeadMeshes.Num();
	if (MHHead->GetSkeletalMeshAsset() != MHHeadMeshes[HeadIndex])
	{
		MHHead->SetSkeletalMesh(MHHeadMeshes[HeadIndex]);
	}
	if (!MHSkinMID)
	{
		MHSkinMID = UMaterialInstanceDynamic::Create(MHSkinMaterial, this);
	}
	const TArray<FName> Slots = MHHead->GetMaterialSlotNames();
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		const FString Name = Slots[Slot].ToString();
		UMaterialInterface* Material = MHHiddenMaterial;
		if (Name.StartsWith(TEXT("head")))
		{
			Material = MHSkinMID;
		}
		else if (Name.StartsWith(TEXT("eyeLeft")) || Name.StartsWith(TEXT("eyeRight")))
		{
			Material = MHEyeMaterial ? MHEyeMaterial.Get() : Material;
		}
		else if (Name.StartsWith(TEXT("teeth")))
		{
			Material = MHTeethMaterial ? MHTeethMaterial.Get() : Material;
		}
		MHHead->SetMaterial(Slot, Material);
	}
	MHSkinMID->SetVectorParameterValue(TEXT("Skin"), Look.Skin);
	MHSkinMID->SetVectorParameterValue(TEXT("Hair"), Look.Hair);
	MHSkinMID->SetScalarParameterValue(TEXT("Beard"), Look.Beard);
	if (MHHeadNormals.IsValidIndex(HeadIndex) && MHHeadNormals[HeadIndex])
	{
		MHSkinMID->SetTextureParameterValue(TEXT("HeadNormal"), MHHeadNormals[HeadIndex]);
	}
	// Groom cards on the head bone (Tools/editor_scripts/build_player_hair.py bakes them in head-bone space).
	// Beard cards only on the head they were groomed for (on other jaws they float); elsewhere the beard is painted.
	const int32 BeardMesh = Look.Beard > 0.75f && HeadIndex == 1 ? 6 : (Look.Beard > 0.5f && HeadIndex == 2 ? 7 : -1);
	// melanin from the hair colour (1 = black), redness from its red bias
	const float HairLum = Look.Hair.GetLuminance();
	const float Melanin = FMath::Clamp(1.f - FMath::Sqrt(HairLum) * 1.6f, 0.05f, 0.8f); // never pure black: dark brown reads better
	const float Redness = FMath::Clamp((Look.Hair.R - Look.Hair.B) / FMath::Max(Look.Hair.R, 0.01f), 0.f, 1.f) * 0.6f;
	auto Groom = [&](UStaticMeshComponent* Comp, int32 MeshIndex)
	{
		UStaticMesh* GroomMesh = MHHairMeshes.IsValidIndex(MeshIndex) ? MHHairMeshes[MeshIndex].Get() : nullptr;
		Comp->SetVisibility(GroomMesh != nullptr);
		static const int32 ForcedLod = [] { int32 L = 0; FParse::Value(FCommandLine::Get(), TEXT("PKHairLod="), L); return L; }();
		Comp->SetForcedLodModel(Comp == MHHairBase ? 3 : ForcedLod); // the base layer always draws the helmet (LOD 2)
		if (!GroomMesh)
		{
			return;
		}
		if (Comp->GetStaticMesh() != GroomMesh)
		{
			Comp->SetStaticMesh(GroomMesh);
			for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
			{
				Comp->CreateDynamicMaterialInstance(Slot);
			}
		}
		for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(Slot)))
			{
				// M_PK_HairCards (the card LODs): a plain colour, lifted off pure black so dark hair keeps its shape
				MID->SetVectorParameterValue(TEXT("HairColor"), Look.Hair * 0.9f + FLinearColor(0.022f, 0.015f, 0.01f));
				// the pack's helmet material (lowest LOD)
				MID->SetScalarParameterValue(TEXT("hairMelanin"), Melanin);
				MID->SetScalarParameterValue(TEXT("hairRedness"), Redness);
				MID->SetScalarParameterValue(TEXT("PixelDepthOffset"), 0.f); // the pack's depth push hides the cards near the scalp
				MID->SetScalarParameterValue(TEXT("HairRoughness"), 0.52f);  // less plastic shine in the stadium sun
				MID->SetScalarParameterValue(TEXT("RoughnessOverall"), 0.62f);
				MID->SetScalarParameterValue(TEXT("Spec0"), 0.3f);           // dull sheen instead of black foil
				MID->SetScalarParameterValue(TEXT("Spec1"), 0.45f);
				MID->SetScalarParameterValue(TEXT("RIntensity"), 0.1f);
				MID->SetScalarParameterValue(TEXT("OpacityNear"), 1.0f);     // let the card alpha fall off (2.0 clipped it hard)
				MID->SetScalarParameterValue(TEXT("OpacityFar"), 1.2f);
				MID->SetScalarParameterValue(TEXT("OpacityPowNear"), 1.0f);
			}
		}
	};
	Groom(MHHair, Cut == EPKHairCut::Buzz ? -1 : (int32)Cut - 1);
	Groom(MHHairBase, Cut == EPKHairCut::Buzz ? -1 : (int32)Cut - 1);
	Groom(MHBeard, BeardMesh);

	auto Pick = [](const TArray<TObjectPtr<UTexture>>& Maps, int32 I) -> UTexture* { return Maps.Num() ? Maps[FMath::Clamp(I, 0, Maps.Num() - 1)].Get() : nullptr; };
	if (UTexture* HairMap = Pick(MHHairMaps, (int32)Cut))
	{
		MHSkinMID->SetTextureParameterValue(TEXT("HairAttributeMap"), HairMap);
	}
	if (UTexture* BrowMap = Pick(MHBrowMaps, HeadIndex + ShirtNumber))
	{
		MHSkinMID->SetTextureParameterValue(TEXT("EyebrowsAttributeMap"), BrowMap);
	}
	if (UTexture* BeardMap = Pick(MHBeardMaps, Look.Beard > 0.6f && BeardMesh < 0 ? 1 : 0))
	{
		MHSkinMID->SetTextureParameterValue(TEXT("BeardAttributeMap"), BeardMap);
	}
}

const FPKHead* APKFootballer::PickHead() const
{
	if (!HeadSet || HeadSet->Heads.IsEmpty())
	{
		return nullptr;
	}
	// every player on a side has his own face; the away side starts half way round the set
	const int32 N = HeadSet->Heads.Num();
	const FPKHead& Head = HeadSet->Heads[(Index + Team * (N / 2)) % N];
	return Head.Face ? &Head : nullptr;
}

void APKFootballer::DressHeadFromSet(const FPKHead& Head)
{
	if (MHHead->GetSkeletalMeshAsset() != Head.Face)
	{
		MHHead->SetSkeletalMesh(Head.Face);
		MHHead->EmptyOverrideMaterials(); // the build's baked skin, eye, teeth and lash materials
	}
	auto Groom = [&Head](UStaticMeshComponent* Comp, UStaticMesh* GroomMesh)
	{
		Comp->SetVisibility(GroomMesh != nullptr);
		Comp->SetForcedLodModel(0);
		if (!GroomMesh)
		{
			return;
		}
		if (Comp->GetStaticMesh() != GroomMesh)
		{
			Comp->SetStaticMesh(GroomMesh);
			Comp->EmptyOverrideMaterials();
			Comp->CreateDynamicMaterialInstance(0);
		}
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
		{
			// lifted off pure black so dark hair keeps its shape (as on the crowd-pack heads)
			MID->SetVectorParameterValue(TEXT("HairColor"), Head.HairColor * 0.9f + FLinearColor(0.022f, 0.015f, 0.01f));
		}
	};
	Groom(MHHair, Head.Hair);
	Groom(MHBeard, Head.Beard);
	Groom(MHMustache, Head.Mustache);
	Groom(MHBrows, Head.Eyebrows);
	// a dense, slightly darker copy of the hair just under it: thin cuts showed the pale scalp through the cards
	Groom(MHHairBase, Head.Hair);
	if (UMaterialInstanceDynamic* BaseMID = Cast<UMaterialInstanceDynamic>(MHHairBase->GetMaterial(0)))
	{
		BaseMID->SetScalarParameterValue(TEXT("Coverage"), 1.6f); // (3 made the faint outer cards solid: a grey halo)
		BaseMID->SetVectorParameterValue(TEXT("HairColor"), Head.HairColor * 0.7f + FLinearColor(0.012f, 0.008f, 0.006f));
	}
}

void APKFootballer::SetShirtName()
{
	if (!MHShirtMID)
	{
		return;
	}
	// "J. Smith" -> SMITH; kit-font codes: 0-9 digits, 10-35 A-Z, 36 '.', 37 '-', 38 apostrophe, 39 space
	FString Surname = PlayerName;
	int32 Space = INDEX_NONE;
	if (Surname.FindLastChar(TEXT(' '), Space))
	{
		Surname = Surname.Mid(Space + 1);
	}
	Surname = Surname.ToUpper().Left(12);
	float Codes[12];
	int32 Len = 0;
	for (const TCHAR Ch : Surname)
	{
		float Code = 39.f;
		if (Ch >= '0' && Ch <= '9') Code = float(Ch - '0');
		else if (Ch >= 'A' && Ch <= 'Z') Code = float(10 + Ch - 'A');
		else if (Ch == '.') Code = 36.f;
		else if (Ch == '-') Code = 37.f;
		else if (Ch == TEXT('\'')) Code = 38.f;
		Codes[Len++] = Code;
	}
	for (int32 i = Len; i < 12; ++i)
	{
		Codes[i] = 39.f;
	}
	MHShirtMID->SetVectorParameterValue(TEXT("NameA"), FLinearColor(Codes[0], Codes[1], Codes[2], Codes[3]));
	MHShirtMID->SetVectorParameterValue(TEXT("NameB"), FLinearColor(Codes[4], Codes[5], Codes[6], Codes[7]));
	MHShirtMID->SetVectorParameterValue(TEXT("NameC"), FLinearColor(Codes[8], Codes[9], Codes[10], Codes[11]));
	MHShirtMID->SetScalarParameterValue(TEXT("NameLen"), (float)Len);
}

void APKFootballer::ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint32 InTraits, int32 InNumber, const FString& InName, uint32 InPlus)
{
	Role = InRole;
	Home = InHome;
	Attr = InAttr;
	Traits = InTraits;
	TraitsPlus = InPlus & InTraits;
	ShirtNumber = InNumber;
	PlayerName = InName;
	SetShirtName();
	if (MHShirtMID)
	{
		FLinearColor KitA;
		if (MHShirtMID->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("KitA")), KitA))
		{
			KitA.G = (float)ShirtNumber; // the profile's squad number (the kit was applied before it)
			MHShirtMID->SetVectorParameterValue(TEXT("KitA"), KitA);
		}
	}
	AccelType = AccelTypeFor(Attr);
	PaceScale = FMath::Lerp(0.9f, 1.06f, Attr.Pace) * (1.f + 0.04f * StyleLevel(PKTrait::Rapid)) * (IsKeeper() ? 0.92f : 1.f);
}

uint8 APKFootballer::AccelTypeFor(const FPKAttributes& A)
{
	// Agile and light: Explosive. Strong with pace: Lengthy. Otherwise Controlled.
	if (A.Pace > 0.68f && A.Control - A.Physical > 0.06f)
	{
		return 1;
	}
	if (A.Pace > 0.6f && A.Physical - A.Control > 0.06f)
	{
		return 2;
	}
	return 0;
}

const TCHAR* APKFootballer::AccelName(uint8 Type)
{
	return Type == 1 ? TEXT("Explosive") : (Type == 2 ? TEXT("Lengthy") : TEXT("Controlled"));
}

float APKFootballer::AccelFactor(float SpeedAlpha) const
{
	// Acceleration across the speed range, plus Quick Step off the mark.
	float F = 1.f;
	if (AccelType == 1)
	{
		F = FMath::Lerp(1.4f, 0.9f, SpeedAlpha);
	}
	else if (AccelType == 2)
	{
		F = FMath::Lerp(0.78f, 1.2f, SpeedAlpha);
	}
	return F * (1.f + 0.12f * StyleLevel(PKTrait::QuickStep) * (1.f - SpeedAlpha));
}

namespace PKTrait
{
	const TCHAR* Name(int32 Bit)
	{
		static const TCHAR* Names[Count] = {
			TEXT("Technical"), TEXT("Finesse Shot"), TEXT("Power Shot"), TEXT("Tiki Taka"), TEXT("Rapid"), TEXT("Press Proven"), TEXT("Anticipate"),
			TEXT("Whipped Pass"), TEXT("Incisive Pass"), TEXT("Long Ball Pass"), TEXT("Trickster"), TEXT("Quick Step"), TEXT("Relentless"), TEXT("Bruiser"),
			TEXT("Intercept"), TEXT("Block"), TEXT("Aerial"), TEXT("Jockey"), TEXT("Acrobatic"), TEXT("Far Throw"), TEXT("Dead Ball"), TEXT("Chip Shot"), TEXT("Slide Tackle") };
		return Bit >= 0 && Bit < Count ? Names[Bit] : TEXT("");
	}

	FString Names(uint32 Traits, uint32 Plus, int32 MaxNames)
	{
		TArray<FString> Out;
		for (int32 Bit = 0; Bit < Count; ++Bit)
		{
			if (Traits & (1u << Bit))
			{
				Out.Add(FString(Name(Bit)) + ((Plus & (1u << Bit)) ? TEXT("+") : TEXT("")));
			}
		}
		// PlayStyle+ first: they are what the player is known for.
		Out.StableSort([](const FString& L, const FString& R) { return L.EndsWith(TEXT("+")) && !R.EndsWith(TEXT("+")); });
		if (MaxNames > 0 && Out.Num() > MaxNames)
		{
			Out.SetNum(MaxNames);
		}
		return FString::Join(Out, TEXT(", "));
	}
}

void APKFootballer::SetMarkersSuppressed(bool bSuppressed)
{
	Marker->SetHiddenInGame(bSuppressed);
	BallArrow->SetHiddenInGame(bSuppressed);
}

void APKFootballer::SetBallArrow(bool bVisible)
{
	if (bVisible != bArrowVisible)
	{
		bArrowVisible = bVisible;
		BallArrow->SetVisibility(bVisible);
	}
}

FString APKFootballer::RoleShort() const
{
	switch (Role)
	{
	case EPKRole::Keeper: return TEXT("GK");
	case EPKRole::Defender: return TEXT("DEF");
	case EPKRole::Midfielder: return TEXT("MID");
	default: return TEXT("FWD");
	}
}

FString APKFootballer::TraitNames(int32 MaxNames) const
{
	return PKTrait::Names(Traits, TraitsPlus, MaxNames);
}

void APKFootballer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bReplaying)
	{
		return; // posed from the replay recording
	}

	StunTimer = FMath::Max(0.f, StunTimer - DeltaSeconds);
	TackleCooldown = FMath::Max(0.f, TackleCooldown - DeltaSeconds);
	StealCooldown = FMath::Max(0.f, StealCooldown - DeltaSeconds);
	LungeTimer = FMath::Max(0.f, LungeTimer - DeltaSeconds);
	ProtectedTimer = FMath::Max(0.f, ProtectedTimer - DeltaSeconds);
	TouchTimer = FMath::Max(0.f, TouchTimer - DeltaSeconds);
	HeaderRequestTimer = FMath::Max(0.f, HeaderRequestTimer - DeltaSeconds);
	FallbackKickTimer = FMath::Max(0.f, FallbackKickTimer - DeltaSeconds);

	if (bArrowVisible)
	{
		ArrowPhase += DeltaSeconds * 6.f;
		BallArrow->SetRelativeLocation(FVector(0.f, 0.f, 135.f + 8.f * FMath::Sin(ArrowPhase)));
		BallArrow->AddRelativeRotation(FRotator(0.f, 180.f * DeltaSeconds, 0.f));
	}

	UpdateAction(DeltaSeconds);
	UpdateMovementModel(DeltaSeconds);
	UpdateFootSteps();
	if (!bHasBall)
	{
		bControlledSprint = false;
		DribbleSpeedScale = 1.f;
		DribbleSteerWeight = 0.f;
	}
}

void APKFootballer::UpdateFootSteps()
{
	// The pose used here is last frame's; that is fine for counting steps. The ankle bones are used because the
	// toe bones are dropped from the lower mesh LODs (a player seen from the broadcast camera would stop stepping).
	const USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->DoesSocketExist(TEXT("foot_l")))
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	const FVector Facing = GetFacing();
	const float GroundZ = Loc.Z - CapsuleHalfHeight;
	const float Speed = PK::Flat(GetVelocity()).Size();
	const double Now = GetWorld()->GetTimeSeconds();
	constexpr float SwingLine = -3.f; // ankle position (cm ahead of the body centre) when the toes pass ~10 cm ahead
	for (int32 Foot = 0; Foot < 2; ++Foot)
	{
		const FVector P = Body->GetSocketLocation(Foot == 0 ? TEXT("foot_l") : TEXT("foot_r"));
		const float Fwd = FVector::DotProduct(PK::Flat(P - Loc), Facing);
		const float Height = P.Z - GroundZ;
		if (Speed > 20.f && FootFwdPrev[Foot] < SwingLine && Fwd >= SwingLine && Height < 40.f)
		{
			const float Interval = (float)(Now - LastSwingTime);
			if (Interval > 0.1f && Interval < 0.8f)
			{
				StepInterval = FMath::Lerp(StepInterval, Interval, 0.35f);
			}
			LastSwingTime = Now;
			LastSwingFoot = Foot;
			++SwingCounter;
		}
		FootFwdPrev[Foot] = Fwd;
	}
}

FVector APKFootballer::GetFootContactPoint(int32 Foot) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	const FVector Ankle = Body ? Body->GetSocketLocation(Foot == 0 ? TEXT("foot_l") : TEXT("foot_r")) : GetActorLocation() + GetFacing() * 25.f;
	// The ball centre sits a ball radius in front of the toes, ~13 cm ahead of the ankle.
	return PK::Flat(Ankle) + GetFacing() * (13.f + PK::BallRadius);
}

FVector APKFootballer::GetFootSideContactPoint(int32 Foot, float Side) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	const FVector Ankle = Body ? Body->GetSocketLocation(Foot == 0 ? TEXT("foot_l") : TEXT("foot_r")) : GetActorLocation() + GetFacing() * 25.f;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, GetFacing());
	// Beside the front half of the foot, a ball radius plus half a boot width out.
	return PK::Flat(Ankle) + GetFacing() * 9.f + Right * Side * (5.f + PK::BallRadius);
}

void APKFootballer::UpdateStamina(float Dt, float Rate)
{
	const float Speed = PK::Flat(GetVelocity()).Size();
	const float Fitness = FMath::Lerp(1.3f, 0.75f, Attr.Physical) * (1.f - 0.2f * StyleLevel(PKTrait::Relentless)); // strong players last longer
	float Drain = 0.f;
	if (IsSprinting())
	{
		Drain = 0.017f;
	}
	else if (bControlledSprint && Speed > PK::JogSpeed)
	{
		Drain = 0.008f;
	}
	else if (Speed > 400.f)
	{
		Drain = 0.003f;
	}
	MaxStamina = FMath::Max(0.5f, MaxStamina - 0.0009f * Rate * Dt);
	if (Drain > 0.f)
	{
		Stamina = FMath::Max(0.f, Stamina - Drain * Fitness * Rate * Dt);
	}
	else if (Speed < 320.f)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + 0.012f * Rate * Dt);
	}
	Stamina = FMath::Min(Stamina, MaxStamina);
}

void APKFootballer::StartSkillPose(uint8 Pose, float Side)
{
	SkillPose = Pose;
	SkillTime = GetWorld()->GetTimeSeconds();
	SkillSide = Side >= 0.f ? 1.f : -1.f;
}

void APKFootballer::NoteFootTouch(int32 Foot, float Side, bool bOutside)
{
	FootTouchTime = GetWorld()->GetTimeSeconds();
	FootTouchFoot = Foot;
	FootTouchSide = Side;
	bFootTouchOutside = bOutside;
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------

bool APKFootballer::PlayAction(EPKAction Action, EPKAction Then, float Rate)
{
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	if (Action == EPKAction::None || !Clip.IsValid())
	{
		return false;
	}
	if (Action != KickClip)
	{
		Kick.bPending = false; // anything else interrupts a kick
		KickClip = EPKAction::None;
	}
	CurrentAction = Action;
	NextAction = Then;
	ActionTime = Clip.Start;
	ActionEntry = Clip.Start;
	ActionRate = FMath::Clamp(Rate, 0.5f, 2.5f);
	return true;
}

bool APKFootballer::PlayActionTimed(EPKAction Action, float SecondsToContact, float MaxRate)
{
	if (!PlayAction(Action))
	{
		return false;
	}
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	const float Lead = FMath::Max(Clip.Contact - Clip.Start, 0.01f);
	const float T = FMath::Max(SecondsToContact, 0.02f);
	ActionRate = FMath::Clamp(Lead / T, 0.8f, FMath::Clamp(MaxRate, 0.8f, 2.5f));
	if (Lead / ActionRate > T)
	{
		// Even the fastest swing is late: join the move nearer the contact.
		ActionTime = FMath::Max(Clip.Start, Clip.Contact - T * ActionRate);
		ActionEntry = ActionTime;
	}
	return true;
}

void APKFootballer::StopAction()
{
	CurrentAction = EPKAction::None;
	NextAction = EPKAction::None;
	Kick.bPending = false;
	KickClip = EPKAction::None;
}

void APKFootballer::UpdateAction(float DeltaSeconds)
{
	if (CurrentAction == EPKAction::None)
	{
		return;
	}
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(CurrentAction);
	const float Previous = ActionTime;
	ActionTime += DeltaSeconds * ActionRate;

	// Clip-driven travel (slides, dives, falls): mesh space (+Y forward, +X left) -> actor space.
	if (Clip.bApplyTravel)
	{
		const FVector2D D = Clip.TravelAt(ActionTime) - Clip.TravelAt(Previous);
		const FVector World = GetActorRotation().RotateVector(FVector(D.Y, -D.X, 0.f));
		AddActorWorldOffset(World, true);
	}

	if (IsTackleActive())
	{
		LungeTimer = 0.05f;
	}

	if (ActionTime >= Clip.Stop && Clip.bHoldAtEnd)
	{
		ActionTime = Clip.Stop; // hold the pose
	}
	else if (ActionTime >= Clip.Stop)
	{
		const EPKAction Then = NextAction;
		CurrentAction = EPKAction::None;
		NextAction = EPKAction::None;
		KickClip = EPKAction::None;
		if (Then != EPKAction::None)
		{
			PlayAction(Then, Then == EPKAction::Fallen ? EPKAction::StandUp : EPKAction::None);
		}
	}
}

bool APKFootballer::IsActionLockingMovement() const
{
	return CurrentAction != EPKAction::None && UPKAnimInstance::GetActionClip(CurrentAction).bLockMovement;
}

bool APKFootballer::IsStunned() const
{
	return StunTimer > 0.f || (CurrentAction != EPKAction::None && UPKAnimInstance::GetActionClip(CurrentAction).bGrounded);
}

bool APKFootballer::IsTackleActive() const
{
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(CurrentAction);
	if (CurrentAction == EPKAction::Tackle)
	{
		return FMath::Abs(ActionTime - Clip.Contact) < 0.18f;
	}
	if (CurrentAction == EPKAction::SlideTackle)
	{
		return ActionTime > Clip.Start + 0.15f && ActionTime < FMath::Lerp(Clip.Start, Clip.Stop, 0.6f);
	}
	if (CurrentAction == EPKAction::SlideTackleB)
	{
		return ActionTime > 0.6f && ActionTime < 1.45f; // poke, then the slide
	}
	if (CurrentAction == EPKAction::SlideTackleC)
	{
		return ActionTime > 0.3f && ActionTime < 1.0f; // diving tackle
	}
	return false;
}

// ---------------------------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------------------------

void APKFootballer::UpdateMovementModel(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Vel = PK::Flat(GetVelocity());
	const float Speed = Vel.Size();

	float MaxSpeed = PK::JogSpeed;
	switch (Stance)
	{
	case EPKStance::CloseControl: MaxSpeed = PK::WalkSpeed; break;
	case EPKStance::Jockey:       MaxSpeed = PK::JockeySpeed * (1.f + 0.1f * StyleLevel(PKTrait::Jockey)); break;
	case EPKStance::Shield:       MaxSpeed = PK::ShieldSpeed; break;
	default:                      MaxSpeed = (bSprint && Stamina > 0.06f) ? PK::SprintSpeed : (bControlledSprint ? PK::ControlledSprintSpeed : PK::JogSpeed); break;
	}
	if (bHasBall && Stance == EPKStance::Normal)
	{
		MaxSpeed *= 0.93f;
	}
	if (IsKicking())
	{
		MaxSpeed *= CurrentAction == EPKAction::RunningShot ? 0.8f : 0.45f;
	}
	// AcceleRATE top end: a lengthy runner's long stride is worth a little more in a full sprint, an explosive one a
	// little less.
	const float StrideTop = IsSprinting() ? (AccelType == 2 ? 1.035f : (AccelType == 1 ? 0.985f : 1.f)) : 1.f;
	Move->MaxWalkSpeed = MaxSpeed * PaceScale * (bHasBall ? DribbleSpeedScale : 1.f) * StaminaSpeedScale() * StrideTop;

	const float SpeedAlpha = FMath::Clamp(Speed / PK::SprintSpeed, 0.f, 1.f);
	Move->MaxAcceleration = FMath::Lerp(2100.f, 900.f, SpeedAlpha) * AccelFactor(SpeedAlpha);

	const bool bFacingStance = (Stance == EPKStance::Jockey || Stance == EPKStance::Shield) && !FaceTarget.IsNearlyZero() && !IsInAction();
	// Off the ball at walking pace: keep the body open to the ball and side-step / back-pedal (8-way locomotion).
	const float WatchSpeed = bHuman ? 40.f : 280.f;
	const bool bFaceBall = bWatchBall && !bFacingStance && !bHasBall && !IsInAction() && !Kick.bPending && Stance == EPKStance::Normal
		&& Speed < WatchSpeed && MoveInput.Size() < 0.75f && !LookTarget.IsNearlyZero();
	Move->bOrientRotationToMovement = !bFacingStance && !bFaceBall && !IsActionLockingMovement() && !Kick.bPending;
	if (bFaceBall)
	{
		const FVector ToBall = PK::Flat(LookTarget - GetActorLocation());
		if (ToBall.SizeSquared() > 40.f * 40.f)
		{
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, ToBall.Rotation().Yaw, 0.f), DeltaSeconds, 5.f));
		}
	}
	Move->RotationRate = FRotator(0.f, FMath::Lerp(900.f, 300.f, SpeedAlpha), 0.f);
	if (bFacingStance)
	{
		FVector ToTarget = PK::Flat(FaceTarget - GetActorLocation());
		if (Stance == EPKStance::Shield)
		{
			ToTarget = -ToTarget; // back into the opponent
		}
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Want(0.f, ToTarget.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 9.f));
		}
	}

	if (Kick.bPending)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, KickYaw, 0.f), DeltaSeconds, 16.f));
	}

	// Roulette: a full turn over the ball, whatever the movement says.
	if (SkillSpinTimer > 0.f)
	{
		Move->bOrientRotationToMovement = false;
		AddActorWorldRotation(FRotator(0.f, SkillSpinRate * DeltaSeconds, 0.f));
		SkillSpinTimer -= DeltaSeconds;
	}

	if (IsStunned() || IsActionLockingMovement())
	{
		return;
	}

	FVector Input = MoveInput.GetClampedToMaxSize(1.f);
	// Dribbling: when the ball is not on the line the stick asks for, the body first goes to the ball
	// (Active-Touch-style path to the ball); the next touch then takes it the new way.
	if (bHasBall && DribbleSteerWeight > 0.f && !Input.IsNearlyZero())
	{
		Input = FMath::Lerp(Input.GetSafeNormal(), DribbleSteer, DribbleSteerWeight).GetSafeNormal() * Input.Size();
	}

	// Plant and cut: a hard change of direction at pace bleeds speed first.
	if (!Input.IsNearlyZero() && Speed > 450.f && Stance == EPKStance::Normal)
	{
		const float Dot = FVector::DotProduct(Vel / Speed, Input.GetSafeNormal());
		if (Dot < -0.1f)
		{
			Move->Velocity *= FMath::Max(0.f, 1.f - 4.f * DeltaSeconds);
		}
	}

	AddMovementInput(Input, 1.f);
}

bool APKFootballer::IsSprinting() const
{
	return bSprint && Stamina > 0.06f && Stance == EPKStance::Normal && PK::Flat(GetVelocity()).Size() > PK::JogSpeed * 0.95f;
}

void APKFootballer::SetHumanControlled(bool bInHuman)
{
	bHuman = bInHuman;
	Marker->SetVisibility(bInHuman);
	HumanInput = FVector::ZeroVector; // a newly selected player starts with no stale stick input
	if (!bInHuman)
	{
		MoveInput = FVector::ZeroVector;
		bSprint = false;
		Stance = EPKStance::Normal;
	}
}

void APKFootballer::Teleport2D(const FVector& Location, float Yaw)
{
	SetActorLocationAndRotation(FVector(Location.X, Location.Y, CapsuleHalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	MoveInput = FVector::ZeroVector;
	StopAction();
	FallbackKickTimer = 0.f;
}

FVector APKFootballer::GetFlatLocation() const
{
	return PK::Flat(GetActorLocation());
}

FVector APKFootballer::GetFacing() const
{
	return PK::Flat(GetActorForwardVector()).GetSafeNormal();
}

FVector APKFootballer::GetFootPoint() const
{
	return GetFlatLocation() + GetFacing() * 45.f;
}

FVector APKFootballer::GetHoldPoint() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->DoesSocketExist(TEXT("hand_l")) && Body->DoesSocketExist(TEXT("hand_r")))
	{
		return (Body->GetSocketLocation(TEXT("hand_l")) + Body->GetSocketLocation(TEXT("hand_r"))) * 0.5f;
	}
	return GetActorLocation() + GetFacing() * 30.f + FVector(0.f, 0.f, 20.f);
}

// ---------------------------------------------------------------------------------------------
// Kicks, tackles, keeper
// ---------------------------------------------------------------------------------------------

float APKFootballer::EstimateLaunchSpeed(const FPKKickRequest& Request) const
{
	const float D = FMath::Max(FVector::Dist2D(GetActorLocation(), Request.Target), 1.f);
	switch (Request.Type)
	{
	case EPKKickType::Shot:
	{
		if (Request.ShotType == EPKShotType::Chip)
		{
			return 900.f + 500.f * Request.Power;
		}
		const float Technique = Request.ShotType == EPKShotType::Power ? 1.22f : (Request.ShotType == EPKShotType::Finesse ? 0.85f : (Request.ShotType == EPKShotType::Low ? 1.05f : 1.f));
		const float Style = 1.f + (Request.ShotType == EPKShotType::Power ? 0.1f : 0.12f) * StyleLevel(PKTrait::PowerShot);
		return PK::CapShotSpeed((2000.f + 1900.f * Request.Power) * FMath::Lerp(0.9f, 1.05f, Attr.Shooting) * Technique * Style);
	}
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
	{
		const float T = FMath::Clamp(D / 1300.f, 0.9f, 2.1f);
		const float H = D / T * 0.82f;
		const float V = 0.5f * PK::Gravity * T;
		return FMath::Sqrt(H * H + V * V) * (Request.Type == EPKKickType::Clearance ? 1.15f : 1.f);
	}
	default:
	{
		const float Speed = Request.bPrecision ? FMath::Sqrt(300.f * 300.f + 2.f * PK::RollDecel * D) * 1.04f : FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);
		return Speed * (Request.Type == EPKKickType::Through ? 1.1f : 1.f);
	}
	}
}

float APKFootballer::FitKickRate(EPKAction Clip, float LaunchSpeed)
{
	const FPKActionClip& Info = UPKAnimInstance::GetActionClip(Clip);
	if (!Info.IsValid() || Info.ContactSpeed < 50.f)
	{
		return 1.f;
	}
	// Ball leaves at about 1.3x the foot speed off the side foot, 1.45x off the laces, 1.6x for a full volley.
	const bool bSideFoot = Clip == EPKAction::Pass || Clip == EPKAction::PassLeft;
	const float Ratio = bSideFoot ? 1.3f : (Clip == EPKAction::Volley ? 1.6f : 1.45f);
	const float MaxRate = bSideFoot ? 1.6f : 1.65f;
	return FMath::Clamp(LaunchSpeed / (Ratio * Info.ContactSpeed), 0.7f, MaxRate);
}

EPKAction APKFootballer::KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, float LaunchSpeed) const
{
	if (bThrowIn)
	{
		return EPKAction::ThrowIn;
	}
	if (bFromHands)
	{
		if (Request.Type == EPKKickType::Ground || Request.Type == EPKKickType::Through)
		{
			return FVector::Dist2D(GetActorLocation(), Request.Target) > 1800.f ? EPKAction::GKThrow : EPKAction::GKRoll;
		}
		return EPKAction::GKPunt;
	}

	const float Speed = PK::Flat(GetVelocity()).Size();
	const FVector ToTarget = PK::Flat(Request.Target - GetActorLocation()).GetSafeNormal();
	const float Side = FVector::DotProduct(ToTarget, PK::Flat(GetActorRightVector()));
	// The clip has to fit how hard the ball is struck: a side-foot pass can't send it 30 m, a full strike looks
	// wrong on a 10 m pass. The penalty-kick strike tops out around 2200 cm/s; harder than that is the running strike.
	switch (Request.Type)
	{
	case EPKKickType::Shot:
		if (Speed > 300.f || LaunchSpeed > 2200.f || (Speed > 150.f && FMath::FRand() < 0.5f))
		{
			return EPKAction::RunningShot;
		}
		return EPKAction::Shot;
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
		return Speed > 330.f || LaunchSpeed > 2200.f ? EPKAction::RunningShot : EPKAction::Shot;
	default:
		if (LaunchSpeed > 1150.f || (Speed > 450.f && LaunchSpeed > 1000.f))
		{
			// Driven pass / pass on the run: struck with the laces.
			return Speed > 330.f || LaunchSpeed > 1700.f ? EPKAction::RunningShot : EPKAction::Shot;
		}
		// Standing: strike with the foot on the side the ball is going (left foot for passes to the left).
		if (Side < -0.25f)
		{
			return EPKAction::PassLeft;
		}
		if (Side > 0.25f)
		{
			return EPKAction::Pass;
		}
		return FMath::FRand() < 0.35f ? EPKAction::PassLeft : EPKAction::Pass;
	}
}

bool APKFootballer::StartKick(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, bool bAirborneBall)
{
	const bool bFromHold = CurrentAction == EPKAction::GKCatch || CurrentAction == EPKAction::GKCatchHigh || CurrentAction == EPKAction::GKCatchLow
		|| CurrentAction == EPKAction::GKScoop || CurrentAction == EPKAction::GKHold || CurrentAction == EPKAction::ThrowHold;
	if (IsKicking() || IsStunned() || (IsInAction() && !bFromHold))
	{
		return false;
	}

	float Launch = EstimateLaunchSpeed(Request);
	EPKAction Clip = KickAction(Request, bFromHands, bThrowIn, Launch);
	if (bAirborneBall && Request.Type == EPKKickType::Shot && !bFromHands)
	{
		Clip = EPKAction::Volley; // overhead / scissor volley at a dropping ball
		Launch = PK::CapShotSpeed(Launch * 0.85f);
	}
	bKickLeftFoot = Clip == EPKAction::PassLeft;

	// Face the target before the strike: the shorter the wind-up, the more of the turn is done up front.
	const FVector Dir = PK::Flat(Request.Target - GetActorLocation());
	if (!Dir.IsNearlyZero())
	{
		KickYaw = Dir.Rotation().Yaw;
		const FPKActionClip& Info = UPKAnimInstance::GetActionClip(Clip);
		const float WindUp = (Info.IsValid() ? Info.Contact - Info.Start : 0.2f) / FMath::Max(FitKickRate(Clip, Launch), 0.5f);
		const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, KickYaw);
		const float MaxLeft = WindUp < 0.25f ? 0.f : 35.f; // what the interpolation finishes during the wind-up
		if (FMath::Abs(Delta) > MaxLeft)
		{
			SetActorRotation(FRotator(0.f, KickYaw - FMath::Sign(Delta) * MaxLeft, 0.f));
		}
	}

	// Tempo: the swing fits the ball's speed (a little variety on top). The power shot's longer wind-up and the quick
	// release's short one still show, within what the swing can do.
	KickClip = Clip;
	const bool bHandsOrThrow = bFromHands || bThrowIn;
	const float Fit = bHandsOrThrow ? 1.f : FitKickRate(Clip, Launch);
	const float Tempo = FMath::Lerp(1.f, FMath::Clamp(Request.ActionRate, 0.5f, 1.6f), 0.5f);
	const float Rate = bHandsOrThrow ? FMath::FRandRange(0.92f, 1.1f) * FMath::Clamp(Request.ActionRate, 0.5f, 1.6f) : FMath::Clamp(Fit * Tempo * FMath::FRandRange(0.97f, 1.03f), 0.65f, 1.7f);
	if (!PlayAction(Clip, EPKAction::None, Rate))
	{
		KickClip = EPKAction::None;
		FallbackKickTimer = FallbackKickDuration;
	}
	else if (!bHandsOrThrow)
	{
		// Keep the wind-up as long as it was (the power shot's longer, the quick release's shorter): a faster swing
		// starts further back in the clip's run-up instead of reaching the ball sooner.
		const FPKActionClip& Info = UPKAnimInstance::GetActionClip(Clip);
		const float WindUpSeconds = (Info.Contact - Info.Start) / FMath::Clamp(Request.ActionRate, 0.5f, 1.6f);
		ActionTime = FMath::Max(0.f, Info.Contact - WindUpSeconds * ActionRate);
		ActionEntry = ActionTime;
	}
	Kick = Request;
	Kick.bPending = true;
	return true;
}

FVector APKFootballer::GetKickFootPoint() const
{
	if (KickClip != EPKAction::None && UPKAnimInstance::GetActionClip(KickClip).IsValid() && !UPKAnimInstance::GetActionClip(KickClip).ContactPos.IsNearlyZero())
	{
		return PK::Flat(GetKickContactPoint());
	}
	const FVector Right = PK::Flat(GetActorRightVector()).GetSafeNormal();
	return GetFlatLocation() + GetFacing() * 38.f + Right * (bKickLeftFoot ? -14.f : 14.f);
}

FVector APKFootballer::GetActionContactPoint(EPKAction Action, float Yaw) const
{
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	const USkeletalMeshComponent* Body = GetMesh();
	if (!Clip.IsValid() || !Body)
	{
		return GetFlatLocation() + GetFacing() * 40.f + FVector(0.f, 0.f, PK::BallRadius);
	}
	const FVector Local = Body->GetRelativeTransform().TransformPosition(Clip.ContactPos);
	return GetActorLocation() + FRotator(0.f, Yaw, 0.f).RotateVector(Local);
}

FVector APKFootballer::GetKickContactPoint() const
{
	// The foot meets the back of the ball: the ball centre sits a little ahead of the foot bone.
	const FVector Foot = GetActionContactPoint(KickClip, KickYaw);
	const FVector Dir = FRotator(0.f, KickYaw, 0.f).Vector();
	return FVector(Foot.X, Foot.Y, 0.f) + Dir * 6.f + FVector(0.f, 0.f, FMath::Max(PK::BallRadius, Foot.Z));
}

FVector APKFootballer::GetContactBoneLocation(EPKAction Action) const
{
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body && !Clip.ContactBone.IsNone() && Body->DoesSocketExist(Clip.ContactBone))
	{
		return Body->GetSocketLocation(Clip.ContactBone);
	}
	return GetActorLocation();
}

float APKFootballer::TimeToContact() const
{
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(CurrentAction);
	if (CurrentAction == EPKAction::None || !Clip.IsValid())
	{
		return 0.f;
	}
	return (Clip.Contact - ActionTime) / FMath::Max(ActionRate, 0.1f);
}

bool APKFootballer::IsKicking() const
{
	return Kick.bPending || FallbackKickTimer > 0.f;
}

bool APKFootballer::ReadyToReleaseKick() const
{
	if (!Kick.bPending)
	{
		return false;
	}
	if (KickClip != EPKAction::None)
	{
		return CurrentAction == KickClip && ActionTime >= UPKAnimInstance::GetActionClip(KickClip).Contact;
	}
	return FallbackKickTimer <= FallbackKickDuration - FallbackKickRelease;
}

FPKKickRequest APKFootballer::ConsumeKick()
{
	FPKKickRequest Out = Kick;
	Kick.bPending = false;
	return Out;
}

float APKFootballer::GetKickAlpha() const
{
	return FallbackKickTimer > 0.f ? 1.f - FallbackKickTimer / FallbackKickDuration : -1.f;
}

void APKFootballer::PlayTackle()
{
	if (IsInAction() || IsStunned())
	{
		return;
	}
	if (PlayAction(EPKAction::Tackle))
	{
		GetCharacterMovement()->Velocity = GetFacing() * 250.f;
	}
	else
	{
		LungeTimer = 0.35f;
	}
}

void APKFootballer::PlaySlideTackle()
{
	if (IsInAction() || IsStunned())
	{
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	const float Pick = FMath::FRand();
	const EPKAction Slide = Pick < 0.5f ? EPKAction::SlideTackle : (Pick < 0.8f ? EPKAction::SlideTackleB : EPKAction::SlideTackleC);
	if (!PlayAction(Slide, EPKAction::None, FMath::FRandRange(0.95f, 1.08f)) && !PlayAction(EPKAction::SlideTackle))
	{
		LungeTimer = 0.5f;
	}
}

void APKFootballer::PlayTrip()
{
	StopAction();
	GetCharacterMovement()->StopMovementImmediately();
	if (!PlayAction(EPKAction::Trip, EPKAction::Fallen))
	{
		Stun(1.5f);
	}
}

void APKFootballer::Dive(float Direction, float SecondsToContact)
{
	if (CurrentAction == EPKAction::GKDiveLeft || CurrentAction == EPKAction::GKDiveRight)
	{
		return;
	}
	StopAction();
	GetCharacterMovement()->StopMovementImmediately();
	const EPKAction DiveAction = Direction >= 0.f ? EPKAction::GKDiveRight : EPKAction::GKDiveLeft;
	if (!(SecondsToContact >= 0.f ? PlayActionTimed(DiveAction, SecondsToContact, 2.f) : PlayAction(DiveAction)))
	{
		Stun(0.8f);
	}
}
