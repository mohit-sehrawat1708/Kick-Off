#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PKFootballer.h"
#include "PKGameMode.generated.h"

class APKBall;

UENUM()
enum class EPKPhase : uint8
{
	Kickoff,
	Playing,
	Stoppage,
	GoalScored,
	FullTime,
	Menu // front end open: players lined up, nothing moves
};

// Team play styles (chosen with 1-5 / D-pad for the human team, randomly for the AI team).
UENUM()
enum class EPKSetPiece : uint8
{
	None,
	Kickoff,
	FreeKick,
	Penalty,
	Corner,
	GoalKick,
	ThrowIn
};

UENUM()
enum class EPKTeamStyle : uint8
{
	Balanced,
	Possession,
	Counter,
	LongBall,
	HighPress,
	Count UMETA(Hidden)
};

// What a play style changes in the team AI.
struct FPKStyleParams
{
	float Progress;      // weight on passes that move the ball forward
	float Safety;        // weight on open, unblocked passes
	float MaxPassDist;   // cm
	float ThroughChance; // through balls when the target is ahead
	float LobBias;       // long balls in the air
	float DecisionMin;   // tempo (s between decisions on the ball)
	float DecisionMax;
	float PassUrge;      // how readily an unpressured carrier passes
	float LinePush;      // team frame shift of the whole block
	float PressDistance; // when the nearest defender engages
	bool bDoublePress;   // second player presses too
};

