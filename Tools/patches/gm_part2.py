import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings"))
p = "PKGameMode.cpp"
s = open(p, encoding="utf-8-sig").read().replace("\r\n", "\n")


def rep(a, b):
    global s
    if a not in s:
        raise SystemExit("NOT FOUND: " + a[:100])
    s = s.replace(a, b, 1)


# ---- interception reach (Anticipate) and volleys at dropping balls near goal
rep("\t\tfloat Reach = bKeeperHands ? 120.f : (bSliding ? 145.f : (bLunging ? 125.f : 75.f));",
    "\t\tfloat Reach = bKeeperHands ? 120.f : (bSliding ? 145.f : (bLunging ? 125.f : 75.f));\n"
    "\t\tif (!bKeeperHands && P->HasTrait(PKTrait::Anticipate))\n\t\t{\n\t\t\tReach += 20.f;\n\t\t}")
rep("\t\t// Headers: a dropping ball at head height is nodded on.",
"""\t\t// Volley: an attacker meets a dropping ball at waist/chest height within shooting range.
\t\tif (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f && Ball->Velocity.Z < 50.f
\t\t\t&& P->StealCooldown <= 0.f && FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < 2400.f && Ball->Velocity.Size() > 400.f)
\t\t{
\t\t\tP->StealCooldown = 0.8f;
\t\t\tif (FMath::FRand() < 0.5f + 0.3f * P->Attr.Shooting)
\t\t\t{
\t\t\t\tBall->SetController(P);
\t\t\t\tRequestShot(P, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.7f, 1.f), true);
\t\t\t\tEventShot(TEXT("Volley"), P);
\t\t\t\tcontinue;
\t\t\t}
\t\t}

\t\t// Headers: a dropping ball at head height is nodded on.""")

# ---- steal chances use the ratings
rep("\t\t\t\tChance -= Controller->Stance == EPKStance::CloseControl ? 0.06f : 0.f;",
    "\t\t\t\tChance -= Controller->Stance == EPKStance::CloseControl ? 0.06f : 0.f;\n"
    "\t\t\t\tChance -= Controller->Stance == EPKStance::Shield ? 0.1f * Controller->Attr.Physical : 0.f;\n"
    "\t\t\t\tChance *= FMath::Lerp(0.7f, 1.3f, P->Attr.Defending) * (P->HasTrait(PKTrait::PressProven) ? 1.15f : 1.f);\n"
    "\t\t\t\tChance *= FMath::Lerp(1.3f, 0.75f, Controller->Attr.Control);")
rep("\t\t\tconst float Chance = bKeeperHands ? FMath::Clamp(1.3f - Speed / 4000.f, 0.25f, 0.92f) * (D > 130.f ? 0.6f : 1.f) : 0.4f;",
    "\t\t\tconst float Chance = bKeeperHands ? FMath::Clamp(1.3f - Speed / 4000.f, 0.25f, 0.92f) * (D > 130.f ? 0.6f : 1.f) * FMath::Lerp(0.85f, 1.1f, P->Attr.Defending)\n"
    "\t\t\t\t: 0.25f + 0.25f * P->Attr.Defending + (P->HasTrait(PKTrait::Anticipate) ? 0.1f : 0.f);")

# ---- first touch: rating-based heavy touches, directional control for the human
rep("""	// First touch: kill most of the pace and bring it to the feet.
	const FVector FirstTouch = PK::Flat(P->GetVelocity()) + P->GetFacing() * 60.f;
	Ball->SetController(P);""",
"""	// First touch: kill the pace and bring it to the feet. Poor control can mean a heavy touch;
	// the human can steer the touch with the stick, or cushion it dead with the modifier.
	const float ArrivalSpeed = PK::Flat(Ball->Velocity).Size();
	FVector TouchDir = P->GetFacing();
	if (P->bHuman && !P->MoveInput.IsNearlyZero())
	{
		TouchDir = P->MoveInput.GetSafeNormal2D();
	}
	FVector FirstTouch = PK::Flat(P->GetVelocity()) + TouchDir * 60.f;
	if (P->bHuman && (P->Stance == EPKStance::CloseControl || P->Stance == EPKStance::Shield))
	{
		FirstTouch = PK::Flat(P->GetVelocity()) * 0.4f; // cushioned dead
	}
	else if (P->bHuman && !P->MoveInput.IsNearlyZero())
	{
		FirstTouch = TouchDir * (PK::Flat(P->GetVelocity()).Size() + 150.f + 120.f * P->Attr.Control); // directional touch into space
	}
	const float HeavyChance = FMath::Clamp((1.f - P->Attr.Control) * 0.5f * ArrivalSpeed / 1800.f, 0.f, 0.45f) * (P->HasTrait(PKTrait::Technical) ? 0.5f : 1.f);
	if (FMath::FRand() < HeavyChance)
	{
		FirstTouch += TouchDir.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector) * FMath::FRandRange(250.f, 450.f);
		EventShot(TEXT("HeavyTouch"), P);
	}
	Ball->SetController(P);""")

