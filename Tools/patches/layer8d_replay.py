# Layer 8d: goal celebration (scorer to the corner flag, arms up, team-mates follow) and an instant replay of the
# goal (last 5 s recorded at 30 Hz: positions, velocities for the run cycle, actions for kicks / dives / headers).
import os, sys
SRC = os.path.join(os.path.dirname(__file__), "..", "..", "Source", "PitchKings")

def patch(name, pairs):
    p = os.path.join(SRC, name)
    s = open(p, encoding="utf-8").read()
    for a, b in pairs:
        if b in s:
            continue
        if a not in s:
            sys.exit(f"{name}: not found: {a[:70]!r}")
        s = s.replace(a, b, 1)
    open(p, "w", encoding="utf-8").write(s)

patch("PKBall.h", [
("""	void ClearNet() { bInNet = false; } // staged tests restart play after a goal""",
"""	void ClearNet() { bInNet = false; } // staged tests restart play after a goal
	// Replay: the ball is posed from the recording instead of simulated.
	bool bReplaying = false;
	FQuat GetSpinRotation() const;
	void SetReplayPose(const FVector& Location, const FQuat& SpinRotation);"""),
])

patch("PKBall.cpp", [
("""void APKBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
""", """FQuat APKBall::GetSpinRotation() const
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
"""),
])

patch("PKFootballer.h", [
("""	void StartSkillPose(uint8 Pose, float Side);""", """	void StartSkillPose(uint8 Pose, float Side);
	// Goal celebration pose (arms up) and replay playback (the game mode poses him from the recording).
	bool bCelebrating = false;
	bool bReplaying = false;"""),
])

patch("PKFootballer.cpp", [
("""void APKFootballer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
""", """void APKFootballer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bReplaying)
	{
		return; // posed from the replay recording
	}
"""),
])

patch("PKAnimInstance.h", [
("""	uint8 SkillPose = 0;      // see APKFootballer::SkillPose""", """	uint8 SkillPose = 0;      // see APKFootballer::SkillPose
	float CelebrateAlpha = 0.f; // arms up after scoring"""),
])

patch("PKAnimInstance.cpp", [
("""	Inputs.TouchFoot = Footballer->FootTouchFoot;""", """	Inputs.TouchFoot = Footballer->FootTouchFoot;
	Inputs.CelebrateAlpha = FMath::FInterpTo(Inputs.CelebrateAlpha, Footballer->bCelebrating ? 1.f : 0.f, DeltaSeconds, 4.f);"""),
("""	// Skill moves (no clips in the pack): procedural leg work layered on the run.""", """	// Goal celebration: both arms up, chest out.
	if (Inputs.CelebrateAlpha > 0.01f)
	{
		const float C = Inputs.CelebrateAlpha * (1.f - Inputs.ActionWeight);
		RotateBone(Pose, TEXT("upperarm_l"), AxisForward, -95.f * C);
		RotateBone(Pose, TEXT("upperarm_r"), AxisForward, 95.f * C);
		RotateBone(Pose, TEXT("lowerarm_l"), AxisForward, -20.f * C);
		RotateBone(Pose, TEXT("lowerarm_r"), AxisForward, 20.f * C);
		RotateBone(Pose, TEXT("spine_03"), AxisSide, 10.f * C);
	}

	// Skill moves (no clips in the pack): procedural leg work layered on the run."""),
])

patch("PKGameMode.h", [
("""	void UpdateAISubs(float Dt);""", """	void UpdateAISubs(float Dt);
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
	void BeginReplay();
	void UpdateReplay(float Dt);
	void EndReplay();
	bool IsReplaying() const { return bReplaying; }
	void SkipReplay() { if (bReplaying) { PhaseTimer = FMath::Min(PhaseTimer, 0.3f); EndReplay(); } }
	bool GetCelebrationFocus(FVector& Out) const;"""),
])

