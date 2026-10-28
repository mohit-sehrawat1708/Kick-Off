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


# ---------------------------------------------------------------- footballer
patch("PKFootballer.h", [
    ("\tbool bWatchBall = true;",
     "\tbool bWatchBall = true;\n\n"
     "\t// Discipline and physical play.\n"
     "\tint32 YellowCards = 0;\n"
     "\tbool bSentOff = false;\n"
     "\tint32 OffsideKickId = -1;   // in an offside position when this kick was played\n"
     "\tfloat ShoulderCooldown = 0.f;\n"
     "\tbool bHoldPosition = false;  // set piece: stay put (wall, penalty) until the ball is played\n"
     "\tfloat StuckTimer = 0.f;      // test metric: wants to move but cannot"),
])
patch("PKFootballer.cpp", [
    ("\tSuper::BeginPlay();\n\tLookTarget = GetActorLocation() + GetActorForwardVector() * 500.f;",
     "\tSuper::BeginPlay();\n\tLookTarget = GetActorLocation() + GetActorForwardVector() * 500.f;\n"
     "\t// Bodies do not block each other rigidly (players got wedged between two opponents);\n"
     "\t// the game mode resolves contact softly and turns it into pushing and shoulder challenges.\n"
     "\tif (!FParse::Param(FCommandLine::Get(), TEXT(\"PKHardCollision\")))\n\t{\n"
     "\t\tGetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);\n\t}"),
])

# ---------------------------------------------------------------- game mode header
patch("PKGameMode.h", [
    ("UENUM()\nenum class EPKTeamStyle : uint8",
     "UENUM()\nenum class EPKSetPiece : uint8\n{\n\tNone,\n\tKickoff,\n\tFreeKick,\n\tPenalty,\n\tCorner,\n\tGoalKick,\n\tThrowIn\n};\n\n"
     "UENUM()\nenum class EPKTeamStyle : uint8"),
    ("\tvoid HumanFlickUp(const FVector& Aim);",
     "\tvoid HumanFlickUp(const FVector& Aim);\n\tvoid HumanShoulder();"),
    ("\tint32 BufferedDropped = 0;",
     "\tint32 BufferedDropped = 0;\n"
     "\tint32 Fouls[2] = { 0, 0 };\n\tint32 Yellows[2] = { 0, 0 };\n\tint32 Reds[2] = { 0, 0 };\n"
     "\tint32 Penalties[2] = { 0, 0 };\n\tint32 Offsides[2] = { 0, 0 };\n\tint32 FreeKickShots = 0;\n"
     "\tint32 ShoulderWins = 0;\n\tint32 ShoulderLosses = 0;\n\tfloat StuckSeconds = 0.f;\n"
     "\tbool bHardCollision = false;\n"
     "\tEPKSetPiece SetPiece = EPKSetPiece::None;\n"
     "\tUPROPERTY()\n\tTArray<TObjectPtr<APKFootballer>> SentOff;\n"
     "\tFString CardText;\n\tFLinearColor CardColor = FLinearColor::Yellow;\n\tfloat CardTimer = 0.f;"),
    ("\tvoid UpdatePendingTouches(float Dt);",
     "\tvoid UpdatePendingTouches(float Dt);\n"
     "\tvoid UpdateBodyContacts(float Dt);\n"
     "\tbool ShoulderChallenge(APKFootballer* Carrier, APKFootballer* Challenger, bool bBarge);\n"
     "\tvoid AwardFoul(APKFootballer* Fouler, APKFootballer* Victim, float Severity, const TCHAR* Label);\n"
     "\tvoid AwardFreeKick(int32 Team, FVector Spot, const FString& Label);\n"
     "\tvoid AwardPenalty(int32 Team);\n"
     "\tvoid SendOff(APKFootballer* P);\n"
     "\tvoid RestoreSentOff();\n"
     "\tbool IsOffsidePosition(const APKFootballer* P, float BallX) const;\n"
     "\tvoid MarkOffside(const APKFootballer* Kicker);\n"
     "\tbool CheckOffside(APKFootballer* Receiver);\n"
     "\tbool InPenaltyArea(int32 DefendingTeam, const FVector& Point) const;"),
])

