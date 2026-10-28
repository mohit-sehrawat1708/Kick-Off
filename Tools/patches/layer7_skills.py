# Layer 7: skill moves (step-over, ball roll, roulette, rainbow flick, nutmeg) with procedural poses,
# on B + direction / right stick flick, used by the AI too.
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

patch("PKFootballer.h", [
("""	void NoteFootTouch(int32 Foot, float Side, bool bOutside);""", """	void NoteFootTouch(int32 Foot, float Side, bool bOutside);
	// Skill move in progress: procedural pose (1 step-over, 2 ball roll, 3 roulette, 4 rainbow, 5 nutmeg) and the
	// roulette spin.
	uint8 SkillPose = 0;
	double SkillTime = -10.0;
	float SkillSide = 1.f;
	float SkillSpinTimer = 0.f;
	float SkillSpinRate = 0.f; // deg/s, + = clockwise seen from above (turning right)
	void StartSkillPose(uint8 Pose, float Side);"""),
])

patch("PKFootballer.cpp", [
("""void APKFootballer::NoteFootTouch(int32 Foot, float Side, bool bOutside)
{""", """void APKFootballer::StartSkillPose(uint8 Pose, float Side)
{
	SkillPose = Pose;
	SkillTime = GetWorld()->GetTimeSeconds();
	SkillSide = Side >= 0.f ? 1.f : -1.f;
}

void APKFootballer::NoteFootTouch(int32 Foot, float Side, bool bOutside)
{"""),
("""	if (Kick.bPending)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, KickYaw, 0.f), DeltaSeconds, 16.f));
	}""", """	if (Kick.bPending)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, KickYaw, 0.f), DeltaSeconds, 16.f));
	}

	// Roulette: a full turn over the ball, whatever the movement says.
	if (SkillSpinTimer > 0.f)
	{
		Move->bOrientRotationToMovement = false;
		AddActorWorldRotation(FRotator(0.f, SkillSpinRate * DeltaSeconds, 0.f));
		SkillSpinTimer -= DeltaSeconds;
	}"""),
])

patch("PKAnimInstance.h", [
("""	bool bTouchOutside = false;""", """	bool bTouchOutside = false;
	uint8 SkillPose = 0;      // see APKFootballer::SkillPose
	float SkillT = -1.f;      // 0..1 through the move, < 0 when none
	float SkillSide = 1.f;"""),
])

