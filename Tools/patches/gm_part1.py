import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings"))
p = "PKGameMode.cpp"
s = open(p, encoding="utf-8-sig").read().replace("\r\n", "\n")


def rep(a, b):
    global s
    if a not in s:
        raise SystemExit("NOT FOUND: " + a[:100])
    s = s.replace(a, b, 1)


# ---- styles table
rep("\tconstexpr float ControlLoseDistance = 260.f;",
"""\t// Balanced, Possession, Counter, Long Ball, High Press.
\tconst FPKStyleParams StyleTable[(int32)EPKTeamStyle::Count] = {
\t\t{ 3.0f, 1.0f, 4000.f, 0.4f, 0.0f, 0.25f, 0.45f, 0.50f,  0.00f, 450.f, false },
\t\t{ 1.6f, 1.8f, 2600.f, 0.2f, 0.0f, 0.35f, 0.60f, 0.70f,  0.02f, 450.f, false },
\t\t{ 4.5f, 0.7f, 4500.f, 0.7f, 0.2f, 0.15f, 0.30f, 0.40f, -0.08f, 380.f, false },
\t\t{ 3.5f, 0.8f, 5500.f, 0.3f, 0.6f, 0.20f, 0.40f, 0.50f, -0.03f, 450.f, false },
\t\t{ 3.0f, 1.0f, 4000.f, 0.4f, 0.0f, 0.25f, 0.45f, 0.50f,  0.12f, 750.f, true  },
\t};

\tconstexpr float ControlLoseDistance = 260.f;""")

# ---- random styles at the start of a match
rep("\tTimeLeft = bAutoTest ? 150.f : PK::MatchSeconds;\n\tSetupKickoff(0);",
    "\tTimeLeft = bAutoTest ? 150.f : PK::MatchSeconds;\n"
    "\tTeamStyle[1] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n"
    "\tif (bAutoTest)\n\t{\n\t\tTeamStyle[0] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n\t}\n"
    "\tUE_LOG(LogPitchKings, Log, TEXT(\"Styles: BLUE %s, RED %s\"), *StyleName(TeamStyle[0]), *StyleName(TeamStyle[1]));\n"
    "\tSetupKickoff(0);")

# ---- tick: indicator + kick alignment
rep("\tauto FreezeAll = [this]()", "\tUpdatePredictedTaker(DeltaSeconds);\n\n\tauto FreezeAll = [this]()")
rep("\t\t\tUpdateDribble(Dribbler, DeltaSeconds);\n\t\t}\n\t\tCheckPickups();",
    "\t\t\tUpdateDribble(Dribbler, DeltaSeconds);\n\t\t}\n\t\tAlignBallForKicks(DeltaSeconds);\n\t\tCheckPickups();")

# ---- UpdateAI branches
rep("\t\telse if (bTeamHasBall || bTeamPassing)\n\t\t{\n\t\t\tMoveTo(P, SupportSpot(P, true), false);\n\t\t}",
    "\t\telse if (bTeamHasBall || bTeamPassing)\n\t\t{\n"
    "\t\t\tAPKFootballer* Focus = BallOwner ? BallOwner : Ball->IntendedReceiver.Get();\n"
    "\t\t\tconst FVector Spot = AttackSupportSpot(P, Focus);\n"
    "\t\t\tMoveTo(P, Spot, P->RunTimer > 0.f);\n\t\t}")
