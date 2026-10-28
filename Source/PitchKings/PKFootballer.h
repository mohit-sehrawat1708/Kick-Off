#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PKAnimInstance.h"
#include "PKFootballer.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UENUM()
enum class EPKRole : uint8
{
	Keeper,
	Defender,
	Midfielder,
	Forward
};

// How the player is carrying themselves this frame (drives speed, turning, facing and the anim stance).
UENUM()
enum class EPKStance : uint8
{
	Normal,       // jog / sprint, face movement
	CloseControl, // slow dribble, tight touches
	Jockey,       // defending: face the ball, side-step
	Shield        // attacking: back into the opponent, protect the ball
};

UENUM()
enum class EPKKickType : uint8
{
	Ground,
	Through,
	Lob,
	Shot,
	Clearance
};

// A club's kit (the keeper wears his own colours with long sleeves and gloves). Pattern: 0 plain, 1 stripes,
// 2 hoops, 3 sash, 4 halves, 5 pinstripes.
struct FPKKit
{
	FLinearColor Primary = FLinearColor(0.02f, 0.12f, 0.6f);
	FLinearColor Secondary = FLinearColor(0.01f, 0.03f, 0.15f);
	FLinearColor Trim = FLinearColor(0.85f, 0.85f, 0.85f);
	FLinearColor Shorts = FLinearColor(0.85f, 0.85f, 0.85f);
	FLinearColor Socks = FLinearColor(0.02f, 0.12f, 0.6f);
	FLinearColor Number = FLinearColor(0.9f, 0.9f, 0.9f);
	int32 Pattern = 0;
	FLinearColor KeeperPrimary = FLinearColor(0.1f, 0.55f, 0.08f);
	FLinearColor KeeperSecondary = FLinearColor(0.02f, 0.2f, 0.02f);
	FLinearColor KeeperShorts = FLinearColor(0.02f, 0.02f, 0.02f);
	FLinearColor KeeperGloves = FLinearColor(0.85f, 0.85f, 0.85f);
};

// How a player looks: skin, hair (0 shaved, 1 buzz, 2 short, 3 medium, 4 big), beard (0..1), boots, build.
struct FPKLook
{
	FLinearColor Skin = FLinearColor(0.5f, 0.32f, 0.2f);
	FLinearColor Hair = FLinearColor(0.02f, 0.015f, 0.01f);
	FLinearColor Boots = FLinearColor(0.02f, 0.02f, 0.02f);
	int32 HairStyle = 2;
	float Beard = 0.f;
	bool bBootFlash = true;
	float Height = 1.f; // body scale
	float Build = 1.f;
};

// MetaHuman haircuts on the players (FPKLook::HairStyle 0-1 is the buzz cut, longer styles pick by skin tone).
enum class EPKHairCut : uint8
{
	Buzz,
	AfroFade,
	CurlyFade,
	SideSwept,
	PulledBack,
	HairLoss,
	Coil
};

// Shot techniques (FC-style): picked by the modifiers held when shoot is released.
enum class EPKShotType : uint8
{
	Normal,
	Finesse, // placed and curled into the corner (Precision held)
	Chip,    // lofted over the keeper (Runs held)
	Power,   // longer wind-up, much harder (Precision + Runs)
	Low      // low driven, skims the grass (Hold)
};

