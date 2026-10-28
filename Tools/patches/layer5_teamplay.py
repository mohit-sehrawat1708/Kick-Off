# Layer 5: send a run (Runs tapped), one-two (Runs held + pass; the return goes to the runner), AI one-twos,
# game plans: mentality (-2 ultra defensive .. +2 ultra attacking) switched mid-match, AI changes plan late on.
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
("""	void HumanTeamPress(int32 Team, bool bPress) { bTeamPress[FMath::Clamp(Team, 0, 1)] = bPress; }""",
"""	void HumanTeamPress(int32 Team, bool bPress) { bTeamPress[FMath::Clamp(Team, 0, 1)] = bPress; }
	// Runs: send the best-placed team-mate on a run in behind; a one-two pass makes the passer run for the return.
	void HumanSendRun(int32 Team);
	APKFootballer* SendRun(APKFootballer* Carrier, APKFootballer* Runner = nullptr);
	FVector RunSpot(const APKFootballer* Runner) const;
	TWeakObjectPtr<APKFootballer> OneTwoPasser[2];
	TWeakObjectPtr<APKFootballer> OneTwoReceiver[2];
	float OneTwoTimer[2] = { 0.f, 0.f };
	int32 RunsSent = 0;
	int32 OneTwos = 0;
	int32 OneTwoReturns = 0;
	// Game plan mentality: -2 ultra defensive .. +2 ultra attacking (pushes the block, the line and the press).
	int32 Mentality[2] = { 0, 0 };
	float MatchTotal = 360.f;
	float PlanTimer = 0.f;
	void SetMentality(int32 Team, int32 Value, bool bAnnounce = true);
	static const TCHAR* MentalityName(int32 Value);
	int32 GetMentality(int32 Team) const { return Mentality[FMath::Clamp(Team, 0, 1)]; }
	void UpdateGamePlans(float Dt);"""),
("""	void HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve = false);""",
"""	void HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve = false, bool bOneTwo = false);"""),
])

