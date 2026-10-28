#include "PKPitch.h"
#include "PKTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"

// Layout of the seating bowl: must match Tools/art/gen_stadium.py, which builds SM_PK_Bowl from the same numbers.
namespace PKStadium
{
	// The 3D fans' resting loop (baked frames: idle 0-179, arms up 180-220, fist pump 221-261): most idle, some chant
	// with a slow fist pump, a few hold their arms up, so the front rows are not one pose.
	inline FVector2f CalmFrames(float Seed)
	{
		return Seed < 0.16f ? FVector2f(221.f, 261.f) : (Seed > 0.94f ? FVector2f(180.f, 220.f) : FVector2f(0.f, 179.f));
	}
	constexpr float HalfX = 6150.f;          // bowl front: rounded rectangle half extents (cm)
	constexpr float HalfY = 4300.f;
	constexpr float CornerR = 1700.f;
	constexpr float RowDepth = 80.f;
	constexpr float Rise = 50.f;
	constexpr int32 Rows = 36;               // single tier (ends, corners, near side)
	constexpr float FrontH = 150.f;
	constexpr int32 LowerRows = 16;          // main stand (far side, +Y): lower tier
	constexpr float BoxH = 380.f;
	constexpr int32 UpperRows = 20;
	constexpr float UpperRise = 62.f;
	constexpr float UpperOverhang = 420.f;
	constexpr float UpperD0 = LowerRows * RowDepth - UpperOverhang;
	constexpr float UpperZ0 = FrontH + LowerRows * Rise + BoxH + 60.f;
	constexpr float SeatPitch = 55.f;
	constexpr float AisleEvery = 1400.f;

	// A point at arc length S along the bowl loop offset outwards by D. Segments, counter-clockwise from the +X end:
	// corner, main (far) side, corner, -X end, corner, near side, corner, +X end. Returns the segment index.
	int32 Sample(float D, float S, FVector2D& OutPos, FVector2D& OutNormal, float* OutFrontS = nullptr)
	{
		const float CX = HalfX - CornerR;
		const float CY = HalfY - CornerR;
		const float R = CornerR + D;
		const float Arc = R * UE_HALF_PI;
		const float Lengths[8] = { Arc, 2.f * CX, Arc, 2.f * CY, Arc, 2.f * CX, Arc, 2.f * CY };
		const FVector2D Centres[4] = { FVector2D(CX, CY), FVector2D(-CX, CY), FVector2D(-CX, -CY), FVector2D(CX, -CY) };
		float Front = 0.f; // the same position measured along the bowl front (the seat material's U, in cm)
		for (int32 Seg = 0; Seg < 8; ++Seg)
		{
			if (S > Lengths[Seg] && Seg < 7)
			{
				S -= Lengths[Seg];
				Front += Seg % 2 == 0 ? CornerR * UE_HALF_PI : Lengths[Seg];
				continue;
			}
			if (OutFrontS)
			{
				*OutFrontS = Front + (Seg % 2 == 0 ? S / R * CornerR : S);
			}
			const int32 Corner = Seg / 2;
			if (Seg % 2 == 0)
			{
				const float A = UE_HALF_PI * Corner + S / R;
				OutNormal = FVector2D(FMath::Cos(A), FMath::Sin(A));
				OutPos = Centres[Corner] + OutNormal * R;
			}
			else
			{
				const float A = UE_HALF_PI * (Corner + 1);
				OutNormal = FVector2D(FMath::Cos(A), FMath::Sin(A));
				const FVector2D Dir(-OutNormal.Y, OutNormal.X);
				OutPos = Centres[Corner] + OutNormal * R + Dir * S;
			}
			return Seg;
		}
		return 7;
	}

	float LoopLength(float D)
	{
		return 4.f * (HalfX - CornerR) + 4.f * (HalfY - CornerR) + 2.f * UE_PI * (CornerR + D);
	}

