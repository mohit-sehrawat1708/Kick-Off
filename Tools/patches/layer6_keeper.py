# Layer 6: keeper rush on demand, one-on-one narrowing + smother, claiming crosses.
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

patch("PKGameMode.h", [
("""	void UpdateGamePlans(float Dt);""", """	void UpdateGamePlans(float Dt);
	// Keeper rush: the person holds Through without the ball and the keeper charges the carrier.
	void HumanKeeperRush(int32 Team, bool bRush) { bKeeperRush[FMath::Clamp(Team, 0, 1)] = bRush; }
	bool bKeeperRush[2] = { false, false };
	bool bOldKeeper = false; // -PKOldKeeper: previous keeper positioning, for comparison
	int32 KeeperSmothers = 0;"""),
])

patch("PKGameMode.cpp", [
("""	bOldLine = FParse::Param(FCommandLine::Get(), TEXT("PKOldLine"));""", """	bOldLine = FParse::Param(FCommandLine::Get(), TEXT("PKOldLine"));
	bOldKeeper = FParse::Param(FCommandLine::Get(), TEXT("PKOldKeeper"));"""),
("""	// Come and claim a loose, slow ball in the box.
	if (bLoose && InOwnBox(P, BallLoc) && BallVel.Size() < 900.f)
	{
		MoveTo(P, PredictBall(P), true);
		return;
	}""", """	// Come and claim a loose, slow ball in the box.
	if (bLoose && InOwnBox(P, BallLoc) && BallVel.Size() < 900.f)
	{
		MoveTo(P, PredictBall(P), true);
		return;
	}

	const APKFootballer* Carrier = Ball->GetOwnerPlayer();
	const bool bOppCarrier = Carrier && Carrier->Team != P->Team && !Ball->GetHolder();
	const float CarrierToGoal = bOppCarrier ? FVector::Dist(Carrier->GetFlatLocation(), Goal) : 1.e9f;

	// Keeper rush (the person asked): charge the carrier and go down at his feet when close.
	if (!bOldKeeper && bKeeperRush[P->Team] && bOppCarrier && CarrierToGoal < 3500.f)
	{
		MoveTo(P, BallLoc, true);
		if (FVector::Dist(P->GetFlatLocation(), BallLoc) < 260.f && P->LungeTimer <= 0.f)
		{
			P->LungeTimer = 0.45f; // smother: longer reach for the pick-up
			++KeeperSmothers;
		}
		return;
	}

	if (!bOldKeeper)
	{
		// Cross into the area: come and claim it if he gets there first.
		if (bLoose && Ball->GetActorLocation().Z > 120.f && BallVel.Size() > 400.f)
		{
			const FVector Drop = PredictAerial(P, 150.f, 265.f);
			const float DropToGoal = FMath::Abs(Drop.X - Goal.X);
			if (DropToGoal < 700.f && FMath::Abs(Drop.Y) < 1100.f)
			{
				float MyT = FVector::Dist(P->GetFlatLocation(), Drop) / PK::SprintSpeed;
				bool bFirst = true;
				for (const APKFootballer* O : Footballers)
				{
					if (O->Team != P->Team && FVector::Dist(O->GetFlatLocation(), Drop) / PK::SprintSpeed < MyT - 0.25f)
					{
						bFirst = false; // an attacker is clearly there first: stay on the line
					}
				}
				if (bFirst)
				{
					MoveTo(P, Drop, true);
					return;
				}
			}
		}

		// One-on-one: an attacker through on goal with nobody goal-side of him. Narrow the angle, then smother.
		if (bOppCarrier && CarrierToGoal < 2400.f)
		{
			const FVector CarrierLoc = Carrier->GetFlatLocation();
			bool bCovered = false;
			for (const APKFootballer* D : Footballers)
			{
				if (D->Team == P->Team && D != P && !D->IsStunned())
				{
					const float Along = FVector::DotProduct(D->GetFlatLocation() - Goal, (CarrierLoc - Goal).GetSafeNormal2D());
					const float Off = FMath::Abs(FVector::DotProduct(D->GetFlatLocation() - Goal, FVector::CrossProduct(FVector::UpVector, (CarrierLoc - Goal).GetSafeNormal2D())));
					if (Along > 0.f && Along < CarrierToGoal && Off < 250.f)
					{
						bCovered = true;
						break;
					}
				}
			}
			if (!bCovered)
			{
				const float Out = FMath::Min(CarrierToGoal * 0.42f, 950.f);
				MoveTo(P, Goal + (CarrierLoc - Goal).GetSafeNormal2D() * Out, true);
				if (FVector::Dist(P->GetFlatLocation(), BallLoc) < 240.f && P->LungeTimer <= 0.f)
				{
					P->LungeTimer = 0.45f;
					++KeeperSmothers;
				}
				return;
			}
		}
	}"""),
])

patch("PKPlayerController.cpp", [
("""		GM->HumanTeamPress(Team, bPrecision);""", """		GM->HumanTeamPress(Team, bPrecision);
		GM->HumanKeeperRush(Team, ActionDown(Dev, EPKInput::Through));"""),
("""		GM->HumanTeamPress(Team, false);
		// Pass buttons""", """		GM->HumanTeamPress(Team, false);
		GM->HumanKeeperRush(Team, false);
		// Pass buttons"""),
])
print("layer6 ok")