patch("PKAnimInstance.cpp", [
("""	Inputs.TouchFoot = Footballer->FootTouchFoot;""", """	Inputs.TouchFoot = Footballer->FootTouchFoot;
	{
		static const float SkillSeconds[] = { 0.f, 0.45f, 0.45f, 0.6f, 0.5f, 0.3f };
		const uint8 Pose = Footballer->SkillPose < 6 ? Footballer->SkillPose : 0;
		const float Age = (float)(Footballer->GetWorld()->GetTimeSeconds() - Footballer->SkillTime);
		Inputs.SkillPose = Pose;
		Inputs.SkillT = (Pose != 0 && Age >= 0.f && Age < SkillSeconds[Pose]) ? Age / SkillSeconds[Pose] : -1.f;
		Inputs.SkillSide = Footballer->SkillSide;
	}"""),
("""	// Procedural kick, only used when an action clip is missing.""", """	// Skill moves (no clips in the pack): procedural leg work layered on the run.
	if (Inputs.SkillT >= 0.f && Inputs.SkillPose != 0)
	{
		const float T = Inputs.SkillT;
		const float Env = FMath::Sin(PI * T) * (1.f - Inputs.ActionWeight);
		const float S = Inputs.SkillSide;
		const TCHAR* Leg = S > 0.f ? TEXT("r") : TEXT("l");
		const FString Thigh = FString::Printf(TEXT("thigh_%s"), Leg);
		const FString Calf = FString::Printf(TEXT("calf_%s"), Leg);
		const FString Foot = FString::Printf(TEXT("foot_%s"), Leg);
		switch (Inputs.SkillPose)
		{
		case 1: // step-over: the leg circles over the ball from inside to outside, body dips that way
			RotateBone(Pose, *Thigh, AxisSide, 35.f * Env);
			RotateBone(Pose, *Thigh, AxisForward, S * FMath::Lerp(-18.f, 28.f, SmoothStep01(T)) * Env);
			RotateBone(Pose, *Calf, AxisSide, -40.f * Env);
			RotateBone(Pose, TEXT("pelvis"), AxisForward, -S * 9.f * Env);
			break;
		case 2: // ball roll: sole on the ball, sweeping across
			RotateBone(Pose, *Thigh, AxisSide, 22.f * Env);
			RotateBone(Pose, *Thigh, AxisForward, S * FMath::Lerp(-22.f, 24.f, SmoothStep01(T)) * Env);
			RotateBone(Pose, *Calf, AxisSide, -35.f * Env);
			RotateBone(Pose, *Foot, AxisSide, 22.f * Env);
			break;
		case 3: // roulette: low and compact while turning
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				RotateBone(Pose, *FString::Printf(TEXT("thigh_%s"), Side), AxisSide, 14.f * Env);
				RotateBone(Pose, *FString::Printf(TEXT("calf_%s"), Side), AxisSide, -24.f * Env);
			}
			OffsetBone(Pose, TEXT("pelvis"), FVector(0.f, 0.f, -6.f * Env));
			break;
		case 4: // rainbow flick: heel kicks up behind
			RotateBone(Pose, *Thigh, AxisSide, -22.f * Env);
			RotateBone(Pose, *Calf, AxisSide, -105.f * Env);
			RotateBone(Pose, *Foot, AxisSide, -25.f * Env);
			break;
		case 5: // nutmeg: quick poke
			RotateBone(Pose, *Thigh, AxisSide, 20.f * Env);
			RotateBone(Pose, *Calf, AxisSide, -18.f * Env);
			break;
		default:
			break;
		}
	}

	// Procedural kick, only used when an action clip is missing."""),
])

patch("PKGameMode.h", [
("""	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }""",
"""	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }
	// Skill moves: 0 step-over, 1 ball roll, 2 roulette, 3 rainbow flick, 4 nutmeg.
	bool PerformSkill(APKFootballer* P, int32 Skill, float Side);
	void HumanSkill(int32 Team, const FVector& Dir);
	static const TCHAR* SkillName(int32 Skill);
	int32 SkillMoves = 0;
	int32 SkillBeats = 0; // defenders wrong-footed by a skill"""),
])