# ---------------------------------------------------------------- game mode
patch("PKGameMode.cpp", [
    # flags
    ('\tbContactShots = FParse::Param(FCommandLine::Get(), TEXT("PKContactShots"));\n',
     '\tbContactShots = FParse::Param(FCommandLine::Get(), TEXT("PKContactShots"));\n'
     '\tbHardCollision = FParse::Param(FCommandLine::Get(), TEXT("PKHardCollision"));\n'),

    # match restart: everyone back, discipline reset
    ("\tPossessionTime[0] = PossessionTime[1] = 0.f;\n\tTimeLeft = bAutoTest ? 150.f : PK::MatchSeconds;",
     "\tPossessionTime[0] = PossessionTime[1] = 0.f;\n\tRestoreSentOff();\n"
     "\tFouls[0] = Fouls[1] = Yellows[0] = Yellows[1] = Reds[0] = Reds[1] = 0;\n"
     "\tPenalties[0] = Penalties[1] = Offsides[0] = Offsides[1] = 0;\n"
     "\tTimeLeft = bAutoTest ? 150.f : PK::MatchSeconds;"),

    # kickoff: fall back if the usual kicker was sent off
    ("\tBall->PlaceAt(FVector::ZeroVector, Kicker);\n\tif (Kicker && Kicker->Team == 0)",
     "\tif (!Kicker)\n\t{\n\t\tKicker = ClosestOutfield(KickingTeam, FVector::ZeroVector, nullptr);\n"
     "\t\tif (Kicker)\n\t\t{\n\t\t\tKicker->Teleport2D(TeamToWorld(KickingTeam, -0.012f, 0.f), KickingTeam == 0 ? 0.f : 180.f);\n\t\t}\n\t}\n"
     "\tSetPiece = EPKSetPiece::Kickoff;\n"
     "\tBall->PlaceAt(FVector::ZeroVector, Kicker);\n\tif (Kicker && Kicker->Team == 0)"),

    # tick: body contact after the AI, set piece end
    ("\t\tUpdateAI(DeltaSeconds);\n",
     "\t\tUpdateAI(DeltaSeconds);\n\t\tUpdateBodyContacts(DeltaSeconds);\n"
     "\t\tif (SetPiece != EPKSetPiece::None && !Ball->IsPlaced() && !Ball->GetHolder())\n\t\t{\n"
     "\t\t\tSetPiece = EPKSetPiece::None;\n"
     "\t\t\tfor (APKFootballer* F : Footballers)\n\t\t\t{\n\t\t\t\tF->bHoldPosition = false;\n\t\t\t}\n\t\t}\n"),
    ("\tif (RestoreDilationFrames > 0 && --RestoreDilationFrames == 0)",
     "\tCardTimer = FMath::Max(0.f, CardTimer - DeltaSeconds);\n"
     "\tif (RestoreDilationFrames > 0 && --RestoreDilationFrames == 0)"),

    # AI: hold position at set pieces
    ("\t\tif (P->IsStunned())\n\t\t{\n\t\t\tP->MoveInput = FVector::ZeroVector;\n\t\t\tcontinue;\n\t\t}\n\n\t\tconst bool bTeamHasBall",
     "\t\tif (P->IsStunned())\n\t\t{\n\t\t\tP->MoveInput = FVector::ZeroVector;\n\t\t\tcontinue;\n\t\t}\n"
     "\t\tif (P->bHoldPosition && Ball->IsPlaced() && Ball->GetSetPieceTaker() != P)\n\t\t{\n"
     "\t\t\tP->MoveInput = FVector::ZeroVector;\n\t\t\tP->bSprint = false;\n\t\t\tcontinue;\n\t\t}\n\n\t\tconst bool bTeamHasBall"),
    ("\tconst bool bDeadBallTaker = !Ball->IsFree() && Ball->GetOwnerPlayer() == Human;",
     "\tconst bool bDeadBallTaker = (!Ball->IsFree() && Ball->GetOwnerPlayer() == Human) || (Human->bHoldPosition && Ball->IsPlaced());"),

    # restarts: remember what kind of set piece it is
    ("\tPhase = EPKPhase::Stoppage;\n\tPhaseTimer = 1.f;\n\tRestartTimer = 0.f;\n\tShowMessage(FString::Printf(TEXT(\"%s - %s\"), *Label, PK::TeamName(Team)), 1.5f);",
     "\tSetPiece = bGoalKick ? EPKSetPiece::GoalKick : (bThrowIn ? EPKSetPiece::ThrowIn : (Label == TEXT(\"CORNER\") ? EPKSetPiece::Corner : EPKSetPiece::FreeKick));\n"
     "\tPhase = EPKPhase::Stoppage;\n\tPhaseTimer = 1.f;\n\tRestartTimer = 0.f;\n\tShowMessage(FString::Printf(TEXT(\"%s - %s\"), *Label, PK::TeamName(Team)), 1.5f);"),

    # AI restarts: penalties and direct free kicks are shot at goal
    ("\tfloat Score_ = 0.f;\n\tAPKFootballer* Target = BestPassTarget(P, false, Score_);\n\tif (Target)\n\t{\n\t\tconst float D = FVector::Dist(P->GetFlatLocation(), Target->GetFlatLocation());",
     "\tif (SetPiece == EPKSetPiece::Penalty)\n\t{\n"
     "\t\tRequestShot(P, (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(0.45f, 0.95f), FMath::FRandRange(0.6f, 0.9f));\n\t\treturn;\n\t}\n"
     "\tif (SetPiece == EPKSetPiece::FreeKick && Ball->IsPlaced())\n\t{\n"
     "\t\tconst FVector Spot = PK::Flat(Ball->GetActorLocation());\n"
     "\t\tif (FVector::Dist(Spot, OppGoal(P->Team)) < 2900.f && FMath::Abs(Spot.Y) < 1700.f && FMath::FRand() < 0.6f)\n\t\t{\n"
     "\t\t\t++FreeKickShots;\n"
     "\t\t\tRequestShot(P, Spot.Y * PK::AttackDir(P->Team) > 0.f ? -0.85f : 0.85f, FMath::FRandRange(0.65f, 0.9f));\n\t\t\treturn;\n\t\t}\n\t}\n"
     "\tfloat Score_ = 0.f;\n\tAPKFootballer* Target = BestPassTarget(P, false, Score_);\n\tif (Target)\n\t{\n\t\tconst float D = FVector::Dist(P->GetFlatLocation(), Target->GetFlatLocation());"),

    # AI defending: fewer reckless slides when already booked
    ("\telse if (Dist > 220.f && Dist < 380.f && P->TackleCooldown <= 0.f && Carrier->ProtectedTimer <= 0.f && FMath::FRand() < 0.004f)",
     "\telse if (Dist > 220.f && Dist < 380.f && P->TackleCooldown <= 0.f && Carrier->ProtectedTimer <= 0.f && FMath::FRand() < (P->YellowCards > 0 ? 0.0015f : 0.004f))"),

    # passing: AI does not pass to players in offside positions
    ("\t\tconst bool bRunner = T->RunTimer > 0.f && !T->AITarget.IsNearlyZero();\n",
     "\t\tif (IsOffsidePosition(T, P->GetActorLocation().X))\n\t\t{\n\t\t\tcontinue;\n\t\t}\n"
     "\t\tconst bool bRunner = T->RunTimer > 0.f && !T->AITarget.IsNearlyZero();\n"),

    # offside: flag at the pass, call it at the reception
    ("\tBall->Kick(P, Velocity, Spin);\n\tP->HoldTimer = 0.f;",
     "\tconst EPKSetPiece KickedFrom = Ball->IsPlaced() || Ball->GetHolder() ? SetPiece : EPKSetPiece::None;\n"
     "\tBall->Kick(P, Velocity, Spin);\n\tP->HoldTimer = 0.f;\n"
     "\tif (Kick.Type != EPKKickType::Shot && KickedFrom != EPKSetPiece::GoalKick && KickedFrom != EPKSetPiece::Corner && KickedFrom != EPKSetPiece::ThrowIn && !bFromHands)\n\t{\n"
     "\t\tMarkOffside(P);\n\t}"),
    ("\tconst float ArrivalSpeed = PK::Flat(Ball->Velocity).Size();\n\tconst bool bHumanTouch",
     "\tif (CheckOffside(P))\n\t{\n\t\treturn;\n\t}\n"
     "\tconst float ArrivalSpeed = PK::Flat(Ball->Velocity).Size();\n\tconst bool bHumanTouch"),
    ("\t\tif (!bKeeperHands && BallLoc.Z > 120.f && BallLoc.Z < 230.f && D < 70.f && Ball->Velocity.Z < 0.f && P->StealCooldown <= 0.f)\n\t\t{",
     "\t\tif (!bKeeperHands && BallLoc.Z > 120.f && BallLoc.Z < 230.f && D < 70.f && Ball->Velocity.Z < 0.f && P->StealCooldown <= 0.f)\n\t\t{\n"
     "\t\t\tif (CheckOffside(P))\n\t\t\t{\n\t\t\t\treturn;\n\t\t\t}"),

    # tackles: fouls
    ("\t\t\t\tif (FMath::FRand() < Chance)\n\t\t\t\t{\n\t\t\t\t\tEventShot(bSliding ? TEXT(\"SlideWin\") : (bLunging ? TEXT(\"TackleWin\") : TEXT(\"Steal\")), P);",
     "\t\t\t\t// Fouls: from behind, late, or a missed lunge that takes the man.\n"
     "\t\t\t\tconst bool bFromBehind = FVector::DotProduct((P->GetFlatLocation() - Controller->GetFlatLocation()).GetSafeNormal2D(), Controller->GetFacing()) < -0.4f;\n"
     "\t\t\t\tconst float Care = FMath::Lerp(1.3f, 0.6f, P->Attr.Defending) * (P->YellowCards > 0 ? 0.7f : 1.f);\n"
     "\t\t\t\tconst bool bWon = FMath::FRand() < Chance;\n"
     "\t\t\t\tfloat FoulChance = 0.f;\n"
     "\t\t\t\tif (bSliding)\n\t\t\t\t{\n\t\t\t\t\tFoulChance = bWon ? (bFromBehind ? 0.55f : 0.08f) : (bFromBehind ? 0.75f : 0.4f);\n\t\t\t\t}\n"
     "\t\t\t\telse if (bLunging)\n\t\t\t\t{\n\t\t\t\t\tFoulChance = bWon ? (bFromBehind ? 0.3f : 0.03f) : (bFromBehind ? 0.4f : 0.15f);\n\t\t\t\t}\n"
     "\t\t\t\telse if (bFromBehind)\n\t\t\t\t{\n\t\t\t\t\tFoulChance = bWon ? 0.12f : 0.f; // nicked from behind through the legs\n\t\t\t\t}\n"
     "\t\t\t\tif (FoulChance > 0.f && FMath::FRand() < FoulChance * Care)\n\t\t\t\t{\n"
     "\t\t\t\t\tconst float Severity = (bSliding ? 0.55f : 0.25f) + (bFromBehind ? 0.35f : 0.f) + (bWon ? -0.15f : 0.f);\n"
     "\t\t\t\t\tAwardFoul(P, Controller, Severity, bSliding ? TEXT(\"SLIDE\") : TEXT(\"TACKLE\"));\n\t\t\t\t\treturn;\n\t\t\t\t}\n"
     "\t\t\t\tif (bWon)\n\t\t\t\t{\n\t\t\t\t\tEventShot(bSliding ? TEXT(\"SlideWin\") : (bLunging ? TEXT(\"TackleWin\") : TEXT(\"Steal\")), P);"),

    # autotest stats
    ("| off-ball facing avg %.0f deg, back to ball %.0f%%\"),",
     "| off-ball facing avg %.0f deg, back to ball %.0f%% | fouls %d-%d yellow %d-%d red %d-%d pens %d-%d offside %d-%d fk-shots %d | shoulder won %d lost %d | stuck %.1fs\"),"),
    ("WatchSamples ? 100.0 * WatchAway / WatchSamples : 0.0);",
     "WatchSamples ? 100.0 * WatchAway / WatchSamples : 0.0,\n"
     "\t\tFouls[0], Fouls[1], Yellows[0], Yellows[1], Reds[0], Reds[1], Penalties[0], Penalties[1], Offsides[0], Offsides[1], FreeKickShots, ShoulderWins, ShoulderLosses, StuckSeconds);"),
])