# ---- human skill moves + styles + helpers (append before the helpers section)
rep("""// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------
""",
r"""void APKGameMode::HumanDragBack()
{
	// Drag-back: roll the ball back under the sole and turn away from pressure.
	if (!Human || Ball->GetController() != Human || !CanReachBall(Human, 95.f) || Human->IsKicking() || Human->IsInAction())
	{
		return;
	}
	const FVector Back = -Human->GetFacing();
	Ball->Touch(Human, Back * 160.f);
	Human->SetActorRotation(FRotator(0.f, Back.Rotation().Yaw, 0.f));
	Human->TouchTimer = 0.45f;
	Human->ProtectedTimer = FMath::Max(Human->ProtectedTimer, 0.35f);
	EventShot(TEXT("DragBack"), Human);
}

void APKGameMode::HumanFeint(const FVector& Aim)
{
	// Body feint: a sharp side-step with the ball that can wrong-foot nearby defenders.
	if (!Human || Ball->GetController() != Human || !CanReachBall(Human, 110.f) || Human->IsKicking() || Human->IsInAction())
	{
		return;
	}
	const FVector Right = PK::Flat(Human->GetActorRightVector()).GetSafeNormal();
	float SideSign = FVector::DotProduct(Aim, Right);
	SideSign = FMath::Abs(SideSign) > 0.2f ? FMath::Sign(SideSign) : (FMath::FRand() < 0.5f ? -1.f : 1.f);
	Human->GetCharacterMovement()->Velocity += Right * SideSign * 420.f;
	Ball->Touch(Human, PK::Flat(Human->GetVelocity()) + Right * SideSign * 380.f);
	Human->TouchTimer = 0.35f;
	for (APKFootballer* O : Footballers)
	{
		if (O->Team != Human->Team && FVector::Dist(O->GetFlatLocation(), Human->GetFlatLocation()) < 320.f
			&& FMath::FRand() < 0.35f + 0.4f * Human->Attr.Control - 0.2f * O->Attr.Defending)
		{
			O->Stun(0.4f); // bought the feint
			O->StealCooldown = 0.8f;
		}
	}
	EventShot(TEXT("Feint"), Human);
}

FString APKGameMode::StyleName(EPKTeamStyle InStyle)
{
	switch (InStyle)
	{
	case EPKTeamStyle::Possession: return TEXT("Possession");
	case EPKTeamStyle::Counter:    return TEXT("Counter Attack");
	case EPKTeamStyle::LongBall:   return TEXT("Long Ball");
	case EPKTeamStyle::HighPress:  return TEXT("High Press");
	default:                       return TEXT("Balanced");
	}
}

const FPKStyleParams& APKGameMode::Style(int32 Team) const
{
	return StyleTable[FMath::Clamp((int32)TeamStyle[FMath::Clamp(Team, 0, 1)], 0, (int32)EPKTeamStyle::Count - 1)];
}

void APKGameMode::SetTeamStyle(int32 Team, EPKTeamStyle InStyle)
{
	TeamStyle[FMath::Clamp(Team, 0, 1)] = InStyle;
	ShowMessage(FString::Printf(TEXT("%s: %s"), PK::TeamName(Team), *StyleName(InStyle)), 1.5f);
}

void APKGameMode::CycleTeamStyle(int32 Team, int32 Step)
{
	const int32 Count = (int32)EPKTeamStyle::Count;
	SetTeamStyle(Team, (EPKTeamStyle)(((int32)TeamStyle[Team] + Step + Count) % Count));
}

void APKGameMode::AfterKickStart(APKFootballer* P)
{
	// Set pieces: step the taker up so the striking foot lands on the dead ball.
	if (Ball->IsPlaced() && Ball->GetSetPieceTaker() == P)
	{
		const FVector Offset = P->GetKickFootPoint() - P->GetFlatLocation();
		const FVector BallFlat = PK::Flat(Ball->GetActorLocation());
		P->SetActorLocation(FVector(BallFlat.X - Offset.X, BallFlat.Y - Offset.Y, P->GetActorLocation().Z));
	}
}

void APKGameMode::AlignBallForKicks(float Dt)
{
	// During a wind-up the ball is eased onto the striking foot, so it leaves from the foot
	// that kicks it and in the direction the body faces.
	APKFootballer* P = Ball->GetController();
	if (!P || !P->IsKicking() || !Ball->IsFree() || Ball->GetActorLocation().Z > 40.f)
	{
		return;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Foot = P->GetKickFootPoint();
	if (FVector::Dist(PK::Flat(BallLoc), Foot) > 150.f)
	{
		return;
	}
	const FVector Eased = FMath::VInterpTo(PK::Flat(BallLoc), Foot, Dt, 14.f);
	Ball->SetActorLocation(FVector(Eased.X, Eased.Y, BallLoc.Z));
	Ball->Velocity.X *= FMath::Max(0.f, 1.f - 8.f * Dt);
	Ball->Velocity.Y *= FMath::Max(0.f, 1.f - 8.f * Dt);
}

void APKGameMode::UpdatePredictedTaker(float Dt)
{
	// Arrow over the player most likely to have the ball next: the owner, or whoever reaches the
	// ball's path first (simple ballistic / rolling projection against each player's running speed).
	APKFootballer* Best = Ball->GetOwnerPlayer();
	float BestTime = 0.f;
	if (!Best && Ball->IsFree())
	{
		const FVector P0 = Ball->GetActorLocation();
		const FVector V0 = Ball->Velocity;
		const float GroundSpeed = PK::Flat(V0).Size();
		BestTime = TNumericLimits<float>::Max();
		for (APKFootballer* P : Footballers)
		{
			if (P->IsStunned() || (P == Ball->LastKicker && Ball->KickerIgnoreTimer > 0.f))
			{
				continue;
			}
			const float RunSpeed = FMath::Max(PK::Flat(P->GetVelocity()).Size(), PK::JogSpeed) * P->PaceScale;
			const float Reach = P->IsKeeper() ? 120.f : 75.f;
			const float MaxZ = P->IsKeeper() && InOwnBox(P, P0) ? 260.f : 110.f;
			for (int32 k = 0; k <= 40; ++k)
			{
				const float T = k * 0.06f;
				FVector B;
				if (P0.Z > PK::BallRadius + 5.f || V0.Z > 50.f)
				{
					B = P0 + V0 * T + FVector(0.f, 0.f, -0.5f * PK::Gravity * T * T);
					B.Z = FMath::Max(B.Z, PK::BallRadius);
				}
				else
				{
					const float Stop = GroundSpeed / PK::RollDecel;
					const float Tc = FMath::Min(T, Stop);
					const float Dist = GroundSpeed * Tc - 0.5f * PK::RollDecel * Tc * Tc;
					B = P0 + (GroundSpeed > 1.f ? PK::Flat(V0) / GroundSpeed : FVector::ZeroVector) * Dist;
				}
				if (B.Z > MaxZ)
				{
					continue;
				}
				const float Need = FVector::Dist2D(P->GetActorLocation(), B) - Reach;
				if (Need <= RunSpeed * T)
				{
					if (T < BestTime)
					{
						BestTime = T;
						Best = P;
					}
					break;
				}
			}
		}
		// Stick with the current pick unless the new one is clearly earlier (no flicker).
		if (PredictedTaker && Best != PredictedTaker && BestTime > PredictedTakerTime - 0.15f)
		{
			Best = PredictedTaker;
		}
	}
	PredictedTaker = Best;
	PredictedTakerTime = BestTime;

	const bool bShow = Phase == EPKPhase::Playing || Phase == EPKPhase::Kickoff || Phase == EPKPhase::Stoppage;
	for (APKFootballer* P : Footballers)
	{
		P->SetBallArrow(bShow && P == PredictedTaker);
	}
}

void APKGameMode::UpdateMarking()
{
	// Each defending team marks the most dangerous opponents goal-side (nearest to its goal first);
	// forwards stay up for the counter unless the team presses high.
	MarkTargets.Reset();
	APKFootballer* BallOwner = Ball->GetOwnerPlayer();
	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (BallOwner && BallOwner->Team == Team)
		{
			continue; // in possession: no marking
		}
		TArray<APKFootballer*> Threats;
		TArray<APKFootballer*> Markers;
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team && !P->IsKeeper() && P != BallOwner)
			{
				Threats.Add(P);
			}
			else if (P->Team == Team && !P->IsKeeper() && !P->bHuman
				&& (P->Role != EPKRole::Forward || TeamStyle[Team] == EPKTeamStyle::HighPress))
			{
				Markers.Add(P);
			}
		}
		const FVector Goal = OwnGoal(Team);
		Threats.Sort([&Goal](const APKFootballer& A, const APKFootballer& B)
		{
			return FVector::Dist2D(A.GetActorLocation(), Goal) < FVector::Dist2D(B.GetActorLocation(), Goal);
		});
		for (APKFootballer* Opp : Threats)
		{
			if (FVector::Dist2D(Opp->GetActorLocation(), Goal) > 0.75f * 2.f * PK::HalfLength)
			{
				continue; // too far from our goal to matter
			}
			APKFootballer* Pick = nullptr;
			float PickDist = 1800.f;
			for (APKFootballer* M : Markers)
			{
				if (MarkTargets.Contains(M))
				{
					continue;
				}
				const float D = FVector::Dist2D(SupportSpot(M, false), Opp->GetActorLocation());
				if (D < PickDist)
				{
					PickDist = D;
					Pick = M;
				}
			}
			if (Pick)
			{
				MarkTargets.Add(Pick, Opp);
			}
		}
	}
}

FVector APKGameMode::AttackSupportSpot(APKFootballer* P, APKFootballer* Carrier)
{
	const FVector Base = SupportSpot(P, true);
	P->RunTimer = FMath::Max(0.f, P->RunTimer - 0.016f);
	P->AITargetTimer -= GetWorld()->GetDeltaSeconds();
	if (P->AITargetTimer > 0.f && !P->AITarget.IsNearlyZero())
	{
		return P->AITarget;
	}
	P->AITargetTimer = FMath::FRandRange(0.8f, 1.4f);
	P->RunTimer = 0.f;
	P->AITarget = Base;
	if (!Carrier)
	{
		return Base;
	}

	const FVector CarrierLoc = Carrier->GetFlatLocation();
	const float Dir = PK::AttackDir(P->Team);

	// Forwards: time a run in behind the last defender when the carrier can see it.
	if (P->Role == EPKRole::Forward && FMath::FRand() < (TeamStyle[P->Team] == EPKTeamStyle::Counter ? 0.7f : 0.4f))
	{
		float LastLine = -1.f;
		for (APKFootballer* O : Footballers)
		{
			if (O->Team != P->Team && !O->IsKeeper())
			{
				LastLine = FMath::Max(LastLine, WorldToFx(P->Team, O->GetActorLocation().X));
			}
		}
		const float RunFx = FMath::Clamp(LastLine + 0.08f, WorldToFx(P->Team, Base.X), 0.86f);
		const float CarrierFx = WorldToFx(P->Team, CarrierLoc.X);
		if (RunFx - CarrierFx > 0.1f && RunFx - CarrierFx < 0.6f)
		{
			P->AITarget = FVector(Dir * RunFx * PK::HalfLength, Base.Y, 0.f);
			P->RunTimer = P->AITargetTimer;
			return P->AITarget;
		}
	}

	// Midfielders / forwards near the ball: offer an open angle (passing triangle) around the carrier.
	if (P->Role != EPKRole::Defender && FVector::Dist2D(Base, CarrierLoc) < 3000.f && P != Carrier)
	{
		const FVector Forward(Dir, 0.f, 0.f);
		const float Angles[] = { -70.f, -35.f, 35.f, 70.f };
		float BestScore = -TNumericLimits<float>::Max();
		FVector BestSpot = Base;
		for (const float A : Angles)
		{
			FVector Spot = CarrierLoc + Forward.RotateAngleAxis(A, FVector::UpVector) * FMath::FRandRange(1100.f, 1700.f);
			Spot.X = FMath::Clamp(Spot.X, -PK::HalfLength + 300.f, PK::HalfLength - 300.f);
			Spot.Y = FMath::Clamp(Spot.Y, -PK::HalfWidth + 200.f, PK::HalfWidth - 200.f);
			float Lane = TNumericLimits<float>::Max();
			float Space = TNumericLimits<float>::Max();
			for (APKFootballer* O : Footballers)
			{
				if (O->Team == P->Team)
				{
					if (O != P && O != Carrier && FVector::Dist2D(O->AITarget, Spot) < 700.f)
					{
						Space = FMath::Min(Space, 200.f); // a teammate already offers here
					}
					continue;
				}
				Lane = FMath::Min(Lane, FMath::PointDistToSegment(O->GetFlatLocation(), CarrierLoc, Spot));
				Space = FMath::Min(Space, FVector::Dist2D(O->GetActorLocation(), Spot));
			}
			const float Score = FMath::Min(Lane, 600.f) / 600.f + FMath::Min(Space, 900.f) / 900.f - FVector::Dist2D(Spot, Base) / 3000.f;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestSpot = Spot;
			}
		}
		P->AITarget = FMath::Lerp(Base, BestSpot, 0.7f);
	}
	return P->AITarget;
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------
""")
if '#include "GameFramework/CharacterMovementComponent.h"' not in s:
    rep('#include "EngineUtils.h"', '#include "EngineUtils.h"\n#include "GameFramework/CharacterMovementComponent.h"')
open(p, "w", encoding="utf-8").write(s)
print("gm part2 ok")