patch("PKGameMode.cpp", [
("""	bOldKeeper = FParse::Param(FCommandLine::Get(), TEXT("PKOldKeeper"));""", """	bOldKeeper = FParse::Param(FCommandLine::Get(), TEXT("PKOldKeeper"));
	bNoReplay = FParse::Param(FCommandLine::Get(), TEXT("PKNoReplay"));"""),
# goal: celebrate, then replay
("""		NextKickoffTeam = 1 - Scorer;
		Phase = EPKPhase::GoalScored;
		PhaseTimer = 3.5f;""", """		NextKickoffTeam = 1 - Scorer;
		Phase = EPKPhase::GoalScored;
		PhaseTimer = 3.5f;
		BeginCelebration(Scorer);"""),
("""	case EPKPhase::GoalScored:
		FreezeAll();
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{""", """	case EPKPhase::GoalScored:
		FreezeAll();
		if (Celebrator.IsValid() && GoalStage == 0 && !bHalfTimeBreak)
		{
			// Celebration: the scorer wheels away to the corner flag, team-mates chase him.
			APKFootballer* S = Celebrator.Get();
			const FVector Flag(PK::AttackDir(S->Team) * (PK::HalfLength - 200.f), (S->GetActorLocation().Y >= 0.f ? 1.f : -1.f) * (PK::HalfWidth - 250.f), 0.f);
			MoveTo(S, Flag, true);
			for (APKFootballer* P : Footballers)
			{
				if (P != S && P->Team == S->Team && !P->IsKeeper() && FVector::Dist(P->GetFlatLocation(), S->GetFlatLocation()) < 3000.f)
				{
					MoveTo(P, S->GetFlatLocation(), FVector::Dist(P->GetFlatLocation(), S->GetFlatLocation()) > 300.f);
				}
			}
		}
		if (GoalStage == 0 && PhaseTimer <= ReplaySeconds + 0.6f && !bHalfTimeBreak && ReplayCount > ReplayHz)
		{
			BeginReplay();
		}
		if (bReplaying)
		{
			UpdateReplay(DeltaSeconds);
		}
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			EndReplay();"""),
# record during play
("""		UpdateAdvantage(DeltaSeconds);""", """		UpdateAdvantage(DeltaSeconds);
		RecordReplay(DeltaSeconds);"""),
("""void APKGameMode::SetupKickoff(int32 KickingTeam)
{""", """void APKGameMode::RecordReplay(float Dt)
{
	if (bNoReplay || bAutoTest)
	{
		return;
	}
	ReplayAccum += Dt;
	if (ReplayAccum < 1.f / ReplayHz)
	{
		return;
	}
	ReplayAccum = FMath::Fmod(ReplayAccum, 1.f / ReplayHz);
	const int32 Capacity = ReplayHz * (ReplaySeconds + 1);
	if (ReplayBuf.Num() != Capacity)
	{
		ReplayBuf.SetNum(Capacity);
		ReplayHead = ReplayCount = 0;
	}
	FPKReplayFrame& F = ReplayBuf[ReplayHead];
	F.Ball = Ball->GetActorLocation();
	F.BallSpin = Ball->GetSpinRotation();
	F.Actors.SetNum(Footballers.Num());
	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		const APKFootballer* P = Footballers[i];
		F.Actors[i] = { P->GetActorLocation(), (float)P->GetActorRotation().Yaw, P->GetVelocity(), (uint8)P->GetCurrentAction(), P->GetActionTime() };
	}
	ReplayHead = (ReplayHead + 1) % Capacity;
	ReplayCount = FMath::Min(ReplayCount + 1, Capacity);
}

void APKGameMode::BeginCelebration(int32 Scorer)
{
	GoalStage = 0;
	APKFootballer* S = Ball->LastKicker;
	if (!S || S->Team != Scorer)
	{
		// Own goal or a deflection: the nearest attacker celebrates.
		float Best = 1.e9f;
		for (APKFootballer* P : Footballers)
		{
			const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(Ball->GetActorLocation()));
			if (P->Team == Scorer && !P->IsKeeper() && D < Best)
			{
				Best = D;
				S = P;
			}
		}
	}
	Celebrator = S;
	if (S)
	{
		S->bCelebrating = true;
	}
	if (!bNoReplay && !bAutoTest && ReplayCount > ReplayHz)
	{
		PhaseTimer = 2.6f + ReplaySeconds + 0.6f; // celebrate, replay, then kick off
	}
}

void APKGameMode::BeginReplay()
{
	GoalStage = 1;
	bReplaying = true;
	ReplayClock = 0.f;
	const int32 Capacity = ReplayBuf.Num();
	ReplayLength = FMath::Min(ReplayCount, ReplayHz * ReplaySeconds);
	ReplayFirst = (ReplayHead - ReplayLength + Capacity) % Capacity;
	Ball->bReplaying = true;
	for (APKFootballer* P : Footballers)
	{
		P->bReplaying = true;
		P->bCelebrating = false;
		P->GetCharacterMovement()->DisableMovement();
	}
	ShotFeedback.Reset();
}

void APKGameMode::UpdateReplay(float Dt)
{
	ReplayClock += Dt;
	const float FrameF = FMath::Clamp(ReplayClock * ReplayHz, 0.f, (float)(ReplayLength - 1));
	const int32 I0 = FMath::FloorToInt(FrameF);
	const int32 I1 = FMath::Min(I0 + 1, ReplayLength - 1);
	const float A = FrameF - I0;
	const int32 Capacity = ReplayBuf.Num();
	const FPKReplayFrame& F0 = ReplayBuf[(ReplayFirst + I0) % Capacity];
	const FPKReplayFrame& F1 = ReplayBuf[(ReplayFirst + I1) % Capacity];
	Ball->SetReplayPose(FMath::Lerp(F0.Ball, F1.Ball, A), FQuat::Slerp(F0.BallSpin, F1.BallSpin, A));
	for (int32 i = 0; i < Footballers.Num() && i < F0.Actors.Num() && i < F1.Actors.Num(); ++i)
	{
		APKFootballer* P = Footballers[i];
		const FPKReplayActor& R0 = F0.Actors[i];
		const FPKReplayActor& R1 = F1.Actors[i];
		P->SetActorLocationAndRotation(FMath::Lerp(R0.Loc, R1.Loc, A), FRotator(0.f, FMath::Lerp(R0.Yaw, R0.Yaw + FMath::FindDeltaAngleDegrees(R0.Yaw, R1.Yaw), A), 0.f));
		P->GetCharacterMovement()->Velocity = FMath::Lerp(R0.Vel, R1.Vel, A); // drives the run cycle
		// Actions (kicks, dives, headers) play from the recorded clip time.
		const EPKAction Action = (EPKAction)R0.Action;
		const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
		if (Action != EPKAction::None && Clip.IsValid())
		{
			P->DebugClip = Clip.Seq;
			P->DebugTime = R0.Action == R1.Action ? FMath::Lerp(R0.ActionTime, R1.ActionTime, A) : R0.ActionTime;
		}
		else
		{
			P->DebugClip = nullptr;
		}
	}
	if (ReplayClock > (float)ReplayLength / ReplayHz + 0.3f)
	{
		EndReplay();
	}
}

void APKGameMode::EndReplay()
{
	if (Celebrator.IsValid())
	{
		Celebrator->bCelebrating = false;
	}
	if (!bReplaying)
	{
		return;
	}
	bReplaying = false;
	Ball->bReplaying = false;
	for (APKFootballer* P : Footballers)
	{
		P->bReplaying = false;
		P->DebugClip = nullptr;
		P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		P->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
	ReplayCount = 0; // the next replay records afresh
	PhaseTimer = FMath::Min(PhaseTimer, 0.3f);
}

bool APKGameMode::GetCelebrationFocus(FVector& Out) const
{
	if (Phase == EPKPhase::GoalScored && GoalStage == 0 && Celebrator.IsValid() && !bHalfTimeBreak)
	{
		Out = Celebrator->GetActorLocation();
		return true;
	}
	return false;
}

void APKGameMode::SetupKickoff(int32 KickingTeam)
{"""),
])

