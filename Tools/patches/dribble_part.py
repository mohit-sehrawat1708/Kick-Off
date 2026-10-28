"""Active-Touch-style dribbling: touches happen on real foot steps (detected from the animated feet) and each
touch sends the ball to where that player's foot will be N steps later; stride adjustment in between.
The previous system stays available with -PKOldDribble for comparison; -PKDribbleTest measures both."""
import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def load(p):
    return open(os.path.join(base, p), encoding="utf-8").read()


def save(p, s):
    open(os.path.join(base, p), "w", encoding="utf-8").write(s)


def rep(s, a, b):
    if a not in s:
        raise SystemExit("NOT FOUND: " + a[:100])
    return s.replace(a, b, 1)


# ---------------------------------------------------------------- footballer: steps from the animated feet
h = load("PKFootballer.h")
h = rep(h, "\tFVector HumanInput = FVector::ZeroVector;",
        "\tFVector HumanInput = FVector::ZeroVector;\n\n"
        "\t// Steps read from the animated feet: a step is the swinging foot passing under the body near the ground.\n"
        "\tint32 SwingCounter = 0;         // +1 per step\n"
        "\tint32 SeenSwingCounter = 0;     // dribble code's last processed step\n"
        "\tint32 LastSwingFoot = 0;        // 0 left, 1 right\n"
        "\tdouble LastSwingTime = -10.0;\n"
        "\tfloat StepInterval = 0.3f;      // smoothed seconds per step\n"
        "\tint32 StepsSinceTouch = 0;\n"
        "\tfloat DribbleSpeedScale = 1.f;  // stride adjustment while dribbling\n"
        "\tFVector GetFootContactPoint(int32 Foot) const; // where that foot meets the ball (flat)\n"
        "\tFVector HumanInputPlaceholder;")
h = h.replace("\tFVector HumanInputPlaceholder;", "")
h = rep(h, "\tvoid UpdateMovementModel(float DeltaSeconds);", "\tvoid UpdateMovementModel(float DeltaSeconds);\n\tvoid UpdateFootSteps();\n\tfloat FootFwdPrev[2] = { 0.f, 0.f };")
save("PKFootballer.h", h)

f = load("PKFootballer.cpp")
f = rep(f, "\tUpdateAction(DeltaSeconds);\n\tUpdateMovementModel(DeltaSeconds);\n}",
        "\tUpdateAction(DeltaSeconds);\n\tUpdateMovementModel(DeltaSeconds);\n\tUpdateFootSteps();\n\tif (!bHasBall)\n\t{\n\t\tDribbleSpeedScale = 1.f;\n\t}\n}\n\n"
        r"""void APKFootballer::UpdateFootSteps()
{
	// The pose used here is last frame's; that is fine for counting steps.
	const USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->DoesSocketExist(TEXT("ball_l")))
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	const FVector Facing = GetFacing();
	const float GroundZ = Loc.Z - CapsuleHalfHeight;
	const float Speed = PK::Flat(GetVelocity()).Size();
	const double Now = GetWorld()->GetTimeSeconds();
	constexpr float SwingLine = 10.f; // cm ahead of the body centre
	for (int32 Foot = 0; Foot < 2; ++Foot)
	{
		const FVector P = Body->GetSocketLocation(Foot == 0 ? TEXT("ball_l") : TEXT("ball_r"));
		const float Fwd = FVector::DotProduct(PK::Flat(P - Loc), Facing);
		const float Height = P.Z - GroundZ;
		if (Speed > 60.f && FootFwdPrev[Foot] < SwingLine && Fwd >= SwingLine && Height < 35.f)
		{
			const float Interval = (float)(Now - LastSwingTime);
			if (Interval > 0.1f && Interval < 0.8f)
			{
				StepInterval = FMath::Lerp(StepInterval, Interval, 0.35f);
			}
			LastSwingTime = Now;
			LastSwingFoot = Foot;
			++SwingCounter;
		}
		FootFwdPrev[Foot] = Fwd;
	}
}

FVector APKFootballer::GetFootContactPoint(int32 Foot) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	const FVector Toe = Body ? Body->GetSocketLocation(Foot == 0 ? TEXT("ball_l") : TEXT("ball_r")) : GetActorLocation() + GetFacing() * 40.f;
	// The ball centre sits a ball radius in front of the toe joint.
	return PK::Flat(Toe) + GetFacing() * PK::BallRadius;
}""")
f = rep(f, "\tMove->MaxWalkSpeed = MaxSpeed * PaceScale;", "\tMove->MaxWalkSpeed = MaxSpeed * PaceScale * (bHasBall ? DribbleSpeedScale : 1.f);")
save("PKFootballer.cpp", f)

