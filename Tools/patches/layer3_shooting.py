# Layer 3: shot types (finesse / chip / power / low driven), timed finishing, quick-release shots, fake shot,
# AI shot choice, HUD feedback.
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
("""// Player traits (FIFA-style PlayStyles), bit flags.""", """// Shot techniques (FC-style): picked by the modifiers held when shoot is released.
enum class EPKShotType : uint8
{
	Normal,
	Finesse, // placed and curled into the corner (Precision held)
	Chip,    // lofted over the keeper (Runs held)
	Power,   // longer wind-up, much harder (Precision + Runs)
	Low      // low driven, skims the grass (Hold)
};

// Player traits (FIFA-style PlayStyles), bit flags."""),
("""	bool bPrecision = false; // played to an exact spot: no automatic chip over a defender, softer arrival""",
"""	bool bPrecision = false; // played to an exact spot: no automatic chip over a defender, softer arrival
	EPKShotType ShotType = EPKShotType::Normal;
	FVector CleanTarget = FVector::ZeroVector; // shots: where it goes with no finishing error (timed finishing pulls toward it)
	float ActionRate = 1.f;  // wind-up tempo (power shot slower, quick release faster)"""),
("""	float HeaderRequestTimer = 0.f;""", """	float HeaderRequestTimer = 0.f;
	// Timed finishing: when the shot started and when shoot was pressed again during the wind-up.
	double ShotStartTime = -10.0;
	double TimedShotPress = -10.0;"""),
])

patch("PKFootballer.cpp", [
("""	if (!PlayAction(Clip, EPKAction::None, FMath::FRandRange(0.92f, 1.1f)))""",
"""	if (!PlayAction(Clip, EPKAction::None, FMath::FRandRange(0.92f, 1.1f) * FMath::Clamp(Request.ActionRate, 0.5f, 1.6f)))"""),
])

patch("PKGameMode.h", [
("""	void HumanShoot(int32 Team, const FVector& Aim, float Power);""",
"""	void HumanShoot(int32 Team, const FVector& Aim, float Power, EPKShotType Type = EPKShotType::Normal);
	// Timed finishing: shoot pressed again during the wind-up. Returns false if no shot is winding up.
	bool HumanTimedFinish(int32 Team);
	// Fake shot: shape to shoot, then drag it across instead; close defenders may buy it.
	void HumanFakeShot(int32 Team, const FVector& Aim);
	void FakeShot(APKFootballer* P, const FVector& Aim);
	EPKShotType ChooseAIShot(const APKFootballer* P, float DGoal) const;
	static const TCHAR* ShotTypeName(EPKShotType Type);
	// Shot feedback for the HUD ("PERFECT TIMING", "CHIP" ...).
	FString ShotFeedback;
	float ShotFeedbackTimer = 0.f;
	FLinearColor ShotFeedbackColor = FLinearColor::White;
	int32 TimedGreen = 0;
	int32 TimedRed = 0;
	int32 QuickShots = 0;
	int32 FakeShots = 0;
	int32 ShotTypeCount[5] = { 0, 0, 0, 0, 0 };"""),
("""	void RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne = false);""",
"""	void RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne = false, EPKShotType Type = EPKShotType::Normal);"""),
("""	void ExecuteKick(APKFootballer* P, const FPKKickRequest& Kick);""",
"""	void ExecuteKick(APKFootballer* P, const FPKKickRequest& InKick);"""),
])

