#include "PKBall.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PKFootballer.h"
#include "PKTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float StepSeconds = 1.f / 120.f;
	constexpr float BounceRestitution = 0.62f;
	constexpr float PostRestitution = 0.6f;
	constexpr float SpinDecay = 1.2f;
}

APKBall::APKBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// smooth 64 x 48 sphere (Tools/editor_scripts/make_ball_mesh.py); the engine sphere is the fallback
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BallMesh(TEXT("/Game/PitchKings/Stadium/SM_PK_Ball.SM_PK_Ball"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/PitchKings/Materials/M_PK_Surface.M_PK_Surface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BallMat(TEXT("/Game/PitchKings/Materials/M_PK_Ball.M_PK_Ball"));
	BallMaterial = BallMat.Object;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetStaticMesh(BallMesh.Object ? BallMesh.Object : SphereMesh.Object);
	Mesh->SetRelativeScale3D(FVector(PK::BallRadius * 2.f / 100.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BallMaterial || Surface.Object)
	{
		Mesh->SetMaterial(0, BallMaterial ? BallMaterial.Get() : static_cast<UMaterialInterface*>(Surface.Object));
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMesh(TEXT("/Game/PitchKings/Stadium/SM_PK_Ring.SM_PK_Ring"));
	bThinRings = RingMesh.Object != nullptr;
	auto MakeRing = [&](const TCHAR* Name)
	{
		UStaticMeshComponent* Ring = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Ring->SetupAttachment(Root);
		Ring->SetUsingAbsoluteLocation(true);
		Ring->SetUsingAbsoluteRotation(true);
		Ring->SetUsingAbsoluteScale(true);
		Ring->SetStaticMesh(bThinRings ? RingMesh.Object : CylinderMesh.Object);
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring->SetCastShadow(false);
		Ring->SetVisibility(false);
		if (Surface.Object)
		{
			Ring->SetMaterial(0, Surface.Object);
		}
		return Ring;
	};
	ShadowRing = MakeRing(TEXT("ShadowRing"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShadowMat(TEXT("/Game/PitchKings/Materials/M_PK_BallShadow.M_PK_BallShadow"));
	ContactShadow = CreateDefaultSubobject<UDecalComponent>(TEXT("ContactShadow"));
	ContactShadow->SetupAttachment(Root);
	ContactShadow->SetUsingAbsoluteRotation(true);
	ContactShadow->SetUsingAbsoluteLocation(true);
	ContactShadow->SetWorldRotation(FRotator(-90.f, 0.f, 0.f)); // projects straight down
	ContactShadow->DecalSize = FVector(30.f, PK::BallRadius * 1.6f, PK::BallRadius * 1.6f);
	ContactShadow->SetDecalMaterial(ShadowMat.Object);
	ContactShadow->SetVisibility(ShadowMat.Object != nullptr);
	LandingRing = MakeRing(TEXT("LandingRing"));
}

APKFootballer* APKBall::GetOwnerPlayer() const
{
	if (Holder)
	{
		return Holder;
	}
	if (bPlaced && SetPieceTaker)
	{
		return SetPieceTaker;
	}
	return Controller;
}

void APKBall::BeginPlay()
{
	Super::BeginPlay();
	if (!BallMaterial)
	{
		PK::Paint(Mesh, Mesh->GetMaterial(0), FLinearColor(0.9f, 0.9f, 0.86f), 0.35f, 0.6f); // plain ball fallback
	}
	PK::Paint(ShadowRing, ShadowRing->GetMaterial(0), FLinearColor(0.95f, 0.95f, 0.95f), 0.5f, 1500.f); // emissive in nits
	PK::Paint(LandingRing, LandingRing->GetMaterial(0), FLinearColor(1.f, 0.7f, 0.05f), 0.5f, 2500.f);
}

FQuat APKBall::GetSpinRotation() const
{
	return Mesh ? Mesh->GetComponentQuat() : FQuat::Identity;
}

void APKBall::SetReplayPose(const FVector& Location, const FQuat& SpinRotation)
{
	SetActorLocation(Location);
	if (Mesh)
	{
		Mesh->SetWorldRotation(SpinRotation);
	}
}

void APKBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bReplaying)
	{
		return;
	}


	KickerIgnoreTimer = FMath::Max(0.f, KickerIgnoreTimer - DeltaSeconds);

	if (Holder)
	{
		// In an action (catch, dive, throw) the ball rides in the hands; otherwise it is carried at HoldHeight.
		const FVector Hands = Holder->IsInAction()
			? Holder->GetHoldPoint() + Holder->GetFacing() * 8.f
			: Holder->GetFlatLocation() + Holder->GetFacing() * 30.f + FVector(0.f, 0.f, HoldHeight);
		Velocity = Holder->GetVelocity();
		SetActorLocation(Hands);
	}
	else if (!bPlaced)
	{
		Accumulator += FMath::Min(DeltaSeconds, 0.1f);
		while (Accumulator >= StepSeconds)
		{
			Simulate(StepSeconds);
			Accumulator -= StepSeconds;
		}
	}

	UpdateHighlights();

	// Roll the mesh so movement reads visually.
	const FVector Ground = PK::Flat(Velocity);
	const float Speed = Ground.Size();
	if (Speed > 1.f && !Holder)
	{
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Ground / Speed);
		Mesh->AddWorldRotation(FQuat(Axis, Speed * DeltaSeconds / PK::BallRadius));
	}
}

void APKBall::Simulate(float Dt)
{
	FVector Pos = GetActorLocation();
	const bool bAirborne = Pos.Z > PK::BallRadius + 1.f;

	Velocity.Z -= PK::Gravity * Dt;
	Velocity *= FMath::Max(0.f, 1.f - PK::AirDrag * Dt);
	if (bAirborne && !Spin.IsNearlyZero())
	{
		Velocity += Spin * Dt; // curl
	}
	Spin *= FMath::Max(0.f, 1.f - SpinDecay * Dt);
	Pos += Velocity * Dt;

	if (Pos.Z <= PK::BallRadius)
	{
		Pos.Z = PK::BallRadius;
		if (Velocity.Z < -70.f)
		{
			// Grass bounce: keeps some height, skids a little off a fast ball.
			Velocity.Z = -Velocity.Z * BounceRestitution;
			Velocity.X *= 0.88f;
			Velocity.Y *= 0.88f;
		}
		else
		{
			Velocity.Z = 0.f;
			if (!Spin.IsNearlyZero())
			{
				// A swerved ground pass bends too (less than in the air): sideways to the way it is rolling.
				const FVector Roll = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Roll);
				Velocity += Side * FVector::DotProduct(Spin, Side) * 0.5f * Dt;
			}
			const float Speed = FVector2D(Velocity.X, Velocity.Y).Size();
			if (Speed > KINDA_SMALL_NUMBER)
			{
				const float NewSpeed = FMath::Max(0.f, Speed - PK::RollDecel * Dt);
				Velocity.X *= NewSpeed / Speed;
				Velocity.Y *= NewSpeed / Speed;
			}
		}
	}

	for (const float Side : { -1.f, 1.f })
	{
		const float X = Side * PK::HalfLength;
		CollideSegment(Pos, FVector(X, -PK::GoalHalfWidth, 0.f), FVector(X, -PK::GoalHalfWidth, PK::GoalHeight));
		CollideSegment(Pos, FVector(X, PK::GoalHalfWidth, 0.f), FVector(X, PK::GoalHalfWidth, PK::GoalHeight));
		CollideSegment(Pos, FVector(X, -PK::GoalHalfWidth, PK::GoalHeight), FVector(X, PK::GoalHalfWidth, PK::GoalHeight));

		if (!bInNet && Pos.X * Side > PK::HalfLength && FMath::Abs(Pos.Y) < PK::GoalHalfWidth && Pos.Z < PK::GoalHeight)
		{
			bInNet = true;
			NetSide = Side;
		}
	}

	if (bInNet)
	{
		ConstrainToNet(Pos);
	}

	SetActorLocation(Pos);
}

void APKBall::CollideSegment(FVector& Pos, const FVector& A, const FVector& B)
{
	const FVector Closest = FMath::ClosestPointOnSegment(Pos, A, B);
	const FVector Delta = Pos - Closest;
	const float Dist = Delta.Size();
	const float MinDist = PK::BallRadius + PK::PostRadius;
	if (Dist < MinDist && Dist > KINDA_SMALL_NUMBER)
	{
		const FVector Normal = Delta / Dist;
		const float Vn = FVector::DotProduct(Velocity, Normal);
		if (Vn < 0.f)
		{
			Velocity -= (1.f + PostRestitution) * Vn * Normal;
		}
		Pos = Closest + Normal * MinDist;
		Spin = FVector::ZeroVector;
	}
}

void APKBall::ConstrainToNet(FVector& Pos)
{
	const float R = PK::BallRadius;
	const float InnerX = NetSide * (PK::HalfLength + PK::GoalDepth - R);
	if (Pos.X * NetSide > InnerX * NetSide)
	{
		Pos.X = InnerX;
		Velocity.X *= -0.15f;
		Velocity.Y *= 0.5f;
	}
	const float MaxY = PK::GoalHalfWidth - R;
	if (FMath::Abs(Pos.Y) > MaxY)
	{
		Pos.Y = FMath::Sign(Pos.Y) * MaxY;
		Velocity.Y *= -0.2f;
	}
	const float MaxZ = PK::GoalHeight - R;
	if (Pos.Z > MaxZ)
	{
		Pos.Z = MaxZ;
		Velocity.Z = FMath::Min(Velocity.Z, 0.f);
	}
	Spin = FVector::ZeroVector;
}

void APKBall::SetController(APKFootballer* NewController)
{
	if (Controller)
	{
		Controller->bHasBall = false;
	}
	Controller = NewController;
	if (Controller)
	{
		Controller->bHasBall = true;
		LastTouchTeam = Controller->Team;
		IntendedReceiver = nullptr;
	}
}

void APKBall::ClearControl()
{
	SetController(nullptr);
	if (Holder)
	{
		Holder->bHasBall = false;
	}
	if (SetPieceTaker)
	{
		SetPieceTaker->bHasBall = false; // the restart has been taken
	}
	Holder = nullptr;
	bPlaced = false;
	SetPieceTaker = nullptr;
}

void APKBall::Kick(APKFootballer* By, const FVector& NewVelocity, const FVector& NewSpin)
{
	ClearControl();
	Velocity = NewVelocity;
	Spin = NewSpin;
	if (By)
	{
		LastTouchTeam = By->Team;
	}
	LastKicker = By;
	KickerIgnoreTimer = 0.3f;
	IntendedReceiver = nullptr;
	Accumulator = 0.f;
	++KickId;
}

void APKBall::Touch(APKFootballer* By, const FVector& NewVelocity)
{
	Velocity = NewVelocity;
	Spin = FVector::ZeroVector;
	if (By)
	{
		LastTouchTeam = By->Team;
	}
}

void APKBall::SetHeld(APKFootballer* Holder_, float Height)
{
	ClearControl();
	Holder = Holder_;
	HoldHeight = Height;
	Velocity = FVector::ZeroVector;
	Spin = FVector::ZeroVector;
	bInNet = false;
	if (Holder)
	{
		Holder->bHasBall = true;
		LastTouchTeam = Holder->Team;
	}
}

void APKBall::PlaceAt(const FVector& Location, APKFootballer* Taker)
{
	ClearControl();
	Velocity = FVector::ZeroVector;
	Spin = FVector::ZeroVector;
	bInNet = false;
	bPlaced = true;
	SetPieceTaker = Taker;
	if (Taker)
	{
		Taker->bHasBall = true;
		LastTouchTeam = Taker->Team;
	}
	SetActorLocation(FVector(Location.X, Location.Y, PK::BallRadius));
}

void APKBall::UpdateHighlights()
{
	const FVector Pos = GetActorLocation();
	const float Height = Pos.Z - PK::BallRadius;
	const bool bAir = !Holder && Height > 35.f && !bMarkersSuppressed;

	// Contact shadow: tight and dark on the grass, fading as the ball rises.
	if (ContactShadow)
	{
		const float Fade = FMath::Clamp(1.f - Height / 120.f, 0.f, 1.f);
		ContactShadow->SetWorldLocation(FVector(Pos.X, Pos.Y, 5.f));
		ContactShadow->SetFadeScreenSize(0.f);
		ContactShadow->SetVisibility(Fade > 0.02f);
		if (UMaterialInstanceDynamic* MID = ContactShadow->CreateDynamicMaterialInstance())
		{
			MID->SetScalarParameterValue(TEXT("Strength"), 0.75f * Fade);
		}
	}

	// Shadow ring straight under the ball; grows a little with height so depth reads at a glance.
	ShadowRing->SetVisibility(bAir);
	if (bAir)
	{
		const float Size = FMath::Clamp(0.45f + Height / 700.f, 0.45f, 1.1f);
		ShadowRing->SetWorldLocation(FVector(Pos.X, Pos.Y, 0.8f));
		ShadowRing->SetWorldScale3D(bThinRings ? FVector(Size * 0.6f, Size * 0.6f, 1.f) : FVector(Size, Size, 0.004f));
	}

	// Landing marker for lofted balls: solve the ballistic arc for where it comes back to grass.
	bool bLanding = false;
	if (bAir && !bInNet && Height > 80.f)
	{
		const float G = PK::Gravity;
		const float Vz = Velocity.Z;
		const float T = (Vz + FMath::Sqrt(FMath::Max(0.f, Vz * Vz + 2.f * G * Height))) / G;
		if (T > 0.3f)
		{
			const float Drag = FMath::Max(0.f, 1.f - PK::AirDrag * T * 0.5f);
			const FVector Land = FVector(Pos.X + Velocity.X * T * Drag, Pos.Y + Velocity.Y * T * Drag, 1.f);
			LandingRing->SetWorldLocation(Land);
			const float Pulse = 1.1f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f);
			LandingRing->SetWorldScale3D(bThinRings ? FVector(Pulse, Pulse, 1.f) : FVector(Pulse, Pulse, 0.004f));
			bLanding = true;
		}
	}
	LandingRing->SetVisibility(bLanding);
}