# ---------------------------------------------------------------- game mode
g = load("PKGameMode.h")
g = rep(g, "\tvoid UpdateDribble(APKFootballer* P, float Dt);",
        "\tvoid UpdateDribble(APKFootballer* P, float Dt);\n\tvoid UpdateDribbleLegacy(APKFootballer* P, float Dt);\n\tvoid UpdateDribbleTest(float Dt);")
g = rep(g, "\tint32 StalemateBreaks = 0;",
        "\tint32 StalemateBreaks = 0;\n"
        "\t// Dribbling metrics (-PKDribbleTest and the autotest line).\n"
        "\tbool bOldDribble = false;\n\tbool bDribbleTest = false;\n"
        "\tint32 DribbleTouches = 0;\n\tint32 StepTouches = 0;\n\tdouble TouchGapSum = 0.0;\n\tdouble DribbleSepSum = 0.0;\n\tint32 DribbleSepSamples = 0;\n"
        "\tfloat DribbleSepMax = 0.f;\n\tint32 DribbleLosses = 0;\n\tdouble DribbleSpeedSum = 0.0;\n"
        "\tfloat DribbleTestClock = 0.f;\n\tint32 DribbleTestPhase = -1;\n\tTWeakObjectPtr<APKFootballer> DribbleTestPlayer;")
save("PKGameMode.h", g)

c = load("PKGameMode.cpp")
c = rep(c, '\tbHardCollision = FParse::Param(FCommandLine::Get(), TEXT("PKHardCollision"));\n',
        '\tbHardCollision = FParse::Param(FCommandLine::Get(), TEXT("PKHardCollision"));\n'
        '\tbOldDribble = FParse::Param(FCommandLine::Get(), TEXT("PKOldDribble"));\n'
        '\tbDribbleTest = FParse::Param(FCommandLine::Get(), TEXT("PKDribbleTest"));\n')
