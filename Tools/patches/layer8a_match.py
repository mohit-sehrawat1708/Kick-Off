# Layer 8a/8b: stamina + fatigue, two halves with ends changed, advantage rule.
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

patch("PKTypes.h", [
("""	inline float AttackDir(int32 Team) { return Team == 0 ? 1.f : -1.f; }""",
"""	// Ends change at half time: Blue attacks +X in the first half and -X in the second.
	inline bool& SidesSwapped() { static bool bSwapped = false; return bSwapped; }
	inline float AttackDir(int32 Team) { return (Team == 0 ? 1.f : -1.f) * (SidesSwapped() ? -1.f : 1.f); }"""),
])

patch("PKFootballer.h", [
("""	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step""",
"""	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step
	// Stamina: Stamina drains with sprinting and comes back at walking pace, up to MaxStamina, which itself falls
	// slowly over the match. A tired player is slower, can't sprint when empty and is less accurate.
	float Stamina = 1.f;
	float MaxStamina = 1.f;
	void UpdateStamina(float Dt, float Rate);
	float StaminaSpeedScale() const { return FMath::Lerp(0.86f, 1.f, FMath::Clamp(Stamina / 0.55f, 0.f, 1.f)); }
	float FatigueError() const { return 1.f + 0.5f * (1.f - Stamina); } // multiplies passing / shooting error"""),
])

patch("PKFootballer.cpp", [
("""void APKFootballer::StartSkillPose(uint8 Pose, float Side)
{""", """void APKFootballer::UpdateStamina(float Dt, float Rate)
{
	const float Speed = PK::Flat(GetVelocity()).Size();
	const float Fitness = FMath::Lerp(1.3f, 0.75f, Attr.Physical); // strong players last longer
	float Drain = 0.f;
	if (IsSprinting())
	{
		Drain = 0.010f;
	}
	else if (bControlledSprint && Speed > PK::JogSpeed)
	{
		Drain = 0.005f;
	}
	else if (Speed > 400.f)
	{
		Drain = 0.0015f;
	}
	MaxStamina = FMath::Max(0.55f, MaxStamina - 0.0005f * Rate * Dt);
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
{"""),
("""	default:                      MaxSpeed = bSprint ? PK::SprintSpeed : (bControlledSprint ? PK::ControlledSprintSpeed : PK::JogSpeed); break;""",
"""	default:                      MaxSpeed = (bSprint && Stamina > 0.06f) ? PK::SprintSpeed : (bControlledSprint ? PK::ControlledSprintSpeed : PK::JogSpeed); break;"""),
("""	Move->MaxWalkSpeed = MaxSpeed * PaceScale * (bHasBall ? DribbleSpeedScale : 1.f);""",
"""	Move->MaxWalkSpeed = MaxSpeed * PaceScale * (bHasBall ? DribbleSpeedScale : 1.f) * StaminaSpeedScale();"""),
("""	return bSprint && Stance == EPKStance::Normal && PK::Flat(GetVelocity()).Size() > PK::JogSpeed * 0.95f;""",
"""	return bSprint && Stamina > 0.06f && Stance == EPKStance::Normal && PK::Flat(GetVelocity()).Size() > PK::JogSpeed * 0.95f;"""),
])

patch("PKGameMode.h", [
("""	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }""",
"""	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }
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
	bool TeamHasBall(int32 Team) const;"""),
])