// Player traits (FIFA-style PlayStyles), bit flags.
namespace PKTrait
{
	enum : uint32
	{
		Technical = 1 << 0,   // tighter dribbling touches
		Finesse = 1 << 1,     // curled, placed shots
		PowerShot = 1 << 2,   // harder shots
		TikiTaka = 1 << 3,    // accurate short passing
		Rapid = 1 << 4,       // extra top speed
		PressProven = 1 << 5, // better at winning the ball
		Anticipate = 1 << 6,  // reads passes, longer interception reach
		WhippedPass = 1 << 7, // crosses faster and more accurate
		IncisivePass = 1 << 8, // through balls
		LongBallPass = 1 << 9, // long passes
		Trickster = 1 << 10,  // skill moves
		QuickStep = 1 << 11,  // acceleration off the mark
		Relentless = 1 << 12, // stamina
		Bruiser = 1 << 13,    // physical duels
		Intercept = 1 << 14,  // cutting out passes
		Block = 1 << 15,      // blocking shots
		Aerial = 1 << 16,     // headers
		Jockey = 1 << 17,     // quicker jockeying
		Acrobatic = 1 << 18,  // volleys
		FarThrow = 1 << 19,   // long throw-ins
		DeadBall = 1 << 20,   // free kicks and corners
		ChipShot = 1 << 21,   // chips
		SlideTackle = 1 << 22, // longer slides
		Count = 23
	};
	const TCHAR* Name(int32 Bit);
	// "Finesse Shot+, Rapid" (a + marks the PlayStyle+ version). MaxNames 0 = all.
	FString Names(uint32 Traits, uint32 Plus, int32 MaxNames = 0);
}

// Player ratings, 0..1 (shown x100 in the HUD).
USTRUCT()
struct FPKAttributes
{
	GENERATED_BODY()

	float Pace = 0.7f;
	float Control = 0.7f;
	float Passing = 0.7f;
	float Shooting = 0.7f;
	float Defending = 0.7f;
	float Physical = 0.7f;

	int32 Overall() const { return FMath::RoundToInt((Pace + Control + Passing + Shooting + Defending + Physical) / 6.f * 100.f); }
};

// A kick the player has started; the game mode releases the ball on the clip's contact frame.
USTRUCT()
struct FPKKickRequest
{
	GENERATED_BODY()

	EPKKickType Type = EPKKickType::Ground;
	FVector Target = FVector::ZeroVector; // ground point for passes, goal-mouth point for shots
	float Power = 0.5f;
	TWeakObjectPtr<class APKFootballer> Receiver;
	bool bPending = false;
	bool bPrecision = false; // played to an exact spot: no automatic chip over a defender, softer arrival
	EPKShotType ShotType = EPKShotType::Normal;
	FVector CleanTarget = FVector::ZeroVector; // shots: where it goes with no finishing error (timed finishing pulls toward it)
	float ActionRate = 1.f;  // wind-up tempo (power shot slower, quick release faster)
	float Swerve = 0.f;      // bend: +1 the path bulges right of the straight line, -1 left
};

// A footballer: UE5 mannequin with team kit, code-driven locomotion plus football action clips
// (UPKAnimInstance), and a movement model with stances. MoveInput / bSprint / Stance are written
// by the human input or the team AI.
UCLASS()
class PITCHKINGS_API APKFootballer : public ACharacter
{
	GENERATED_BODY()

public:
	APKFootballer();

