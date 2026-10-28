# Layer 4: teammate contain (second-man press on demand), dynamic defensive line (flat line that steps up and
# drops, cover when a fullback is caught upfield, recovery sprints).
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
("""	EPKShotType ChooseAIShot(const APKFootballer* P, float DGoal) const;""",
"""	EPKShotType ChooseAIShot(const APKFootballer* P, float DGoal) const;
	// Teammate contain: the person holds Precision without the ball and the nearest team-mate presses the carrier.
	void HumanTeamPress(int32 Team, bool bPress) { bTeamPress[FMath::Clamp(Team, 0, 1)] = bPress; }
	bool bTeamPress[2] = { false, false };
	TWeakObjectPtr<APKFootballer> PressMate[2];
	float TeamPressSeconds = 0.f;
	// Dynamic defensive line (team frame x, -1 own goal line) and the side a caught-upfield fullback left open.
	float DefLine[2] = { -0.6f, -0.6f };
	float VacatedSide[2] = { 0.f, 0.f };
	TWeakObjectPtr<APKFootballer> CaughtUpfield[2];
	bool bOldLine = false; // -PKOldLine: previous per-player line, for comparison
	void UpdateDefensiveLine();"""),
])

patch("PKGameMode.cpp", [
("""	bDribbleTest = FParse::Param(FCommandLine::Get(), TEXT("PKDribbleTest"));""",
"""	bDribbleTest = FParse::Param(FCommandLine::Get(), TEXT("PKDribbleTest"));
	bOldLine = FParse::Param(FCommandLine::Get(), TEXT("PKOldLine"));"""),
# compute press mate + line once per AI update, after chasers
("""			else if (D < Second)
			{
				Second = D;
				Cover[Team] = P;
			}
		}""", """			else if (D < Second)
			{
				Second = D;
				Cover[Team] = P;
			}
		}

		// Teammate contain: the closest team-mate to the carrier (not the person's player, not the keeper) presses.
		PressMate[Team] = nullptr;
		if (bTeamPress[Team] && BallOwner && BallOwner->Team != Team)
		{
			float BestPress = TNumericLimits<float>::Max();
			for (APKFootballer* P : Footballers)
			{
				if (P->Team != Team || P->IsKeeper() || P->bHuman || P->IsStunned())
				{
					continue;
				}
				const float D = FVector::Dist(P->GetFlatLocation(), BallOwner->GetFlatLocation());
				if (D < BestPress)
				{
					BestPress = D;
					PressMate[Team] = P;
				}
			}
			TeamPressSeconds += PressMate[Team].IsValid() ? Dt : 0.f;
		}"""),
("""		else if (P == Chaser[P->Team])
		{
			AIDefend(P, BallOwner);
		}""", """		else if (P == PressMate[P->Team].Get() && BallOwner)
		{
			AIDefend(P, BallOwner); // sent to press by the person (teammate contain)
		}
		else if (P == Chaser[P->Team])
		{
			AIDefend(P, BallOwner);
		}"""),
("""		else
		{
			MoveTo(P, SupportSpot(P, false), false);
		}
	}
}""", """		else
		{
			// Out of position defenders (a fullback caught upfield) sprint back into the line.
			const FVector Spot = SupportSpot(P, false);
			MoveTo(P, Spot, !bOldLine && P->Role == EPKRole::Defender && FVector::Dist(Spot, P->GetFlatLocation()) > 900.f);
		}
	}
}

void APKGameMode::UpdateDefensiveLine()
{
	// One line for the back four: it follows the ball up and down the pitch, never sits ahead of the ball, and
	// drops off when the opposing passer has time to play a ball in behind (so runners stay in front of it).
	const FVector BallLoc = Ball->GetActorLocation();
	const APKFootballer* Owner = Ball->GetOwnerPlayer();
	for (int32 Team = 0; Team < 2; ++Team)
	{
		float HomeSum = 0.f;
		int32 Count = 0;
		for (const APKFootballer* P : Footballers)
		{
			if (P->Team == Team && P->Role == EPKRole::Defender)
			{
				HomeSum += P->Home.X;
				++Count;
			}
		}
		if (Count == 0)
		{
			continue;
		}
		const float BallFx = WorldToFx(Team, BallLoc.X);
		float Line = HomeSum / Count + 0.42f * BallFx - 0.06f + Style(Team).LinePush;
		Line = FMath::Min(Line, BallFx - 0.08f);

		// The deepest opposing outfield player (towards our goal) and whether their carrier has time.
		float DeepestOpp = 1.f;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != Team && !O->IsKeeper())
			{
				DeepestOpp = FMath::Min(DeepestOpp, WorldToFx(Team, O->GetActorLocation().X));
			}
		}
		bool bPasserFree = false;
		if (Owner && Owner->Team != Team)
		{
			float Near = 0.f;
			bPasserFree = !NearestOpponent(Owner, Near) || Near > 450.f;
		}
		if (bPasserFree)
		{
			Line = FMath::Min(Line, DeepestOpp - 0.03f); // drop: nobody gets in behind
		}
		Line = FMath::Clamp(Line, -0.9f, 0.25f);
		// Move the line smoothly (a line steps up and drops together, it doesn't jump).
		DefLine[Team] = FMath::FInterpTo(DefLine[Team], Line, GetWorld()->GetDeltaSeconds(), 2.5f);

		// A fullback still far upfield after the ball is lost: the others shift across to cover his side.
		VacatedSide[Team] = 0.f;
		CaughtUpfield[Team] = nullptr;
		const bool bDefending = Owner && Owner->Team != Team;
		if (bDefending)
		{
			for (APKFootballer* P : Footballers)
			{
				if (P->Team == Team && P->Role == EPKRole::Defender && FMath::Abs(P->Home.Y) > 0.5f
					&& WorldToFx(Team, P->GetActorLocation().X) > DefLine[Team] + 0.25f)
				{
					VacatedSide[Team] = FMath::Sign(P->Home.Y);
					CaughtUpfield[Team] = P;
					break;
				}
			}
		}
	}
}"""),
# hook UpdateDefensiveLine before the per-team chaser loop
("""	// Per team: the closest outfield player engages the ball, the second closest covers.
	APKFootballer* Chaser[2] = { nullptr, nullptr };""", """	// Per team: the closest outfield player engages the ball, the second closest covers.
	UpdateDefensiveLine();
	APKFootballer* Chaser[2] = { nullptr, nullptr };"""),
# SupportSpot: defenders use the shared line
("""	if (P->Role == EPKRole::Defender)
	{
		Fx = FMath::Min(Fx, BallFx - 0.05f);
	}
	Fx = FMath::Clamp(Fx, -0.92f, 0.86f);

	float Fy = P->Home.Y * 0.9f + 0.28f * BallFy;""", """	if (P->Role == EPKRole::Defender)
	{
		Fx = FMath::Min(Fx, BallFx - 0.05f);
		if (!bAttacking && !bOldLine)
		{
			Fx = DefLine[P->Team]; // one flat line
		}
	}
	Fx = FMath::Clamp(Fx, -0.92f, 0.86f);

	float Fy = P->Home.Y * 0.9f + 0.28f * BallFy;
	if (!bAttacking && !bOldLine && P->Role == EPKRole::Defender && VacatedSide[P->Team] != 0.f && CaughtUpfield[P->Team].Get() != P)
	{
		Fy += 0.11f * VacatedSide[P->Team]; // cover the side a caught-upfield fullback left open
	}"""),
])

# Controller: Precision held without the ball = teammate press
patch("PKPlayerController.cpp", [
("""		bCharging = false;
		bEffortHeld[Team] = false;
		bPassCharging[Team] = false;""", """		bCharging = false;
		bEffortHeld[Team] = false;
		bPassCharging[Team] = false;
		GM->HumanTeamPress(Team, bPrecision);"""),
("""	if (GM->HumanHasBall(Team))
	{
		// Pass buttons""", """	if (GM->HumanHasBall(Team))
	{
		GM->HumanTeamPress(Team, false);
		// Pass buttons"""),
])
print("layer4 ok")