# ---------------------------------------------------------------- new functions
path = os.path.join(base, "PKGameMode.cpp")
s = open(path, encoding="utf-8").read()
anchor = "void APKGameMode::QueueTouch("
s = s.replace(anchor, r"""void APKGameMode::UpdateBodyContacts(float Dt)
{
	// Soft body contact: overlapping players are pushed apart, the stronger / faster one moving less.
	// Nobody can be wedged between two opponents any more, and contact with the ball carrier becomes
	// a shoulder challenge (legal) or a push from behind (foul).
	for (APKFootballer* P : Footballers)
	{
		P->ShoulderCooldown = FMath::Max(0.f, P->ShoulderCooldown - Dt);
		// Stuck metric: wants to move, is not moving, nothing else explains it.
		const bool bWantsMove = P->MoveInput.Size() > 0.5f && !P->IsInAction() && !P->IsStunned() && !P->bHoldPosition && Phase == EPKPhase::Playing;
		P->StuckTimer = bWantsMove && PK::Flat(P->GetVelocity()).Size() < 60.f ? P->StuckTimer + Dt : 0.f;
		if (P->StuckTimer > 0.6f)
		{
			StuckSeconds += Dt;
		}
	}
	if (bHardCollision)
	{
		return;
	}

	auto Strength = [this](const APKFootballer* F, const FVector& TowardOther)
	{
		float S = 0.7f + 0.6f * F->Attr.Physical;
		S += FMath::Max(0.f, FVector::DotProduct(PK::Flat(F->GetVelocity()), TowardOther)) * 0.0006f; // momentum
		S += F->Stance == EPKStance::Shield ? 0.35f : 0.f;
		S += F->IsKeeper() ? 0.3f : 0.f;
		S *= F->IsStunned() ? 0.4f : 1.f;
		return S;
	};

	const float MinDist = 58.f;
	APKFootballer* Carrier = Ball->GetController();
	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		APKFootballer* A = Footballers[i];
		for (int32 j = i + 1; j < Footballers.Num(); ++j)
		{
			APKFootballer* B = Footballers[j];
			FVector Delta = B->GetFlatLocation() - A->GetFlatLocation();
			const float D = Delta.Size();
			if (D >= MinDist + 25.f)
			{
				continue;
			}
			const FVector N = D > 1.f ? Delta / D : A->GetActorRightVector();

			// Physical duel with the ball carrier.
			if (A->Team != B->Team && (A == Carrier || B == Carrier) && Phase == EPKPhase::Playing)
			{
				APKFootballer* C = A == Carrier ? A : B;
				APKFootballer* O = A == Carrier ? B : A;
				if (O->ShoulderCooldown <= 0.f && !O->IsInAction() && !C->IsInAction() && !O->bHuman)
				{
					ShoulderChallenge(C, O, false);
				}
			}

			if (D >= MinDist)
			{
				continue;
			}
			const float Overlap = MinDist - D;
			const float SA = Strength(A, N);
			const float SB = Strength(B, -N);
			const float ShareA = SB / (SA + SB); // A moves by the other's strength share
			const float ShareB = 1.f - ShareA;
			A->AddActorWorldOffset(-N * Overlap * ShareA, false, nullptr, ETeleportType::None);
			B->AddActorWorldOffset(N * Overlap * ShareB, false, nullptr, ETeleportType::None);
			// Bodies absorb some of the closing speed.
			UCharacterMovementComponent* MA = A->GetCharacterMovement();
			UCharacterMovementComponent* MB = B->GetCharacterMovement();
			const float Closing = FVector::DotProduct(PK::Flat(MA->Velocity - MB->Velocity), N);
			if (Closing > 0.f)
			{
				MA->Velocity -= N * Closing * ShareA * 0.8f;
				MB->Velocity += N * Closing * ShareB * 0.8f;
			}
		}
	}
}

bool APKGameMode::ShoulderChallenge(APKFootballer* Carrier, APKFootballer* Challenger, bool bBarge)
{
	// Side by side: a legal shoulder-to-shoulder duel. From behind: a push, usually a foul.
	const FVector ToChallenger = (Challenger->GetFlatLocation() - Carrier->GetFlatLocation()).GetSafeNormal2D();
	const float Along = FVector::DotProduct(ToChallenger, Carrier->GetFacing());
	const bool bBehind = Along < -0.5f;
	const bool bAlongside = FMath::Abs(Along) < 0.6f;
	const float CarrierSpeed = PK::Flat(Carrier->GetVelocity()).Size();
	if (!bBarge && (!bAlongside || CarrierSpeed < 250.f) && !bBehind)
	{
		return false;
	}
	if (!bBarge && bBehind && FVector::DotProduct(PK::Flat(Challenger->GetVelocity()), -ToChallenger) < 250.f)
	{
		return false; // trailing, not pushing
	}
	Challenger->ShoulderCooldown = bBarge ? 0.8f : 1.4f;

	if (bBehind)
	{
		const float PushFoul = (bBarge ? 0.7f : 0.3f) * FMath::Lerp(1.3f, 0.6f, Challenger->Attr.Defending);
		if (FMath::FRand() < PushFoul)
		{
			AwardFoul(Challenger, Carrier, 0.3f, TEXT("PUSH"));
			return true;
		}
	}
	const float CarrierPower = (0.6f + 0.8f * Carrier->Attr.Physical + (Carrier->Stance == EPKStance::Shield ? 0.3f : 0.f)) * FMath::FRandRange(0.6f, 1.2f);
	const float ChallengerPower = (0.6f + 0.8f * Challenger->Attr.Physical) * FMath::Lerp(0.85f, 1.15f, Challenger->Attr.Defending) * (bBarge ? 1.15f : 1.f) * FMath::FRandRange(0.6f, 1.2f);
	if (ChallengerPower > CarrierPower * 1.1f)
	{
		// Knocked off the ball: the carrier stumbles sideways and the ball runs loose.
		Carrier->Stun(0.45f);
		Carrier->LaunchCharacter(-ToChallenger * 280.f, false, false);
		Ball->SetController(nullptr);
		Ball->LastTouchTeam = Challenger->Team;
		Challenger->StealCooldown = 0.f;
		++ShoulderWins;
		EventShot(TEXT("Shoulder"), Challenger);
		return true;
	}
	if (CarrierPower > ChallengerPower * 1.1f)
	{
		// Bounced off: the challenger loses a stride.
		Challenger->Stun(0.35f);
		Challenger->LaunchCharacter(ToChallenger * 220.f, false, false);
		Carrier->ProtectedTimer = FMath::Max(Carrier->ProtectedTimer, 0.4f);
		++ShoulderLosses;
		EventShot(TEXT("ShoulderHold"), Carrier);
	}
	return false;
}

void APKGameMode::HumanShoulder()
{
	// X / RB without the ball: barge the carrier (fair from the side, a foul from behind).
	APKFootballer* Carrier = Ball->GetController();
	if (!Human || !Carrier || Carrier->Team == Human->Team || Human->ShoulderCooldown > 0.f || Human->IsInAction() || Human->IsStunned())
	{
		return;
	}
	if (FVector::Dist(Human->GetFlatLocation(), Carrier->GetFlatLocation()) > 120.f)
	{
		return;
	}
	ShoulderChallenge(Carrier, Human, true);
}

bool APKGameMode::InPenaltyArea(int32 DefendingTeam, const FVector& Point) const
{
	const float GoalX = -PK::AttackDir(DefendingTeam) * PK::HalfLength;
	return FMath::Abs(Point.X - GoalX) < PK::PenaltyDepth && FMath::Abs(Point.Y) < PK::PenaltyHalfWidth && FMath::Abs(Point.X) <= PK::HalfLength;
}

void APKGameMode::AwardFoul(APKFootballer* Fouler, APKFootballer* Victim, float Severity, const TCHAR* Label)
{
	if (Phase != EPKPhase::Playing || !Fouler || !Victim)
	{
		return;
	}
	const int32 Team = Victim->Team;
	const FVector Spot = Victim->GetFlatLocation();
	++Fouls[Fouler->Team];
	if (!Victim->IsInAction())
	{
		Victim->PlayTrip(); // brought down
	}
	EventShot(TEXT("Foul"), Victim);

	// Discipline. Denying an obvious goal-scoring chance is a red card.
	bool bClearChance = false;
	if (FVector::Dist(Spot, OppGoal(Team)) < 2600.f && !Fouler->IsKeeper())
	{
		bClearChance = true;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != Team && O != Fouler && !O->IsKeeper()
				&& FVector::Dist(O->GetFlatLocation(), OppGoal(Team)) < FVector::Dist(Spot, OppGoal(Team)) + 100.f)
			{
				bClearChance = false;
				break;
			}
		}
	}
	bool bRed = false;
	bool bYellow = false;
	if (bClearChance && Severity >= 0.4f)
	{
		bRed = true;
	}
	else if (Severity > 0.85f && FMath::FRand() < 0.15f)
	{
		bRed = true;
	}
	else if (FMath::FRand() < FMath::Clamp(Severity - 0.2f, 0.f, 1.f))
	{
		bYellow = true;
		if (++Fouler->YellowCards >= 2)
		{
			bRed = true;
		}
	}
	if (Fouler->IsKeeper() && bRed)
	{
		bRed = false; // no substitute keepers yet: keepers only get booked
		bYellow = true;
	}
	if (bYellow || bRed)
	{
		const bool bSecond = bRed && Fouler->YellowCards >= 2;
		CardText = FString::Printf(TEXT("%s  %s #%d%s"), bRed ? TEXT("RED CARD") : TEXT("YELLOW CARD"), PK::TeamName(Fouler->Team), Fouler->Index + 1, bSecond ? TEXT("  (second yellow)") : TEXT(""));
		CardColor = bRed ? FLinearColor(0.9f, 0.05f, 0.05f) : FLinearColor(1.f, 0.85f, 0.f);
		CardTimer = 3.f;
		++(bRed ? Reds : Yellows)[Fouler->Team];
	}
	UE_LOG(LogPitchKings, Log, TEXT("FOUL (%s) by %s #%d on %s #%d at (%.0f,%.0f) severity %.2f %s"), Label, PK::TeamName(Fouler->Team), Fouler->Index, PK::TeamName(Team), Victim->Index,
		Spot.X, Spot.Y, Severity, bRed ? TEXT("RED") : (bYellow ? TEXT("YELLOW") : TEXT("")));

	const bool bPenalty = InPenaltyArea(Fouler->Team, Spot);
	if (bRed)
	{
		SendOff(Fouler);
	}
	if (bPenalty)
	{
		AwardPenalty(Team);
	}
	else
	{
		AwardFreeKick(Team, Spot, FString::Printf(TEXT("FOUL (%s)  FREE KICK"), Label));
	}
}

void APKGameMode::AwardFreeKick(int32 Team, FVector Spot, const FString& Label)
{
	Spot.X = FMath::Clamp(Spot.X, -PK::HalfLength + 120.f, PK::HalfLength - 120.f);
	Spot.Y = FMath::Clamp(Spot.Y, -PK::HalfWidth + 120.f, PK::HalfWidth - 120.f);
	Spot.Z = 0.f;
	// Never inside the defenders' box for an indirect offence: push it to the edge.
	if (InPenaltyArea(1 - Team, Spot))
	{
		Spot.X = -PK::AttackDir(1 - Team) * (PK::HalfLength - PK::PenaltyDepth - 30.f);
	}
	AwardRestart(Team, Spot, false, false, Label);
	SetPiece = EPKSetPiece::FreeKick;

	// Taker faces the goal; opponents 9.15 m away; a wall when it is within shooting range.
	APKFootballer* Taker = Ball->GetSetPieceTaker();
	const FVector ToGoal = (OppGoal(Team) - Spot).GetSafeNormal2D();
	if (Taker)
	{
		Taker->Teleport2D(Spot - ToGoal * 70.f, ToGoal.Rotation().Yaw);
	}
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team || P->IsKeeper())
		{
			continue;
		}
		const FVector Away = P->GetFlatLocation() - Spot;
		if (Away.Size() < 915.f)
		{
			const FVector Dir = Away.IsNearlyZero() ? -ToGoal : Away.GetSafeNormal2D();
			P->Teleport2D(Spot + Dir * 930.f, (-Dir).Rotation().Yaw);
		}
	}
	const float DGoal = FVector::Dist(Spot, OppGoal(Team));
	if (DGoal < 3200.f)
	{
		TArray<APKFootballer*> Defenders;
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team && !P->IsKeeper())
			{
				Defenders.Add(P);
			}
		}
		Defenders.Sort([&](const APKFootballer& L, const APKFootballer& R)
		{
			return FVector::DistSquared(L.GetFlatLocation(), Spot) < FVector::DistSquared(R.GetFlatLocation(), Spot);
		});
		const int32 WallSize = FMath::Min(Defenders.Num(), DGoal < 2200.f ? 4 : 3);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, ToGoal);
		const FVector WallCentre = Spot + ToGoal * 915.f;
		for (int32 k = 0; k < WallSize; ++k)
		{
			const FVector At = WallCentre + Side * ((k - (WallSize - 1) * 0.5f) * 55.f);
			Defenders[k]->Teleport2D(At, (-ToGoal).Rotation().Yaw);
			Defenders[k]->bHoldPosition = true;
		}
	}
	UE_LOG(LogPitchKings, Log, TEXT("Free kick %s at (%.0f,%.0f), %.0f m from goal"), PK::TeamName(Team), Spot.X, Spot.Y, DGoal / 100.f);
}

void APKGameMode::AwardPenalty(int32 Team)
{
	const float Dir = PK::AttackDir(Team);
	const FVector Spot(Dir * (PK::HalfLength - PK::PenaltySpot), 0.f, 0.f);
	APKFootballer* Taker = nullptr;
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team && !P->IsKeeper() && !P->IsStunned() && (!Taker || P->Attr.Shooting > Taker->Attr.Shooting))
		{
			Taker = P;
		}
	}
	if (!Taker)
	{
		return;
	}
	// Everyone else outside the box and behind the ball; the keeper on the line.
	int32 Slot = 0;
	for (APKFootballer* P : Footballers)
	{
		P->StopAction();
		P->bHoldPosition = true;
		if (P == Taker)
		{
			continue;
		}
		if (P->IsKeeper() && P->Team != Team)
		{
			P->Teleport2D(FVector(Dir * (PK::HalfLength - 20.f), 0.f, 0.f), (-FVector(Dir, 0.f, 0.f)).Rotation().Yaw);
			continue;
		}
		if (P->IsKeeper())
		{
			continue;
		}
		const float X = Dir * (PK::HalfLength - PK::PenaltyDepth - 150.f - (Slot % 3) * 140.f);
		const float Y = ((Slot / 3) - 3) * 420.f + (Slot % 2) * 120.f;
		P->Teleport2D(FVector(X, Y, 0.f), FVector(Dir, 0.f, 0.f).Rotation().Yaw);
		++Slot;
	}
	Taker->Teleport2D(Spot - FVector(Dir * 110.f, 0.f, 0.f), FVector(Dir, 0.f, 0.f).Rotation().Yaw);
	Ball->PlaceAt(Spot, Taker);
	Taker->ProtectedTimer = 3.f;
	Taker->bHoldPosition = false;
	if (Team == 0)
	{
		SetHuman(Taker);
	}
	++Penalties[Team];
	SetPiece = EPKSetPiece::Penalty;
	Phase = EPKPhase::Stoppage;
	PhaseTimer = 1.5f;
	RestartTimer = 0.f;
	ShowMessage(FString::Printf(TEXT("PENALTY - %s"), PK::TeamName(Team)), 2.5f);
	EventShot(TEXT("Penalty"), Taker);
	UE_LOG(LogPitchKings, Log, TEXT("PENALTY to %s, taker #%d"), PK::TeamName(Team), Taker->Index);
}

void APKGameMode::SendOff(APKFootballer* P)
{
	// Off the pitch for the rest of the match: the team plays a man down.
	if (Ball->GetController() == P || Ball->GetHolder() == P)
	{
		Ball->ClearControl();
	}
	P->bSentOff = true;
	P->StopAction();
	P->SetBallArrow(false);
	if (Human == P)
	{
		P->SetHumanControlled(false);
		Human = nullptr;
	}
	Footballers.Remove(P);
	SentOff.Add(P);
	P->Teleport2D(FVector(0.f, (P->Team == 0 ? -1.f : 1.f) * (PK::HalfWidth + 900.f), 0.f), 0.f);
	P->SetActorHiddenInGame(true);
	P->SetActorEnableCollision(false);
	P->SetActorTickEnabled(false);
	if (!Human)
	{
		SetHuman(ClosestOutfield(0, Ball->GetActorLocation(), nullptr));
	}
	UE_LOG(LogPitchKings, Log, TEXT("SENT OFF: %s #%d (%d left)"), PK::TeamName(P->Team), P->Index, Footballers.FilterByPredicate([P](const APKFootballer* F) { return F->Team == P->Team; }).Num());
}

void APKGameMode::RestoreSentOff()
{
	for (APKFootballer* P : SentOff)
	{
		P->bSentOff = false;
		P->SetActorHiddenInGame(false);
		P->SetActorEnableCollision(true);
		P->SetActorTickEnabled(true);
		Footballers.Add(P);
	}
	SentOff.Empty();
	for (APKFootballer* P : Footballers)
	{
		P->YellowCards = 0;
		P->bHoldPosition = false;
	}
}

bool APKGameMode::IsOffsidePosition(const APKFootballer* P, float BallX) const
{
	// In the opponents' half, ahead of the ball and of the second-last opponent.
	const float Fx = WorldToFx(P->Team, P->GetActorLocation().X);
	if (Fx <= 0.f || Fx <= WorldToFx(P->Team, BallX))
	{
		return false;
	}
	float Last = -2.f, SecondLast = -2.f;
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const float OFx = WorldToFx(P->Team, O->GetActorLocation().X);
		if (OFx > Last)
		{
			SecondLast = Last;
			Last = OFx;
		}
		else if (OFx > SecondLast)
		{
			SecondLast = OFx;
		}
	}
	return Fx > SecondLast + 30.f / PK::HalfLength;
}

void APKGameMode::MarkOffside(const APKFootballer* Kicker)
{
	const float BallX = Ball->GetActorLocation().X;
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Kicker->Team && P != Kicker)
		{
			P->OffsideKickId = IsOffsidePosition(P, BallX) ? Ball->KickId : -1;
		}
	}
}

bool APKGameMode::CheckOffside(APKFootballer* Receiver)
{
	if (Phase != EPKPhase::Playing || Receiver->OffsideKickId != Ball->KickId || !Ball->LastKicker || Ball->LastKicker->Team != Receiver->Team)
	{
		return false;
	}
	Receiver->OffsideKickId = -1;
	++Offsides[Receiver->Team];
	UE_LOG(LogPitchKings, Log, TEXT("OFFSIDE %s #%d at (%.0f,%.0f)"), PK::TeamName(Receiver->Team), Receiver->Index, Receiver->GetActorLocation().X, Receiver->GetActorLocation().Y);
	EventShot(TEXT("Offside"), Receiver);
	AwardFreeKick(1 - Receiver->Team, Receiver->GetFlatLocation(), FString::Printf(TEXT("OFFSIDE  %s #%d"), PK::TeamName(Receiver->Team), Receiver->Index + 1));
	return true;
}

""" + anchor, 1)
open(path, "w", encoding="utf-8").write(s)