c = rep(c, "void APKGameMode::UpdateDribble(APKFootballer* P, float Dt)\n{", "void APKGameMode::UpdateDribbleLegacy(APKFootballer* P, float Dt)\n{")
c = rep(c, "void APKGameMode::UpdateDribbleLegacy(APKFootballer* P, float Dt)\n", r"""void APKGameMode::UpdateDribble(APKFootballer* P, float Dt)
{
	// Active-Touch-style dribbling. The ball is never pulled by an invisible force: every touch happens on a
	// real step of the animated feet, with the foot that is swinging, and sends the ball exactly far enough
	// to meet that player's foot again N steps later (rolling friction included). Between touches the player
	// lengthens or shortens his stride a little so the foot and the ball meet.
	if (bOldDribble || P->Stance == EPKStance::Shield)
	{
		UpdateDribbleLegacy(P, Dt);
		return;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Flat = PK::Flat(BallLoc);
	const FVector Body = P->GetFlatLocation();
	const float Dist = FVector::Dist(Flat, Body);
	if (Dist > ControlLoseDistance)
	{
		Ball->SetController(nullptr); // heavy touch / outran it: ball is loose
		++DribbleLosses;
		return;
	}
	const bool bSwing = P->SwingCounter != P->SeenSwingCounter;
	P->SeenSwingCounter = P->SwingCounter;
	if (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None || P->BufferTimer > 0.f)
	{
		P->DribbleSpeedScale = 1.f;
		return;
	}
	if (bSwing)
	{
		++P->StepsSinceTouch;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Vel = PK::Flat(P->GetVelocity());
	const float Speed = Vel.Size();
	const FVector Facing = P->GetFacing();
	FVector Desired = P->MoveInput.IsNearlyZero() ? Facing : P->MoveInput.GetSafeNormal2D();
	const bool bClose = P->Stance == EPKStance::CloseControl;
	const bool bSprinting = P->IsSprinting() && !bClose;

	// Touch rhythm: one touch per step in close control, every second step at jog, every third when sprinting.
	// Poor control means longer, less frequent touches; Technical players keep it tighter.
	const float Control = FMath::Clamp(P->Attr.Control * (P->HasTrait(PKTrait::Technical) ? 1.06f : 1.f), 0.f, 1.f);
	int32 StepsPerTouch = bClose ? 1 : (bSprinting ? (Control > 0.8f ? 3 : 4) : 2);

	// Turning: the body can only take the ball ~40 degrees off its facing; a sharp turn is a short touch on the next step.
	const float TurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Desired), -1.f, 1.f)));
	const bool bTurnTouch = TurnDeg > 35.f;
	if (TurnDeg > 40.f)
	{
		const float SideSign = FVector::CrossProduct(Facing, Desired).Z >= 0.f ? 1.f : -1.f;
		Desired = Facing.RotateAngleAxis(40.f * SideSign, FVector::UpVector);
	}
	if (bTurnTouch)
	{
		StepsPerTouch = 1;
	}

	// Standing still: the ball is stopped under the sole.
	if (P->MoveInput.IsNearlyZero() && Speed < 120.f)
	{
		P->DribbleSpeedScale = 1.f;
		const FVector Sole = P->GetFootPoint();
		if (FVector::Dist(Flat, Sole) < 80.f)
		{
			const FVector Eased = FMath::VInterpTo(Flat, Sole, Dt, 4.f);
			Ball->SetActorLocation(FVector(Eased.X, Eased.Y, BallLoc.Z));
			Ball->Velocity.X *= FMath::Max(0.f, 1.f - 6.f * Dt);
			Ball->Velocity.Y *= FMath::Max(0.f, 1.f - 6.f * Dt);
		}
		return;
	}

	// Where the ball should be when a foot reaches it: just in front of the swing line.
	const FVector MeetZone = Body + Desired * 45.f;
	const float Gap = FVector::DotProduct(Flat - MeetZone, Desired); // + ahead of the zone, - behind it
	const float Lateral = FMath::Abs(FVector::DotProduct(Flat - Body, FVector::CrossProduct(FVector::UpVector, Desired)));

	bool bTouch = false;
	bool bOnStep = false;
	float FootGap = 0.f;
	if (bSwing && P->TouchTimer <= 0.f)
	{
		const FVector FootContact = P->GetFootContactPoint(P->LastSwingFoot);
		FootGap = FVector::Dist(Flat, FootContact);
		const float Reach = bClose ? 45.f : 55.f;
		// Due (enough steps since the last touch), or the ball is about to be overrun, or the player is turning.
		if (FootGap < Reach && (P->StepsSinceTouch >= StepsPerTouch || Gap < -10.f || bTurnTouch))
		{
			bTouch = true;
			bOnStep = true;
		}
	}
	// No steps seen for a while (starting from a standstill, coming out of an action): play it from the front foot.
	if (!bTouch && Now - P->LastSwingTime > 0.6 && P->TouchTimer <= 0.f && FVector::Dist(Flat, P->GetFootPoint()) < 60.f)
	{
		bTouch = true;
	}
	// About to run over the ball between steps: nudge it on now rather than stepping on it.
	if (!bTouch && Gap < -30.f && Dist < 70.f && Lateral < 40.f && P->TouchTimer <= 0.f)
	{
		bTouch = true;
	}

	if (bTouch)
	{
		// Plan the meeting: the player's position N steps from now, the ball rolling there with friction.
		const float StepT = FMath::Clamp(P->StepInterval, 0.16f, 0.5f);
		const float T = FMath::Max(0.24f, StepT * StepsPerTouch);
		const float TopSpeed = (bClose ? PK::WalkSpeed : (bSprinting ? PK::SprintSpeed : PK::JogSpeed)) * 0.93f * P->PaceScale * FMath::Clamp(P->MoveInput.Size(), 0.3f, 1.f);
		const float AvgSpeed = FMath::Max(80.f, FMath::Lerp(Speed, TopSpeed, Speed < TopSpeed ? 0.5f : 0.2f));
		const FVector Target = Body + Desired * (AvgSpeed * T + 45.f);
		FVector ToTarget = Target - Flat;
		const float S = FMath::Max(ToTarget.Size(), 1.f);
		ToTarget /= S;
		float V0 = S / T + 0.5f * PK::RollDecel * T;
		// Touch quality: a less controlled player's touch wanders and its weight varies.
		const float Wander = (1.f - Control) * (bSprinting ? 10.f : 6.f);
		ToTarget = ToTarget.RotateAngleAxis(FMath::FRandRange(-Wander, Wander), FVector::UpVector);
		V0 *= 1.f + FMath::FRandRange(-0.12f, 0.12f) * (1.f - Control);
		FVector NewVel = ToTarget * FMath::Clamp(V0, 60.f, 1600.f);
		if (bSprinting)
		{
			NewVel.Z = FMath::FRandRange(40.f, 110.f); // knocked on, bobbling a little
		}
		if (bOnStep)
		{
			// The touching foot is where the ball leaves from.
			const FVector Contact = P->GetFootContactPoint(P->LastSwingFoot);
			const FVector Eased = FMath::Lerp(Flat, Contact, 0.4f);
			Ball->SetActorLocation(FVector(Eased.X, Eased.Y, BallLoc.Z));
			TouchGapSum += FootGap;
			++StepTouches;
		}
		Ball->Touch(P, NewVel);
		P->StepsSinceTouch = 0;
		P->TouchTimer = 0.12f;
		++DribbleTouches;
	}

	// Stride adjustment: run a touch faster to catch a ball that is getting away, ease off before overrunning it.
	float Scale = 1.f;
	if (Gap > 70.f)
	{
		Scale = 1.f + FMath::Clamp((Gap - 70.f) / 160.f, 0.f, 0.18f);
	}
	else if (Gap < -5.f)
	{
		Scale = FMath::Clamp(1.f + Gap / 90.f, 0.72f, 1.f);
	}
	P->DribbleSpeedScale = FMath::FInterpTo(P->DribbleSpeedScale, Scale, Dt, 8.f);

	// Metrics: how far the ball is from where the foot wants it.
	const float Sep = FVector::Dist(Flat, MeetZone);
	DribbleSepSum += Sep;
	DribbleSpeedSum += Speed;
	++DribbleSepSamples;
	DribbleSepMax = FMath::Max(DribbleSepMax, Sep);
}

void APKGameMode::UpdateDribbleTest(float Dt)
{
	// -PKDribbleTest: one Blue player dribbles a fixed course alone (jog, sprint, zig-zag, close control,
	// turn back, stop); the dribbling metrics are logged per phase. Run with -PKOldDribble to compare.
	if (!bDribbleTest || Phase != EPKPhase::Playing)
	{
		return;
	}
	DribbleTestClock += Dt;
	APKFootballer* P = DribbleTestPlayer.Get();
	if (DribbleTestPhase < 0)
	{
		if (DribbleTestClock < 1.f)
		{
			return;
		}
		for (APKFootballer* F : Footballers)
		{
			if (F->Team == 0 && F->Index == 9)
			{
				P = F;
			}
		}
		if (!P)
		{
			return;
		}
		int32 k = 0;
		for (APKFootballer* F : Footballers)
		{
			if (F != P)
			{
				F->Teleport2D(FVector(-4000.f + (k++) * 380.f, PK::HalfWidth - 300.f, 0.f), -90.f);
				F->Stun(9999.f);
			}
		}
		P->Teleport2D(FVector(-3500.f, -600.f, 0.f), 0.f);
		P->bHuman = true; // keeps the AI away from him; the test drives his input
		Ball->ClearControl();
		Ball->SetActorLocation(FVector(PK::Flat(P->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
		Ball->Velocity = FVector::ZeroVector;
		Ball->SetController(P);
		DribbleTestPlayer = P;
		DribbleTestPhase = 0;
		DribbleTestClock = 0.f;
		DribbleTouches = StepTouches = DribbleSepSamples = DribbleLosses = 0;
		TouchGapSum = DribbleSepSum = DribbleSpeedSum = 0.0;
		DribbleSepMax = 0.f;
		return;
	}
	if (!P)
	{
		return;
	}
	struct FPhaseDef { const TCHAR* Name; float Seconds; };
	static const FPhaseDef Phases[] = { { TEXT("jog"), 3.f }, { TEXT("sprint"), 3.f }, { TEXT("zigzag"), 4.f }, { TEXT("close control"), 3.f }, { TEXT("turn back"), 2.5f }, { TEXT("stop"), 2.f } };
	if (DribbleTestPhase >= (int32)UE_ARRAY_COUNT(Phases))
	{
		return;
	}
	const FPhaseDef& Ph = Phases[DribbleTestPhase];
	// Drive the input.
	P->Stance = EPKStance::Normal;
	P->bSprint = false;
	FVector Input(1.f, 0.f, 0.f);
	switch (DribbleTestPhase)
	{
	case 1: P->bSprint = true; break;
	case 2: Input = FVector(1.f, FMath::Fmod(DribbleTestClock, 1.6f) < 0.8f ? 1.f : -1.f, 0.f).GetSafeNormal(); break;
	case 3: P->Stance = EPKStance::CloseControl; Input = FVector(0.f, 1.f, 0.f); break;
	case 4: Input = FVector(-1.f, 0.f, 0.f); break;
	case 5: Input = FVector::ZeroVector; break;
	default: break;
	}
	P->MoveInput = Input;
	if (Ball->GetController() != P && Ball->IsFree() && FVector::Dist(PK::Flat(Ball->GetActorLocation()), P->GetFlatLocation()) < 120.f)
	{
		Ball->SetController(P); // won it back: keep the course going
	}
	if (bCloseCamera && FMath::IsNearlyEqual(DribbleTestClock, Ph.Seconds * 0.5f, Dt * 0.6f))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("Dribble_%s_%d.png"), bOldDribble ? TEXT("old") : TEXT("new"), DribbleTestPhase)), true, false);
	}
	if (DribbleTestClock >= Ph.Seconds)
	{
		UE_LOG(LogPitchKings, Log, TEXT("[DribbleTest %s] %-13s touches %2d (on steps %2d, foot-ball gap at touch %.0f cm) | ball from foot zone avg %.0f max %.0f cm | lost %d | speed %.0f | step %.2fs"),
			bOldDribble ? TEXT("OLD") : TEXT("NEW"), Ph.Name, DribbleTouches, StepTouches, StepTouches ? TouchGapSum / StepTouches : 0.0,
			DribbleSepSamples ? DribbleSepSum / DribbleSepSamples : 0.0, DribbleSepMax, DribbleLosses, DribbleSepSamples ? DribbleSpeedSum / DribbleSepSamples : 0.0, P->StepInterval);
		DribbleTouches = StepTouches = DribbleSepSamples = DribbleLosses = 0;
		TouchGapSum = DribbleSepSum = DribbleSpeedSum = 0.0;
		DribbleSepMax = 0.f;
		++DribbleTestPhase;
		DribbleTestClock = 0.f;
		if (DribbleTestPhase >= (int32)UE_ARRAY_COUNT(Phases))
		{
			FGenericPlatformMisc::RequestExit(false);
		}
	}
}

void APKGameMode::UpdateDribbleLegacy(APKFootballer* P, float Dt)
""")
# legacy keeps its own metrics so OLD vs NEW compare like for like
c = rep(c, "\t\tBall->Touch(P, NewVel);\n\t\tP->TouchTimer = Interval;\n",
        "\t\tBall->Touch(P, NewVel);\n\t\tP->TouchTimer = Interval;\n\t\t++DribbleTouches;\n")