rep("\t\telse if (P == Cover[P->Team] && BallOwner)\n\t\t{\n\t\t\tMoveTo(P, FMath::Lerp(BallOwner->GetFlatLocation(), OwnGoal(P->Team), 0.3f), false);\n\t\t}\n\t\telse\n\t\t{\n\t\t\tMoveTo(P, SupportSpot(P, false), false);\n\t\t}",
"""\t\telse if (P == Cover[P->Team] && BallOwner)
\t\t{
\t\t\tif (Style(P->Team).bDoublePress && FVector::Dist(P->GetFlatLocation(), BallOwner->GetFlatLocation()) < 1200.f)
\t\t\t{
\t\t\t\tAIDefend(P, BallOwner); // high press: two players hunt the ball
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tMoveTo(P, FMath::Lerp(BallOwner->GetFlatLocation(), OwnGoal(P->Team), 0.3f), false);
\t\t\t}
\t\t}
\t\telse if (const TObjectPtr<APKFootballer>* Mark = MarkTargets.Find(P))
\t\t{
\t\t\t// Mark goal-side and slightly ball-side of the assigned opponent.
\t\t\tconst APKFootballer* Opp = *Mark;
\t\t\tconst FVector OppLoc = Opp->GetFlatLocation();
\t\t\tconst FVector GoalSide = (OwnGoal(P->Team) - OppLoc).GetSafeNormal2D();
\t\t\tconst FVector BallSide = (PK::Flat(Ball->GetActorLocation()) - OppLoc).GetSafeNormal2D();
\t\t\tconst FVector Spot = OppLoc + GoalSide * 170.f + BallSide * 60.f;
\t\t\tMoveTo(P, Spot, FVector::Dist(Spot, P->GetFlatLocation()) > 500.f);
\t\t}
\t\telse
\t\t{
\t\t\tMoveTo(P, SupportSpot(P, false), false);
\t\t}""")
rep("\tconst float Dist = FVector::Dist(P->GetFlatLocation(), BallLoc);\n\tif (Dist > 450.f)\n\t{\n\t\tMoveTo(P, PredictBall(P), true);\n\t\treturn;\n\t}",
    "\tconst float Dist = FVector::Dist(P->GetFlatLocation(), BallLoc);\n\tif (Dist > Style(P->Team).PressDistance)\n\t{\n\t\tMoveTo(P, PredictBall(P), true);\n\t\treturn;\n\t}")
rep("void APKGameMode::UpdateAI(float Dt)\n{\n\tAPKFootballer* BallOwner = Ball->GetOwnerPlayer();",
    "void APKGameMode::UpdateAI(float Dt)\n{\n\tMarkingTimer -= Dt;\n\tif (MarkingTimer <= 0.f)\n\t{\n\t\tMarkingTimer = 0.3f;\n\t\tUpdateMarking();\n\t}\n\tAPKFootballer* BallOwner = Ball->GetOwnerPlayer();")

# ---- style in SupportSpot
rep("\tfloat Fx = P->Home.X + Shift * BallFx + Push;", "\tfloat Fx = P->Home.X + Shift * BallFx + Push + Style(P->Team).LinePush;")

# ---- BestPassTarget by style
rep("\tconst float MyFx = WorldToFx(P->Team, Loc.X);\n\n\tfor (APKFootballer* T : Footballers)\n\t{\n\t\tif (T == P || T->Team != P->Team || (T->IsKeeper() && !bAllowKeeper))",
    "\tconst float MyFx = WorldToFx(P->Team, Loc.X);\n\tconst FPKStyleParams& S = Style(P->Team);\n"
    "\tconst float MaxDist = S.MaxPassDist * (P->HasTrait(PKTrait::TikiTaka) ? 0.85f : 1.f);\n\n"
    "\tfor (APKFootballer* T : Footballers)\n\t{\n\t\tif (T == P || T->Team != P->Team || (T->IsKeeper() && !bAllowKeeper))")
rep("\t\tif (D < 400.f || D > 4000.f)\n\t\t{\n\t\t\tcontinue;\n\t\t}\n\n\t\tfloat Lane",
    "\t\tif (D < 400.f || D > MaxDist)\n\t\t{\n\t\t\tcontinue;\n\t\t}\n\n\t\tfloat Lane")