	void InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome);
	// Dress the player: club kit, shirt number, appearance and body build.
	void ApplyKit(const FPKKit& Kit, const FPKLook& Look);
	FPKLook Look;
	// Team sheet: position in the formation and the squad player's ratings.
	void ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint32 InTraits, int32 InNumber, const FString& InName, uint32 InPlus = 0);
	virtual void Tick(float DeltaSeconds) override;

	void SetHumanControlled(bool bInHuman);
	bool IsKeeper() const { return Role == EPKRole::Keeper; }
	bool IsStunned() const;
	void Stun(float Seconds) { StunTimer = FMath::Max(StunTimer, Seconds); }
	void Teleport2D(const FVector& Location, float Yaw);
	FVector GetFlatLocation() const;
	FVector GetFacing() const;
	FVector GetFootPoint() const; // where the ball sits when under control
	FVector GetHoldPoint() const; // between the hands

	// Actions (full-body clips)
	bool PlayAction(EPKAction Action, EPKAction Then = EPKAction::None, float Rate = 1.f);
	// Play an action so its contact frame comes in SecondsToContact (faster swing, or joining the move part-way if
	// even MaxRate is too slow).
	bool PlayActionTimed(EPKAction Action, float SecondsToContact, float MaxRate = 2.2f);
	float GetActionEntry() const { return ActionEntry; } // clip time the action was entered at (for the blend-in)
	int32 SaveKickId = -1; // keeper: shot already being saved
	float TouchQueuedSpeed = 0.f; // ball speed when a header / thigh control was started
	void StopAction();
	EPKAction GetCurrentAction() const { return CurrentAction; }
	float GetActionTime() const { return ActionTime; }
	bool IsInAction() const { return CurrentAction != EPKAction::None; }
	bool IsActionLockingMovement() const;
	bool IsTackleActive() const;  // inside the tackle's contact window
	bool IsSliding() const { return IsSlideAction(CurrentAction) && IsTackleActive(); }
	static bool IsSlideAction(EPKAction A) { return A == EPKAction::SlideTackle || A == EPKAction::SlideTackleB || A == EPKAction::SlideTackleC; }

	// Kicks
	bool StartKick(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, bool bAirborneBall = false);
	FVector GetKickFootPoint() const; // where the striking foot meets the ball
	FVector GetKickContactPoint() const; // 3D: where the striking foot is at the contact frame
	FVector GetActionContactPoint(EPKAction Action, float Yaw) const; // contact bone at the contact frame, world
	FVector GetContactBoneLocation(EPKAction Action) const; // live contact bone position
	EPKAction GetKickClip() const { return KickClip; }
	float GetKickYaw() const { return KickYaw; }
	float GetActionRate() const { return ActionRate; }
	float TimeToContact() const; // seconds until the current action's contact frame (<= 0 once past)
	bool IsKickLeftFoot() const { return bKickLeftFoot; }
	bool IsKicking() const;
	bool ReadyToReleaseKick() const;
	FPKKickRequest ConsumeKick();
	float GetKickAlpha() const;
	float GetKickPower() const { return Kick.Power; }

	// Tackles, falls and keeper actions
	void PlayTackle();
	void PlaySlideTackle();
	void PlayTrip();
	void Dive(float Direction, float SecondsToContact = -1.f); // keeper, sideways: +1 right / -1 left of facing

	// Anim queries
	bool IsSprinting() const;
	bool IsInStance() const { return Stance == EPKStance::Jockey || Stance == EPKStance::Shield; }
	FVector GetLookTarget() const { return LookTarget; }

	// Indicator arrow above the head (predicted next player on the ball).
	void SetBallArrow(bool bVisible);
	// Hides the selection ring and chevron (spectating, replays, clean screenshots).
	void SetMarkersSuppressed(bool bSuppressed);
	bool HasTrait(uint32 Trait) const { return (Traits & Trait) != 0; }
	// 0 without the PlayStyle, 1 with it, 1.6 with its PlayStyle+ version.
	float StyleLevel(uint32 Trait) const { return (Traits & Trait) ? ((TraitsPlus & Trait) ? 1.6f : 1.f) : 0.f; }
	FString TraitNames(int32 MaxNames = 0) const;
	// AcceleRATE: 0 Controlled, 1 Explosive (quick off the mark), 2 Lengthy (slow start, long stride at the top end).
	uint8 AccelType = 0;
	static uint8 AccelTypeFor(const FPKAttributes& A);
	static const TCHAR* AccelName(uint8 Type);
	float AccelFactor(float SpeedAlpha) const;
	FString RoleShort() const;

	FPKAttributes Attr;
	uint32 Traits = 0;
	uint32 TraitsPlus = 0;

	int32 Team = 0;
	EPKRole Role = EPKRole::Forward;
	int32 Index = 0;        // formation slot (0 = keeper)
	int32 ShirtNumber = 0;  // from the squad
	FString PlayerName;
	FVector2D Home = FVector2D::ZeroVector; // formation slot in team frame, x: -1 own goal .. +1 opponent goal

	bool bHuman = false;
	bool bHasBall = false;
	FVector MoveInput = FVector::ZeroVector;
	bool bSprint = false;
	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step
	// Stamina: Stamina drains with sprinting and comes back at walking pace, up to MaxStamina, which itself falls
	// slowly over the match. A tired player is slower, can't sprint when empty and is less accurate.
	float Stamina = 1.f;
	float MaxStamina = 1.f;
	void UpdateStamina(float Dt, float Rate);
	float StaminaSpeedScale() const { return FMath::Lerp(0.86f, 1.f, FMath::Clamp(Stamina / 0.55f, 0.f, 1.f)); }
	float FatigueError() const { return 1.f + 0.5f * (1.f - Stamina); } // multiplies passing / shooting error
	// Header / volley on demand: the person pressed pass or shoot for a ball in the air.
	float HeaderRequestTimer = 0.f;
	// Timed finishing: when the shot started and when shoot was pressed again during the wind-up.
	double ShotStartTime = -10.0;
	double TimedShotPress = -10.0;
	bool bHeaderRequestShot = false;
	FVector HeaderRequestAim = FVector::ZeroVector;
	EPKStance Stance = EPKStance::Normal;
	FVector FaceTarget = FVector::ZeroVector; // world point to face in Jockey / Shield
	FVector LookTarget = FVector::ZeroVector;

	float PaceScale = 1.f; // small per-player variety
	float StunTimer = 0.f;
	float TackleCooldown = 0.f;
	float StealCooldown = 0.f;
	float LungeTimer = 0.f;
	float DecisionTimer = 0.f;
	float HoldTimer = 0.f;
	float ProtectedTimer = 0.f;
	float TouchTimer = 0.f;
	int32 LastRolledKickId = -1;
	FVector AIDribbleDir = FVector::ForwardVector;
	FVector AITarget = FVector::ZeroVector; // sticky off-ball target (support spot / run)
	float AITargetTimer = 0.f;
	float RunTimer = 0.f;

	// A ball touch that waits for the action's contact frame (headers, thigh control, flicks).
	EPKAction TouchAction = EPKAction::None;
	FVector TouchVelocity = FVector::ZeroVector;
	bool bTouchIsKick = false;
	FPKKickRequest BufferedKick; // chosen kick waiting for the ball to come back to the foot
	float BufferTimer = 0.f;
	bool bWatchBall = true;
	FVector HumanInput = FVector::ZeroVector;

	// Steps read from the animated feet: a step is the swinging foot passing under the body near the ground.
	int32 SwingCounter = 0;         // +1 per step
	int32 SeenSwingCounter = 0;     // dribble code's last processed step
	int32 LastSwingFoot = 0;        // 0 left, 1 right
	double LastSwingTime = -10.0;
	float StepInterval = 0.3f;      // smoothed seconds per step
	int32 StepsSinceTouch = 0;
	float DribbleSpeedScale = 1.f;  // stride adjustment while dribbling
	FVector DribbleSteer = FVector::ZeroVector; // path to the ball: direction the body must take to reach it
	float DribbleSteerWeight = 0.f;
	FVector GetFootContactPoint(int32 Foot) const; // where that foot meets the ball (flat)
	FVector GetFootSideContactPoint(int32 Foot, float Side) const; // ball beside the foot: Side +1 right of it, -1 left
	// Last dribble touch, for the foot pose: which foot, which way it pushed the ball (+ right, - left, 0 straight; the
	// size is how sharp the cut was) and whether it was the outside of the foot.
	double FootTouchTime = -10.0;
	int32 FootTouchFoot = 1;
	float FootTouchSide = 0.f;
	bool bFootTouchOutside = false;
	void NoteFootTouch(int32 Foot, float Side, bool bOutside);
	// Skill move in progress: procedural pose (1 step-over, 2 ball roll, 3 roulette, 4 rainbow, 5 nutmeg) and the
	// roulette spin.
	uint8 SkillPose = 0;
	double SkillTime = -10.0;
	float SkillSide = 1.f;
	float SkillSpinTimer = 0.f;
	float SkillSpinRate = 0.f; // deg/s, + = clockwise seen from above (turning right)
	void StartSkillPose(uint8 Pose, float Side);
	// Goal celebration pose (arms up) and replay playback (the game mode poses him from the recording).
	bool bCelebrating = false;
	bool bReplaying = false;
 // what the person is pushing this frame (receiver assist reads it)

	// Discipline and physical play.
	int32 YellowCards = 0;
	bool bSentOff = false;
	int32 OffsideKickId = -1;   // in an offside position when this kick was played
	float ShoulderCooldown = 0.f;
	bool bHoldPosition = false;  // set piece: stay put (wall, penalty) until the ball is played
	float StuckTimer = 0.f;      // test metric: wants to move but cannot // AI: turn to face the ball when standing or walking

	// Debug: play an arbitrary clip at a fixed time (clip sheet tool).
	UPROPERTY()
	TObjectPtr<const UAnimSequence> DebugClip;
	float DebugTime = 0.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BallArrow;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitMaterial;
	// Cloth shell: a second copy of the body following its pose, drawn with M_PK_KitShell (sleeve and shorts hems,
	// flared shorts, sock tops, hair volume).
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> KitShell;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitBodyMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitShellMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> KitBodyMID;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> KitShellMID;

	// MetaHuman parts (City Sample Crowds) over the mannequin body: a scanned head with facial idles, a crew-neck
	// shirt and trousers cut to shorts by M_PK_PlayerCloth. The body then only shows the arms, legs and boots
	// (M_PK_Kit's MHParts mask). -PKMannyKit keeps the old all-mannequin look.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> MHHead;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> MHShirt;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> MHShorts;
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> MHHeadMeshes;
	// Boots ("Vrfs boots" by zanman730, CC-BY; Tools/editor_scripts/build_player_boots.py), on the foot sockets.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BootL;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BootR;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BootMID;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MHHair;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MHBeard;
	// the hair style's solid helmet LOD, a touch smaller, under the cards: gaps between cards show hair, not scalp
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MHHairBase;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> MHHairMeshes; // EPKHairCut AfroFade..Coil, then full beard, chin strap
	// Heads built from MetaHuman Creator presets (DA_PK_Heads): each face keeps its own baked materials and grooms.
	// Without the set the City Sample heads above are used.
	UPROPERTY()
	TObjectPtr<class UPKHeadSet> HeadSet;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MHMustache;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MHBrows;
	const struct FPKHead* PickHead() const;
	UPROPERTY()
	TArray<TObjectPtr<UTexture>> MHHeadNormals;   // per head
	UPROPERTY()
	TArray<TObjectPtr<UTexture>> MHHairMaps;   // scalp maps by EPKHairCut
	UPROPERTY()
	TArray<TObjectPtr<UTexture>> MHBrowMaps;
	UPROPERTY()
	TArray<TObjectPtr<UTexture>> MHBeardMaps;  // stubble, chin strap
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHSkinMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHShirtMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHShortsMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHEyeMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHTeethMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MHHiddenMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MHSkinMID;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MHShirtMID;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MHShortsMID;
	bool bMetaHumanParts = false;
	void DressMetaHuman(const FPKKit& Kit);
	void DressHeadFromSet(const struct FPKHead& Head);
	void SetShirtName(); // surname on the back of the MetaHuman shirt (kit-font character codes)

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

private:
	void UpdateAction(float DeltaSeconds);
	void UpdateMovementModel(float DeltaSeconds);
	void UpdateFootSteps();
	float FootFwdPrev[2] = { 0.f, 0.f };
	EPKAction KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, float LaunchSpeed) const;
	// How fast the ball will leave the foot for this request (mirrors APKGameMode::ExecuteKick).
	float EstimateLaunchSpeed(const FPKKickRequest& Request) const;
	// Play rate that makes the striking foot's speed fit the launch speed (ball / foot ratio by technique).
	static float FitKickRate(EPKAction Clip, float LaunchSpeed);

	FPKKickRequest Kick;
	EPKAction KickClip = EPKAction::None;
	float KickYaw = 0.f;
	float FallbackKickTimer = 0.f; // used only when a kick clip is missing

	EPKAction CurrentAction = EPKAction::None;
	float ActionEntry = 0.f;
	EPKAction NextAction = EPKAction::None;
	float ActionTime = 0.f;
	float ActionRate = 1.f;
	bool bKickLeftFoot = false;
	bool bArrowVisible = false;
	float ArrowPhase = 0.f;
};
