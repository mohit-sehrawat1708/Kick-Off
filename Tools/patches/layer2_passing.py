# Layer 2: Precision Pass (power bar, exact spot), Swerve on demand, ground curl, Headers on demand.
import os, sys
SRC = os.path.join(os.path.dirname(__file__), "..", "..", "Source", "PitchKings")

def patch(name, pairs):
    p = os.path.join(SRC, name)
    s = open(p, encoding="utf-8").read()
    for a, b in pairs:
        if b in s:
            continue  # already applied
        if a not in s:
            sys.exit(f"{name}: not found: {a[:70]!r}")
        s = s.replace(a, b, 1)
    open(p, "w", encoding="utf-8").write(s)

patch("PKFootballer.h", [
("""	TWeakObjectPtr<class APKFootballer> Receiver;
	bool bPending = false;
};""", """	TWeakObjectPtr<class APKFootballer> Receiver;
	bool bPending = false;
	bool bPrecision = false; // played to an exact spot: no automatic chip over a defender, softer arrival
	float Swerve = 0.f;      // bend: +1 the path bulges right of the straight line, -1 left
};"""),
("""	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step""",
"""	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step
	// Header / volley on demand: the person pressed pass or shoot for a ball in the air.
	float HeaderRequestTimer = 0.f;
	bool bHeaderRequestShot = false;
	FVector HeaderRequestAim = FVector::ZeroVector;"""),
])

patch("PKFootballer.cpp", [
("""	TouchTimer = FMath::Max(0.f, TouchTimer - DeltaSeconds);""",
"""	TouchTimer = FMath::Max(0.f, TouchTimer - DeltaSeconds);
	HeaderRequestTimer = FMath::Max(0.f, HeaderRequestTimer - DeltaSeconds);"""),
])

patch("PKBall.cpp", [
("""			Velocity.Z = 0.f;
			const float Speed = FVector2D(Velocity.X, Velocity.Y).Size();""",
"""			Velocity.Z = 0.f;
			if (!Spin.IsNearlyZero())
			{
				// A swerved ground pass bends too (less than in the air): sideways to the way it is rolling.
				const FVector Roll = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Roll);
				Velocity += Side * FVector::DotProduct(Spin, Side) * 0.5f * Dt;
			}
			const float Speed = FVector2D(Velocity.X, Velocity.Y).Size();"""),
])

patch("PKGameMode.h", [
("""	void RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type);
	void RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type);""",
"""	void RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type, bool bSwerve = false);
	void RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type, bool bSwerve = false);"""),
("""	void HumanPass(int32 Team, const FVector& Aim, EPKKickType Type);""",
"""	void HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve = false);
	// Precision pass: to the exact spot aimed at, distance from the power bar (0..1); the nearest team-mate runs onto it.
	void HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve);
	// Header / volley on demand for a ball in the air (pass or shoot pressed without the ball).
	bool HumanRequestHeader(int32 Team, const FVector& Aim, bool bShot);"""),
("""	int32 EffortTouches = 0;""", """	int32 EffortTouches = 0;
	int32 PrecisionPasses = 0;
	int32 SwervePasses = 0;
	int32 HeadersOnDemand = 0;
	float ChooseSwerve(const APKFootballer* P, const FVector& Target) const;"""),
])