patch("PKGameMode.cpp", [
("""void APKGameMode::HumanFakeShot(int32 Team, const FVector& Aim)
{""", """const TCHAR* APKGameMode::SkillName(int32 Skill)
{
	static const TCHAR* Names[] = { TEXT("STEP-OVER"), TEXT("BALL ROLL"), TEXT("ROULETTE"), TEXT("RAINBOW FLICK"), TEXT("NUTMEG") };
	return Names[FMath::Clamp(Skill, 0, 4)];
}

bool APKGameMode::PerformSkill(APKFootballer* P, int32 Skill, float Side)
{
	if (!P || Ball->GetController() != P || !CanReachBall(P, 110.f) || P->IsKicking() || P->IsInAction() || P->TouchTimer > 0.15f || P->SkillSpinTimer > 0.f)
	{
		return false;
	}
	Side = Side >= 0.f ? 1.f : -1.f;
	const FVector Facing = P->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	const FVector Vel = PK::Flat(P->GetVelocity());
	const float Ctrl = FMath::Clamp(P->Attr.Control + (P->HasTrait(PKTrait::Technical) ? 0.05f : 0.f), 0.f, 1.f);
	// A poor dribbler sometimes over-hits it.
	const float Botch = FMath::FRand() < (0.8f - Ctrl) * 0.5f ? FMath::FRandRange(1.3f, 1.7f) : 1.f;
	UCharacterMovementComponent* Move = P->GetCharacterMovement();

	// The defender in front (if any).
	APKFootballer* Front = nullptr;
	float FrontD = 400.f;
	for (APKFootballer* O : Footballers)
	{
		const FVector To = O->GetFlatLocation() - P->GetFlatLocation();
		if (O->Team != P->Team && !O->IsKeeper() && To.Size() < FrontD && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.5f)
		{
			FrontD = To.Size();
			Front = O;
		}
	}
	auto TryBeat = [&](float Base, float Stun)
	{
		if (Front && FMath::FRand() < Base + 0.45f * Ctrl - 0.3f * Front->Attr.Defending)
		{
			Front->Stun(Stun);
			Front->StealCooldown = FMath::Max(Front->StealCooldown, Stun + 0.3f);
			++SkillBeats;
		}
	};

	switch (Skill)
	{
	case 0: // step-over: feint towards Side, take it the other way
		P->StartSkillPose(1, Side);
		Ball->Touch(P, Vel * 0.6f - Right * Side * 190.f * Botch + Facing * 110.f);
		Move->Velocity = Vel * 0.6f - Right * Side * 260.f;
		P->TouchTimer = 0.45f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.3f);
		TryBeat(0.3f, 0.45f);
		break;
	case 1: // ball roll: roll it across with the sole
		P->StartSkillPose(2, Side);
		Ball->Touch(P, Vel * 0.3f + Right * Side * 270.f * Botch);
		Move->Velocity = Vel * 0.3f + Right * Side * 220.f;
		P->TouchTimer = 0.45f;
		TryBeat(0.2f, 0.35f);
		break;
	case 2: // roulette: spin over the ball and away past the defender
		P->StartSkillPose(3, Side);
		P->SkillSpinTimer = 0.6f;
		P->SkillSpinRate = Side * 600.f;
		Ball->Touch(P, (Facing * 0.6f + Right * Side).GetSafeNormal2D() * 230.f * Botch);
		Move->Velocity = (Facing * 0.6f + Right * Side).GetSafeNormal2D() * 260.f;
		P->TouchTimer = 0.6f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.7f);
		TryBeat(0.25f, 0.35f);
		break;
	case 3: // rainbow flick: from behind, over his own head and the defender's
		P->StartSkillPose(4, Side);
		Ball->Touch(P, Facing * (FMath::Max(0.f, FVector::DotProduct(Vel, Facing)) + 360.f * Botch) + FVector(0.f, 0.f, 560.f));
		P->TouchTimer = 1.1f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.6f);
		if (Front)
		{
			Front->StealCooldown = FMath::Max(Front->StealCooldown, 1.f); // it goes over him
		}
		break;
	case 4: // nutmeg: through his legs and run round him
	{
		P->StartSkillPose(5, Side);
		const bool bClean = Front && FMath::FRand() < 0.4f + 0.5f * Ctrl - 0.3f * Front->Attr.Defending;
		const FVector Through = Front ? Front->GetFlatLocation() + Facing * 230.f : P->GetFlatLocation() + Facing * 300.f;
		Ball->Touch(P, (Through - PK::Flat(Ball->GetActorLocation())).GetSafeNormal2D() * 520.f * (bClean ? 1.f : 0.6f) * Botch);
		Move->Velocity = (Facing + Right * Side * 0.8f).GetSafeNormal2D() * 420.f;
		P->TouchTimer = 0.8f;
		if (bClean && Front)
		{
			Front->StealCooldown = FMath::Max(Front->StealCooldown, 1.2f); // through the legs: he can't stop it
			++SkillBeats;
		}
		break;
	}
	default:
		return false;
	}
	++SkillMoves;
	if (P->bHuman)
	{
		ShotFeedback = SkillName(Skill);
		ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
		ShotFeedbackTimer = 0.9f;
	}
	EventShot(TEXT("Feint"), P);
	return true;
}

void APKGameMode::HumanSkill(int32 Team, const FVector& Dir)
{
	// The direction (relative to the way he faces) picks the move: forward = rainbow (nutmeg when a defender is
	// right in front), diagonal = step-over, sideways = ball roll, back = roulette.
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human || !HumanHasBall(Team))
	{
		return;
	}
	const FVector Facing = Human->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	if (Dir.IsNearlyZero())
	{
		PerformSkill(Human, 0, FMath::FRand() < 0.5f ? -1.f : 1.f);
		return;
	}
	const FVector D = Dir.GetSafeNormal2D();
	const float Ang = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(D, Right), FVector::DotProduct(D, Facing)));
	const float Side = Ang >= 0.f ? 1.f : -1.f;
	const float A = FMath::Abs(Ang);
	int32 Skill = 2;
	if (A < 35.f)
	{
		bool bTight = false;
		for (const APKFootballer* O : Footballers)
		{
			const FVector To = O->GetFlatLocation() - Human->GetFlatLocation();
			bTight |= O->Team != Human->Team && To.Size() < 240.f && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.8f;
		}
		Skill = bTight ? 4 : 3;
	}
	else if (A < 75.f)
	{
		Skill = 0;
	}
	else if (A < 125.f)
	{
		Skill = 1;
	}
	PerformSkill(Human, Skill, Side);
}

void APKGameMode::HumanFakeShot(int32 Team, const FVector& Aim)
{"""),
# AI skills near the flick-up decision
("""		if (Opp && NearOpp < 260.f && P->Attr.Control > 0.72f && FMath::FRand() < 0.12f
			&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), (OppGoal(P->Team) - Loc).GetSafeNormal2D()) > 0.6f)
		{
			FlickUp(P, (OppGoal(P->Team) - Loc).GetSafeNormal2D());
			return;
		}""", """		if (Opp && NearOpp < 260.f && P->Attr.Control > 0.72f && FMath::FRand() < 0.12f
			&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), (OppGoal(P->Team) - Loc).GetSafeNormal2D()) > 0.6f)
		{
			FlickUp(P, (OppGoal(P->Team) - Loc).GetSafeNormal2D());
			return;
		}
		// Take a man on with a skill: technical players, defender close in front.
		if (Opp && NearOpp < 300.f && P->Attr.Control > 0.68f && FMath::FRand() < 0.1f + (P->HasTrait(PKTrait::Technical) ? 0.06f : 0.f)
			&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), P->GetFacing()) > 0.5f)
		{
			const float R = FMath::FRand();
			const int32 Skill = NearOpp < 210.f && R < 0.3f ? 4 : (R < 0.6f ? 0 : (R < 0.8f ? 2 : 1));
			if (PerformSkill(P, Skill, FMath::FRand() < 0.5f ? -1.f : 1.f))
			{
				return;
			}
		}"""),
])

patch("PKPlayerController.cpp", [
("""		else if (bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team] && !bEffortHeld[Team])
		{
			bEffortHeld[Team] = true; // RB + right stick: effort touch that way
			EffortTime[Team] = 0.f;
			EffortDir[Team] = RightStick.GetSafeNormal2D();
			bCharging = false;
		}""", """		else if (bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team] && !bEffortHeld[Team])
		{
			bEffortHeld[Team] = true; // RB + right stick: effort touch that way
			EffortTime[Team] = 0.f;
			EffortDir[Team] = RightStick.GetSafeNormal2D();
			bCharging = false;
		}
		else if (!bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team])
		{
			GM->HumanSkill(Team, RightStick.GetSafeNormal2D()); // right stick flick: skill move
		}
		else if (ActionPressed(Dev, EPKInput::Skill))
		{
			GM->HumanSkill(Team, Move); // B + direction (B alone: step-over)
		}"""),
])
print("layer7 ok")