patch("PKGameMode.cpp", [
# match length for game plans
("""	TimeLeft = bAutoTest ? 150.f : (SettingsGI ? SettingsGI->Settings.MatchMinutes * 60.f : PK::MatchSeconds);""",
"""	TimeLeft = bAutoTest ? 150.f : (SettingsGI ? SettingsGI->Settings.MatchMinutes * 60.f : PK::MatchSeconds);
	MatchTotal = TimeLeft;
	Mentality[0] = Mentality[1] = 0;"""),
# HumanPass one-two
("""void APKGameMode::HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}""", """void APKGameMode::HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve, bool bOneTwo)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}
	// The return of a one-two: pass / through within a few seconds goes to the runner.
	const int32 T2 = FMath::Clamp(Team, 0, 1);
	if (OneTwoTimer[T2] > 0.f && OneTwoReceiver[T2].Get() == Human && OneTwoPasser[T2].IsValid() && !bOneTwo)
	{
		RequestPass(Human, OneTwoPasser[T2].Get(), EPKKickType::Through, bSwerve);
		OneTwoTimer[T2] = 0.f;
		++OneTwoReturns;
		return;
	}"""),
("""	if (Best)
	{
		RequestPass(Human, Best, Type, bSwerve);
	}""", """	if (Best)
	{
		RequestPass(Human, Best, Type, bSwerve);
		if (bOneTwo)
		{
			// One-two: play it in and go. The passer runs into space for the return.
			OneTwoPasser[T2] = Human;
			OneTwoReceiver[T2] = Best;
			OneTwoTimer[T2] = 4.f;
			SendRun(Best, Human);
			++OneTwos;
		}
	}"""),
# Send run implementation (after ChooseSwerve)
("""void APKGameMode::HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve)
{""", """FVector APKGameMode::RunSpot(const APKFootballer* Runner) const
{
	// Into the space in behind: up to 15 m ahead, at most 6 m beyond the second-last defender (timed run).
	const int32 Team = Runner->Team;
	const float Dir = PK::AttackDir(Team);
	float SecondLast = -1.f, Last = -1.f; // team frame x of the two deepest opponents
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == Team)
		{
			continue;
		}
		const float Fx = WorldToFx(Team, O->GetActorLocation().X);
		if (Fx > Last)
		{
			SecondLast = Last;
			Last = Fx;
		}
		else if (Fx > SecondLast)
		{
			SecondLast = Fx;
		}
	}
	const float MyFx = WorldToFx(Team, Runner->GetActorLocation().X);
	float RunFx = FMath::Min(MyFx + 1500.f / PK::HalfLength, SecondLast + 600.f / PK::HalfLength);
	RunFx = FMath::Clamp(FMath::Max(RunFx, MyFx + 300.f / PK::HalfLength), -0.9f, 0.9f);
	// Bend the run toward the middle a little (between the centre-backs and full-backs).
	const float Y = Runner->GetActorLocation().Y * 0.8f;
	return FVector(Dir * RunFx * PK::HalfLength, Y, 0.f);
}

APKFootballer* APKGameMode::SendRun(APKFootballer* Carrier, APKFootballer* Runner)
{
	if (!Carrier)
	{
		return nullptr;
	}
	if (!Runner)
	{
		// The best-placed team-mate to go: forwards first, then whoever is furthest forward and not marked tight.
		float Best = -TNumericLimits<float>::Max();
		for (APKFootballer* T : Footballers)
		{
			if (T == Carrier || T->Team != Carrier->Team || T->IsKeeper() || T->IsStunned())
			{
				continue;
			}
			float Near = 0.f;
			NearestOpponent(T, Near);
			const float Score = WorldToFx(T->Team, T->GetActorLocation().X) * 2.f + (T->Role == EPKRole::Forward ? 0.6f : (T->Role == EPKRole::Midfielder ? 0.2f : -0.5f))
				+ FMath::Min(Near, 600.f) / 1500.f - FVector::Dist(T->GetFlatLocation(), Carrier->GetFlatLocation()) / 8000.f;
			if (Score > Best)
			{
				Best = Score;
				Runner = T;
			}
		}
	}
	if (!Runner)
	{
		return nullptr;
	}
	Runner->AITarget = RunSpot(Runner);
	Runner->AITargetTimer = 2.6f;
	Runner->RunTimer = 2.6f;
	++RunsSent;
	return Runner;
}

void APKGameMode::HumanSendRun(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human && HumanHasBall(Team))
	{
		if (APKFootballer* Runner = SendRun(Human))
		{
			ShotFeedback = FString::Printf(TEXT("RUN: #%d"), Runner->ShirtNumber);
			ShotFeedbackColor = FLinearColor(0.5f, 0.85f, 1.f);
			ShotFeedbackTimer = 1.f;
		}
	}
}

void APKGameMode::SetMentality(int32 Team, int32 Value, bool bAnnounce)
{
	const int32 T = FMath::Clamp(Team, 0, 1);
	Value = FMath::Clamp(Value, -2, 2);
	if (Mentality[T] == Value)
	{
		return;
	}
	Mentality[T] = Value;
	if (bAnnounce)
	{
		ShowMessage(FString::Printf(TEXT("%s: %s"), PK::TeamName(T), MentalityName(Value)), 1.5f);
	}
	UE_LOG(LogPitchKings, Log, TEXT("Game plan: %s now %s"), PK::TeamName(T), MentalityName(Value));
}

const TCHAR* APKGameMode::MentalityName(int32 Value)
{
	switch (Value)
	{
	case -2: return TEXT("Ultra defensive");
	case -1: return TEXT("Defensive");
	case 1:  return TEXT("Attacking");
	case 2:  return TEXT("Ultra attacking");
	default: return TEXT("Balanced");
	}
}

void APKGameMode::UpdateGamePlans(float Dt)
{
	for (int32 T = 0; T < 2; ++T)
	{
		OneTwoTimer[T] = FMath::Max(0.f, OneTwoTimer[T] - Dt);
	}
	// The computer changes its plan late on: chasing the game it goes for it, protecting a lead it sits deeper.
	PlanTimer -= Dt;
	if (PlanTimer > 0.f || Phase != EPKPhase::Playing)
	{
		return;
	}
	PlanTimer = 2.f;
	const bool bLate = TimeLeft < MatchTotal * 0.3f;
	for (int32 T = 0; T < 2; ++T)
	{
		if (IsHumanTeam(T) && !bSpectate)
		{
			continue;
		}
		const int32 Diff = Score[T] - Score[1 - T];
		const int32 Want = !bLate ? 0 : (Diff < 0 ? (Diff < -1 ? 2 : 1) : (Diff > 0 ? -1 : 0));
		SetMentality(T, Want, true);
	}
}

void APKGameMode::HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve)
{"""),
# AI: return a one-two first time; AI plays one-twos sometimes
("""	P->DecisionTimer -= Dt;
	if (P->DecisionTimer <= 0.f && !P->IsKicking() && CanReachBall(P, KickReach))
	{""", """	// One-two return: first time, into the runner's path.
	const int32 TeamIdx = P->Team;
	if (OneTwoTimer[TeamIdx] > 0.f && OneTwoReceiver[TeamIdx].Get() == P && OneTwoPasser[TeamIdx].IsValid() && !P->IsKicking() && CanReachBall(P, KickReach))
	{
		RequestPass(P, OneTwoPasser[TeamIdx].Get(), EPKKickType::Through);
		OneTwoTimer[TeamIdx] = 0.f;
		++OneTwoReturns;
		return;
	}

	P->DecisionTimer -= Dt;
	if (P->DecisionTimer <= 0.f && !P->IsKicking() && CanReachBall(P, KickReach))
	{"""),
("""			else if ((bAhead && FMath::FRand() < S.ThroughChance) || Target->RunTimer > 0.f)
			{
				Type = EPKKickType::Through;
			}
			RequestPass(P, Target, Type);
			return;""", """			else if ((bAhead && FMath::FRand() < S.ThroughChance) || Target->RunTimer > 0.f)
			{
				Type = EPKKickType::Through;
			}
			RequestPass(P, Target, Type);
			// Give-and-go: a short pass under pressure in the opponents' half, then run for the return.
			if (Type == EPKKickType::Ground && bPressured && D < 1300.f && P->Role != EPKRole::Defender && WorldToFx(P->Team, Loc.X) > 0.05f && FMath::FRand() < 0.3f)
			{
				OneTwoPasser[P->Team] = P;
				OneTwoReceiver[P->Team] = Target;
				OneTwoTimer[P->Team] = 3.f;
				SendRun(Target, P);
				++OneTwos;
			}
			// Space in behind and a forward to use it: send him (the pass can follow later).
			else if (!bPressured && FMath::FRand() < 0.15f + 0.1f * Mentality[P->Team])
			{
				SendRun(P);
			}
			return;"""),
# mentality effects: block push, line, press
("""	float Fx = P->Home.X + Shift * BallFx + Push + Style(P->Team).LinePush;""",
"""	float Fx = P->Home.X + Shift * BallFx + Push + Style(P->Team).LinePush + 0.045f * Mentality[P->Team];"""),
("""		float Line = HomeSum / Count + 0.42f * BallFx - 0.06f + Style(Team).LinePush;""",
"""		float Line = HomeSum / Count + 0.42f * BallFx - 0.06f + Style(Team).LinePush + 0.04f * Mentality[Team];"""),
("""	if (Dist > Style(P->Team).PressDistance)""", """	if (Dist > Style(P->Team).PressDistance * (1.f + 0.15f * Mentality[P->Team]))"""),
# tick game plans with the AI
("""	// Per team: the closest outfield player engages the ball, the second closest covers.
	UpdateDefensiveLine();""", """	// Per team: the closest outfield player engages the ball, the second closest covers.
	UpdateDefensiveLine();
	UpdateGamePlans(Dt);"""),
])

