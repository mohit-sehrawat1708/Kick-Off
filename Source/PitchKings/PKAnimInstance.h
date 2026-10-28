#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PKAnimInstance.generated.h"

class UAnimSequence;

// Full-body football actions played over locomotion (Mixamo soccer pack retargeted to the mannequin).
UENUM()
enum class EPKAction : uint8
{
	None,
	Pass,
	PassLeft,
	Shot,
	Volley,
	RunningShot,
	Header,
	Tackle,
	SlideTackle,
	SlideTackleB,
	SlideTackleC,
	Trip,
	Fallen,
	StandUp,
	GKDiveLeft,
	GKDiveRight,
	GKCatch,
	GKCatchHigh,
	GKCatchLow,
	GKBlock,
	GKPunt,
	GKRoll,
	GKThrow,
	GKScoop,
	GKMiss,
	ThrowIn,
	ThighControl,
	GKHold,
	ThrowHold,
	Count UMETA(Hidden)
};

// Static description of an action clip plus what we measured from it at startup.
struct FPKActionClip
{
	const UAnimSequence* Seq = nullptr;
	float Start = 0.f;        // clip time the action starts at (skips run-ups)
	float Stop = 1.f;         // clip time the action ends at
	float Contact = 0.f;      // clip time of the strike / catch / release (fastest contact bone)
	FName ContactBone;
	FVector ContactPos = FVector::ZeroVector; // contact bone in mesh space at the contact frame, travel removed (in place)
	float ContactSpeed = 0.f; // contact bone speed at the contact frame (cm/s at play rate 1), body travel removed
	float BlendIn = 0.1f;
	float BlendOut = 0.2f;
	bool bApplyTravel = false; // move the capsule with the clip's body travel (slides, dives, trips)
	bool bLockMovement = true;
	bool bGrounded = false;    // player is on the floor: counts as stunned
	bool bHoldAtEnd = false;   // freeze on the last frame until stopped (holding poses)
	TArray<FVector2D> Travel;  // pelvis XY travel in mesh space, sampled at TravelRate from clip time 0
	static constexpr float TravelRate = 60.f;

	bool IsValid() const { return Seq != nullptr; }
	FVector2D TravelAt(float Time) const;
};

// Game-thread values the proxy needs each frame.
struct FPKAnimInputs
{
	float Speed = 0.f;        // cm/s, ground speed
	float Direction = 0.f;    // deg, velocity relative to facing (0 forward, +90 right)
	float Lean = 0.f;         // deg, lean into turns (+ = turning right)
	float SprintAlpha = 0.f;  // 0..1
	float CrouchAlpha = 0.f;  // 0..1 jockey / shield stance
	float KickAlpha = -1.f;   // procedural kick fallback: 0..1 progress, < 0 when not kicking
	float KickPower = 0.f;    // 0..1
	float HeadYaw = 0.f;      // deg, look-at-ball yaw relative to facing
	float HeadPitch = 0.f;    // deg
	float TouchAlpha = 0.f;   // dribble cut pose weight (0 when not cutting)
	int32 TouchFoot = 1;      // 0 left, 1 right
	float TouchSide = 0.f;    // +1 ball pushed right, -1 left
	bool bTouchOutside = false;
	uint8 SkillPose = 0;      // see APKFootballer::SkillPose
	float CelebrateAlpha = 0.f; // arms out after scoring
	float CelebrateTime = 0.f;  // seconds into the celebration (for the banking sway)
	float SkillT = -1.f;      // 0..1 through the move, < 0 when none
	float SkillSide = 1.f;
	bool bKeeperIdle = false;

	// Action layer
	const UAnimSequence* ActionSeq = nullptr;
	float ActionTime = 0.f;
	float ActionWeight = 0.f;
	FVector2D ActionTravel = FVector2D::ZeroVector;
};

// Code-driven locomotion: blends idle / 8-way walk / 8-way jog clips by speed and direction with
// stride-matched play rate, layers procedural lean, crouch and head look, then blends the current
// football action on top (made in-place: its body travel is applied to the capsule by the footballer).
struct FPKAnimProxy : public FAnimInstanceProxy
{
	FPKAnimProxy() = default;
	explicit FPKAnimProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	struct FClip
	{
		const UAnimSequence* Seq = nullptr;
		float Length = 1.f;
		float Speed = 0.f;
		bool bInPlace = false;            // lock pelvis XY (clips that carry travel in the pose)
		FVector2D PelvisXY = FVector2D::ZeroVector;
	};

	void Accumulate(const FClip& Clip, float Time, float Weight, FPoseContext& Output, bool& bFirst);
	void ApplyProcedural(FPoseContext& Output) const;
	void ApplyAction(FPoseContext& Output);
	void ApplyHeadLook(FPoseContext& Output) const;
	void ApplyHands(FPoseContext& Output) const;

	FClip Idle;
	FClip KeeperIdle;
	FClip KeeperShuffle[2]; // 0 = right, 1 = left
	FClip Walk[8];
	FClip Jog[8];
	FPKAnimInputs Inputs;

	float Phase = 0.f;
	float IdleTime = 0.f;
	float WeightIdle = 1.f;
	float GaitAlpha = 0.f;
	int32 DirA = 0;
	int32 DirB = 1;
	float DirBlend = 0.f;
};

UCLASS(Transient, NotBlueprintable)
class PITCHKINGS_API UPKAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPKAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	// Shared, measured action clips (valid after the first instance initialises).
	static const FPKActionClip& GetActionClip(EPKAction Action);

	// Pelvis XY travel of a clip at a time, relative to its first frame (mesh space).
	static FVector2D MeasureTravel(const UAnimSequence* Seq, float Time);

	// Clips: index 0 = forward, then clockwise in 45 degree steps (FR, R, BR, B, BL, L, FL).
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleClip;

	UPROPERTY()
	TObjectPtr<UAnimSequence> KeeperIdleClip;

	// Keeper side-shuffles (replace the left/right walk clips for keepers).
	UPROPERTY()
	TObjectPtr<UAnimSequence> KeeperShuffleLeft;

	UPROPERTY()
	TObjectPtr<UAnimSequence> KeeperShuffleRight;

	float KeeperShuffleSpeed[2] = { 200.f, 200.f };

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> WalkClips;

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> JogClips;

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> ActionClips; // indexed by EPKAction

	TArray<float> WalkSpeeds;
	TArray<float> JogSpeeds;
	FPKAnimInputs Inputs;

private:
	static void BuildActionTable(const UPKAnimInstance* Source);

	float LastYaw = 0.f;
	float SmoothedYawRate = 0.f;
	float SmoothedDirection = 0.f;
};