	// Stairways and vomitory tunnels: painted by the seat material (M_PK_Prop kinds 2 and 6), kept free of fans here.
	bool IsAisle(float FrontS) { return FMath::Fmod(FrontS, AisleEvery) < 75.f; }
	bool IsVomitory(float FrontS, int32 Row) { return Row >= 10 && Row <= 13 && FMath::Fmod(FrontS + 700.f, 2800.f) < 300.f; }

	constexpr int32 MainSegment = 1;
	constexpr int32 AwaySegment = 3;  // -X end
	constexpr int32 HomeEndSegment = 7;
}

APKPitch::APKPitch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bowl(TEXT("/Game/PitchKings/Stadium/SM_PK_Bowl.SM_PK_Bowl"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Roof(TEXT("/Game/PitchKings/Stadium/SM_PK_Roof.SM_PK_Roof"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Goal(TEXT("/Game/PitchKings/Stadium/SM_PK_Goal.SM_PK_Goal"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Board(TEXT("/Game/PitchKings/Stadium/SM_PK_Board.SM_PK_Board"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Card(TEXT("/Game/PitchKings/Stadium/SM_PK_CrowdCard.SM_PK_CrowdCard"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Flag(TEXT("/Game/PitchKings/Stadium/SM_PK_CornerFlag.SM_PK_CornerFlag"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Dugout(TEXT("/Game/PitchKings/Stadium/SM_PK_Dugout.SM_PK_Dugout"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Pitch(TEXT("/Game/PitchKings/Materials/M_PK_Pitch.M_PK_Pitch"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/PitchKings/Materials/MI_PK_Ground.MI_PK_Ground"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Tree(TEXT("/Game/PitchKings/Stadium/SM_PK_Tree.SM_PK_Tree"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Screen(TEXT("/Game/PitchKings/Stadium/SM_PK_Screen.SM_PK_Screen"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Car(TEXT("/Game/PitchKings/Stadium/SM_PK_Car.SM_PK_Car"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Banner(TEXT("/Game/PitchKings/Stadium/SM_PK_Banner.SM_PK_Banner"));
	for (int32 i = 0; i < 12; ++i)
	{
		const FString Path = FString::Printf(TEXT("/Game/PitchKings/Crowd3D/SM_Fan_%02d.SM_Fan_%02d"), i, i);
		if (UStaticMesh* Fan = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			FanMeshes.Add(Fan);
		}
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Building(TEXT("/Game/PitchKings/Materials/MI_PK_Building.MI_PK_Building"));
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> Params(TEXT("/Game/PitchKings/Materials/MPC_PK_Stadium.MPC_PK_Stadium"));

	CubeMesh = Cube.Object;
	BowlMesh = Bowl.Object;
	RoofMesh = Roof.Object;
	GoalMesh = Goal.Object;
	BoardMesh = Board.Object;
	CardMesh = Card.Object;
	FlagMesh = Flag.Object;
	DugoutMesh = Dugout.Object;
	PitchMaterial = Pitch.Object;
	GroundMaterial = Ground.Object;
	StadiumParams = Params.Object;
	TreeMesh = Tree.Object;
	ScreenMesh = Screen.Object;
	CarMesh = Car.Object;
	BannerMesh = Banner.Object;
	BuildingMaterial = Building.Object;
}

void APKPitch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildAll();
}

void APKPitch::BeginPlay()
{
	Super::BeginPlay();
	// Parts made by the construction script are not saved into cooked (packaged) maps, and the
	// construction script does not run again when a packaged game loads the level: build them here.
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	if (Parts.Num() == 0)
	{
		BuildAll();
	}
	Excite = ExciteTarget = FVector2D(0.06f, 0.06f);
	PushCrowdState();
}

void APKPitch::BuildAll()
{
	// Grass (top at z = 0) out to the front of the stands; the markings are drawn by the pitch material.
	AddShape(CubeMesh, FVector(0.f, 0.f, -50.f), FVector(2.f * PKStadium::HalfX + 400.f, 2.f * PKStadium::HalfY + 400.f, 100.f), PitchMaterial, true, false);
	AddShape(CubeMesh, FVector(0.f, 0.f, -60.f), FVector(2000000.f, 2000000.f, 100.f), GroundMaterial, true, false); // 20 km: no horizon edge

	if (BowlMesh)
	{
		AddMesh(BowlMesh, FVector::ZeroVector, FRotator::ZeroRotator, true);
	}
	if (RoofMesh && bBuildStands)
	{
		AddMesh(RoofMesh, FVector::ZeroVector, FRotator::ZeroRotator, true);
	}
	BuildGoals();
	BuildBoards();

	if (FlagMesh)
	{
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				AddMesh(FlagMesh, FVector(SX * PK::HalfLength, SY * PK::HalfWidth, 0.f), FRotator(0.f, SX > 0.f ? 180.f : 0.f, 0.f), true);
			}
		}
	}
	if (DugoutMesh)
	{
		// Sunk into the front of the main stand, behind the boards, either side of halfway.
		for (const float SX : { -1.f, 1.f })
		{
			AddMesh(DugoutMesh, FVector(SX * 900.f, PKStadium::HalfY - 205.f, 0.f), FRotator::ZeroRotator, true);
		}
	}
	if (bBuildStands)
	{
		BuildCrowd();
		BuildSurroundings();
		if (BannerMesh)
		{
			// Supporters' banners over the front walls of both ends: the home end (+X) and the away section (-X, -Y).
			struct FBanner { float X, Y; float Row; float Team; };
			const FBanner Banners[] = { { 1.f, -900.f, 6.f, 0.f }, { 1.f, 1300.f, 7.f, 0.f }, { -1.f, -1500.f, 7.f, 1.f }, { -1.f, 900.f, 6.f, 0.f } };
			for (const FBanner& B : Banners)
			{
				UStaticMeshComponent* Comp = AddMesh(BannerMesh, FVector(B.X * (PKStadium::HalfX - 4.f), B.Y, 158.f), FRotator(0.f, B.X > 0.f ? -90.f : 90.f, 0.f), false);
				if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
				{
					MID->SetScalarParameterValue(TEXT("Row"), B.Row);
					MID->SetScalarParameterValue(TEXT("Team"), B.Team);
				}
			}
		}
		if (ScreenMesh)
		{
			// Video screens hung from the roof truss in the two far corners, facing the centre spot.
			for (const float SX : { -1.f, 1.f })
			{
				FVector2D P, N;
				// Corner 0 (+X+Y) is the first segment of the loop, corner 1 (-X+Y) the third: halfway round each.
				const float D = 700.f;
				const float R = PKStadium::CornerR + D;
				const float S = SX > 0.f ? R * UE_HALF_PI * 0.5f : R * UE_HALF_PI + 2.f * (PKStadium::HalfX - PKStadium::CornerR) + R * UE_HALF_PI * 0.5f;
				PKStadium::Sample(D, S, P, N);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-N.X, N.Y));
				AddMesh(ScreenMesh, FVector(P.X, P.Y, 2650.f), FRotator(0.f, Yaw, 0.f), true);
			}
		}
	}
}

void APKPitch::BuildSurroundings()
{
	// The world outside the bowl is a single backdrop: a photographed city skyline (Poly Haven CC0, cut to the
	// horizon band by Tools/art/make_skyline.py) on a far cylinder, drawn above the roof line only. The cameras
	// never leave the bowl, so the old modelled city, car parks and trees were never needed.
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* SkylineMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PitchKings/Stadium/M_PK_Skyline.M_PK_Skyline"));
	if (!Cylinder || !SkylineMat)
	{
		return;
	}
	UStaticMeshComponent* Skyline = NewObject<UStaticMeshComponent>(this, TEXT("Skyline"));
	Skyline->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Skyline->SetStaticMesh(Cylinder);
	Skyline->SetupAttachment(GetRootComponent());
	// basic cylinder: radius 50, height 100, centred -> radius 600 m, from the ground to 450 m up
	Skyline->SetRelativeLocation(FVector(0.f, 0.f, 22500.f));
	Skyline->SetRelativeScale3D(FVector(1200.f, 1200.f, 450.f));
	Skyline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skyline->SetCastShadow(false);
	Skyline->SetAffectDistanceFieldLighting(false);
	Skyline->bReceivesDecals = false;
	Skyline->bVisibleInRealTimeSkyCaptures = false; // not part of the sky light: it would darken the pitch
	Skyline->bVisibleInReflectionCaptures = false;
	Skyline->bVisibleInRayTracing = false;
	Skyline->bAffectDynamicIndirectLighting = false;
	Skyline->bAffectIndirectLightingWhileHidden = false;
	Skyline->SetCastShadow(false);
	Skyline->SetMaterial(0, SkylineMat);
	Skyline->RegisterComponent();
}

UStaticMeshComponent* APKPitch::AddShape(UStaticMesh* Mesh, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide, bool bShadow, const FRotator& Rotation)
{
	// A unit mesh (100 cm) scaled to Size, centred on Center.
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Comp->SetStaticMesh(Mesh);
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocationAndRotation(Center, Rotation);
	Comp->SetRelativeScale3D(Size / 100.f);
	Comp->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Comp->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Comp->SetCastShadow(bShadow);
	if (Material)
	{
		Comp->SetMaterial(0, Material);
	}
	Comp->RegisterComponent();
	return Comp;
}

UStaticMeshComponent* APKPitch::AddMesh(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, bool bShadow)
{
	// A generated stadium mesh at its authored size (materials come with the mesh).
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Comp->SetStaticMesh(Mesh);
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocationAndRotation(Location, Rotation);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(bShadow);
	Comp->RegisterComponent();
	return Comp;
}

void APKPitch::AddBlocker(const FVector& Center, const FVector& Size)
{
	UBoxComponent* Box = NewObject<UBoxComponent>(this);
	Box->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Box->SetupAttachment(GetRootComponent());
	Box->SetBoxExtent(Size * 0.5f);
	Box->SetRelativeLocation(Center);
	Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Box->SetHiddenInGame(true);
	Box->RegisterComponent();
}

void APKPitch::BuildGoals()
{
	for (const float Side : { -1.f, 1.f })
	{
		if (GoalMesh)
		{
			// The mesh's mouth faces -X with the net behind it: turn it round for the -X goal.
			AddMesh(GoalMesh, FVector(Side * PK::HalfLength, 0.f, 0.f), FRotator(0.f, Side > 0.f ? 0.f : 180.f, 0.f), true);
		}
		// Posts block players.
		for (const float SY : { -1.f, 1.f })
		{
			UCapsuleComponent* Post = NewObject<UCapsuleComponent>(this);
			Post->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			Post->SetupAttachment(GetRootComponent());
			Post->SetCapsuleSize(PK::PostRadius, PK::GoalHeight * 0.5f + PK::PostRadius);
			Post->SetRelativeLocation(FVector(Side * PK::HalfLength, SY * PK::GoalHalfWidth, PK::GoalHeight * 0.5f));
			Post->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Post->SetHiddenInGame(true);
			Post->RegisterComponent();
		}
	}
}

void APKPitch::BuildBoards()
{
	const float BoardX = PK::HalfLength + 550.f;
	const float BoardY = PK::HalfWidth + 500.f;
	constexpr float Panel = 600.f;
	if (BoardMesh)
	{
		UInstancedStaticMeshComponent* Boards = NewObject<UInstancedStaticMeshComponent>(this);
		Boards->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Boards->SetStaticMesh(BoardMesh);
		Boards->SetupAttachment(GetRootComponent());
		Boards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Boards->SetCastShadow(true);
		Boards->NumCustomDataFloats = 2;
		Boards->RegisterComponent();

		int32 Index = 0;
		// A run of boards from A to B (screens facing Yaw): groups of three show one ad as a continuous ribbon.
		auto Run = [&](const FVector2D& A, const FVector2D& B, float Yaw)
		{
			const float Len = FVector2D::Distance(A, B);
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Len / Panel));
			const float Scale = Len / (Count * Panel);
			for (int32 i = 0; i < Count; ++i)
			{
				const FVector2D P = FMath::Lerp(A, B, (i + 0.5f) / Count);
				const int32 Id = Boards->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 0.f), FVector(Scale * 0.995f, 1.f, 1.f)));
				Boards->SetCustomDataValue(Id, 0, float((Index / 3) % 6), false);
				Boards->SetCustomDataValue(Id, 1, float(Index % 3), false);
				++Index;
			}
		};
		// Long runs, end runs and an angled board across each corner, so the ring of boards is closed.
		const float Cut = 450.f; // both ends of the angled corner board stay inside the curved front wall
		Run(FVector2D(-BoardX + Cut, BoardY), FVector2D(BoardX - Cut, BoardY), 0.f);
		Run(FVector2D(BoardX - Cut, -BoardY), FVector2D(-BoardX + Cut, -BoardY), 180.f);
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				const FVector2D A(SX * (BoardX - Cut), SY * BoardY), B(SX * BoardX, SY * (BoardY - Cut));
				const FVector2D Out = FVector2D(SX, SY).GetSafeNormal();
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Out.X, Out.Y));
				const FVector2D P = (A + B) * 0.5f;
				const int32 Id = Boards->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 0.f), FVector(FVector2D::Distance(A, B) / Panel, 1.f, 1.f)));
				Boards->SetCustomDataValue(Id, 0, float((Index / 3) % 6), false); // full-length board: normal logo
				Boards->SetCustomDataValue(Id, 1, 0.f, false);
				++Index;
			}
		}
		for (const float SX : { -1.f, 1.f })
		{
			const float Yaw = SX > 0.f ? -90.f : 90.f;
			// one run behind each goal, well clear of the net (2.1 m deep)
			Run(FVector2D(SX * BoardX, -BoardY + 480.f), FVector2D(SX * BoardX, BoardY - 480.f), Yaw); // stops short of the corner boards
		}
		Boards->MarkRenderStateDirty();
	}

	// The boards stop players (invisible boxes along each run).
	AddBlocker(FVector(0.f, BoardY + 20.f, 50.f), FVector(2.f * BoardX, 40.f, 100.f));
	AddBlocker(FVector(0.f, -BoardY - 20.f, 50.f), FVector(2.f * BoardX, 40.f, 100.f));
	const float Gap = PK::GoalHalfWidth + 300.f;
	for (const float SX : { -1.f, 1.f })
	{
		const float Len = BoardY - Gap;
		AddBlocker(FVector(SX * (BoardX + 20.f), -(Gap + Len * 0.5f), 50.f), FVector(40.f, Len, 100.f));
		AddBlocker(FVector(SX * (BoardX + 20.f), Gap + Len * 0.5f, 50.f), FVector(40.f, Len, 100.f));
	}
}