patch("PKGameMode.cpp", [
# RequestShot with types
("""void APKGameMode::RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne)
{
	// Finishing rating and traits: Finesse places it toward the far post, Power Shot trades accuracy for pace.
	if (P->HasTrait(PKTrait::Finesse) && FMath::Abs(AimLateral) < 0.5f)
	{
		AimLateral = (P->GetActorLocation().Y * PK::AttackDir(P->Team) > 0.f ? -1.f : 1.f) * 0.85f;
	}
	float Err = (50.f + 180.f * Power * Power * Power) * FMath::Lerp(1.6f, 0.7f, P->Attr.Shooting);
	Err *= P->HasTrait(PKTrait::Finesse) ? 0.8f : (P->HasTrait(PKTrait::PowerShot) ? 1.1f : 1.f);
	if (bAirborne)
	{
		Err *= 1.4f; // volleys are harder to keep on target
	}
	const float AimY = FMath::Clamp(AimLateral, -1.f, 1.f) * (PK::GoalHalfWidth - 50.f) + FMath::FRandRange(-Err, Err);
	float AimZ = 30.f + 180.f * Power * Power + FMath::FRandRange(-35.f, 35.f) * Power;
	AimZ += FMath::FRandRange(-20.f, 110.f) * Power * Power * FMath::Lerp(1.4f, 0.4f, P->Attr.Shooting) * (bAirborne ? 1.5f : 1.f);

	FPKKickRequest Kick;
	Kick.Type = EPKKickType::Shot;
	Kick.Target = FVector(PK::AttackDir(P->Team) * PK::HalfLength, AimY, AimZ);
	Kick.Power = Power;
	TryStartKick(P, Kick, bAirborne);
}""", """void APKGameMode::RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne, EPKShotType Type)
{
	if (bAirborne)
	{
		Type = EPKShotType::Normal; // volleys are struck as they come
	}
	// Quick release: a light tap inside the box gets the shortest wind-up.
	const bool bQuick = !bAirborne && Power < 0.3f && (Type == EPKShotType::Normal || Type == EPKShotType::Low)
		&& FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < PK::PenaltyDepth + 150.f;
	if (bQuick)
	{
		Power = 0.5f;
		++QuickShots;
	}
	const float FarPost = P->GetActorLocation().Y * PK::AttackDir(P->Team) > 0.f ? -1.f : 1.f;
	if (Type == EPKShotType::Finesse)
	{
		// Finesse: into the corner (the far one when not aiming).
		AimLateral = FMath::Abs(AimLateral) < 0.5f ? FarPost * 0.85f : FMath::Sign(AimLateral) * FMath::Max(FMath::Abs(AimLateral), 0.8f);
	}
	else if (Type == EPKShotType::Normal && P->HasTrait(PKTrait::Finesse) && FMath::Abs(AimLateral) < 0.5f)
	{
		AimLateral = FarPost * 0.85f; // Finesse PlayStyle: always looks for the far corner
	}

	// Finishing rating, technique and PlayStyles set the error.
	float Err = (50.f + 180.f * Power * Power * Power) * FMath::Lerp(1.6f, 0.7f, P->Attr.Shooting);
	switch (Type)
	{
	case EPKShotType::Finesse: Err *= P->HasTrait(PKTrait::Finesse) ? 0.6f : 0.75f; break;
	case EPKShotType::Power:   Err *= P->HasTrait(PKTrait::PowerShot) ? 1.05f : 1.3f; break;
	case EPKShotType::Chip:    Err *= 0.7f; break;
	case EPKShotType::Low:     Err *= 0.9f; break;
	default:                   Err *= P->HasTrait(PKTrait::Finesse) ? 0.8f : (P->HasTrait(PKTrait::PowerShot) ? 1.1f : 1.f); break;
	}
	if (bAirborne)
	{
		Err *= 1.4f; // volleys are harder to keep on target
	}
	const float CleanY = FMath::Clamp(AimLateral, -1.f, 1.f) * (PK::GoalHalfWidth - 50.f);
	const float AimY = CleanY + FMath::FRandRange(-Err, Err);
	const float Wild = FMath::Lerp(1.4f, 0.4f, P->Attr.Shooting) * (bAirborne ? 1.5f : 1.f);
	float CleanZ = 30.f + 180.f * Power * Power;
	float AimZ = CleanZ + FMath::FRandRange(-35.f, 35.f) * Power + FMath::FRandRange(-20.f, 110.f) * Power * Power * Wild;
	switch (Type)
	{
	case EPKShotType::Finesse: CleanZ = 45.f + 110.f * Power * Power; AimZ = CleanZ + FMath::FRandRange(-25.f, 60.f) * Power * Wild; break;
	case EPKShotType::Power:   CleanZ = 50.f + 120.f * Power * Power; AimZ = CleanZ + FMath::FRandRange(-30.f, 90.f) * Power * Wild; break;
	case EPKShotType::Low:     CleanZ = 20.f; AimZ = CleanZ + FMath::FRandRange(0.f, 30.f) * Wild; break;
	case EPKShotType::Chip:    CleanZ = 190.f; AimZ = CleanZ + FMath::FRandRange(-20.f, 50.f) * Wild; break;
	default: break;
	}

	FPKKickRequest Kick;
	Kick.Type = EPKKickType::Shot;
	Kick.Target = FVector(PK::AttackDir(P->Team) * PK::HalfLength, AimY, AimZ);
	Kick.CleanTarget = FVector(Kick.Target.X, CleanY, CleanZ);
	Kick.Power = Power;
	Kick.ShotType = Type;
	Kick.ActionRate = Type == EPKShotType::Power ? 0.72f : (bQuick ? 1.35f : (Type == EPKShotType::Chip ? 0.95f : 1.f));
	if (TryStartKick(P, Kick, bAirborne))
	{
		P->ShotStartTime = GetWorld()->GetTimeSeconds();
		P->TimedShotPress = -10.0;
	}
}

EPKShotType APKGameMode::ChooseAIShot(const APKFootballer* P, float DGoal) const
{
	// Keeper off his line: chip him. Long range: power. Angled / technical: finesse. Sometimes low and hard.
	if (const APKFootballer* GK = Keeper(1 - P->Team))
	{
		const float Off = FMath::Abs(GK->GetActorLocation().X - OppGoal(P->Team).X);
		if (Off > 450.f && DGoal > 1100.f && DGoal < 3000.f && FMath::FRand() < 0.6f)
		{
			return EPKShotType::Chip;
		}
	}
	if (DGoal > 2200.f && (P->HasTrait(PKTrait::PowerShot) || FMath::FRand() < 0.3f))
	{
		return EPKShotType::Power;
	}
	if (P->HasTrait(PKTrait::Finesse) || (DGoal < 2200.f && FMath::FRand() < 0.35f))
	{
		return EPKShotType::Finesse;
	}
	return FMath::FRand() < 0.2f ? EPKShotType::Low : EPKShotType::Normal;
}

const TCHAR* APKGameMode::ShotTypeName(EPKShotType Type)
{
	switch (Type)
	{
	case EPKShotType::Finesse: return TEXT("FINESSE SHOT");
	case EPKShotType::Chip:    return TEXT("CHIP SHOT");
	case EPKShotType::Power:   return TEXT("POWER SHOT");
	case EPKShotType::Low:     return TEXT("LOW DRIVEN SHOT");
	default:                   return TEXT("SHOT");
	}
}"""),
# AI open play shot uses a technique
("""			RequestShot(P, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.55f, 1.f));
			return;
		}

		if (TryCross(P))""", """			RequestShot(P, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.55f, 1.f), false, ChooseAIShot(P, DGoal));
			return;
		}

		if (TryCross(P))"""),
# ExecuteKick: local copy, timed finishing
("""void APKGameMode::ExecuteKick(APKFootballer* P, const FPKKickRequest& Kick)
{""", """void APKGameMode::ExecuteKick(APKFootballer* P, const FPKKickRequest& InKick)
{
	FPKKickRequest Kick = InKick;
	if (Kick.Type == EPKKickType::Shot && P->TimedShotPress > P->ShotStartTime)
	{
		// Timed finishing: the second press landed just before the strike (green) or not (red).
		const double Early = GetWorld()->GetTimeSeconds() - P->TimedShotPress;
		const bool bGreen = Early >= 0.0 && Early <= 0.11;
		Kick.Target = bGreen ? FMath::Lerp(Kick.Target, Kick.CleanTarget, 0.65f) : Kick.CleanTarget + (Kick.Target - Kick.CleanTarget) * 1.7f + FVector(0.f, 0.f, 35.f);
		bGreen ? ++TimedGreen : ++TimedRed;
		if (P->bHuman)
		{
			ShotFeedback = bGreen ? TEXT("PERFECT TIMING") : TEXT("MISTIMED");
			ShotFeedbackColor = bGreen ? FLinearColor(0.2f, 1.f, 0.3f) : FLinearColor(1.f, 0.25f, 0.2f);
			ShotFeedbackTimer = 1.4f;
		}
		P->TimedShotPress = -10.0;
	}"""),
# Shot physics per technique
("""		float Speed = (2000.f + 1900.f * Kick.Power) * FMath::Lerp(0.9f, 1.05f, P->Attr.Shooting);
		if (P->HasTrait(PKTrait::PowerShot))
		{
			Speed *= 1.12f;
		}
		if (P->HasTrait(PKTrait::Finesse))
		{
			Speed *= 0.9f;
		}
		const float T = D / Speed;
		Velocity = Dir * Speed;
		Velocity.Z = (Kick.Target.Z - From.Z) / T + 0.5f * PK::Gravity * T;
		Spin = Side * FMath::FRandRange(-250.f, 250.f);
		if (P->HasTrait(PKTrait::Finesse))
		{""", """		const EPKShotType ShotType = Kick.ShotType;
		float Speed = (2000.f + 1900.f * Kick.Power) * FMath::Lerp(0.9f, 1.05f, P->Attr.Shooting);
		if (P->HasTrait(PKTrait::PowerShot))
		{
			Speed *= ShotType == EPKShotType::Power ? 1.1f : 1.12f;
		}
		switch (ShotType)
		{
		case EPKShotType::Power:   Speed *= 1.22f; break;
		case EPKShotType::Finesse: Speed *= 0.85f; break;
		case EPKShotType::Low:     Speed *= 1.05f; break;
		default:                   Speed *= P->HasTrait(PKTrait::Finesse) ? 0.9f : 1.f; break;
		}
		float T = D / Speed;
		Velocity = Dir * Speed;
		Velocity.Z = (Kick.Target.Z - From.Z) / T + 0.5f * PK::Gravity * T;
		Spin = Side * FMath::FRandRange(-250.f, 250.f);
		if (ShotType == EPKShotType::Chip)
		{
			// Chip: a slow, high arc that drops under the bar (air drag included).
			T = D / (800.f + 500.f * Kick.Power);
			const float K = PK::AirDrag;
			const float E = 1.f - FMath::Exp(-K * T);
			Velocity = Dir * (D * K / E);
			Velocity.Z = (Kick.Target.Z - From.Z) / T * (K * T / E) + PK::Gravity * T / E - PK::Gravity / K;
			Spin = FVector::ZeroVector;
		}
		else if (ShotType == EPKShotType::Low)
		{
			Spin *= 0.4f; // struck through the middle: little swerve
		}
		if (ShotType == EPKShotType::Finesse || (ShotType == EPKShotType::Normal && P->HasTrait(PKTrait::Finesse)))
		{"""),
("""		++Shots[P->Team];
		UE_LOG(LogPitchKings, Log, TEXT("%s #%d shoots from %.0f m (power %.2f)"), PK::TeamName(P->Team), P->Index, D / 100.f, Kick.Power);""",
"""		++Shots[P->Team];
		++ShotTypeCount[FMath::Clamp((int32)ShotType, 0, 4)];
		if (P->bHuman && ShotType != EPKShotType::Normal && ShotFeedbackTimer <= 0.f)
		{
			ShotFeedback = ShotTypeName(ShotType);
			ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
			ShotFeedbackTimer = 1.2f;
		}
		UE_LOG(LogPitchKings, Log, TEXT("%s #%d %s from %.0f m (power %.2f)"), PK::TeamName(P->Team), P->Index, ShotType == EPKShotType::Normal ? TEXT("shoots") : *FString(ShotTypeName(ShotType)).ToLower(), D / 100.f, Kick.Power);"""),
# Human shoot with type
("""void APKGameMode::HumanShoot(int32 Team, const FVector& Aim, float Power)
{""", """void APKGameMode::HumanShoot(int32 Team, const FVector& Aim, float Power, EPKShotType Type)
{"""),
("""	RequestShot(Human, Aim.Y * PK::AttackDir(Human->Team), FMath::Clamp(Power, 0.1f, 1.f));
}""", """	RequestShot(Human, Aim.Y * PK::AttackDir(Human->Team), FMath::Clamp(Power, 0.1f, 1.f), false, Type);
}

bool APKGameMode::HumanTimedFinish(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Human || !Human->IsKicking() || Now - Human->ShotStartTime > 1.5)
	{
		return false;
	}
	if (Human->TimedShotPress < Human->ShotStartTime)
	{
		Human->TimedShotPress = Now; // only the first press counts
	}
	return true;
}

void APKGameMode::HumanFakeShot(int32 Team, const FVector& Aim)
{
	FakeShot(Humans[FMath::Clamp(Team, 0, 1)], Aim);
}

void APKGameMode::FakeShot(APKFootballer* P, const FVector& Aim)
{
	// Shape to shoot, then drag it across the body with the inside of the other foot.
	if (!P || Ball->GetController() != P || !CanReachBall(P, 100.f) || P->IsKicking() || P->IsInAction())
	{
		return;
	}
	const FVector Facing = P->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	float SideSign = FVector::DotProduct(Aim, Right);
	SideSign = FMath::Abs(SideSign) > 0.2f ? FMath::Sign(SideSign) : (FMath::FRand() < 0.5f ? -1.f : 1.f);
	Ball->Touch(P, PK::Flat(P->GetVelocity()) * 0.4f + Right * SideSign * 330.f + Facing * 60.f);
	P->GetCharacterMovement()->Velocity = PK::Flat(P->GetVelocity()) * 0.5f + Right * SideSign * 300.f;
	P->NoteFootTouch(SideSign > 0.f ? 0 : 1, SideSign, false);
	P->TouchTimer = 0.4f;
	P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.4f);
	for (APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const FVector To = O->GetFlatLocation() - P->GetFlatLocation();
		const bool bKeeperNear = O->IsKeeper() && To.Size() < 1600.f;
		if ((To.Size() < 420.f && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.f) || bKeeperNear)
		{
			const float Chance = (bKeeperNear ? 0.3f : 0.45f) + 0.3f * P->Attr.Control - 0.25f * O->Attr.Defending;
			if (FMath::FRand() < Chance)
			{
				O->Stun(bKeeperNear ? 0.5f : 0.55f); // bought it: committed to the block
				O->StealCooldown = 0.9f;
			}
		}
	}
	++FakeShots;
	if (P->bHuman)
	{
		ShotFeedback = TEXT("FAKE SHOT");
		ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
		ShotFeedbackTimer = 1.f;
	}
	EventShot(TEXT("Feint"), P);
}"""),
])