patch("PKGameMode.cpp", [
# restart: sides, half state, stamina
("""	MatchTotal = TimeLeft;
	Mentality[0] = Mentality[1] = 0;""", """	MatchTotal = TimeLeft;
	Mentality[0] = Mentality[1] = 0;
	PK::SidesSwapped() = false;
	bHalfTimeDone = bHalfTimeBreak = false;
	FirstKickoffTeam = 0;
	AdvantageTeam = -1;
	for (APKFootballer* F : Footballers)
	{
		F->Stamina = F->MaxStamina = 1.f;
	}"""),
# kickoff yaw follows the attacking direction
("""		P->Teleport2D(TeamToWorld(P->Team, Fx, Fy), P->Team == 0 ? 0.f : 180.f);
		P->StunTimer = 0.f;
		P->Stance = EPKStance::Normal;""", """		P->Teleport2D(TeamToWorld(P->Team, Fx, Fy), PK::AttackDir(P->Team) > 0.f ? 0.f : 180.f);
		P->StunTimer = 0.f;
		P->Stance = EPKStance::Normal;"""),
# goal scored side + goal line restarts through AttackDir
("""		const int32 Scorer = B.X > 0.f ? 0 : 1;""", """		const int32 Scorer = B.X * PK::AttackDir(0) > 0.f ? 0 : 1;"""),
("""		const int32 Defending = Side > 0.f ? 1 : 0;""", """		const int32 Defending = Side * PK::AttackDir(0) > 0.f ? 1 : 0;"""),
# half time: in the goal-scored phase handler
("""	case EPKPhase::GoalScored:
		FreezeAll();
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			SetupKickoff(NextKickoffTeam);
		}
		break;""", """	case EPKPhase::GoalScored:
		FreezeAll();
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			if (bHalfTimeBreak)
			{
				// Second half: change ends, a breather for everyone, the other team kicks off.
				bHalfTimeBreak = false;
				PK::SidesSwapped() = !PK::SidesSwapped();
				for (APKFootballer* F : Footballers)
				{
					F->Stamina = FMath::Min(F->MaxStamina, F->Stamina + 0.2f);
				}
				ShowMessage(TEXT("SECOND HALF"), 1.5f);
				SetupKickoff(1 - FirstKickoffTeam);
			}
			else
			{
				SetupKickoff(NextKickoffTeam);
			}
		}
		break;"""),
("""	case EPKPhase::Playing:
		TimeLeft -= DeltaSeconds;
		if (TimeLeft <= 0.f)
		{""", """	case EPKPhase::Playing:
		TimeLeft -= DeltaSeconds;
		if (!bHalfTimeDone && TimeLeft <= MatchTotal * 0.5f && !bFeatureTest)
		{
			bHalfTimeDone = true;
			bHalfTimeBreak = true;
			Phase = EPKPhase::GoalScored; // same pause and restart path as after a goal
			PhaseTimer = 3.5f;
			Ball->ClearControl();
			ShowMessage(FString::Printf(TEXT("HALF TIME  BLUE %d - %d RED"), Score[0], Score[1]), 3.5f);
			UE_LOG(LogPitchKings, Log, TEXT("Half time: BLUE %d - %d RED"), Score[0], Score[1]);
			break;
		}
		UpdateAdvantage(DeltaSeconds);
		{
			// Stamina: a short match stands for 90 minutes, so players tire faster.
			const float Rate = FMath::Clamp(360.f / FMath::Max(MatchTotal, 60.f), 0.4f, 2.f);
			for (APKFootballer* F : Footballers)
			{
				F->UpdateStamina(DeltaSeconds, Rate);
			}
		}
		if (TimeLeft <= 0.f)
		{"""),
# fatigue in passing / shooting error
("""		const float Sloppy = (1.f - P->Attr.Passing) * (P->HasTrait(PKTrait::TikiTaka) ? 0.5f : 1.f);""",
"""		const float Sloppy = (1.f - P->Attr.Passing) * (P->HasTrait(PKTrait::TikiTaka) ? 0.5f : 1.f) * P->FatigueError();"""),
("""	if (bAirborne)
	{
		Err *= 1.4f; // volleys are harder to keep on target
	}
	const float CleanY""", """	if (bAirborne)
	{
		Err *= 1.4f; // volleys are harder to keep on target
	}
	Err *= P->FatigueError();
	const float CleanY"""),
# advantage in AwardFoul
("""	const bool bPenalty = InPenaltyArea(Fouler->Team, Spot);
	if (bRed)
	{
		SendOff(Fouler);
	}
	if (bPenalty)""", """	const bool bPenalty = InPenaltyArea(Fouler->Team, Spot);
	if (bRed)
	{
		SendOff(Fouler);
	}
	// Advantage: the fouled team still has the ball (or a team-mate is first to it) in the opponents' half.
	if (!bPenalty && !bRed && WorldToFx(Team, Spot.X) > -0.1f && TeamHasBall(Team))
	{
		AdvantageTeam = Team;
		AdvantageTimer = 2.5f;
		AdvantageSpot = Spot;
		AdvantageLabel = FString::Printf(TEXT("FOUL (%s)  FREE KICK"), Label);
		++Advantages;
		ShowMessage(TEXT("ADVANTAGE"), 1.2f);
		UE_LOG(LogPitchKings, Log, TEXT("Advantage %s"), PK::TeamName(Team));
		return;
	}
	if (bPenalty)"""),
("""void APKGameMode::AwardFreeKick(int32 Team, FVector Spot, const FString& Label)
{""", """bool APKGameMode::TeamHasBall(int32 Team) const
{
	const APKFootballer* C = Ball->GetController();
	if (C && !C->IsStunned())
	{
		return C->Team == Team;
	}
	// Loose: whoever is first to it.
	const FVector B = PK::Flat(Ball->GetActorLocation());
	float Mine = 1.e9f, Theirs = 1.e9f;
	for (const APKFootballer* P : Footballers)
	{
		if (P->IsStunned())
		{
			continue;
		}
		const float D = FVector::Dist(P->GetFlatLocation(), B);
		(P->Team == Team ? Mine : Theirs) = FMath::Min(P->Team == Team ? Mine : Theirs, D);
	}
	return Mine < 400.f && Mine + 150.f < Theirs;
}

void APKGameMode::UpdateAdvantage(float Dt)
{
	if (AdvantageTeam < 0)
	{
		return;
	}
	AdvantageTimer -= Dt;
	const APKFootballer* C = Ball->GetController();
	if (C && C->Team != AdvantageTeam && AdvantageTimer > 1.f)
	{
		// Lost it straight away: no advantage after all, back for the free kick.
		const int32 Team = AdvantageTeam;
		AdvantageTeam = -1;
		++AdvantageCallbacks;
		UE_LOG(LogPitchKings, Log, TEXT("Advantage called back: free kick %s"), PK::TeamName(Team));
		AwardFreeKick(Team, AdvantageSpot, AdvantageLabel);
		return;
	}
	if (AdvantageTimer <= 0.f)
	{
		AdvantageTeam = -1; // advantage played
	}
}

void APKGameMode::AwardFreeKick(int32 Team, FVector Spot, const FString& Label)
{"""),
])

