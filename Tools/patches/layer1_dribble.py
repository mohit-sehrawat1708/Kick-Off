# Layer 1: new input actions (Precision / Runs / Skill), Controlled Sprint, Effort Dribble Touch.
import os, sys
SRC = os.path.join(os.path.dirname(__file__), "..", "..", "Source", "PitchKings")

def patch(name, pairs):
    p = os.path.join(SRC, name)
    s = open(p, encoding="utf-8").read()
    for a, b in pairs:
        if a not in s:
            sys.exit(f"{name}: not found: {a[:70]!r}")
        s = s.replace(a, b, 1)
    open(p, "w", encoding="utf-8").write(s)

patch("PKSettings.h", [
("""	FlickUp,
	Pause,
	Count""", """	FlickUp,
	Pause,
	// Added later: kept after the original actions so older saved bindings still load.
	Precision, // with the ball: controlled sprint, precision pass, finesse shot; without: teammate press
	Runs,      // with the ball: send a run, one-two, chip
	Skill,     // hold + direction: skill move (gamepad: right stick flick)
	Count"""),
])

patch("PKSettings.cpp", [
("""			{ TEXT("Shoulder barge"), EKeys::X, FKey(), EKeys::Gamepad_RightShoulder, FKey() },
			{ TEXT("Knock-on"), EKeys::F, FKey(), EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightThumbstick },""",
"""			{ TEXT("Shoulder barge"), EKeys::X, FKey(), EKeys::Gamepad_FaceButton_Bottom, FKey() },
			{ TEXT("Effort touch (tap short, hold long)"), EKeys::F, FKey(), EKeys::Gamepad_RightThumbstick, FKey() },"""),
("""			{ TEXT("Pause menu"), EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right, FKey() },
		};""", """			{ TEXT("Pause menu"), EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right, FKey() },
			{ TEXT("Controlled sprint / precision / finesse; no ball: teammate press"), EKeys::R, FKey(), EKeys::Gamepad_RightShoulder, FKey() },
			{ TEXT("Runs: send a run / one-two / chip"), EKeys::Z, FKey(), EKeys::Gamepad_LeftShoulder, FKey() },
			{ TEXT("Skill move (hold + direction)"), EKeys::B, FKey(), EKeys::Gamepad_Right2D, FKey() },
		};"""),
("""		if (Key == EKeys::Gamepad_Special_Right) return TEXT("Start");""",
"""		if (Key == EKeys::Gamepad_Special_Right) return TEXT("Start");
		if (Key == EKeys::Gamepad_Right2D) return TEXT("right stick flick");"""),
])

patch("PKGameInstance.cpp", [
("""	if (Data->Keys.Num() == 2 * (int32)EPKInput::Count)
	{
		for (int32 i = 0; i < (int32)EPKInput::Count; ++i)""",
"""	// Saves from older versions have fewer actions; the new ones keep their defaults.
	if (Data->Keys.Num() % 2 == 0 && Data->Keys.Num() <= 2 * (int32)EPKInput::Count)
	{
		for (int32 i = 0; i < Data->Keys.Num() / 2; ++i)"""),
])

patch("PKTypes.h", [
("""	constexpr float SprintSpeed = 840.f;""", """	constexpr float SprintSpeed = 840.f;
	constexpr float ControlledSprintSpeed = 700.f; // FC-style controlled sprint: quicker than a jog, ball kept close"""),
])

patch("PKFootballer.h", [
("""	bool bSprint = false;""", """	bool bSprint = false;
	bool bControlledSprint = false; // carrier only: between jog and sprint, a touch every step"""),
])

patch("PKFootballer.cpp", [
("""	default:                      MaxSpeed = bSprint ? PK::SprintSpeed : PK::JogSpeed; break;""",
"""	default:                      MaxSpeed = bSprint ? PK::SprintSpeed : (bControlledSprint ? PK::ControlledSprintSpeed : PK::JogSpeed); break;"""),
("""	UpdateFootSteps();
	if (!bHasBall)
	{""", """	UpdateFootSteps();
	if (!bHasBall)
	{
		bControlledSprint = false;"""),
])

patch("PKGameMode.h", [
("""	void HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier);""",
"""	void HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier, bool bPrecision = false);"""),
("""	void HumanKnockOn(int32 Team, const FVector& Aim);""",
"""	// Effort dribble touch: knock the ball any way; Strength 0..1 (tap = short knock, hold = long one).
	void HumanKnockOn(int32 Team, const FVector& Aim, float Strength = 1.f);"""),
])