void APKPitch::BuildCrowd()
{
	if (!CardMesh)
	{
		return;
	}
	Crowd = NewObject<UInstancedStaticMeshComponent>(this);
	Crowd->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Crowd->SetStaticMesh(CardMesh);
	Crowd->SetupAttachment(GetRootComponent());
	Crowd->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crowd->SetCastShadow(false);
	Crowd->bAffectDistanceFieldLighting = false;
	Crowd->SetMobility(EComponentMobility::Static);
	Crowd->NumCustomDataFloats = 3;
	Crowd->RegisterComponent();

	FRandomStream Rand(1904);
	TArray<FTransform> Transforms;
	TArray<FVector3f> Data;

	// Front rows get 3D fans (fewer rows at lower graphics quality; none at Low).
	static IConsoleVariable* ViewDistance = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.ViewDistanceQuality"));
	const int32 Quality = ViewDistance ? ViewDistance->GetInt() : 3;
	const int32 Rows3D = FanMeshes.Num() == 0 ? 0 : (Quality <= 0 ? 0 : (Quality == 1 ? 6 : 15)); // the cards start further back (out of close-up frames)
	TArray<TArray<FTransform>> FanTransforms;
	TArray<TArray<TArray<float>>> FanData;
	FanTransforms.SetNum(FanMeshes.Num());
	FanData.SetNum(FanMeshes.Num());
	Transforms.Reserve(40000);
	Data.Reserve(40000);

	// One row of fans at offset D (cm behind the bowl front), standing on a tread at height Z.
	auto Row = [&](float D, float Z, bool bUpper, int32 RowIndex)
	{
		const float Length = PKStadium::LoopLength(D);
		for (float S = Rand.FRandRange(0.f, 20.f); S < Length; S += PKStadium::SeatPitch)
		{
			FVector2D P, N;
			float FrontS = 0.f;
			const int32 Seg = PKStadium::Sample(D, S, P, N, &FrontS);
			const bool bMain = Seg == PKStadium::MainSegment;
			if (bUpper != bMain && (bUpper || D >= PKStadium::LowerRows * PKStadium::RowDepth))
			{
				continue; // upper tier only on the main stand; the main stand has no single-tier rows above its lower tier
			}
			if (PKStadium::IsAisle(FrontS) || (!bUpper && PKStadium::IsVomitory(FrontS, RowIndex)) || Rand.FRand() > 0.9f)
			{
				continue; // stairways, tunnels and empty seats
			}
			float Team = 0.f;
			// the 3D fans all stand: the card rows just behind them stand too, or a band of upright figures seemed to
			// hover over seated rows where the two meet
			float Standing = (RowIndex < Rows3D + 6 || Rand.FRand() < 0.12f) ? 1.f : 0.f;
			if (Seg == PKStadium::AwaySegment && P.Y < 300.f)
			{
				Team = 1.f;
				Standing = 1.f;
			}
			else if (Seg == PKStadium::HomeEndSegment)
			{
				Standing = Rand.FRand() < 0.7f ? 1.f : 0.f; // the home end stands
			}
			else if (Rand.FRand() < 0.07f)
			{
				Team = 2.f;
			}
			const float Size = Rand.FRandRange(0.92f, 1.07f);
			const float Tall = Standing > 0.5f ? 1.22f : 1.f;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-N.X, N.Y)) + Rand.FRandRange(-6.f, 6.f);
			const FVector2D Jitter = N * Rand.FRandRange(-6.f, 6.f);
			const FVector Loc(P.X + Jitter.X, P.Y + Jitter.Y, Z + (Standing > 0.5f ? 2.f : 12.f));
			if (!bUpper && RowIndex < Rows3D)
			{
				// A real fan: the variant mesh faces +Y, so turn it round to face the pitch; everybody stands.
				const int32 V = Rand.RandRange(0, FanMeshes.Num() - 1);
				const float H = Rand.FRandRange(0.94f, 1.05f);
				FanTransforms[V].Add(FTransform(FRotator(0.f, Yaw + 180.f, 0.f), FVector(Loc.X, Loc.Y, Z), FVector(H)));
				const float Seed = Rand.FRand();
				const FVector2f Calm = PKStadium::CalmFrames(Seed);
				// chanting fans pump slowly, the rest idle at their own pace
				const float Rate = Calm.X > 0.f ? Rand.FRandRange(0.45f, 0.7f) : Rand.FRandRange(0.8f, 1.2f);
				FanData[V].Add({ (float)Rand.FRandRange(0.f, 6.f), Rate, Calm.X, Calm.Y, Team, Seed, (float)Rand.FRand() });
				continue;
			}
			Transforms.Add(FTransform(FRotator(0.f, Yaw, 0.f), Loc, FVector(Size, 1.f, Size * Tall)));
			Data.Add(FVector3f(Team, Rand.FRand(), Standing));
		}
	};
	for (int32 R = 0; R < PKStadium::Rows; ++R)
	{
		Row(R * PKStadium::RowDepth + 32.f, PKStadium::FrontH + (R + 1) * PKStadium::Rise, false, R);
	}
	for (int32 R = 0; R < PKStadium::UpperRows; ++R)
	{
		Row(PKStadium::UpperD0 + R * PKStadium::RowDepth + 32.f, PKStadium::UpperZ0 + 60.f + (R + 1) * PKStadium::UpperRise, true, 40 + R);
	}

	// (no pitch-side photographer cards: flat black shapes up close)
	Crowd->AddInstances(Transforms, false, false, false);

	int32 NumFans = 0;
	for (int32 V = 0; V < FanMeshes.Num(); ++V)
	{
		if (FanTransforms[V].Num() == 0)
		{
			continue;
		}
		// Hierarchical: clusters of fans are frustum-culled (the bowl is mostly off-screen from any camera).
		UHierarchicalInstancedStaticMeshComponent* Fans = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Fans->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Fans->SetStaticMesh(FanMeshes[V]);
		Fans->SetupAttachment(GetRootComponent());
		Fans->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fans->SetCastShadow(false);
		Fans->bAffectDistanceFieldLighting = false;
		Fans->NumCustomDataFloats = 7;
		Fans->RegisterComponent();
		Fans->AddInstances(FanTransforms[V], false, false, false);
		for (int32 i = 0; i < FanData[V].Num(); ++i)
		{
			Fans->SetCustomData(i, FanData[V][i], false);
		}
		Fans->BuildTreeIfOutdated(false, true);
		Fans->MarkRenderStateDirty();
		Fans3D.Add(Fans);
		NumFans += FanTransforms[V].Num();
	}
	UE_LOG(LogTemp, Log, TEXT("[Stadium] crowd: %d 3D fans in %d front rows (%d variants), %d cards"), NumFans, Rows3D, Fans3D.Num(), Transforms.Num());
	for (int32 i = 0; i < Data.Num(); ++i)
	{
		Crowd->SetCustomDataValue(i, 0, Data[i].X, false);
		Crowd->SetCustomDataValue(i, 1, Data[i].Y, false);
		Crowd->SetCustomDataValue(i, 2, Data[i].Z, false);
	}
	Crowd->MarkRenderStateDirty();
}