patch("PKGameMode.cpp", [
# HumanPass with swerve
("""void APKGameMode::HumanPass(int32 Team, const FVector& Aim, EPKKickType Type)
{""", """void APKGameMode::HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve)
{"""),
("""	if (Best)
	{
		RequestPass(Human, Best, Type);
	}
	else
	{
		RequestPassToPoint(Human, Loc + AimDir * 1500.f, Type);
	}
}""", """	if (Best)
	{
		RequestPass(Human, Best, Type, bSwerve);
	}
	else
	{
		RequestPassToPoint(Human, Loc + AimDir * 1500.f, Type, bSwerve);
	}
	SwervePasses += bSwerve ? 1 : 0;
}

float APKGameMode::ChooseSwerve(const APKFootballer* P, const FVector& Target) const
{
	// Bend it around the defender nearest the line: the path bulges away from him. Nobody there: bend it the
	// way the passing foot naturally curls it (right-footed: bulges right).
	const FVector A = P->GetFlatLocation();
	const FVector B = PK::Flat(Target);
	const FVector Dir = (B - A).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
	const float Len = FVector::Dist(A, B);
	float BestDist = 350.f;
	float Bulge = 1.f;
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const FVector L = O->GetFlatLocation();
		const float Along = FVector::DotProduct(L - A, Dir);
		if (Along < 150.f || Along > Len - 150.f)
		{
			continue;
		}
		const float Off = FVector::DotProduct(L - A, Side);
		if (FMath::Abs(Off) < BestDist)
		{
			BestDist = FMath::Abs(Off);
			Bulge = Off >= 0.f ? -1.f : 1.f;
		}
	}
	return Bulge;
}

void APKGameMode::HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}
	if (!Ball->IsFree() && RestartTimer < 0.4f)
	{
		return;
	}
	const FVector AimDir = Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D();
	const FVector Loc = Human->GetFlatLocation();
	const float C = FMath::Clamp(Charge, 0.f, 1.f);
	const float Dist = Type == EPKKickType::Lob ? FMath::Lerp(900.f, 4500.f, C) : FMath::Lerp(400.f, 4000.f, C);
	FVector Point = Loc + AimDir * Dist;
	Point.X = FMath::Clamp(Point.X, -PK::HalfLength + 100.f, PK::HalfLength - 100.f);
	Point.Y = FMath::Clamp(Point.Y, -PK::HalfWidth + 100.f, PK::HalfWidth - 100.f);
	// The team-mate best placed to get there runs onto it.
	APKFootballer* Receiver = nullptr;
	float BestT = 3.5f;
	for (APKFootballer* T : Footballers)
	{
		if (T == Human || T->Team != Human->Team || T->IsKeeper())
		{
			continue;
		}
		const float Time = FVector::Dist(T->GetFlatLocation(), Point) / PK::SprintSpeed;
		if (Time < BestT)
		{
			BestT = Time;
			Receiver = T;
		}
	}
	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;
	Kick.Power = FMath::Clamp(Dist / 4000.f, 0.2f, 1.f);
	Kick.Receiver = Receiver;
	Kick.bPrecision = true;
	Kick.Swerve = bSwerve ? ChooseSwerve(Human, Point) : 0.f;
	if (Type == EPKKickType::Through)
	{
		++ThroughBalls[Human->Team];
	}
	++PrecisionPasses;
	SwervePasses += bSwerve ? 1 : 0;
	TryStartKick(Human, Kick);
}

bool APKGameMode::HumanRequestHeader(int32 Team, const FVector& Aim, bool bShot)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human || Phase != EPKPhase::Playing || Ball->GetOwnerPlayer() || Ball->GetHolder() || Human->IsInAction())
	{
		return false;
	}
	// Only for a ball in the air that is coming his way.
	const FVector BallLoc = Ball->GetActorLocation();
	if (BallLoc.Z < 80.f || FVector::Dist(PK::Flat(BallLoc), Human->GetFlatLocation()) > 900.f)
	{
		return false;
	}
	Human->HeaderRequestTimer = 1.2f;
	Human->bHeaderRequestShot = bShot;
	Human->HeaderRequestAim = Aim;
	return true;
}"""),
("""void APKGameMode::RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type)
{""", """void APKGameMode::RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type, bool bSwerve)
{"""),
("""	Kick.Receiver = Target;
	if (Type == EPKKickType::Through)
	{
		++ThroughBalls[P->Team];
	}""", """	Kick.Receiver = Target;
	Kick.Swerve = bSwerve ? ChooseSwerve(P, Point) : 0.f;
	if (Type == EPKKickType::Through)
	{
		++ThroughBalls[P->Team];
	}"""),
("""void APKGameMode::RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type)
{
	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;""", """void APKGameMode::RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type, bool bSwerve)
{
	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;
	Kick.Swerve = bSwerve ? ChooseSwerve(P, Point) : 0.f;"""),
# ExecuteKick: precision accuracy, no auto chip, softer, swerve
("""		Dir = Dir.RotateAngleAxis(FMath::FRandRange(-10.f, 10.f) * Sloppy, FVector::UpVector);
		SpeedScale = 1.f + FMath::FRandRange(-0.15f, 0.15f) * Sloppy;""",
"""		if (Kick.bPrecision)
		{
			// Played to a spot: the error grows with distance instead of being a fixed spray.
			const float Far = FMath::Clamp(D / 3000.f, 0.3f, 1.2f);
			Dir = Dir.RotateAngleAxis(FMath::FRandRange(-6.f, 6.f) * Sloppy * Far, FVector::UpVector);
			SpeedScale = 1.f + FMath::FRandRange(-0.08f, 0.08f) * Sloppy;
		}
		else
		{
			Dir = Dir.RotateAngleAxis(FMath::FRandRange(-10.f, 10.f) * Sloppy, FVector::UpVector);
			SpeedScale = 1.f + FMath::FRandRange(-0.15f, 0.15f) * Sloppy;
		}"""),
("""		else if (!bThrow)
		{
			// Whipped in: slight curl toward the target side.
			Spin = Side * FMath::FRandRange(-180.f, 180.f);
		}
		break;""", """		else if (!bThrow && Kick.Swerve != 0.f)
		{
			// Swerved lob: starts out on the bulge side and bends back onto the spot.
			Spin = -Side * Kick.Swerve * 420.f;
			Velocity += Side * Kick.Swerve * 420.f * T * 0.5f;
		}
		else if (!bThrow)
		{
			// Whipped in: slight curl toward the target side.
			Spin = Side * FMath::FRandRange(-180.f, 180.f);
		}
		break;"""),
("""		if (!bFromHands && D > 900.f && PassLaneBlocked(P, From, Kick.Target))""", """		if (!bFromHands && D > 900.f && !Kick.bPrecision && Kick.Swerve == 0.f && PassLaneBlocked(P, From, Kick.Target))"""),
("""		float Speed = FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);""", """		float Speed = FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + (Kick.bPrecision ? 150.f : 250.f), Kick.bPrecision ? 500.f : 700.f, 3000.f);"""),
("""		else if (D > 1800.f)
		{
			// Driven pass""", """		else if (Kick.Swerve != 0.f)
		{
			// Swerved along the ground: aimed out on the bulge side, the spin bends it back (the ball curls at
			// half strength while rolling) so it still arrives at the spot.
			const float Curl = 520.f;
			const float T = D / FMath::Max(Speed * 0.7f, 1.f);
			Spin = -Side * Kick.Swerve * Curl;
			Velocity += Side * Kick.Swerve * Curl * 0.5f * T * 0.5f;
			Velocity.Z = 0.f; // a bounce would take the spin off
		}
		else if (D > 1800.f)
		{
			// Driven pass"""),
# CheckPickups: header / volley on demand
("""		// Volley: an attacker meets a dropping ball at waist/chest height within shooting range.
		if (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f && Ball->Velocity.Z < 50.f
			&& P->StealCooldown <= 0.f && FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < 2400.f && Ball->Velocity.Size() > 400.f)
		{
			P->StealCooldown = 0.8f;
			if (FMath::FRand() < 0.5f + 0.3f * P->Attr.Shooting)""",
"""		// Header / volley on demand: the person asked for it, so he goes for it whatever the situation.
		const bool bRequested = P->HeaderRequestTimer > 0.f && P->bHuman;
		if (bRequested && !P->IsInAction() && P->StealCooldown <= 0.f && D < 110.f && BallLoc.Z > 85.f && BallLoc.Z < 240.f)
		{
			if (CheckOffside(P))
			{
				return;
			}
			P->StealCooldown = 0.5f;
			P->HeaderRequestTimer = 0.f;
			++HeadersOnDemand;
			const FVector Aim = P->HeaderRequestAim.IsNearlyZero() ? P->GetFacing() : P->HeaderRequestAim.GetSafeNormal2D();
			if (P->bHeaderRequestShot && BallLoc.Z < 120.f)
			{
				Ball->SetController(P); // waist high: volley it
				RequestShot(P, FMath::Clamp(FVector::DotProduct(Aim, FVector(0.f, PK::AttackDir(P->Team), 0.f)), -1.f, 1.f), 0.85f, true);
				EventShot(TEXT("Volley"), P);
				continue;
			}
			FVector Vel;
			if (P->bHeaderRequestShot)
			{
				// Header at goal: down towards the corner he aims at.
				const FVector Goal = OppGoal(P->Team);
				const float Lateral = FMath::Clamp(FVector::DotProduct(Aim, FVector(0.f, PK::AttackDir(P->Team), 0.f)), -1.f, 1.f);
				const FVector Corner = Goal + FVector(0.f, Lateral * (PK::GoalHalfWidth - 60.f) * PK::AttackDir(P->Team), 0.f);
				const FVector To = PK::Flat(Corner - BallLoc);
				const float Speed = FMath::Lerp(1100.f, 1600.f, P->Attr.Shooting);
				Vel = To.GetSafeNormal() * Speed;
				Vel.Z = FMath::Clamp((60.f - BallLoc.Z) / FMath::Max(To.Size() / Speed, 0.2f) + 0.5f * PK::Gravity * To.Size() / Speed, -400.f, 200.f);
				++Shots[P->Team];
			}
			else
			{
				// Headed pass to the team-mate in that direction (or into space).
				APKFootballer* Mate = nullptr;
				float BestCos = 0.7f;
				for (APKFootballer* T : Footballers)
				{
					if (T == P || T->Team != P->Team || T->IsKeeper())
					{
						continue;
					}
					const FVector To = T->GetFlatLocation() - P->GetFlatLocation();
					const float Cos = FVector::DotProduct(To.GetSafeNormal2D(), Aim) - To.Size() / 8000.f;
					if (To.Size() < 2200.f && Cos > BestCos)
					{
						BestCos = Cos;
						Mate = T;
					}
				}
				const FVector Point = Mate ? Mate->GetFlatLocation() : P->GetFlatLocation() + Aim * 900.f;
				const float Dist = FVector::Dist(PK::Flat(BallLoc), Point);
				const float T = FMath::Clamp(Dist / 1000.f, 0.5f, 1.3f);
				Vel = (Point - PK::Flat(BallLoc)).GetSafeNormal2D() * (Dist / T * 0.85f);
				Vel.Z = 0.5f * PK::Gravity * T - (BallLoc.Z - PK::BallRadius) / T;
				if (Mate)
				{
					Ball->IntendedReceiver = Mate;
					++Passes[P->Team];
				}
			}
			P->SetActorRotation(FRotator(0.f, PK::Flat(Vel).Rotation().Yaw, 0.f));
			QueueTouch(P, EPKAction::Header, Vel, true);
			EventShot(TEXT("Header"), P);
			continue;
		}

		// Volley: an attacker meets a dropping ball at waist/chest height within shooting range.
		if (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f && Ball->Velocity.Z < 50.f
			&& P->StealCooldown <= 0.f && FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < 2400.f && Ball->Velocity.Size() > 400.f)
		{
			P->StealCooldown = 0.8f;
			if (FMath::FRand() < 0.5f + 0.3f * P->Attr.Shooting)"""),
])