// Owns the match: stadium/lighting fallback, ball, two 11-a-side teams in a 4-4-2,
// the team AI, ball control (touch dribbling, steals, keeper handling), restarts and the clock.
UCLASS()
class PITCHKINGS_API APKGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APKGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// Human controls (called by APKPlayerController).
	void HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier, bool bPrecision = false);
	void HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve = false, bool bOneTwo = false);
	// Precision pass: to the exact spot aimed at, distance from the power bar (0..1); the nearest team-mate runs onto it.
	void HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve);
	// Header / volley on demand for a ball in the air (pass or shoot pressed without the ball).
	bool HumanRequestHeader(int32 Team, const FVector& Aim, bool bShot);
	void HumanShoot(int32 Team, const FVector& Aim, float Power, EPKShotType Type = EPKShotType::Normal);
	// Timed finishing: shoot pressed again during the wind-up. Returns false if no shot is winding up.
	bool HumanTimedFinish(int32 Team);
	// Fake shot: shape to shoot, then drag it across instead; close defenders may buy it.
	void HumanFakeShot(int32 Team, const FVector& Aim);
	void FakeShot(APKFootballer* P, const FVector& Aim);
	EPKShotType ChooseAIShot(const APKFootballer* P, float DGoal) const;
	// Teammate contain: the person holds Precision without the ball and the nearest team-mate presses the carrier.
	void HumanTeamPress(int32 Team, bool bPress) { bTeamPress[FMath::Clamp(Team, 0, 1)] = bPress; }
	// Runs: send the best-placed team-mate on a run in behind; a one-two pass makes the passer run for the return.
	void HumanSendRun(int32 Team);
	APKFootballer* SendRun(APKFootballer* Carrier, APKFootballer* Runner = nullptr);
	FVector RunSpot(const APKFootballer* Runner) const;
	TWeakObjectPtr<APKFootballer> OneTwoPasser[2];
	TWeakObjectPtr<APKFootballer> OneTwoReceiver[2];
	float OneTwoTimer[2] = { 0.f, 0.f };
	int32 RunsSent = 0;
	int32 OneTwos = 0;
	int32 OneTwoReturns = 0;
	// Game plan mentality: -2 ultra defensive .. +2 ultra attacking (pushes the block, the line and the press).
	int32 Mentality[2] = { 0, 0 };
	float MatchTotal = 360.f;
	float PlanTimer = 0.f;
	void SetMentality(int32 Team, int32 Value, bool bAnnounce = true);
	static const TCHAR* MentalityName(int32 Value);
	int32 GetMentality(int32 Team) const { return Mentality[FMath::Clamp(Team, 0, 1)]; }
	void UpdateGamePlans(float Dt);
	// Keeper rush: the person holds Through without the ball and the keeper charges the carrier.
	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }
	// Halves: ends change at half time, the other team kicks off the second half.
	bool bHalfTimeDone = false;
	bool bHalfTimeBreak = false;
	int32 FirstKickoffTeam = 0;
	bool IsSecondHalf() const { return bHalfTimeDone; }
	// Advantage: a foul where the fouled team keeps the ball is played on; called back if it is lost quickly.
	int32 AdvantageTeam = -1;
	float AdvantageTimer = 0.f;
	FVector AdvantageSpot = FVector::ZeroVector;
	FString AdvantageLabel;
	int32 Advantages = 0;
	int32 AdvantageCallbacks = 0;
	void UpdateAdvantage(float Dt);
	// Substitutions: the person's from the pause menu, the computer's own at stoppages late on.
	struct FPKSubEntry
	{
		int32 Slot = -1;   // formation slot (on the pitch) or -1 (bench)
		int32 Squad = -1;  // squad index
		int32 Number = 0;
		FString Name;
		FString Pos;
		float Stamina = 1.f;
		int32 Rating = 0;
		bool bAvailable = true; // bench: not yet used / on the pitch: not sent off
		bool bKeeper = false;
	};
	void GetSubsInfo(int32 Team, TArray<FPKSubEntry>& OnPitch, TArray<FPKSubEntry>& Bench) const;
	bool MakeSub(int32 Team, int32 Slot, int32 SquadIndex);
	int32 SubsLeft(int32 Team) const { return MaxSubs - SubsUsed[FMath::Clamp(Team, 0, 1)]; }
	static constexpr int32 MaxSubs = 5;
	int32 SubsUsed[2] = { 0, 0 };
	TArray<int32> MatchLineup[2];  // squad index per slot, as on the pitch now
	TArray<int32> SubbedOff[2];    // squad indices that have left (can't come back)
	int32 MatchFormation[2] = { 0, 0 };
	float AISubTimer = 0.f;
	void UpdateAISubs(float Dt);
	// Goal celebration and instant replay.
	struct FPKReplayActor
	{
		FVector Loc;
		float Yaw;
		FVector Vel;
		uint8 Action;
		float ActionTime;
	};
	struct FPKReplayFrame
	{
		FVector Ball;
		FQuat BallSpin;
		TArray<FPKReplayActor> Actors;
	};
	TArray<FPKReplayFrame> ReplayBuf; // ring buffer, 30 Hz
	int32 ReplayHead = 0;
	int32 ReplayCount = 0;
	float ReplayAccum = 0.f;
	bool bReplaying = false;
	float ReplayClock = 0.f;
	int32 ReplayFirst = 0;
	int32 ReplayLength = 0;
	int32 GoalStage = 0; // 0 celebration, 1 replay
	TWeakObjectPtr<APKFootballer> Celebrator;
	bool bNoReplay = false; // -PKNoReplay
	static constexpr int32 ReplayHz = 30;
	static constexpr int32 ReplaySeconds = 5;
	void RecordReplay(float Dt);
	void BeginCelebration(int32 Scorer);
	void CrowdCheer(int32 Team, float Amount);
	TWeakObjectPtr<class APKPitch> StadiumPitch;
	float MatchDuration = 1.f;
	// Match time as a broadcast clock: counts up to 90:00 whatever the real match length (45:00 at half time).
	FString GetBroadcastClock() const;
	// Bumped whenever players are teleported (kick-off): the camera cuts so nothing ghosts.
	uint32 CameraCutStamp = 0;
	TArray<FVector> LastPositions;
	void BeginReplay();
	void UpdateReplay(float Dt);
	void EndReplay();
	bool IsReplaying() const { return bReplaying; }
	void SkipReplay() { if (bReplaying) { PhaseTimer = FMath::Min(PhaseTimer, 0.3f); EndReplay(); } }
	bool GetCelebrationFocus(FVector& Out) const;
	// Self-test: a Blue striker through on an empty goal (the replay test needs a goal).
	void DebugForceGoalChance();
	void ScaleForDifficulty(int32 Team, FPKAttributes& Attr) const;
	bool TeamHasBall(int32 Team) const;
	// Skill moves: 0 step-over, 1 ball roll, 2 roulette, 3 rainbow flick, 4 nutmeg.
	bool PerformSkill(APKFootballer* P, int32 Skill, float Side);
	void HumanSkill(int32 Team, const FVector& Dir);
	static const TCHAR* SkillName(int32 Skill);
	int32 SkillMoves = 0;
	int32 SkillBeats = 0; // defenders wrong-footed by a skill
	bool bKeeperRush[2] = { false, false };
	bool bOldKeeper = false; // -PKOldKeeper: previous keeper positioning, for comparison
	int32 KeeperSmothers = 0;
	bool bTeamPress[2] = { false, false };
	TWeakObjectPtr<APKFootballer> PressMate[2];
	float TeamPressSeconds = 0.f;
	// Dynamic defensive line (team frame x, -1 own goal line) and the side a caught-upfield fullback left open.
	float DefLine[2] = { -0.6f, -0.6f };
	float VacatedSide[2] = { 0.f, 0.f };
	TWeakObjectPtr<APKFootballer> CaughtUpfield[2];
	bool bOldLine = false; // -PKOldLine: previous per-player line, for comparison
	void UpdateDefensiveLine();
	// -PKKitShots: both teams lined up for kit / look screenshots.
	bool bKitShots = false;
	float KitClock = 0.f;
	int32 KitStep = 0;
	FVector KitCamLoc = FVector::ZeroVector;
	FVector KitCamLook = FVector::ZeroVector;
	void UpdateKitShots(float Dt);
	// -PKPerf (and the auto-test): frame-time log every 5 s (FPS, worst frames, game / render / GPU ms).
	bool bPerfLog = false;
	float PerfClock = 0.f;
	TArray<float> PerfFrames;
	double PerfGT = 0.0, PerfRT = 0.0, PerfGPU = 0.0;
	void UpdatePerfLog(float Dt);
	// -PKCritic=<file>: screenshots for the art-direction critic from its own camera positions (PKCritic.cpp).
	struct FPKCriticShot
	{
		FString Name;
		float Time = 0.f;
		FString Camera;
		FString Look;
		float Fov = 60.f;
		bool bNoHud = false;
	};
	TArray<FPKCriticShot> CriticShots;
	int32 CriticIndex = 0;
	float CriticClock = 0.f;
	bool bCriticActive = false;
	bool bCriticInShot = false;
	bool bCriticShotTaken = false;
	bool CriticHideHud = false;
	FVector CriticCamLoc = FVector::ZeroVector;
	FVector CriticLookAt = FVector::ZeroVector;
	float CriticFov = 60.f;
	void LoadCriticShots(const FString& File);
	FVector CriticTarget(const FString& Spec) const;
	void UpdateCriticShots(float Dt);
	bool GetCriticCamera(FVector& OutLocation, FVector& OutLookAt, float& OutFov) const;
	bool IsCriticHudHidden() const { return bCriticInShot && CriticHideHud; }
	// -PKCelebrateShots: close-ups of a player on a celebration run (side, front, back, several stride phases).
	bool bCelebrateShots = false;
	void UpdateCelebrateShots(float Dt);
	static const TCHAR* ShotTypeName(EPKShotType Type);
	// Shot feedback for the HUD ("PERFECT TIMING", "CHIP" ...).
	FString ShotFeedback;
	float ShotFeedbackTimer = 0.f;
	FLinearColor ShotFeedbackColor = FLinearColor::White;
	int32 TimedGreen = 0;
	int32 TimedRed = 0;
	int32 QuickShots = 0;
	int32 FakeShots = 0;
	int32 ShotTypeCount[5] = { 0, 0, 0, 0, 0 };
	void HumanTackle(int32 Team);
	void HumanSlideTackle(int32 Team, const FVector& Aim);
	void HumanSwitch(int32 Team);
	// Effort dribble touch: knock the ball any way; Strength 0..1 (tap = short knock, hold = long one).
	void HumanKnockOn(int32 Team, const FVector& Aim, float Strength = 1.f);
	void EffortTouch(APKFootballer* P, const FVector& Aim, float Strength);
	void HumanDragBack(int32 Team);
	void HumanFeint(int32 Team, const FVector& Aim);
	void HumanFlickUp(int32 Team, const FVector& Aim);
	void HumanShoulder(int32 Team);
	void HumanKickOff(int32 Team, const FVector& Aim); // kick-off: play the ball yourself in that direction
	// The human player is standing on a dead ball (kick-off, free kick, throw-in, corner, penalty).
	bool HumanOnDeadBall(int32 Team) const;
	EPKSetPiece GetSetPiece() const { return SetPiece; }
	float GetRestartTime() const { return RestartTimer; }
	void SetTeamStyle(int32 Team, EPKTeamStyle Style);
	void CycleTeamStyle(int32 Team, int32 Step);
	EPKTeamStyle GetTeamStyle(int32 Team) const { return TeamStyle[Team]; }
	static FString StyleName(EPKTeamStyle Style);
	APKFootballer* GetPredictedTaker() const { return PredictedTaker; }
	void RestartMatch();
	bool HumanHasBall(int32 Team = 0) const;

	APKBall* GetBall() const { return Ball; }
	APKFootballer* GetHuman() const { return Humans[0]; }
	APKFootballer* GetHuman(int32 Team) const { return Humans[FMath::Clamp(Team, 0, 1)]; }
	bool IsHumanTeam(int32 Team) const { return bHumanTeam[FMath::Clamp(Team, 0, 1)]; }
	bool IsVersus() const { return bHumanTeam[0] && bHumanTeam[1] && !bSpectate; }
	// Controller feedback for the player controlling a team.
	void Rumble(int32 Team, float Strength, float Seconds);
	const TArray<TObjectPtr<APKFootballer>>& GetFootballers() const { return Footballers; }
	bool IsCloseCamera() const { return bCloseCamera; }
	bool GetAnimTestCamera(FVector& OutLocation, FVector& OutLookAt) const;
	bool IsAnimTest() const { return bAnimTest; }
	bool IsSpectating() const { return bSpectate; }
	// Front end on normal launches (off for the test / spectate command-line modes and -PKNoMenu).
	static bool UsesMenus();
	void EnterMenu();
	void StartMatch(bool bSpectateMatch, bool bVersus = false);
	void ApplyTeamSetups();
	bool GetEventFocus(FVector& OutFocus) const;
	FString GetAnimTestLabel(const APKFootballer* P) const;

	int32 Score[2] = { 0, 0 };
	float TimeLeft = 0.f;
	EPKPhase Phase = EPKPhase::Kickoff;
	FString Message;
	float MessageTimer = 0.f;

	int32 Passes[2] = { 0, 0 };
	int32 Shots[2] = { 0, 0 };
	int32 Steals[2] = { 0, 0 };
	float PossessionTime[2] = { 0.f, 0.f };
	float FinalThirdTime[2] = { 0.f, 0.f };
	float AirTime = 0.f;
	float MaxBallZ = 0.f;
	int32 Chips = 0;
	int32 KickCount = 0;
	int32 Whiffs = 0;
	float ContactErrSum = 0.f;
	float ContactErrMax = 0.f;
	float BoneErrSum = 0.f;
	float FacingErrSum = 0.f;
	float FacingErrMax = 0.f;
	int32 DeferredTouches = 0;
	int32 BufferedDropped = 0;
	int32 Fouls[2] = { 0, 0 };
	int32 Yellows[2] = { 0, 0 };
	int32 Reds[2] = { 0, 0 };
	int32 Penalties[2] = { 0, 0 };
	int32 Offsides[2] = { 0, 0 };
	int32 FreeKickShots = 0;
	int32 ShoulderWins = 0;
	int32 StalemateBreaks = 0;
	// Dribbling metrics (-PKDribbleTest and the autotest line).
	bool bOldDribble = false;
	bool bDribbleTest = false;
	int32 DribbleTouches = 0;
	int32 StepTouches = 0;
	int32 FallbackTouches = 0;  // no step seen for a while
	int32 OverrunTouches = 0;   // about to run over the ball between steps
	int32 OutsideTouches = 0;   // cuts with the outside of the foot
	int32 InsideCuts = 0;
	int32 EffortTouches = 0;
	// Animation vs ball speed: launch speed / striking-foot speed (at the clip's play rate). Real kicks: ~1.0 (side
	// foot) to ~1.5 (laces); far outside that the swing doesn't fit the ball.
	double KickFitSum = 0.0;
	int32 KickFitCount = 0;
	int32 KickFitBad = 0;
	void NoteKickFit(const APKFootballer* P, float LaunchSpeed);
	int32 PrecisionPasses = 0;
	int32 SwervePasses = 0;
	int32 HeadersOnDemand = 0;
	float ChooseSwerve(const APKFootballer* P, const FVector& Target) const;
	float CutShotTimer = -1.f; // dribble test: screenshot just after an outside cut
	int32 CutShots = 0;
	int32 SwingFar = 0;         // a due step whose foot was too far from the ball
	float SlowDribbleTime = 0.f; // carrying with input but too slow for step detection
	double TouchGapSum = 0.0;
	double DribbleSepSum = 0.0;
	int32 DribbleSepSamples = 0;
	float DribbleSepMax = 0.f;
	int32 DribbleLosses = 0;
	double DribbleSpeedSum = 0.0;
	float DribbleTestClock = 0.f;
	int32 DribbleTestPhase = -1;
	TWeakObjectPtr<APKFootballer> DribbleTestPlayer;
	float StalemateTimer = 0.f;
	int32 ShoulderLosses = 0;
	float StuckSeconds = 0.f;
	bool bHardCollision = false;
	bool bSandwichTest = false;
	bool bFoulTest = false;
	int32 RulesTestStep = 0;
	float SandwichClock = 0.f;
	int32 SandwichTrial = 0;
	int32 SandwichStuck = 0;
	FVector SandwichStart[3];
	TWeakObjectPtr<APKFootballer> SandwichPlayers[3];
	void UpdateRulesTest(float Dt);
	EPKSetPiece SetPiece = EPKSetPiece::None;
	UPROPERTY()
	TArray<TObjectPtr<APKFootballer>> SentOff;
	FString CardText;
	FLinearColor CardColor = FLinearColor::Yellow;
	float CardTimer = 0.f;
	double WatchErrSum = 0.0;
	int32 WatchSamples = 0;
	int32 WatchAway = 0;
	int32 MissedTouches = 0;
	int32 Crosses = 0;
	int32 Flicks = 0;
	int32 ThroughBalls[2] = { 0, 0 };

