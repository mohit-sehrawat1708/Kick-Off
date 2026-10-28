#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PKBall.generated.h"

class UMaterialInterface;

class APKFootballer;
class UStaticMeshComponent;

// Custom fixed-step ball simulation (gravity, drag, bounce, rolling friction, spin curl, goal frame and net).
// The ball is always physically free while dribbling: the controller keeps it with touches.
// It can also be Held (keeper hands / throw-in) or Placed (dead ball for a set piece).
UCLASS()
class PITCHKINGS_API APKBall : public AActor
{
	GENERATED_BODY()

public:
	APKBall();

	virtual void Tick(float DeltaSeconds) override;

	void Kick(APKFootballer* By, const FVector& NewVelocity, const FVector& NewSpin = FVector::ZeroVector);
	void Touch(APKFootballer* By, const FVector& NewVelocity); // dribble touch: keeps the controller
	void SetHeld(APKFootballer* Holder, float Height);
	void PlaceAt(const FVector& Location, APKFootballer* Taker);
	void ClearControl();

	APKFootballer* GetController() const { return Controller; }
	APKFootballer* GetHolder() const { return Holder; }
	APKFootballer* GetSetPieceTaker() const { return SetPieceTaker; }
	APKFootballer* GetOwnerPlayer() const;  // holder, else set-piece taker, else dribbler
	bool IsInNet() const { return bInNet; }
	void ClearNet() { bInNet = false; } // staged tests restart play after a goal
	// Replay: the ball is posed from the recording instead of simulated.
	bool bReplaying = false;
	FQuat GetSpinRotation() const;
	void SetReplayPose(const FVector& Location, const FQuat& SpinRotation);
	bool IsPlaced() const { return bPlaced; }
	bool IsFree() const { return !Holder && !bPlaced; }

	void SetController(APKFootballer* NewController);

	FVector Velocity = FVector::ZeroVector;
	FVector Spin = FVector::ZeroVector; // sideways curl acceleration (cm/s^2), decays
	int32 LastTouchTeam = -1;
	int32 KickId = 0;
	float KickerIgnoreTimer = 0.f;

	UPROPERTY()
	TObjectPtr<APKFootballer> LastKicker;

	UPROPERTY()
	TObjectPtr<APKFootballer> IntendedReceiver;

protected:
	virtual void BeginPlay() override;

private:
	void Simulate(float Dt);
	void CollideSegment(FVector& Pos, const FVector& A, const FVector& B);
	void ConstrainToNet(FVector& Pos);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BallMaterial;

	bool bThinRings = false;
	bool bMarkersSuppressed = false;

public:
	// Hides the shadow / landing rings (spectating, replays, clean screenshots).
	void SetMarkersSuppressed(bool bSuppressed) { bMarkersSuppressed = bSuppressed; }

private:

	// Readability: ring under the ball when it is in the air, and where a lofted ball will land.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShadowRing;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LandingRing;

	// Soft contact shadow projected under the ball (in the shade there is no sun shadow to ground it).
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UDecalComponent> ContactShadow;

	void UpdateHighlights();

	UPROPERTY()
	TObjectPtr<APKFootballer> Controller;

	UPROPERTY()
	TObjectPtr<APKFootballer> Holder;

	UPROPERTY()
	TObjectPtr<APKFootballer> SetPieceTaker;

	float HoldHeight = 100.f;
	float Accumulator = 0.f;
	bool bInNet = false;
	bool bPlaced = false;
	float NetSide = 0.f;
};
