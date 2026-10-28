import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def patch(name, pairs):
    path = os.path.join(base, name)
    s = open(path, encoding="utf-8-sig").read().replace("\r\n", "\n")
    for a, b in pairs:
        if a not in s:
            raise SystemExit("NOT FOUND in %s: %s" % (name, a[:100]))
        s = s.replace(a, b, 1)
    open(path, "w", encoding="utf-8").write(s)


# ---- ball: livelier bounces on grass, backspin/topspin visual roll in the air
patch("PKBall.cpp", [
    ("constexpr float BounceRestitution = 0.55f;", "constexpr float BounceRestitution = 0.62f;"),
    ("\t\tif (Velocity.Z < -120.f)\n\t\t{\n\t\t\tVelocity.Z = -Velocity.Z * BounceRestitution;\n\t\t\tVelocity.X *= 0.9f;\n\t\t\tVelocity.Y *= 0.9f;",
     "\t\tif (Velocity.Z < -70.f)\n\t\t{\n\t\t\t// Grass bounce: keeps some height, skids a little off a fast ball.\n"
     "\t\t\tVelocity.Z = -Velocity.Z * BounceRestitution;\n\t\t\tVelocity.X *= 0.88f;\n\t\t\tVelocity.Y *= 0.88f;"),
])

# ---- game mode
patch("PKGameMode.h", [
    ("\tvoid HumanFeint(const FVector& Aim);",
     "\tvoid HumanFeint(const FVector& Aim);\n\tvoid HumanFlickUp(const FVector& Aim);"),
    ("\tfloat FinalThirdTime[2] = { 0.f, 0.f };",
     "\tfloat FinalThirdTime[2] = { 0.f, 0.f };\n\tfloat AirTime = 0.f;\n\tfloat MaxBallZ = 0.f;\n\tint32 Chips = 0;\n\tint32 Crosses = 0;\n\tint32 Flicks = 0;"),
    ("\tvoid EventShot(const TCHAR* Label, const AActor* Focus = nullptr);",
     "\tvoid EventShot(const TCHAR* Label, const AActor* Focus = nullptr);\n"
     "\tbool PassLaneBlocked(const APKFootballer* P, const FVector& From, const FVector& To) const;\n"
     "\tbool TryCross(APKFootballer* P);\n"
     "\tvoid FlickUp(APKFootballer* P, const FVector& Dir);"),
])