rep("\t\tconst float Rating = Progress * 3.f + FMath::Clamp(Open / 700.f, 0.f, 1.f) * 0.9f + FMath::Clamp(Lane / 500.f, 0.f, 1.f) * 0.5f - D / 6000.f;",
    "\t\tconst float Safety = FMath::Clamp(Open / 700.f, 0.f, 1.f) * 0.9f + FMath::Clamp(Lane / 500.f, 0.f, 1.f) * 0.5f;\n"
    "\t\tconst float Rating = Progress * S.Progress + Safety * S.Safety - D / 6000.f;")

# ---- AICarry by style
rep("\t\tP->DecisionTimer = FMath::FRandRange(0.25f, 0.45f);\n\n\t\tconst float DGoal",
    "\t\tconst FPKStyleParams& S = Style(P->Team);\n\t\tP->DecisionTimer = FMath::FRandRange(S.DecisionMin, S.DecisionMax);\n\n\t\tconst float DGoal")
rep("\t\tif (Target && ((bPressured && PassScore > -0.6f) || (PassScore > 0.5f && FMath::FRand() < 0.5f) || (NearOpp < 450.f && PassScore > 0.f)))",
    "\t\tif (Target && ((bPressured && PassScore > -0.6f) || (PassScore > 0.5f && FMath::FRand() < S.PassUrge) || (NearOpp < 450.f && PassScore > 0.f)))")
rep("\t\t\tif (D > 2800.f)\n\t\t\t{\n\t\t\t\tType = EPKKickType::Lob;\n\t\t\t}\n\t\t\telse if (bAhead && FMath::FRand() < 0.4f)",
    "\t\t\tif (D > 2800.f || (D > 1800.f && FMath::FRand() < S.LobBias))\n\t\t\t{\n\t\t\t\tType = EPKKickType::Lob;\n\t\t\t}\n\t\t\telse if (bAhead && FMath::FRand() < S.ThroughChance)")

# ---- CanReachBall height
rep("bool APKGameMode::CanReachBall(const APKFootballer* P, float Reach) const",
    "bool APKGameMode::CanReachBall(const APKFootballer* P, float Reach, float MaxHeight) const")
rep("\treturn B.Z < 90.f && FVector::Dist(PK::Flat(B), P->GetFootPoint()) < Reach;",
    "\treturn B.Z < MaxHeight && FVector::Dist(PK::Flat(B), P->GetFootPoint()) < Reach;")

# ---- dribble attributes
rep("\tconst FVector ToBall = Flat - P->GetFlatLocation();\n\tconst float Ahead = FVector::DotProduct(ToBall, Desired);",
    "\t// Ball control rating: better control keeps touches closer and more frequent.\n"
    "\tLead *= FMath::Lerp(1.35f, 0.75f, P->Attr.Control) * (P->HasTrait(PKTrait::Technical) ? 0.85f : 1.f);\n"
    "\tInterval *= FMath::Lerp(1.1f, 0.85f, P->Attr.Control);\n\n"
    "\tconst FVector ToBall = Flat - P->GetFlatLocation();\n\tconst float Ahead = FVector::DotProduct(ToBall, Desired);")

# ---- requests: after-start hook, shot quality, volley
rep("\tif (P->StartKick(Kick, Ball->GetHolder() == P && P->IsKeeper(), Ball->GetHolder() == P && !P->IsKeeper()) && P->Team == 0 && !Target->IsKeeper())\n\t{\n\t\tSetHuman(Target);\n\t}",
    "\tconst bool bStarted = P->StartKick(Kick, Ball->GetHolder() == P && P->IsKeeper(), Ball->GetHolder() == P && !P->IsKeeper());\n"
    "\tif (bStarted)\n\t{\n\t\tAfterKickStart(P);\n\t}\n"
    "\tif (bStarted && P->Team == 0 && !Target->IsKeeper())\n\t{\n\t\tSetHuman(Target);\n\t}")
rep("\tP->StartKick(Kick, Ball->GetHolder() == P && P->IsKeeper(), Ball->GetHolder() == P && !P->IsKeeper());\n}",
    "\tif (P->StartKick(Kick, Ball->GetHolder() == P && P->IsKeeper(), Ball->GetHolder() == P && !P->IsKeeper()))\n\t{\n\t\tAfterKickStart(P);\n\t}\n}")