int32 APKPitch::GetCrowdCount() const
{
	return Crowd ? Crowd->GetInstanceCount() : 0;
}

void APKPitch::SetClubColours(const FLinearColor& HomeA, const FLinearColor& HomeB, const FLinearColor& AwayA, const FLinearColor& AwayB)
{
	if (!StadiumParams || !GetWorld())
	{
		return;
	}
	UKismetMaterialLibrary::SetVectorParameterValue(this, StadiumParams, TEXT("HomeA"), HomeA);
	UKismetMaterialLibrary::SetVectorParameterValue(this, StadiumParams, TEXT("HomeB"), HomeB);
	UKismetMaterialLibrary::SetVectorParameterValue(this, StadiumParams, TEXT("AwayA"), AwayA);
	UKismetMaterialLibrary::SetVectorParameterValue(this, StadiumParams, TEXT("AwayB"), AwayB);
}

void APKPitch::SetScoreboard(int32 Home, int32 Away, float ClockSeconds)
{
	const FIntVector Now(Home, Away, FMath::Max(0, FMath::CeilToInt(ClockSeconds)));
	if (Now == LastScoreboard || !StadiumParams || !GetWorld())
	{
		return;
	}
	LastScoreboard = Now;
	UKismetMaterialLibrary::SetScalarParameterValue(this, StadiumParams, TEXT("HomeScore"), (float)Now.X);
	UKismetMaterialLibrary::SetScalarParameterValue(this, StadiumParams, TEXT("AwayScore"), (float)Now.Y);
	UKismetMaterialLibrary::SetScalarParameterValue(this, StadiumParams, TEXT("Clock"), (float)Now.Z);
}