patch("PKGameMode.cpp", [
    # stats
    ("final third %.0fs-%.0fs | through %d-%d\"), Score[0], Score[1], Shots[0], Shots[1], Passes[0], Passes[1], Steals[0], Steals[1], FinalThirdTime[0], FinalThirdTime[1], ThroughBalls[0], ThroughBalls[1]);",
     "final third %.0fs-%.0fs | through %d-%d | ball airborne %.0fs (max %.1f m) | chips %d crosses %d flicks %d\"), Score[0], Score[1], Shots[0], Shots[1], Passes[0], Passes[1], Steals[0], Steals[1], FinalThirdTime[0], FinalThirdTime[1], ThroughBalls[0], ThroughBalls[1], AirTime, MaxBallZ / 100.f, Chips, Crosses, Flicks);"),
    ("\t\tif (APKFootballer* BallOwner = Ball->GetOwnerPlayer())\n\t\t{\n\t\t\tPossessionTime[BallOwner->Team] += DeltaSeconds;",
     "\t\tif (Ball->IsFree() && Ball->GetActorLocation().Z > 30.f)\n\t\t{\n\t\t\tAirTime += DeltaSeconds;\n"
     "\t\t\tMaxBallZ = FMath::Max(MaxBallZ, Ball->GetActorLocation().Z);\n\t\t}\n"
     "\t\tif (APKFootballer* BallOwner = Ball->GetOwnerPlayer())\n\t\t{\n\t\t\tPossessionTime[BallOwner->Team] += DeltaSeconds;"),

    # shots: power shots can rise over the bar, poor finishers skew higher
    ("\tconst float AimZ = 30.f + 180.f * Power * Power + FMath::FRandRange(-35.f, 35.f) * Power;",
     "\tfloat AimZ = 30.f + 180.f * Power * Power + FMath::FRandRange(-35.f, 35.f) * Power;\n"
     "\tAimZ += FMath::FRandRange(-20.f, 110.f) * Power * Power * FMath::Lerp(1.4f, 0.4f, P->Attr.Shooting) * (bAirborne ? 1.5f : 1.f);"),

    # passes: driven passes leave the ground, blocked lanes get chipped
    ("\tdefault:\n\t{\n\t\tfloat Speed = FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);",
     "\tdefault:\n\t{\n"
     "\t\tif (!bFromHands && D > 900.f && PassLaneBlocked(P, From, Kick.Target))\n\t\t{\n"
     "\t\t\t// Defender in the lane: dink it over him.\n"
     "\t\t\tconst float T = FMath::Clamp(D / 950.f, 1.f, 1.9f);\n"
     "\t\t\tVelocity = Dir * (D / T * 0.8f) * SpeedScale;\n"
     "\t\t\tVelocity.Z = 0.5f * PK::Gravity * T;\n"
     "\t\t\t++Chips;\n\t\t\tEventShot(TEXT(\"Chip\"), P);\n\t\t\tbreak;\n\t\t}\n"
     "\t\tfloat Speed = FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);"),
    ("\t\tif (bFromHands)\n\t\t{\n\t\t\tVelocity.Z = 120.f; // rolled/thrown out of the hands\n\t\t}\n\t\tbreak;",
     "\t\tif (bFromHands)\n\t\t{\n\t\t\tVelocity.Z = 120.f; // rolled/thrown out of the hands\n\t\t}\n"
     "\t\telse if (D > 1800.f)\n\t\t{\n\t\t\t// Driven pass: struck through the ball, flies knee-to-waist high then skips.\n"
     "\t\t\tVelocity.Z = FMath::Lerp(160.f, 340.f, FMath::Clamp((D - 1800.f) / 2200.f, 0.f, 1.f)) * FMath::FRandRange(0.8f, 1.15f);\n\t\t}\n"
     "\t\telse if (D > 900.f)\n\t\t{\n\t\t\tVelocity.Z = FMath::FRandRange(60.f, 170.f); // skidding off the turf\n\t\t}\n"
     "\t\tbreak;"),

    # lobs and clearances: crosses curl, clearances go higher and further
    ("\t\tif (Kick.Type == EPKKickType::Clearance)\n\t\t{\n\t\t\tVelocity *= 1.15f;\n\t\t}\n\t\tbreak;",
     "\t\tif (Kick.Type == EPKKickType::Clearance)\n\t\t{\n\t\t\tVelocity *= 1.15f;\n\t\t\tVelocity.Z *= 1.1f;\n\t\t}\n"
     "\t\telse if (!bThrow)\n\t\t{\n\t\t\t// Whipped in: slight curl toward the target side.\n"
     "\t\t\tSpin = Side * FMath::FRandRange(-180.f, 180.f);\n\t\t}\n\t\tbreak;"),

    # sprint touches pop the ball off the turf
    ("\t\telse\n\t\t{\n\t\t\tNewVel = Desired * (FMath::Max(Speed, 150.f) * (1.f + Lead) + Base);\n\t\t}\n\t\tBall->Touch(P, NewVel);",
     "\t\telse\n\t\t{\n\t\t\tNewVel = Desired * (FMath::Max(Speed, 150.f) * (1.f + Lead) + Base);\n"
     "\t\t\tif (P->IsSprinting() && P->Stance == EPKStance::Normal)\n\t\t\t{\n"
     "\t\t\t\tNewVel.Z = FMath::FRandRange(70.f, 170.f); // knocked ahead, bobbling\n\t\t\t}\n"
     "\t\t\telse if (Speed > 250.f && FMath::FRand() < 0.3f)\n\t\t\t{\n\t\t\t\tNewVel.Z = FMath::FRandRange(40.f, 100.f);\n\t\t\t}\n\t\t}\n"
     "\t\tBall->Touch(P, NewVel);"),

    # heavy first touch pops up
    ("\t\tFirstTouch += TouchDir.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector) * FMath::FRandRange(250.f, 450.f);",
     "\t\tFirstTouch += TouchDir.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector) * FMath::FRandRange(250.f, 450.f);\n"
     "\t\tFirstTouch.Z = FMath::FRandRange(150.f, 380.f); // bounces up off the shin"),

    # human knock-on lifts it slightly
    ("\tBall->Touch(Human, Dir * (PK::Flat(Human->GetVelocity()).Size() + 750.f));",
     "\tBall->Touch(Human, Dir * (PK::Flat(Human->GetVelocity()).Size() + 750.f) + FVector(0.f, 0.f, 180.f));"),

    # AI: crosses from wide, flicks over a close defender
    ("\t\tfloat PassScore = 0.f;\n\t\tAPKFootballer* Target = BestPassTarget(P, false, PassScore);\n\t\t// Open grass ahead",
     "\t\tif (TryCross(P))\n\t\t{\n\t\t\treturn;\n\t\t}\n"
     "\t\tif (Opp && NearOpp < 260.f && P->Attr.Control > 0.72f && FMath::FRand() < 0.12f\n"
     "\t\t\t&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), (OppGoal(P->Team) - Loc).GetSafeNormal2D()) > 0.6f)\n\t\t{\n"
     "\t\t\tFlickUp(P, (OppGoal(P->Team) - Loc).GetSafeNormal2D());\n\t\t\treturn;\n\t\t}\n\n"
     "\t\tfloat PassScore = 0.f;\n\t\tAPKFootballer* Target = BestPassTarget(P, false, PassScore);\n\t\t// Open grass ahead"),
])