# ---------------------------------------------------------------- input
patch("PKPlayerController.cpp", [
    ("\t\tif (AnyPressed({ EKeys::Q, EKeys::Gamepad_LeftShoulder }))\n\t\t{\n\t\t\tGM->HumanSwitch();\n\t\t}",
     "\t\tif (AnyPressed({ EKeys::Q, EKeys::Gamepad_LeftShoulder }))\n\t\t{\n\t\t\tGM->HumanSwitch();\n\t\t}\n"
     "\t\tif (AnyPressed({ EKeys::X, EKeys::Gamepad_RightShoulder }))\n\t\t{\n\t\t\tGM->HumanShoulder();\n\t\t}"),
])

# ---------------------------------------------------------------- HUD: cards and players left
patch("PKHUD.cpp", [
    ("\tDrawScoreboard(GM);\n\tDrawRadar(GM);",
     "\tDrawScoreboard(GM);\n\tDrawRadar(GM);\n"
     "\tif (GM->CardTimer > 0.f)\n\t{\n"
     "\t\t// Referee's card: a coloured card and who was booked.\n"
     "\t\tconst float CX = Canvas->ClipX * 0.5f;\n\t\tconst float CY = Canvas->ClipY * 0.36f;\n"
     "\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - 230.f, CY - 6.f, 460.f, 64.f);\n"
     "\t\tDrawRect(GM->CardColor, CX - 216.f, CY + 2.f, 34.f, 48.f);\n"
     "\t\tDrawText(GM->CardText, FLinearColor::White, CX - 170.f, CY + 14.f, GEngine->GetMediumFont(), 1.2f);\n\t}\n"
     "\t// Sent-off players: red marks next to the score.\n"
     "\t{\n\t\tint32 Count[2] = { 0, 0 };\n"
     "\t\tfor (const APKFootballer* Off : GM->SentOff)\n\t\t{\n"
     "\t\t\tconst int32 T = Off->Team;\n"
     "\t\t\tDrawRect(FLinearColor(0.9f, 0.05f, 0.05f), (T == 0 ? 26.f : 330.f) + Count[T] * 12.f, 120.f, 9.f, 13.f);\n"
     "\t\t\t++Count[T];\n\t\t}\n\t}"),
    ("\t\t\t\tTEXT(\"STYLE  1 Balanced",
     "\t\t\t\tTEXT(\"PHYSICAL  shoulder barge X / RB (no ball) - fair from the side, a foul from behind\"),\n"
     "\t\t\t\tTEXT(\"STYLE  1 Balanced"),
    ("\t\t\tfloat Y = Canvas->ClipY - 150.f;\n\t\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 12.f, Y - 8.f, 700.f, 132.f);",
     "\t\t\tfloat Y = Canvas->ClipY - 170.f;\n\t\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 12.f, Y - 8.f, 700.f, 152.f);"),
])
print("fouls ok")