# Controller: Runs tap = send run, Runs held + pass = one-two; mentality on 9 / 0 and LT + D-pad left / right.
patch("PKPlayerController.h", [
("""	bool bPassCharging[2] = { false, false };""", """	bool bPassCharging[2] = { false, false };
	float RunsHeld[2] = { -1.f, -1.f }; // seconds Runs has been held with the ball (-1 = not held)
	bool bRunsUsed[2] = { false, false }; // a pass / shot used it as a modifier"""),
])

patch("PKPlayerController.cpp", [
("""			else
			{
				GM->HumanPass(Team, Move, PassTypes[i], bModifier);
			}""", """			else
			{
				const bool bOneTwo = ActionDown(Dev, EPKInput::Runs);
				bRunsUsed[Team] |= bOneTwo;
				GM->HumanPass(Team, Move, PassTypes[i], bModifier, bOneTwo);
			}"""),
("""				const bool bRuns = ActionDown(Dev, EPKInput::Runs);""", """				const bool bRuns = ActionDown(Dev, EPKInput::Runs);
				bRunsUsed[Team] |= bRuns;"""),
("""		// Effort touch: short when tapped, longer the longer it is held (full after 0.4 s).""", """		// Runs tapped on its own (not used with a pass or shot): send a team-mate on a run.
		if (ActionDown(Dev, EPKInput::Runs))
		{
			RunsHeld[Team] = RunsHeld[Team] < 0.f ? 0.f : RunsHeld[Team] + DeltaTime;
		}
		else
		{
			if (RunsHeld[Team] >= 0.f && RunsHeld[Team] < 0.35f && !bRunsUsed[Team])
			{
				GM->HumanSendRun(Team);
			}
			RunsHeld[Team] = -1.f;
			bRunsUsed[Team] = false;
		}

		// Effort touch: short when tapped, longer the longer it is held (full after 0.4 s)."""),
("""	if (DevPressed(Dev, { EKeys::Gamepad_DPad_Right }))
	{
		GM->CycleTeamStyle(Team, 1);
	}
	if (DevPressed(Dev, { EKeys::Gamepad_DPad_Left }))
	{
		GM->CycleTeamStyle(Team, -1);
	}""", """	// Game plan mentality: 9 / 0 on the keyboard, LT + D-pad left / right on a gamepad.
	const bool bPadPlan = Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	if (Router.WasPressed(Dev, EKeys::Nine, InputSeq) || (bPadPlan && DevPressed(Dev, { EKeys::Gamepad_DPad_Left })))
	{
		GM->SetMentality(Team, GM->GetMentality(Team) - 1);
	}
	else if (Router.WasPressed(Dev, EKeys::Zero, InputSeq) || (bPadPlan && DevPressed(Dev, { EKeys::Gamepad_DPad_Right })))
	{
		GM->SetMentality(Team, GM->GetMentality(Team) + 1);
	}
	else if (DevPressed(Dev, { EKeys::Gamepad_DPad_Right }))
	{
		GM->CycleTeamStyle(Team, 1);
	}
	else if (DevPressed(Dev, { EKeys::Gamepad_DPad_Left }))
	{
		GM->CycleTeamStyle(Team, -1);
	}"""),
])

# HUD: show the mentality next to the style
patch("PKHUD.cpp", [
("""	DrawText(FString::Printf(TEXT("%s  vs  %s"), *APKGameMode::StyleName(GM->GetTeamStyle(0)), *APKGameMode::StyleName(GM->GetTeamStyle(1))), FLinearColor(0.85f, 0.85f, 0.85f), 30.f, 69.f, Small, 1.f);""",
"""	auto PlanText = [GM](int32 T)
	{
		const int32 M = GM->GetMentality(T);
		return M == 0 ? APKGameMode::StyleName(GM->GetTeamStyle(T)) : FString::Printf(TEXT("%s (%s)"), *APKGameMode::StyleName(GM->GetTeamStyle(T)), APKGameMode::MentalityName(M));
	};
	DrawText(FString::Printf(TEXT("%s  vs  %s"), *PlanText(0), *PlanText(1)), FLinearColor(0.85f, 0.85f, 0.85f), 30.f, 69.f, Small, 1.f);"""),
])
print("layer5 ok")