# new functions appended before ExecuteKick
path = os.path.join(base, "PKGameMode.cpp")
s = open(path, encoding="utf-8").read()
anchor = "void APKGameMode::ExecuteKick(APKFootballer* P, const FPKKickRequest& Kick)\n"
if anchor not in s:
    raise SystemExit("NOT FOUND ExecuteKick")
s = s.replace(anchor, r"""bool APKGameMode::PassLaneBlocked(const APKFootballer* P, const FVector& From, const FVector& To) const
{
	const FVector A = PK::Flat(From);
	const FVector B = PK::Flat(To);
	const float Len = FVector::Dist(A, B);
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team || O->IsStunned())
		{
			continue;
		}
		const FVector Loc = O->GetFlatLocation();
		const float Along = FVector::DotProduct(Loc - A, (B - A) / FMath::Max(Len, 1.f));
		if (Along < 250.f || Along > Len - 300.f)
		{
			continue; // too close to kicker or receiver to go over
		}
		if (FVector::Dist(Loc, FMath::ClosestPointOnSegment(Loc, A, B)) < 85.f)
		{
			return true;
		}
	}
	return false;
}

bool APKGameMode::TryCross(APKFootballer* P)
{
	// Wide in the final third: whip it into the box for a header or volley.
	const FVector Loc = P->GetFlatLocation();
	if (FMath::Abs(Loc.Y) < 1700.f || WorldToFx(P->Team, Loc.X) < 0.55f || FMath::FRand() > 0.75f)
	{
		return false;
	}
	const FVector Goal = OppGoal(P->Team);
	APKFootballer* Best = nullptr;
	float BestScore = -1.f;
	for (APKFootballer* T : Footballers)
	{
		if (T->Team != P->Team || T == P || T->IsKeeper())
		{
			continue;
		}
		const FVector TL = T->GetFlatLocation();
		const float DGoal = FVector::Dist(TL, Goal);
		if (DGoal > 1800.f || FMath::Abs(TL.Y) > 1300.f)
		{
			continue;
		}
		float NearestDef = 9999.f;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != P->Team)
			{
				NearestDef = FMath::Min(NearestDef, FVector::Dist(O->GetFlatLocation(), TL));
			}
		}
		const float CrossScore = NearestDef - DGoal * 0.15f;
		if (CrossScore > BestScore)
		{
			BestScore = CrossScore;
			Best = T;
		}
	}
	if (!Best)
	{
		return false;
	}
	// Aim just beyond the runner so the ball drops through head height as it reaches him.
	const FVector Dir = (Best->GetFlatLocation() - Loc).GetSafeNormal2D();
	const FVector Point = Best->GetFlatLocation() + Dir * 170.f + PK::Flat(Best->GetVelocity()) * 0.6f;
	++Crosses;
	EventShot(TEXT("Cross"), P);
	RequestPassToPoint(P, Point, EPKKickType::Lob);
	Ball->IntendedReceiver = Best;
	return true;
}

void APKGameMode::FlickUp(APKFootballer* P, const FVector& Dir)
{
	// Scoop the ball up and over a close challenger, then run onto it.
	if (Ball->GetController() != P || !CanReachBall(P, 90.f) || P->IsKicking())
	{
		return;
	}
	P->PlayAction(EPKAction::ThighControl);
	Ball->Touch(P, Dir * (PK::Flat(P->GetVelocity()).Size() * 0.6f + 320.f) + FVector(0.f, 0.f, 520.f));
	P->TouchTimer = 0.9f;
	P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.6f);
	++Flicks;
	EventShot(TEXT("Flick"), P);
}

void APKGameMode::HumanFlickUp(const FVector& Aim)
{
	if (Human)
	{
		FlickUp(Human, Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D());
	}
}

""" + anchor, 1)
open(path, "w", encoding="utf-8").write(s)

patch("PKPlayerController.cpp", [
    ("\t\telse if (AnyPressed({ EKeys::T, EKeys::Gamepad_DPad_Up }))\n\t\t{\n\t\t\tGM->HumanFeint(Move);\n\t\t}",
     "\t\telse if (AnyPressed({ EKeys::T, EKeys::Gamepad_DPad_Up }))\n\t\t{\n\t\t\tGM->HumanFeint(Move);\n\t\t}\n"
     "\t\telse if (AnyPressed({ EKeys::V, EKeys::Gamepad_LeftThumbstick }))\n\t\t{\n\t\t\tGM->HumanFlickUp(Move);\n\t\t}"),
])

patch("PKHUD.cpp", [
    ("feint T / D-up   first touch",
     "feint T / D-up   flick-up V / L3   first touch"),
])
print("air ok")