# include for the clip table used by the replay
patch("PKGameMode.cpp", [
("""#include "PKTeamData.h\"""", """#include "PKTeamData.h"
#include "PKAnimInstance.h\""""),
])

# Camera: follow the celebration; replay from a low side angle. Skip replay with pass / shoot / pause.
patch("PKPlayerController.cpp", [
("""	const FVector BallLoc = PK::Flat(GM->GetBall()->GetActorLocation());
	CameraFocus = FMath::VInterpTo(CameraFocus, BallLoc, DeltaTime, 2.2f);""", """	const FVector BallLoc = PK::Flat(GM->GetBall()->GetActorLocation());
	FVector Celebration;
	CameraFocus = FMath::VInterpTo(CameraFocus, GM->GetCelebrationFocus(Celebration) ? PK::Flat(Celebration) : BallLoc, DeltaTime, 2.2f);"""),
("""	else if (GM->IsCloseCamera())
	{""", """	else if (GM->IsReplaying())
	{
		// Replay: low from the side and a little behind the play, tighter than the match camera.
		CamLoc = CameraFocus + FVector(-PK::AttackDir(0) * 300.f, -1300.f, 330.f);
		LookAt = CameraFocus + FVector(0.f, 0.f, 70.f);
	}
	else if (GM->IsCloseCamera())
	{"""),
])

patch("PKPlayerController.cpp", [
("""	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (GM->IsHumanTeam(Team))
		{
			HandlePlayer(GM, Team, DeltaTime);
		}
	}""", """	if (GM->IsReplaying())
	{
		if (ActionPressed(AnyDevice, EPKInput::Pass) || ActionPressed(AnyDevice, EPKInput::Shoot) || DevPressed(AnyDevice, { EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom }))
		{
			GM->SkipReplay();
		}
		UpdateCamera(DeltaTime);
		return;
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (GM->IsHumanTeam(Team))
		{
			HandlePlayer(GM, Team, DeltaTime);
		}
	}"""),
])

# HUD: REPLAY tag
patch("PKHUD.cpp", [
("""	if (GM->MessageTimer > 0.f && !GM->Message.IsEmpty())
	{""", """	if (GM->IsReplaying())
	{
		const float Blink = FMath::Fmod((float)GetWorld()->GetRealTimeSeconds(), 1.f) < 0.6f ? 1.f : 0.25f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 30.f, 100.f, 150.f, 34.f);
		DrawRect(FLinearColor(0.9f, 0.1f, 0.1f, Blink), 40.f, 110.f, 14.f, 14.f);
		DrawText(TEXT("REPLAY"), FLinearColor::White, 62.f, 104.f, GEngine->GetMediumFont(), 1.2f);
		DrawText(TEXT("pass / shoot: skip"), FLinearColor(0.8f, 0.8f, 0.8f), 30.f, 138.f, GEngine->GetSmallFont(), 1.f);
	}
	if (GM->MessageTimer > 0.f && !GM->Message.IsEmpty() && !GM->IsReplaying())
	{"""),
])
print("layer8d ok")