# Controller: precision charge, swerve flag, header request.
patch("PKPlayerController.h", [
("""	bool bRightStickFlick[2] = { false, false };""", """	bool bRightStickFlick[2] = { false, false };
	// Precision pass power being charged (Precision held + pass / through / lob).
	bool bPassCharging[2] = { false, false };
	float PassCharge[2] = { 0.f, 0.f };
	EPKInput PassChargeInput[2] = { EPKInput::Pass, EPKInput::Pass };
public:
	float GetPassCharge(int32 Team = 0) const { return bPassCharging[Team & 1] ? PassCharge[Team & 1] : 0.f; }
private:"""),
])

patch("PKPlayerController.cpp", [
("""		if (ActionPressed(Dev, EPKInput::Pass))
		{
			GM->HumanPass(Team, Move, EPKKickType::Ground);
		}
		else if (ActionPressed(Dev, EPKInput::Through))
		{
			GM->HumanPass(Team, Move, EPKKickType::Through);
		}
		else if (ActionPressed(Dev, EPKInput::Lob))
		{
			GM->HumanPass(Team, Move, EPKKickType::Lob);
		}""", """		// Pass buttons: with Precision held, hold for power and release to play it to the spot; with Hold (LT / Ctrl)
		// held the pass is swerved around the nearest defender.
		const EPKInput PassInputs[] = { EPKInput::Pass, EPKInput::Through, EPKInput::Lob };
		const EPKKickType PassTypes[] = { EPKKickType::Ground, EPKKickType::Through, EPKKickType::Lob };
		bool bPassPressed = false;
		for (int32 i = 0; i < 3 && !bPassPressed; ++i)
		{
			if (!ActionPressed(Dev, PassInputs[i]))
			{
				continue;
			}
			bPassPressed = true;
			if (bPrecision)
			{
				bPassCharging[Team] = true;
				PassCharge[Team] = 0.f;
				PassChargeInput[Team] = PassInputs[i];
			}
			else
			{
				GM->HumanPass(Team, Move, PassTypes[i], bModifier);
			}
		}
		if (bPassCharging[Team])
		{
			PassCharge[Team] = FMath::Min(1.f, PassCharge[Team] + DeltaTime / 0.9f);
			if (!ActionDown(Dev, PassChargeInput[Team]) || PassCharge[Team] >= 1.f)
			{
				const int32 Idx = PassChargeInput[Team] == EPKInput::Through ? 1 : (PassChargeInput[Team] == EPKInput::Lob ? 2 : 0);
				GM->HumanPrecisionPass(Team, Move, PassTypes[Idx], PassCharge[Team], bModifier);
				bPassCharging[Team] = false;
			}
		}
"""),
("""	else
	{
		bCharging = false;
		bEffortHeld[Team] = false;
		if (ActionPressed(Dev, EPKInput::Tackle))""", """	else
	{
		bCharging = false;
		bEffortHeld[Team] = false;
		bPassCharging[Team] = false;
		// A ball in the air coming his way: pass / shoot ask for a header (or a volley at waist height).
		bool bHeaderAsked = false;
		if (ActionPressed(Dev, EPKInput::Pass) || ActionPressed(Dev, EPKInput::Through) || ActionPressed(Dev, EPKInput::Lob))
		{
			bHeaderAsked = GM->HumanRequestHeader(Team, Move, false);
		}
		else if (ActionPressed(Dev, EPKInput::Shoot))
		{
			bHeaderAsked = GM->HumanRequestHeader(Team, Move, true);
		}
		if (ActionPressed(Dev, EPKInput::Tackle) && !bHeaderAsked)"""),
("""		if (ActionPressed(Dev, EPKInput::Barge))
		{
			GM->HumanShoulder(Team);
		}""", """		if (ActionPressed(Dev, EPKInput::Barge) && !bHeaderAsked)
		{
			GM->HumanShoulder(Team);
		}"""),
])

