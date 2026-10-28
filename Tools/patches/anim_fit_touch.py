# Actions timed to the ball: keeper saves start before the ball arrives (dive / catch contact on arrival), headers
# and thigh control play at the rate that meets the ball, and a waiting ball keeps flying instead of being frozen.
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
("""	bool PlayAction(EPKAction Action, EPKAction Then = EPKAction::None, float Rate = 1.f);""",
"""	bool PlayAction(EPKAction Action, EPKAction Then = EPKAction::None, float Rate = 1.f);
	// Play an action so its contact frame comes in SecondsToContact (faster swing, or joining the move part-way if
	// even MaxRate is too slow).
	bool PlayActionTimed(EPKAction Action, float SecondsToContact, float MaxRate = 2.2f);
	float GetActionEntry() const { return ActionEntry; } // clip time the action was entered at (for the blend-in)
	int32 SaveKickId = -1; // keeper: shot already being saved
	float TouchQueuedSpeed = 0.f; // ball speed when a header / thigh control was started"""),
("""	void Dive(float Direction); // keeper, sideways: +1 right / -1 left of facing""",
"""	void Dive(float Direction, float SecondsToContact = -1.f); // keeper, sideways: +1 right / -1 left of facing"""),
("""	EPKAction CurrentAction = EPKAction::None;""", """	EPKAction CurrentAction = EPKAction::None;
	float ActionEntry = 0.f;"""),
])

patch("PKFootballer.cpp", [
("""	CurrentAction = Action;
	NextAction = Then;
	ActionTime = Clip.Start;
	ActionRate = FMath::Clamp(Rate, 0.5f, 2.f);
	return true;
}""", """	CurrentAction = Action;
	NextAction = Then;
	ActionTime = Clip.Start;
	ActionEntry = Clip.Start;
	ActionRate = FMath::Clamp(Rate, 0.5f, 2.5f);
	return true;
}

bool APKFootballer::PlayActionTimed(EPKAction Action, float SecondsToContact, float MaxRate)
{
	if (!PlayAction(Action))
	{
		return false;
	}
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	const float Lead = FMath::Max(Clip.Contact - Clip.Start, 0.01f);
	const float T = FMath::Max(SecondsToContact, 0.02f);
	ActionRate = FMath::Clamp(Lead / T, 0.8f, FMath::Clamp(MaxRate, 0.8f, 2.5f));
	if (Lead / ActionRate > T)
	{
		// Even the fastest swing is late: join the move nearer the contact.
		ActionTime = FMath::Max(Clip.Start, Clip.Contact - T * ActionRate);
		ActionEntry = ActionTime;
	}
	return true;
}"""),
("""void APKFootballer::Dive(float Direction)
{
	if (CurrentAction == EPKAction::GKDiveLeft || CurrentAction == EPKAction::GKDiveRight)
	{
		return;
	}
	StopAction();
	GetCharacterMovement()->StopMovementImmediately();
	if (!PlayAction(Direction >= 0.f ? EPKAction::GKDiveRight : EPKAction::GKDiveLeft))""", """void APKFootballer::Dive(float Direction, float SecondsToContact)
{
	if (CurrentAction == EPKAction::GKDiveLeft || CurrentAction == EPKAction::GKDiveRight)
	{
		return;
	}
	StopAction();
	GetCharacterMovement()->StopMovementImmediately();
	const EPKAction DiveAction = Direction >= 0.f ? EPKAction::GKDiveRight : EPKAction::GKDiveLeft;
	if (!(SecondsToContact >= 0.f ? PlayActionTimed(DiveAction, SecondsToContact, 2.f) : PlayAction(DiveAction)))"""),
])

patch("PKAnimInstance.cpp", [
("""		const float In = Clip.BlendIn > 0.f ? FMath::Clamp((T - Clip.Start) / Clip.BlendIn, 0.f, 1.f) : 1.f;""",
"""		const float In = Clip.BlendIn > 0.f ? FMath::Clamp((T - Footballer->GetActionEntry()) / Clip.BlendIn, 0.f, 1.f) : 1.f;"""),
])