void APKPitch::Cheer(int32 Team, float Amount)
{
	if (Team < 0 || Team > 1)
	{
		return;
	}
	ExciteTarget[Team] = FMath::Max(ExciteTarget[Team], FMath::Clamp(Amount, 0.f, 1.f));
	ExciteHold[Team] = FMath::Max(ExciteHold[Team], 2.f + 5.f * Amount);
}

void APKPitch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	static const bool bForceCheer = FParse::Param(FCommandLine::Get(), TEXT("PKFansCheer")); // test: both ends celebrating
	if (bForceCheer)
	{
		Cheer(0, 1.f);
		Cheer(1, 1.f);
	}
	const FVector2D Before = Excite;
	for (int32 T = 0; T < 2; ++T)
	{
		ExciteHold[T] = FMath::Max(0.f, ExciteHold[T] - DeltaSeconds);
		if (ExciteHold[T] <= 0.f)
		{
			ExciteTarget[T] = FMath::FInterpTo(ExciteTarget[T], 0.06f, DeltaSeconds, 0.6f);
		}
		Excite[T] = FMath::FInterpTo(Excite[T], ExciteTarget[T], DeltaSeconds, 4.f);
	}
	if (!Excite.Equals(Before, 0.002f))
	{
		PushCrowdState();
	}
	UpdateFanAnimations();
}