patch("PKGameMode.cpp", [
# human move: controlled sprint
("""void APKGameMode::HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier)""",
"""void APKGameMode::HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier, bool bPrecision)"""),
("""		Human->MoveInput = Human->HumanInput;
		Human->bSprint = bCanMove && bInSprint && !bModifier;
	}""", """		Human->MoveInput = Human->HumanInput;
		// Controlled sprint (Precision held while running with the ball) replaces the full sprint.
		Human->bControlledSprint = bCanMove && bPrecision && !bModifier && Ball->GetController() == Human && Direction.Size() > 0.3f;
		Human->bSprint = bCanMove && bInSprint && !bModifier && !Human->bControlledSprint;
	}"""),
# AI carrier: controlled sprint under moderate pressure
("""	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = NearOpp > 500.f && P->Stance == EPKStance::Normal;
}""", """	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = NearOpp > 500.f && P->Stance == EPKStance::Normal;
	// Closing defender: keep the speed up but the ball tight.
	P->bControlledSprint = !P->bSprint && NearOpp > 180.f && P->Stance == EPKStance::Normal && P->Attr.Control > 0.55f;
}"""),
# dribble rhythm
("""	const bool bSprinting = P->IsSprinting() && !bClose;
""", """	const bool bSprinting = P->IsSprinting() && !bClose;
	const bool bControlled = P->bControlledSprint && !bSprinting && !bClose;
"""),
("""	int32 StepsPerTouch = bClose ? 1 : (bSprinting ? (Control > 0.75f ? 3 : 4) : 2);
	float PressDist = 0.f;""", """	int32 StepsPerTouch = bClose ? 1 : (bSprinting ? (Control > 0.75f ? 3 : 4) : 2);
	if (bControlled)
	{
		StepsPerTouch = Control > 0.7f ? 1 : 2; // controlled sprint: a touch every step for good dribblers
	}
	float PressDist = 0.f;"""),
("""		const float TopSpeed = (bClose ? PK::WalkSpeed : (bSprinting ? PK::SprintSpeed : PK::JogSpeed)) * 0.93f""",
"""		const float TopSpeed = (bClose ? PK::WalkSpeed : (bSprinting ? PK::SprintSpeed : (bControlled ? PK::ControlledSprintSpeed : PK::JogSpeed))) * 0.93f"""),
# effort touch
("""void APKGameMode::HumanKnockOn(int32 Team, const FVector& Aim)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	// Push the ball into space and chase it.
	if (!Human || Ball->GetController() != Human || !CanReachBall(Human, 90.f) || Human->IsKicking())
	{
		return;
	}
	const FVector Dir = Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D();
	Ball->Touch(Human, Dir * (PK::Flat(Human->GetVelocity()).Size() + 750.f) + FVector(0.f, 0.f, 180.f));
	Human->TouchTimer = 0.7f;
	Human->bSprint = true;
}""", """void APKGameMode::HumanKnockOn(int32 Team, const FVector& Aim, float Strength)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	// Effort dribble touch: knock the ball any way at once. A tap is a short touch that stays under control,
	// a held press pushes it into space to run onto.
	if (!Human || Ball->GetController() != Human || !CanReachBall(Human, 100.f) || Human->IsKicking())
	{
		return;
	}
	const float S = FMath::Clamp(Strength, 0.2f, 1.f);
	const FVector Facing = Human->GetFacing();
	const FVector Dir = Aim.IsNearlyZero() ? Facing : Aim.GetSafeNormal2D();
	const float Along = FMath::Max(0.f, FVector::DotProduct(PK::Flat(Human->GetVelocity()), Dir));
	FVector V = Dir * (Along + FMath::Lerp(260.f, 900.f, S));
	V.Z = S > 0.7f ? 160.f * (S - 0.7f) / 0.3f : 0.f; // a long knock skips off the grass
	Ball->Touch(Human, V);
	Human->TouchTimer = FMath::Lerp(0.2f, 0.6f, S);
	Human->bSprint = S > 0.6f;
	// Sideways knocks go with the outside of the foot on that side; forward ones with the laces.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	const float Lat = FVector::DotProduct(Dir, Right);
	if (FMath::Abs(Lat) > 0.3f)
	{
		Human->NoteFootTouch(Lat > 0.f ? 1 : 0, FMath::Sign(Lat) * FMath::Min(1.f, FMath::Abs(Lat) + 0.3f), true);
	}
	++EffortTouches;
}"""),
])

patch("PKGameMode.h", [
("""	int32 InsideCuts = 0;""", """	int32 InsideCuts = 0;
	int32 EffortTouches = 0;"""),
])