c = rep(c, "\tif (Dist > ControlLoseDistance)\n\t{\n\t\tBall->SetController(nullptr); // heavy touch / outran it: ball is loose\n\t\treturn;\n\t}\n\tif (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None || P->BufferTimer > 0.f)\n\t{\n\t\treturn;\n\t}\n\n\tconst FVector Vel = PK::Flat(P->GetVelocity());",
        "\tif (Dist > ControlLoseDistance)\n\t{\n\t\tBall->SetController(nullptr); // heavy touch / outran it: ball is loose\n\t\t++DribbleLosses;\n\t\treturn;\n\t}\n\tif (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None || P->BufferTimer > 0.f)\n\t{\n\t\treturn;\n\t}\n"
        "\t{\n\t\tconst FVector Zone = P->GetFlatLocation() + (P->MoveInput.IsNearlyZero() ? P->GetFacing() : P->MoveInput.GetSafeNormal2D()) * 45.f;\n"
        "\t\tconst float Sep = FVector::Dist(Flat, Zone);\n\t\tDribbleSepSum += Sep;\n\t\tDribbleSpeedSum += PK::Flat(P->GetVelocity()).Size();\n\t\t++DribbleSepSamples;\n\t\tDribbleSepMax = FMath::Max(DribbleSepMax, Sep);\n\t}\n\n\tconst FVector Vel = PK::Flat(P->GetVelocity());")
# run the test from the autotest tick
c = rep(c, "\tAutoTestClock += Dt;\n\tUpdateRulesTest(Dt);", "\tAutoTestClock += Dt;\n\tUpdateRulesTest(Dt);\n\tUpdateDribbleTest(Dt);")
# dribbling numbers on the autotest line
c = rep(c, "| shoulder won %d lost %d | stuck %.1fs stalemates %d\"),",
        "| shoulder won %d lost %d | stuck %.1fs stalemates %d | dribble touches %d (steps %d, gap %.0f) zone %.0f lost %d\"),")
c = rep(c, "FreeKickShots, ShoulderWins, ShoulderLosses, StuckSeconds, StalemateBreaks);",
        "FreeKickShots, ShoulderWins, ShoulderLosses, StuckSeconds, StalemateBreaks,\n\t\tDribbleTouches, StepTouches, StepTouches ? TouchGapSum / StepTouches : 0.0, DribbleSepSamples ? DribbleSepSum / DribbleSepSamples : 0.0, DribbleLosses);")
save("PKGameMode.cpp", c)
print("dribble ok")