patch("PKGameMode.h", [
("""	FVector PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const;""",
"""	FVector PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const;
	// Seconds until the ball passes closest to Point (ballistic, up to 1.5 s), and how close it gets.
	float BallArrivalTime(const FVector& Point, float* OutMissBy = nullptr) const;
	// Keeper: start the save before the ball arrives, so the hands meet it at the clip's contact frame.
	void PrepareSave(APKFootballer* Keeper, float SecondsToLine, float CrossY);
	int32 TimedSaves = 0;
	double PendingSlowSum = 0.0; // ball speed kept while waiting for a head / knee (1 = untouched)
	int32 PendingSlowCount = 0;"""),
])

patch("PKGameMode.cpp", [
("""FVector APKGameMode::PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const
{""", """float APKGameMode::BallArrivalTime(const FVector& Point, float* OutMissBy) const
{
	FVector Pos = Ball->GetActorLocation();
	FVector Vel = Ball->Velocity;
	const bool bRolling = Pos.Z <= PK::BallRadius + 2.f && FMath::Abs(Vel.Z) < 50.f;
	float Best = FVector::Dist(Pos, Point);
	float BestT = 0.f;
	constexpr float Step = 1.f / 60.f;
	for (float T = Step; T < 1.5f; T += Step)
	{
		if (!bRolling)
		{
			Vel.Z -= PK::Gravity * Step;
		}
		Vel *= 1.f - PK::AirDrag * Step;
		Pos += Vel * Step;
		Pos.Z = FMath::Max(Pos.Z, PK::BallRadius);
		const float D = FVector::Dist(Pos, Point);
		if (D < Best)
		{
			Best = D;
			BestT = T;
		}
	}
	if (OutMissBy)
	{
		*OutMissBy = Best;
	}
	return BestT;
}

void APKGameMode::PrepareSave(APKFootballer* GK, float SecondsToLine, float CrossY)
{
	if (bOldKeeper || GK->IsInAction() || GK->SaveKickId == Ball->KickId)
	{
		return;
	}
	const float Z = Ball->GetActorLocation().Z + Ball->Velocity.Z * SecondsToLine - 0.5f * PK::Gravity * SecondsToLine * SecondsToLine;
	if (Z > 290.f || Z < -50.f)
	{
		return; // over the bar
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, GK->GetFacing());
	const float Lateral = FVector::DotProduct(FVector(0.f, CrossY - GK->GetActorLocation().Y, 0.f), Right);
	const float Speed = Ball->Velocity.Size();
	EPKAction Action = EPKAction::GKCatch;
	const bool bDive = FMath::Abs(Lateral) > 120.f;
	if (bDive)
	{
		Action = Lateral >= 0.f ? EPKAction::GKDiveRight : EPKAction::GKDiveLeft;
	}
	else if (Z < 45.f)
	{
		Action = Speed > 1200.f ? EPKAction::GKBlock : EPKAction::GKScoop;
	}
	else if (Z < 90.f)
	{
		Action = EPKAction::GKCatchLow;
	}
	else if (Z > 190.f)
	{
		Action = EPKAction::GKCatchHigh;
	}
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	if (!Clip.IsValid())
	{
		return;
	}
	// Start when the clip, at up to 1.6x, reaches its contact as the ball reaches him.
	const float Lead = Clip.Contact - Clip.Start;
	if (SecondsToLine > Lead / 0.9f)
	{
		return; // not yet
	}
	GK->SaveKickId = Ball->KickId;
	++TimedSaves;
	if (bDive)
	{
		GK->Dive(Lateral >= 0.f ? 1.f : -1.f, SecondsToLine);
	}
	else
	{
		GK->PlayActionTimed(Action, SecondsToLine, 1.8f);
	}
}

FVector APKGameMode::PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const
{"""),
# keeper: shot coming -> prepare the save
("""			const float Y = FMath::Clamp(BallLoc.Y + BallVel.Y * T + Curl, -PK::GoalHalfWidth - 80.f, PK::GoalHalfWidth + 80.f);""",
"""			const float Y = FMath::Clamp(BallLoc.Y + BallVel.Y * T + Curl, -PK::GoalHalfWidth - 80.f, PK::GoalHalfWidth + 80.f);
			PrepareSave(P, T, BallLoc.Y + BallVel.Y * T + Curl);"""),
# late dives / catches join the move near its contact
("""				P->Dive(Local.Y >= 0.f ? 1.f : -1.f);
				EventShot(TEXT("Dive"), P);""", """				P->Dive(Local.Y >= 0.f ? 1.f : -1.f, 0.06f); // late: join the dive near the stretch
				EventShot(TEXT("Dive"), P);"""),
("""				P->PlayAction(Catch);
				EventShot(TEXT("Catch"), P);""", """				P->PlayActionTimed(Catch, 0.04f); // the ball is in his hands now: the clip joins at the catch
				EventShot(TEXT("Catch"), P);"""),
# headers / thigh control: timed to the ball
("""void APKGameMode::QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate)
{
	if (!P->PlayAction(Action, EPKAction::None, Rate))""", """void APKGameMode::QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate)
{
	bool bPlaying = false;
	if (Action == EPKAction::Header || Action == EPKAction::ThighControl)
	{
		// Meet the ball when it gets there: the head / knee reaches the contact point on the ball's own time.
		const float Arrival = BallArrivalTime(P->GetActionContactPoint(Action, P->GetActorRotation().Yaw));
		bPlaying = P->PlayActionTimed(Action, Arrival, 2.4f);
	}
	else
	{
		bPlaying = P->PlayAction(Action, EPKAction::None, Rate);
	}
	P->TouchQueuedSpeed = Ball->Velocity.Size();
	if (!bPlaying)"""),
("""		if (Remaining > 0.f)
		{
			if (FVector::Dist(BallLoc, Target) < 220.f)
			{
				Ball->SetActorLocation(FMath::Lerp(BallLoc, Target, Remaining > Dt ? FMath::Clamp(Dt / Remaining, 0.f, 1.f) : 1.f));
				Ball->Velocity = FVector::ZeroVector;
			}
			continue;
		}""", """		if (Remaining > 0.f)
		{
			if (FVector::Dist(BallLoc, Target) < 220.f)
			{
				// The ball keeps flying; it is only nudged onto the head / knee so they meet at the contact frame.
				const FVector Need = (Target - BallLoc) / FMath::Max(Remaining, Dt);
				Ball->Velocity = FMath::Lerp(Ball->Velocity, Need, FMath::Clamp(Dt * 12.f, 0.f, 1.f));
			}
			continue;
		}"""),
("""		Ball->SetActorLocation(Target);
		ContactSnap(P, *UEnum::GetValueAsString(Action).RightChop(11));""", """		if (P->TouchQueuedSpeed > 300.f && (Action == EPKAction::Header || Action == EPKAction::ThighControl))
		{
			PendingSlowSum += FMath::Min(Ball->Velocity.Size() / P->TouchQueuedSpeed, 2.f); // speed kept up to contact
			++PendingSlowCount;
		}
		Ball->SetActorLocation(Target);
		ContactSnap(P, *UEnum::GetValueAsString(Action).RightChop(11));"""),
# report
("""		KickFitCount ? KickFitSum / KickFitCount : 0.0, KickFitBad, KickFitCount);""", """		KickFitCount ? KickFitSum / KickFitCount : 0.0, KickFitBad, KickFitCount, TimedSaves, PendingSlowCount ? PendingSlowSum / PendingSlowCount : 1.0);"""),
("""| kick fit avg %.2f misfit %d/%d\"),""", """| kick fit avg %.2f misfit %d/%d | timed saves %d | ball kept %.2f of its speed waiting for a header\"),"""),
])
print("touch fit ok")