void APKPitch::UpdateFanAnimations()
{
	// Fans of a team that is celebrating switch from the idle loop to one of the two cheers (baked frame ranges:
	// idle 0-179, arms up 180-220, fist pump 221-261), and back when it calms down.
	bool bChanged = false;
	bool Want[2];
	for (int32 T = 0; T < 2; ++T)
	{
		Want[T] = Excite[T] > (bFansCheering[T] ? 0.25f : 0.4f);
		bChanged |= Want[T] != bFansCheering[T];
	}
	if (!bChanged)
	{
		return;
	}
	for (UInstancedStaticMeshComponent* Fans : Fans3D)
	{
		if (!Fans)
		{
			continue;
		}
		const int32 Num = Fans->GetInstanceCount();
		const int32 Stride = Fans->NumCustomDataFloats;
		for (int32 i = 0; i < Num; ++i)
		{
			const int32 Team = FMath::RoundToInt(Fans->PerInstanceSMCustomData[i * Stride + 4]);
			if (Team > 1 ? Want[0] == bFansCheering[0] && Want[1] == bFansCheering[1] : Want[Team] == bFansCheering[Team])
			{
				continue;
			}
			const bool bCheer = Team > 1 ? (Want[0] || Want[1]) : Want[Team];
			const float Seed = Fans->PerInstanceSMCustomData[i * Stride + 5];
			const bool bPump = Seed > 0.6f;
			const FVector2f Calm = PKStadium::CalmFrames(Seed);
			Fans->SetCustomDataValue(i, 2, bCheer ? (bPump ? 221.f : 180.f) : Calm.X, false);
			Fans->SetCustomDataValue(i, 3, bCheer ? (bPump ? 261.f : 220.f) : Calm.Y, false);
		}
		Fans->MarkRenderStateDirty();
	}
	bFansCheering[0] = Want[0];
	bFansCheering[1] = Want[1];
	UE_LOG(LogTemp, Log, TEXT("[Stadium] fans cheering: home %d away %d"), Want[0] ? 1 : 0, Want[1] ? 1 : 0);
}

void APKPitch::PushCrowdState()
{
	if (StadiumParams && GetWorld())
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, StadiumParams, TEXT("Excite"), FLinearColor(Excite.X, Excite.Y, 0.f, 0.f));
	}
}