# Controller: Precision to HumanMove, effort touch charging (F tap/hold, RB + right stick flick, R3).
patch("PKPlayerController.h", [
("""	bool bTeamCharging[2] = { false, false };""", """	bool bTeamCharging[2] = { false, false };
	// Effort touch being held (keyboard F / R3), or a right stick flick with RB.
	bool bEffortHeld[2] = { false, false };
	float EffortTime[2] = { 0.f, 0.f };
	FVector EffortDir[2];
	bool bRightStickFlick[2] = { false, false };
	FVector ReadRightStick(int32 Device) const; // camera-relative, zero inside the dead zone"""),
])

patch("PKPlayerController.cpp", [
("""	const bool bModifier = ActionDown(Dev, EPKInput::Hold) || Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	GM->HumanMove(Team, Move, bSprint, bModifier);""",
"""	const bool bModifier = ActionDown(Dev, EPKInput::Hold) || Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	const bool bPrecision = ActionDown(Dev, EPKInput::Precision);
	GM->HumanMove(Team, Move, bSprint, bModifier, bPrecision);
	const FVector RightStick = ReadRightStick(Dev);"""),
("""		else if (ActionPressed(Dev, EPKInput::KnockOn))
		{
			GM->HumanKnockOn(Team, Move);
		}
		else if""", """		else if (ActionPressed(Dev, EPKInput::KnockOn))
		{
			bEffortHeld[Team] = true;
			EffortTime[Team] = 0.f;
			EffortDir[Team] = FVector::ZeroVector; // the move direction when it is let go
			bCharging = false;                     // an effort touch cancels a shot being charged
		}
		else if (bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team] && !bEffortHeld[Team])
		{
			bEffortHeld[Team] = true; // RB + right stick: effort touch that way
			EffortTime[Team] = 0.f;
			EffortDir[Team] = RightStick.GetSafeNormal2D();
			bCharging = false;
		}
		else if"""),
("""		// Shot: hold to charge, release (or full charge) to shoot.""", """		// Effort touch: short when tapped, longer the longer it is held (full after 0.4 s).
		if (bEffortHeld[Team])
		{
			EffortTime[Team] += DeltaTime;
			const bool bStickHeld = !EffortDir[Team].IsNearlyZero() && RightStick.Size() > 0.3f;
			const bool bKeyHeld = EffortDir[Team].IsNearlyZero() && ActionDown(Dev, EPKInput::KnockOn);
			if ((!bStickHeld && !bKeyHeld) || EffortTime[Team] >= 0.4f)
			{
				const FVector Dir = EffortDir[Team].IsNearlyZero() ? Move : EffortDir[Team];
				GM->HumanKnockOn(Team, Dir, FMath::Clamp(EffortTime[Team] / 0.4f, 0.f, 1.f));
				bEffortHeld[Team] = false;
			}
		}

		// Shot: hold to charge, release (or full charge) to shoot."""),
("""	else
	{
		bCharging = false;
		if (ActionPressed(Dev, EPKInput::Tackle))""", """	else
	{
		bCharging = false;
		bEffortHeld[Team] = false;
		if (ActionPressed(Dev, EPKInput::Tackle))"""),
("""	// Team play style: 1-5 on the keyboard, D-pad left / right on a gamepad.""",
"""	bRightStickFlick[Team] = RightStick.Size() > 0.6f; // a flick counts once until the stick comes back

	// Team play style: 1-5 on the keyboard, D-pad left / right on a gamepad."""),
("""FVector APKPlayerController::ReadMove() const""", """FVector APKPlayerController::ReadRightStick(int32 Device) const
{
	const FPKUserSettings* S = UserSettings();
	const FPKInputRouter& R = FPKInputRouter::Get();
	FVector2D Stick(R.Axis(Device, EKeys::Gamepad_RightX), R.Axis(Device, EKeys::Gamepad_RightY));
	const float DeadZone = S ? S->DeadZone : StickDeadZone;
	if (Stick.Size() <= DeadZone)
	{
		return FVector::ZeroVector;
	}
	Stick = Stick.GetSafeNormal() * FMath::GetMappedRangeValueClamped(FVector2D(DeadZone, 1.f), FVector2D(0.f, 1.f), Stick.Size());
	if (S && S->bInvertX) Stick.X = -Stick.X;
	if (S && S->bInvertY) Stick.Y = -Stick.Y;
	const float Yaw = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Yaw : 90.f;
	const FRotationMatrix M(FRotator(0.f, Yaw, 0.f));
	const FVector Forward = PK::Flat(M.GetUnitAxis(EAxis::X)).GetSafeNormal();
	const FVector Right = PK::Flat(M.GetUnitAxis(EAxis::Y)).GetSafeNormal();
	return (Right * Stick.X + Forward * Stick.Y).GetClampedToMaxSize(1.f);
}

FVector APKPlayerController::ReadMove() const"""),
])
print("layer1 ok")