rep("""void APKGameMode::RequestShot(APKFootballer* P, float AimLateral, float Power)
{
	const float Err = 50.f + 180.f * Power * Power * Power;""",
"""void APKGameMode::RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne)
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
	}""")
rep("\tKick.Power = Power;\n\tP->StartKick(Kick, false, false);",
    "\tKick.Power = Power;\n\tif (P->StartKick(Kick, false, false, bAirborne))\n\t{\n\t\tAfterKickStart(P);\n\t}")
rep("\t\tif (!bOwns && !(Ball->IsFree() && CanReachBall(P, KickReach)))",
    "\t\tconst bool bVolley = P->GetCurrentAction() == EPKAction::Volley;\n"
    "\t\tif (!bOwns && !(Ball->IsFree() && CanReachBall(P, bVolley ? 170.f : KickReach, bVolley ? 220.f : 90.f)))")

# ---- ExecuteKick: accuracy + shot traits
rep("\tconst float D = FMath::Max(Delta.Size(), 1.f);\n\tconst FVector Dir = Delta / D;\n\tconst FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);",
"""\tconst float D = FMath::Max(Delta.Size(), 1.f);
\tFVector Dir = Delta / D;
\tfloat SpeedScale = 1.f;
\tif (Kick.Type != EPKKickType::Shot && !bFromHands)
\t{
\t\t// Passing rating: small direction and weight errors (Tiki Taka halves them).
\t\tconst float Sloppy = (1.f - P->Attr.Passing) * (P->HasTrait(PKTrait::TikiTaka) ? 0.5f : 1.f);
\t\tDir = Dir.RotateAngleAxis(FMath::FRandRange(-10.f, 10.f) * Sloppy, FVector::UpVector);
\t\tSpeedScale = 1.f + FMath::FRandRange(-0.15f, 0.15f) * Sloppy;
\t}
\tconst FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);""")
rep("\t\tconst float Speed = 2000.f + 1900.f * Kick.Power;\n\t\tconst float T = D / Speed;\n\t\tVelocity = Dir * Speed;\n\t\tVelocity.Z = (Kick.Target.Z - From.Z) / T + 0.5f * PK::Gravity * T;\n\t\tSpin = Side * FMath::FRandRange(-250.f, 250.f);",
"""\t\tfloat Speed = (2000.f + 1900.f * Kick.Power) * FMath::Lerp(0.9f, 1.05f, P->Attr.Shooting);
\t\tif (P->HasTrait(PKTrait::PowerShot))
\t\t{
\t\t\tSpeed *= 1.12f;
\t\t}
\t\tif (P->HasTrait(PKTrait::Finesse))
\t\t{
\t\t\tSpeed *= 0.9f;
\t\t}
\t\tconst float T = D / Speed;
\t\tVelocity = Dir * Speed;
\t\tVelocity.Z = (Kick.Target.Z - From.Z) / T + 0.5f * PK::Gravity * T;
\t\tSpin = Side * FMath::FRandRange(-250.f, 250.f);
\t\tif (P->HasTrait(PKTrait::Finesse))
\t\t{
\t\t\t// Start it wide of the target and curl it back in.
\t\t\tconst float Curl = Kick.Target.Y * FVector::DotProduct(Side, FVector(0.f, 1.f, 0.f)) > 0.f ? -1.f : 1.f;
\t\t\tSpin = Side * Curl * 550.f;
\t\t\tVelocity -= Side * Curl * 550.f * T * 0.5f;
\t\t}""")
rep("\t\tVelocity = Dir * Speed;\n\t\tif (bFromHands)", "\t\tVelocity = Dir * Speed * SpeedScale;\n\t\tif (bFromHands)")
open(p, "w", encoding="utf-8").write(s)
print("gm part1 ok")