# Freeze the clock in staged feature tests (no half time in the middle of a scenario).
patch("PKFeatureTest.cpp", [
("""	FeatureClock += Dt;
	if (FeatureStep < 0)""", """	FeatureClock += Dt;
	TimeLeft = FMath::Max(TimeLeft, MatchTotal * 0.9f);
	if (FeatureStep < 0)"""),
])

# HUD: stamina bar under the controlled player's tag, half indicator next to the clock
patch("PKHUD.cpp", [
("""		if (bIsHuman)
		{
			float PW = 0.f, PH = 0.f;""", """		if (bIsHuman)
		{
			// Stamina bar under the tag.
			const float SW = 34.f * Scale;
			const float SX = Screen.X - SW * 0.5f;
			const float SY = (Num.IsEmpty() ? Screen.Y : Y + BoxH) + 3.f;
			const float St = FMath::Clamp(P->Stamina, 0.f, 1.f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), SX - 1.f, SY - 1.f, SW + 2.f, 5.f);
			DrawRect(FLinearColor(0.25f, 0.25f, 0.25f, 0.8f), SX + SW * P->MaxStamina, SY, SW * (1.f - P->MaxStamina), 3.f);
			DrawRect(St > 0.5f ? FLinearColor(0.2f, 0.9f, 0.3f) : (St > 0.25f ? FLinearColor(1.f, 0.8f, 0.1f) : FLinearColor(1.f, 0.2f, 0.15f)), SX, SY, SW * St, 3.f);
			float PW = 0.f, PH = 0.f;"""),
])
print("layer8a ok")