private:
	void SpawnEnvironmentIfMissing();
	void SpawnTeams();
	void SetupKickoff(int32 KickingTeam);
	FString TeamLabel(int32 Team) const;
	void ShowMessage(const FString& Text, float Seconds);

	// AI
	void UpdateAI(float Dt);
	void AICarry(APKFootballer* P, float Dt);
	void AIKeeper(APKFootballer* P);
	void AITakeRestart(APKFootballer* P);
	void AIDefend(APKFootballer* P, APKFootballer* Carrier);
	void MoveTo(APKFootballer* P, const FVector& Target, bool bUrgent);
	FVector SupportSpot(const APKFootballer* P, bool bAttacking) const;
	FVector PredictBall(const APKFootballer* P) const;
	// First point on the ball's flight between MinZ and MaxZ (coming down) that he can reach in time.
	FVector PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const;
	// Seconds until the ball passes closest to Point (ballistic, up to 1.5 s), and how close it gets.
	float BallArrivalTime(const FVector& Point, float* OutMissBy = nullptr) const;
	// Keeper: start the save before the ball arrives, so the hands meet it at the clip's contact frame.
	void PrepareSave(APKFootballer* Keeper, float SecondsToLine, float CrossY);
	int32 TimedSaves = 0;
	double PendingSlowSum = 0.0; // ball speed kept while waiting for a head / knee (1 = untouched)
	int32 PendingSlowCount = 0;
	FVector ComputeDribbleDir(const APKFootballer* P) const;
	APKFootballer* BestPassTarget(const APKFootballer* P, bool bAllowKeeper, float& OutScore) const;
	APKFootballer* NearestOpponent(const APKFootballer* P, float& OutDist) const;

	// Ball control
	void UpdateDribble(APKFootballer* P, float Dt);
	void UpdateDribbleLegacy(APKFootballer* P, float Dt);
	void UpdateDribbleTest(float Dt);
	// -PKFeatureTest[=Layer]: staged scenarios for the FC-style feature layers (PKFeatureTest.cpp).
	bool bFeatureTest = false;
	int32 FeatureLayer = 0; // 0 = every layer
	int32 FeatureStep = -1;
	float FeatureClock = 0.f;
	TArray<int32> FeatureOrder;
	FVector FeatureTarget = FVector::ZeroVector;
	float FeatureMin = 0.f;
	float FeatureAux = 0.f;
	float FeatureMaxZ = 0.f;
	bool bFeatureFlag = false;
	int32 FeatureCount0 = 0;
	TWeakObjectPtr<APKFootballer> FeatureA;
	TWeakObjectPtr<APKFootballer> FeatureB;
	TWeakObjectPtr<APKFootballer> FeatureC;
	void UpdateFeatureTest(float Dt);
	void SetupFeature(int32 Id);
	void TickFeature(int32 Id, float Dt);
	void FinishFeature(int32 Id);
	APKFootballer* FindFootballer(int32 Team, int32 Index) const;
	APKFootballer* StageFootballer(int32 Team, int32 Index, const FVector& Loc, float Yaw);
	void StageBall(APKFootballer* P);
	void StageMakeHuman(APKFootballer* P);
	void ReleaseKicks();
	void RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type, bool bSwerve = false);
	void RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type, bool bSwerve = false);
	void RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne = false, EPKShotType Type = EPKShotType::Normal);
	void AfterKickStart(APKFootballer* P);
	void AlignBallForKicks(float Dt);
	void UpdatePredictedTaker(float Dt);
	void UpdateMarking();
	FVector AttackSupportSpot(APKFootballer* P, APKFootballer* Carrier);
	const FPKStyleParams& Style(int32 Team) const;
	void ExecuteKick(APKFootballer* P, const FPKKickRequest& InKick);
	void TryTackle(APKFootballer* Tackler, float Bonus);
	void CheckPickups();
	void GainControl(APKFootballer* P);
	bool CanReachBall(const APKFootballer* P, float Reach, float MaxHeight = 90.f) const;

	// Rules
	void CheckBallOut();
	void AwardRestart(int32 Team, const FVector& Spot, bool bGoalKick, bool bThrowIn, const FString& Label);
	void SetHuman(APKFootballer* P);
	APKFootballer* ClosestOutfield(int32 Team, const FVector& Point, const APKFootballer* Exclude) const;
	APKFootballer* Keeper(int32 Team) const;

	FVector OppGoal(int32 Team) const;
	FVector OwnGoal(int32 Team) const;
	FVector TeamToWorld(int32 Team, float Fx, float Fy) const;
	float WorldToFx(int32 Team, float X) const;
	bool InOwnBox(const APKFootballer* P, const FVector& Point) const;

	void UpdateAutoTest(float Dt);
	void EventShot(const TCHAR* Label, const AActor* Focus = nullptr);
	bool PassLaneBlocked(const APKFootballer* P, const FVector& From, const FVector& To) const;
	bool TryCross(APKFootballer* P);
	void FlickUp(APKFootballer* P, const FVector& Dir);
	void UpdatePendingTouches(float Dt);
	void UpdateBodyContacts(float Dt);
	bool ShoulderChallenge(APKFootballer* Carrier, APKFootballer* Challenger, bool bBarge);
	void AwardFoul(APKFootballer* Fouler, APKFootballer* Victim, float Severity, const TCHAR* Label);
	void AwardFreeKick(int32 Team, FVector Spot, const FString& Label);
	void AwardPenalty(int32 Team);
	void SendOff(APKFootballer* P);
	void RestoreSentOff();
	bool IsOffsidePosition(const APKFootballer* P, float BallX) const;
	void MarkOffside(const APKFootballer* Kicker);
	bool CheckOffside(APKFootballer* Receiver);
	bool InPenaltyArea(int32 DefendingTeam, const FVector& Point) const;
	bool BallReadyForKick(const APKFootballer* P) const;
	bool TryStartKick(APKFootballer* P, const FPKKickRequest& Kick, bool bAirborne = false);
	void UpdateBufferedKicks(float Dt);
	void QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate = 1.f);
	void SetupScenario(int32 Index);
	void UpdateScenario(float Dt);
	void SetupAnimTestGroup(int32 Group);
	void UpdateAnimTest(float Dt);
	void SetupClipSheetGroup(int32 Group);
	void UpdateClipSheet(float Dt);

	UPROPERTY()
	TObjectPtr<APKBall> Ball;

	UPROPERTY()
	TArray<TObjectPtr<APKFootballer>> Footballers;

	UPROPERTY()
	TObjectPtr<APKFootballer> Humans[2];
	bool bHumanTeam[2] = { true, false }; // which teams a person controls

	EPKTeamStyle TeamStyle[2] = { EPKTeamStyle::Balanced, EPKTeamStyle::Balanced };

	UPROPERTY()
	TObjectPtr<APKFootballer> PredictedTaker;

	UPROPERTY()
	TMap<TObjectPtr<APKFootballer>, TObjectPtr<APKFootballer>> MarkTargets; // defender -> opponent
	float MarkingTimer = 0.f;
	float PredictedTakerTime = 0.f;

	float PhaseTimer = 0.f;
	float RestartTimer = 0.f;
	int32 NextKickoffTeam = 0;

	bool bAutoTest = false;
	bool bCloseCamera = false;
	bool bContactShots = false; // -PKContactShots: slow-motion screenshot at each kick / touch contact
	int32 ContactShotCount = 0;
	int32 RestoreDilationFrames = 0;
	TWeakObjectPtr<APKFootballer> ContactWatch;
	void ContactSnap(APKFootballer* P, const TCHAR* Label);
	bool bAnimTest = false;
	bool bSpectate = false; // AI vs AI with the normal player view
	float AutoSwitchTimer = 0.f;
	bool bClipSheet = false;
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> SheetClips;
	TArray<float> SheetTimes;
	int32 AnimGroup = 0;
	int32 AnimShot = 0;
	float AnimClock = 0.f;
	float AnimFreeze = 0.f;
	float EventShotCooldown = 0.f;
	FString PendingShot;
	int32 PendingShotFrames = 0;
	struct FTimedShot { FString File; float Delay; };
	TArray<FTimedShot> TimedShots;
	bool bScenario = false;
	int32 ScenarioIndex = 0;
	float ScenarioClock = 0.f;
	int32 ScenarioRound = 0;
	TWeakObjectPtr<const AActor> EventFocusActor;
	int32 EventShots = 0;
	TMap<FString, int32> EventShotCounts;
	TMap<TObjectPtr<APKFootballer>, EPKAction> AnimAssignments;
	float AutoTestClock = 0.f;
	int32 AutoTestStep = 0;
};