# HUD: precision pass power bar
patch("PKHUD.cpp", [
("""			const float Charge = PC->GetShotCharge(Team);
			if (Charge <= 0.f)
			{
				continue;
			}""", """			const float PassCharge = PC->GetPassCharge(Team);
			const bool bPassBar = PassCharge > 0.f;
			const float Charge = bPassBar ? PassCharge : PC->GetShotCharge(Team);
			if (Charge <= 0.f)
			{
				continue;
			}"""),
("""			DrawRect(FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.9f, 0.1f), FLinearColor(1.f, 0.1f, 0.05f), Charge), X, Y, W * Charge, 16.f);
			DrawText(bVersus ? (Team == 0 ? TEXT("P1 SHOT POWER") : TEXT("P2 SHOT POWER")) : TEXT("SHOT POWER"), FLinearColor::White, X, Y - 22.f, GEngine->GetSmallFont(), 1.f);""",
"""			DrawRect(bPassBar ? FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.6f, 1.f), FLinearColor(1.f, 0.85f, 0.f), Charge) : FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.9f, 0.1f), FLinearColor(1.f, 0.1f, 0.05f), Charge), X, Y, W * Charge, 16.f);
			const FString BarName = bPassBar ? TEXT("PRECISION PASS") : TEXT("SHOT POWER");
			DrawText(bVersus ? (Team == 0 ? TEXT("P1 ") : TEXT("P2 ")) + BarName : BarName, FLinearColor::White, X, Y - 22.f, GEngine->GetSmallFont(), 1.f);"""),
])
print("layer2 ok")