# Feedback timer tick: next to MessageTimer
patch("PKGameMode.cpp", [
("""	MessageTimer = FMath::Max(0.f, MessageTimer - """, """	ShotFeedbackTimer = FMath::Max(0.f, ShotFeedbackTimer - DeltaSeconds);
	MessageTimer = FMath::Max(0.f, MessageTimer - """),
])

# Controller: shot types on release, timed finishing, fake shot
patch("PKPlayerController.cpp", [
("""		for (int32 i = 0; i < 3 && !bPassPressed; ++i)
		{
			if (!ActionPressed(Dev, PassInputs[i]))
			{
				continue;
			}
			bPassPressed = true;""", """		for (int32 i = 0; i < 3 && !bPassPressed; ++i)
		{
			if (!ActionPressed(Dev, PassInputs[i]))
			{
				continue;
			}
			bPassPressed = true;
			if (bCharging)
			{
				// Pass while charging a shot: fake shot.
				GM->HumanFakeShot(Team, Move);
				bCharging = false;
				break;
			}"""),
("""		// Shot: hold to charge, release (or full charge) to shoot.
		if (ActionPressed(Dev, EPKInput::Shoot))
		{
			bCharging = true;
			Charge = 0.f;
		}
		if (bCharging)
		{
			Charge = FMath::Min(1.f, Charge + DeltaTime / ShotChargeSeconds);
			if (ActionReleased(Dev, EPKInput::Shoot) || !ActionDown(Dev, EPKInput::Shoot) || Charge >= 1.f)
			{
				GM->HumanShoot(Team, Move, Charge);
				bCharging = false;
			}
		}""", """		// Shot: hold to charge, release (or full charge) to shoot. The modifiers held at release pick the technique:
		// Precision = finesse, Runs = chip, both = power shot, Hold = low driven. Pressing shoot again during the
		// wind-up is timed finishing.
		if (ActionPressed(Dev, EPKInput::Shoot) && !GM->HumanTimedFinish(Team))
		{
			bCharging = true;
			Charge = 0.f;
		}
		if (bCharging)
		{
			Charge = FMath::Min(1.f, Charge + DeltaTime / ShotChargeSeconds);
			if (ActionReleased(Dev, EPKInput::Shoot) || !ActionDown(Dev, EPKInput::Shoot) || Charge >= 1.f)
			{
				const bool bRuns = ActionDown(Dev, EPKInput::Runs);
				const EPKShotType Type = bPrecision && bRuns ? EPKShotType::Power : (bPrecision ? EPKShotType::Finesse : (bRuns ? EPKShotType::Chip : (bModifier ? EPKShotType::Low : EPKShotType::Normal)));
				GM->HumanShoot(Team, Move, Charge, Type);
				bCharging = false;
			}
		}"""),
])

# HUD feedback text above the power bar
patch("PKHUD.cpp", [
("""	// Shot power bars (one per human player) and the controls panel.
	if (const APKPlayerController* PC = Cast<APKPlayerController>(GetOwningPlayerController()))
	{""", """	// Shot power bars (one per human player) and the controls panel.
	if (const APKPlayerController* PC = Cast<APKPlayerController>(GetOwningPlayerController()))
	{
		if (GM->ShotFeedbackTimer > 0.f && !GM->ShotFeedback.IsEmpty())
		{
			FLinearColor C = GM->ShotFeedbackColor;
			C.A = FMath::Clamp(GM->ShotFeedbackTimer / 0.4f, 0.f, 1.f);
			DrawCentered(GM->ShotFeedback, Canvas->ClipY - 280.f, 1.4f, C);
		}"""),
])
print("layer3 ok")
