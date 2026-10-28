// -PKFeatureTest[=Layer]: staged situations for the FC-style feature layers. Each scenario puts a few players
// in place (everyone else stands frozen on the far touchline), plays the feature through the same functions the
// controls use, and logs what happened. Run with -PKAutoTest; -PKFeatureTest=2 runs only layer 2.

#include "PKGameMode.h"
#include "PitchKings.h"
#include "PKBall.h"
#include "PKFootballer.h"
#include "PKTypes.h"
#include "PKAnimInstance.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	struct FPKFeatureDef
	{
		int32 Layer;
		const TCHAR* Name;
		float Seconds;
	};

	const FPKFeatureDef FeatureDefs[] = {
		{ 2, TEXT("precision ground pass"), 3.5f },
		{ 2, TEXT("swerve pass round defender"), 3.5f },
		{ 2, TEXT("normal pass, same lane"), 3.5f },
		{ 2, TEXT("precision lob"), 4.f },
		{ 2, TEXT("headed pass on demand"), 4.5f },
		{ 2, TEXT("header at goal on demand"), 4.5f },
		{ 3, TEXT("normal shot"), 3.f },             // 6
		{ 3, TEXT("finesse shot"), 3.f },            // 7
		{ 3, TEXT("chip shot, keeper off his line"), 3.5f }, // 8
		{ 3, TEXT("power shot"), 3.f },              // 9
		{ 3, TEXT("low driven shot"), 3.f },         // 10
		{ 3, TEXT("timed finish: good press 1"), 3.f },  // 11
		{ 3, TEXT("timed finish: good press 2"), 3.f },  // 12
		{ 3, TEXT("timed finish: good press 3"), 3.f },  // 13
		{ 3, TEXT("timed finish: early press 1"), 3.f }, // 14
		{ 3, TEXT("timed finish: early press 2"), 3.f }, // 15
		{ 3, TEXT("timed finish: early press 3"), 3.f }, // 16
		{ 3, TEXT("quick release in the box"), 3.f },    // 17
		{ 3, TEXT("fake shot vs defender"), 3.f },       // 18
		{ 4, TEXT("teammate press ON"), 3.5f },          // 19
		{ 4, TEXT("teammate press OFF"), 3.5f },         // 20
		{ 4, TEXT("defensive line NEW"), 6.f },          // 21
		{ 4, TEXT("defensive line OLD"), 6.f },          // 22
		{ 5, TEXT("send a run"), 3.f },                  // 23
		{ 5, TEXT("one-two"), 5.f },                     // 24
		{ 5, TEXT("game plan ultra attacking"), 4.f },   // 25
		{ 5, TEXT("game plan ultra defensive"), 4.f },   // 26
		{ 6, TEXT("keeper rush ON"), 3.5f },             // 27
		{ 6, TEXT("keeper rush OFF"), 3.5f },            // 28
		{ 6, TEXT("one-on-one NEW keeper (1)"), 6.f },   // 29
		{ 6, TEXT("one-on-one NEW keeper (2)"), 6.f },   // 30
		{ 6, TEXT("one-on-one NEW keeper (3)"), 6.f },   // 31
		{ 6, TEXT("one-on-one OLD keeper (1)"), 6.f },   // 32
		{ 6, TEXT("one-on-one OLD keeper (2)"), 6.f },   // 33
		{ 6, TEXT("one-on-one OLD keeper (3)"), 6.f },   // 34
		{ 6, TEXT("cross into the 6-yard box NEW"), 4.f }, // 35
		{ 6, TEXT("cross into the 6-yard box OLD"), 4.f }, // 36
		{ 7, TEXT("step-over vs defender (1)"), 3.f },   // 37
		{ 7, TEXT("ball roll vs defender (1)"), 3.f },   // 38
		{ 7, TEXT("roulette vs defender (1)"), 3.f },    // 39
		{ 7, TEXT("rainbow flick vs defender (1)"), 3.f }, // 40
		{ 7, TEXT("nutmeg vs defender (1)"), 3.f },      // 41
		{ 7, TEXT("step-over vs defender (2)"), 3.f },   // 42
		{ 7, TEXT("ball roll vs defender (2)"), 3.f },   // 43
		{ 7, TEXT("roulette vs defender (2)"), 3.f },    // 44
		{ 7, TEXT("rainbow flick vs defender (2)"), 3.f }, // 45
		{ 7, TEXT("nutmeg vs defender (2)"), 3.f },      // 46
		{ 8, TEXT("advantage played"), 3.5f },           // 47
		{ 8, TEXT("advantage called back"), 3.5f },      // 48
		{ 10, TEXT("AcceleRATE sprint race"), 3.f },     // 49
	};

	// Shot scenarios 6..17: technique, aim, power.
	EPKShotType ShotScenarioType(int32 Id)
	{
		switch (Id)
		{
		case 7: return EPKShotType::Finesse;
		case 8: return EPKShotType::Chip;
		case 9: return EPKShotType::Power;
		case 10: return EPKShotType::Low;
		default: return EPKShotType::Normal;
		}
	}
}

APKFootballer* APKGameMode::FindFootballer(int32 Team, int32 Index) const
{
	for (APKFootballer* F : Footballers)
	{
		if (F->Team == Team && F->Index == Index)
		{
			return F;
		}
	}
	return nullptr;
}

APKFootballer* APKGameMode::StageFootballer(int32 Team, int32 Index, const FVector& Loc, float Yaw)
{
	APKFootballer* F = FindFootballer(Team, Index);
	if (F)
	{
		F->Teleport2D(Loc, Yaw);
		F->StunTimer = 0.f;
		F->MoveInput = FVector::ZeroVector;
	}
	return F;
}

void APKGameMode::StageBall(APKFootballer* P)
{
	Ball->ClearControl();
	Ball->SetActorLocation(FVector(PK::Flat(P->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
	Ball->Velocity = FVector::ZeroVector;
	Ball->Spin = FVector::ZeroVector;
	Ball->SetController(P);
}

void APKGameMode::StageMakeHuman(APKFootballer* P)
{
	// The functions behind the controls act on Humans[Team]; SetHuman is off in auto-test, so set it directly.
	Humans[P->Team] = P;
	P->bHuman = true;
}

void APKGameMode::UpdateFeatureTest(float Dt)
{
	if (!bFeatureTest)
	{
		return;
	}
	FeatureClock += Dt;
	TimeLeft = FMath::Max(TimeLeft, MatchTotal * 0.9f);
	if (FeatureStep < 0)
	{
		if (FeatureClock < 1.5f)
		{
			return;
		}
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(FeatureDefs); ++i)
		{
			if (FeatureLayer == 0 || FeatureDefs[i].Layer == FeatureLayer)
			{
				FeatureOrder.Add(i);
			}
		}
		if (FeatureOrder.Num() == 0)
		{
			FGenericPlatformMisc::RequestExit(false);
			return;
		}
		FeatureStep = 0;
		FeatureClock = 0.f;
		SetupFeature(FeatureOrder[0]);
		return;
	}
	if (FeatureStep >= FeatureOrder.Num())
	{
		return;
	}
	const int32 Id = FeatureOrder[FeatureStep];
	TickFeature(Id, Dt);
	if (FeatureClock >= FeatureDefs[Id].Seconds)
	{
		FinishFeature(Id);
		++FeatureStep;
		FeatureClock = 0.f;
		if (FeatureStep < FeatureOrder.Num())
		{
			SetupFeature(FeatureOrder[FeatureStep]);
		}
		else
		{
			FGenericPlatformMisc::RequestExit(false);
		}
	}
}

void APKGameMode::SetupFeature(int32 Id)
{
	Phase = EPKPhase::Playing;
	SetPiece = EPKSetPiece::None;
	Ball->ClearNet();
	Ball->ClearControl();
	Ball->Velocity = FVector::ZeroVector;
	Ball->Spin = FVector::ZeroVector;
	Ball->IntendedReceiver = nullptr;
	int32 k = 0;
	for (APKFootballer* F : Footballers)
	{
		F->Teleport2D(FVector(-4500.f + (k++) * 400.f, PK::HalfWidth - 250.f, 0.f), -90.f);
		F->Stun(9999.f);
		F->bHuman = false;
		F->HeaderRequestTimer = 0.f;
		F->bControlledSprint = false;
		F->BufferTimer = 0.f;          // no kick left over from the previous scenario
		F->TouchAction = EPKAction::None;
		F->ConsumeKick();
		F->StopAction();
	}
	Humans[0] = Humans[1] = nullptr;
	FeatureTarget = FVector::ZeroVector;
	FeatureMin = FeatureAux = 1.e9f;
	FeatureMaxZ = 0.f;
	bFeatureFlag = false;
	FeatureCount0 = 0;
	FeatureA = FeatureB = FeatureC = nullptr;

	const float GoalX = PK::HalfLength; // Blue (team 0) attacks +X
	switch (Id)
	{
	case 0: // precision ground pass
	case 3: // precision lob
	{
		FeatureA = StageFootballer(0, 8, FVector(-2000.f, 0.f, 0.f), 0.f);
		FeatureB = StageFootballer(0, 9, FVector(-600.f, Id == 0 ? 900.f : -1200.f, 0.f), 180.f);
		FeatureB->Stun(9999.f); // waits for the pass (the lob receiver stays put so the landing spot is measured)
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		break;
	}
	case 1: // swerve pass round a defender
	case 2: // same without swerve
	{
		FeatureA = StageFootballer(0, 8, FVector(-2000.f, 0.f, 0.f), 0.f);
		FeatureB = StageFootballer(0, 9, FVector(200.f, 0.f, 0.f), 180.f);
		FeatureC = StageFootballer(1, 4, FVector(-900.f, 30.f, 0.f), 180.f);
		FeatureC->Stun(9999.f); // a statue in the lane
		FeatureB->Stun(9999.f); // waits for the pass
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		break;
	}
	case 4: // headed pass on demand
	case 5: // header at goal on demand
	{
		// Crosser wide right, the header taker in the box; two defenders and the keeper keep him onside.
		FeatureA = StageFootballer(0, 7, FVector(GoalX - 1500.f, -2300.f, 0.f), 60.f);
		FeatureB = StageFootballer(0, 9, FVector(GoalX - 1100.f, 150.f, 0.f), 0.f);
		FeatureC = StageFootballer(0, 10, FVector(GoalX - 2600.f, 500.f, 0.f), 0.f);
		StageFootballer(1, 2, FVector(GoalX - 250.f, 1400.f, 0.f), 180.f)->Stun(9999.f);
		StageFootballer(1, 3, FVector(GoalX - 250.f, -1400.f, 0.f), 180.f)->Stun(9999.f);
		if (APKFootballer* GK = Keeper(1))
		{
			GK->Teleport2D(FVector(GoalX - 120.f, 0.f, 0.f), 180.f);
			GK->StunTimer = 0.f;
		}
		if (Id == 5)
		{
			FeatureC->Stun(9999.f);
		}
		StageMakeHuman(FeatureB.Get()); // no AI on the header taker...
		StageMakeHuman(FeatureA.Get()); // ...nor on the crosser, who is "the person" until the cross is struck
		StageBall(FeatureA.Get());
		FeatureCount0 = HeadersOnDemand;
		FeatureAux = (float)(Score[0] + Shots[0] * 100);
		break;
	}
	case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
	{
		// Shooter 18 m out (11 m for the quick one), slightly right; keeper only where the test needs him.
		FeatureA = StageFootballer(0, 9, FVector(GoalX - (Id == 17 ? 1100.f : 1800.f), 400.f, 0.f), 0.f);
		APKFootballer* GK = Keeper(1);
		if (GK && Id == 8)
		{
			GK->Teleport2D(FVector(GoalX - 750.f, 150.f, 0.f), 180.f); // off his line
			GK->StunTimer = 0.f;
		}
		else if (GK)
		{
			GK->Teleport2D(FVector(GoalX - 100.f, 2600.f, 0.f), 180.f); // out of the way: measure where shots go
		}
		FeatureB = GK;
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		FeatureCount0 = Score[0];
		break;
	}
	case 19:
	case 20:
	{
		// Red carrier in midfield; the person's player is closest (he is the chaser), team-mate #4 further back.
		TeamStyle[0] = EPKTeamStyle::Balanced;
		FeatureA = StageFootballer(1, 9, FVector(0.f, 0.f, 0.f), 180.f);
		FeatureB = StageFootballer(0, 6, FVector(-450.f, 250.f, 0.f), 0.f);
		FeatureC = StageFootballer(0, 4, FVector(-1700.f, -900.f, 0.f), 0.f);
		StageMakeHuman(FeatureB.Get());
		StageMakeHuman(FeatureA.Get()); // the carrier just shields it (no AI pass)
		StageBall(FeatureA.Get());
		HumanTeamPress(0, Id == 19);
		break;
	}
	case 21:
	case 22:
	{
		// Blue back four and keeper in shape, a free Red passer in the middle and two Red forwards to run in behind.
		bOldLine = Id == 22;
		TeamStyle[0] = EPKTeamStyle::Balanced;
		for (APKFootballer* F : Footballers)
		{
			if (F->Team == 0 && (F->Role == EPKRole::Defender || F->IsKeeper()))
			{
				const FVector Home = TeamToWorld(0, F->IsKeeper() ? -0.97f : -0.55f, F->Home.Y * 0.9f);
				F->Teleport2D(Home, 0.f);
				F->StunTimer = 0.f;
			}
		}
		FeatureA = StageFootballer(1, 8, FVector(-200.f, 0.f, 0.f), 180.f);
		FeatureB = StageFootballer(1, 9, FVector(-1700.f, 700.f, 0.f), 180.f);
		FeatureC = StageFootballer(1, 10, FVector(-1700.f, -700.f, 0.f), 180.f);
		StageBall(FeatureA.Get());
		FeatureCount0 = Shots[1] * 100 + Score[1];
		break;
	}
	case 23:
	case 24:
	{
		// Blue carrier (the person) in midfield, a team-mate ahead, two Red centre-backs as statues (offside line).
		FeatureA = StageFootballer(0, 6, FVector(0.f, 0.f, 0.f), 0.f);
		FeatureB = StageFootballer(0, Id == 23 ? 9 : 8, Id == 23 ? FVector(1200.f, 600.f, 0.f) : FVector(900.f, 350.f, 0.f), 180.f);
		StageFootballer(1, 4, FVector(2300.f, 500.f, 0.f), 180.f)->Stun(9999.f);
		StageFootballer(1, 5, FVector(2300.f, -500.f, 0.f), 180.f)->Stun(9999.f);
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		FeatureTarget = FeatureB->GetActorLocation();
		FeatureCount0 = OneTwoReturns;
		break;
	}
	case 25:
	case 26:
	{
		// Everyone plays; Blue keeps the ball in the centre circle (the person stands on it).
		for (APKFootballer* F : Footballers)
		{
			F->Teleport2D(TeamToWorld(F->Team, F->Home.X * 0.9f, F->Home.Y * 0.9f), F->Team == 0 ? 0.f : 180.f);
			F->StunTimer = 0.f;
		}
		FeatureA = StageFootballer(0, 6, FVector(0.f, 0.f, 0.f), 0.f);
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		SetMentality(0, Id == 25 ? 2 : -2, false);
		FeatureMin = 0.f;
		FeatureCount0 = 0;
		break;
	}
	case 27: case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36:
	{
		// Blue keeper in his goal (Blue defends -X); Red attacks it.
		const float OwnX = -PK::HalfLength;
		bOldKeeper = (Id >= 32 && Id <= 34) || Id == 36;
		APKFootballer* GK = Keeper(0);
		if (GK)
		{
			GK->Teleport2D(FVector(OwnX + 100.f, 0.f, 0.f), 0.f);
			GK->StunTimer = 0.f;
		}
		FeatureB = GK;
		if (Id <= 28)
		{
			FeatureA = StageFootballer(1, 9, FVector(OwnX + 1400.f, 300.f, 0.f), 180.f);
			StageMakeHuman(FeatureA.Get()); // stands on the ball: does the keeper come for it?
			StageMakeHuman(StageFootballer(0, 4, FVector(OwnX + 3700.f, 2000.f, 0.f), 0.f));
			StageBall(FeatureA.Get());
			HumanKeeperRush(0, Id == 27);
		}
		else if (Id <= 34)
		{
			FeatureA = StageFootballer(1, 9, FVector(OwnX + 2100.f, 250.f, 0.f), 180.f); // through on goal (AI)
			StageBall(FeatureA.Get());
		}
		else
		{
			FeatureA = StageFootballer(1, 7, FVector(OwnX + 1000.f, -2300.f, 0.f), 120.f);  // crosser
			FeatureC = StageFootballer(1, 9, FVector(OwnX + 1300.f, 350.f, 0.f), 180.f);   // attacker
			StageMakeHuman(FeatureA.Get());
			StageBall(FeatureA.Get());
		}
		FeatureCount0 = Score[1];
		break;
	}
	case 37: case 38: case 39: case 40: case 41: case 42: case 43: case 44: case 45: case 46:
	{
		// Blue #10 (the person) runs at a Red defender who is set to press him.
		FeatureA = StageFootballer(0, 10, FVector(-500.f, 0.f, 0.f), 0.f);
		FeatureC = StageFootballer(1, 4, FVector(-180.f, 0.f, 0.f), 180.f);
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		FeatureCount0 = SkillBeats;
		break;
	}
	case 47:
	case 48:
	{
		// Blue on the ball in the Red half; a Red player fouls a Blue team-mate off the ball.
		FeatureA = StageFootballer(0, 9, FVector(1500.f, 0.f, 0.f), 0.f);
		FeatureB = StageFootballer(0, 10, FVector(1800.f, 700.f, 0.f), 0.f);
		FeatureC = StageFootballer(1, 5, FVector(1900.f, 760.f, 0.f), 180.f);
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		FeatureCount0 = Advantages * 1000 + AdvantageCallbacks;
		break;
	}
	case 49:
	{
		// Three identical runners, only the AcceleRATE type differs: Controlled, Explosive, Lengthy.
		APKFootballer* R[3] = { StageFootballer(0, 2, FVector(-3000.f, -1200.f, 0.f), 0.f), StageFootballer(0, 3, FVector(-3000.f, 0.f, 0.f), 0.f), StageFootballer(0, 4, FVector(-3000.f, 1200.f, 0.f), 0.f) };
		for (int32 i = 0; i < 3; ++i)
		{
			FPKAttributes Same = R[0]->Attr;
			Same.Pace = 0.8f;
			R[i]->ApplyProfile(R[i]->Role, R[i]->Home, Same, 0u, R[i]->ShirtNumber, R[i]->PlayerName);
			R[i]->AccelType = (uint8)i;
			R[i]->PaceScale = 1.f;
			R[i]->bHuman = true; // the test drives them
			R[i]->Stamina = R[i]->MaxStamina = 1.f;
		}
		FeatureA = R[0];
		FeatureB = R[1];
		FeatureC = R[2];
		break;
	}
	case 18:
	{
		FeatureA = StageFootballer(0, 9, FVector(GoalX - 2300.f, 0.f, 0.f), 0.f);
		FeatureC = StageFootballer(1, 5, FVector(GoalX - 2060.f, 0.f, 0.f), 180.f);
		StageMakeHuman(FeatureA.Get());
		StageBall(FeatureA.Get());
		break;
	}
	default:
		break;
	}
	UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] --- %s ---"), FeatureDefs[Id].Name);
}

void APKGameMode::TickFeature(int32 Id, float Dt)
{
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector BallFlat = PK::Flat(BallLoc);
	APKFootballer* A = FeatureA.Get();
	APKFootballer* B = FeatureB.Get();
	APKFootballer* C = FeatureC.Get();
	const bool bStart = FeatureClock - Dt < 0.3f && FeatureClock >= 0.3f;
	// Receivers waiting for a pass come alive once it is struck (not the lob receiver: that one measures the landing).
	if (B && Id <= 2 && Ball->LastKicker == A && Ball->GetController() != A && B->StunTimer > 100.f)
	{
		B->StunTimer = 0.f;
	}
	switch (Id)
	{
	case 0:
	case 3:
	{
		if (bStart && A)
		{
			const FVector Aim = (Id == 0 ? FVector(1.f, 0.35f, 0.f) : FVector(1.f, -0.4f, 0.f)).GetSafeNormal();
			const float Charge = Id == 0 ? 0.5f : 0.6f;
			const float Dist = Id == 0 ? FMath::Lerp(400.f, 4000.f, Charge) : FMath::Lerp(900.f, 4500.f, Charge);
			FeatureTarget = A->GetFlatLocation() + Aim * Dist;
			HumanPrecisionPass(0, Aim, Id == 0 ? EPKKickType::Ground : EPKKickType::Lob, Charge, false);
		}
		if (!FeatureTarget.IsZero() && Ball->LastKicker == A && Ball->GetController() != A)
		{
			if (Id == 0 && FeatureAux > 1.e8f && Ball->Velocity.Size2D() > 100.f)
			{
				// Where it would stop on its own (rolling friction plus air drag, simulated at 120 Hz).
				FVector Pos = BallFlat;
				FVector V = PK::Flat(Ball->Velocity);
				for (int32 i = 0; i < 2000 && V.Size() > 1.f; ++i)
				{
					V *= 1.f - PK::AirDrag / 120.f;
					const float Sp = V.Size();
					V *= FMath::Max(0.f, Sp - PK::RollDecel / 120.f) / Sp;
					Pos += V / 120.f;
				}
				FeatureAux = FVector::Dist(Pos, FeatureTarget);
			}
			FeatureMin = FMath::Min(FeatureMin, FVector::Dist(BallFlat, FeatureTarget));
			FeatureMaxZ = FMath::Max(FeatureMaxZ, BallLoc.Z);
			// Lob: where it first comes down.
			if (Id == 3 && !bFeatureFlag && FeatureMaxZ > 150.f && BallLoc.Z < PK::BallRadius + 3.f)
			{
				bFeatureFlag = true;
				FeatureAux = FVector::Dist(BallFlat, FeatureTarget);
			}
		}
		if (B && Ball->GetController() == B)
		{
			bFeatureFlag = Id == 0 ? true : bFeatureFlag;
			FeatureCount0 = 1; // received
		}
		break;
	}
	case 1:
	case 2:
	{
		if (bStart && A)
		{
			HumanPass(0, FVector(1.f, 0.f, 0.f), EPKKickType::Ground, Id == 1);
		}
		if (Ball->LastKicker == A && Ball->GetController() != A && C)
		{
			FeatureMin = FMath::Min(FeatureMin, FVector::Dist(BallFlat, C->GetFlatLocation()));
			FeatureMaxZ = FMath::Max(FeatureMaxZ, BallLoc.Z);
			if (FMath::Fmod(FeatureClock, 0.25f) < Dt)
			{
				UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    t %.2f ball (%.0f, %.0f) vel (%.0f, %.0f) defender (%.0f, %.0f) controller %s"), FeatureClock, BallLoc.X, BallLoc.Y, Ball->Velocity.X, Ball->Velocity.Y,
					C->GetFlatLocation().X, C->GetFlatLocation().Y, Ball->GetController() ? *FString::Printf(TEXT("%s #%d"), PK::TeamName(Ball->GetController()->Team), Ball->GetController()->Index) : TEXT("none"));
			}
		}
		if (B && Ball->GetController() == B)
		{
			FeatureCount0 = 1;
		}
		break;
	}
	case 4:
	case 5:
	{
		if (Id == 4 && C && Ball->GetController() == C && FeatureCount0 < 1000)
		{
			FeatureCount0 += 1000; // the team-mate received the headed pass
		}
		if (bStart && A && B)
		{
			// A precision lob that is still at head height when it reaches him (it lands 2.3 m beyond).
			const FVector Dir = (B->GetFlatLocation() - A->GetFlatLocation()).GetSafeNormal2D();
			const float Dist = FVector::Dist(A->GetFlatLocation(), B->GetFlatLocation()) + 230.f;
			HumanPrecisionPass(0, Dir, EPKKickType::Lob, (Dist - 900.f) / 3600.f, false);
		}
		if (A && B && Humans[0] == A && Ball->LastKicker == A && Ball->GetController() != A)
		{
			A->bHuman = false; // the cross is away: the person now controls the header taker
			StageMakeHuman(B);
		}
		// Once the cross is in the air: he is the receiver, and the "button" is pressed until it is accepted.
		if (A && B && Humans[0] == B && Ball->LastKicker == A && Ball->IsFree() && !Ball->GetController())
		{
			Ball->IntendedReceiver = B;
			if (FMath::Fmod(FeatureClock, 0.2f) < Dt)
			{
				UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    t %.2f ball z %.0f dist to him %.0f, in action %d, owner %d, phase %d, request %.2f"), FeatureClock, BallLoc.Z,
					FVector::Dist(BallFlat, B->GetFlatLocation()), B->IsInAction() ? 1 : 0, Ball->GetOwnerPlayer() ? 1 : 0, (int32)Phase, B->HeaderRequestTimer);
			}
			if (B->HeaderRequestTimer <= 0.f && !bFeatureFlag)
			{
				const FVector Aim = Id == 4 ? (C ? (C->GetFlatLocation() - B->GetFlatLocation()).GetSafeNormal2D() : FVector(-1.f, 0.f, 0.f)) : FVector(0.f, -1.f, 0.f);
				bFeatureFlag = HumanRequestHeader(0, Aim, Id == 5);
			}
		}
		if (Ball->LastKicker == B && B)
		{
			FeatureMaxZ = FMath::Max(FeatureMaxZ, 1.f); // headed
			if (Id == 4 && C)
			{
				FeatureMin = FMath::Min(FeatureMin, FVector::Dist(BallFlat, C->GetFlatLocation()));
			}
			if (Id == 5)
			{
				FeatureMin = FMath::Min(FeatureMin, FMath::Abs(BallLoc.X - PK::HalfLength)); // how close to the goal line
				if (FMath::Abs(BallLoc.X) > PK::HalfLength - 30.f && FeatureTarget.IsZero())
				{
					FeatureTarget = BallLoc; // where it crossed the line
				}
			}
		}
		break;
	}
	case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
	{
		if (bStart && A)
		{
			FeatureAux = FeatureClock; // shot asked for
			HumanShoot(0, FVector(0.f, -0.8f, 0.f), Id == 17 ? 0.2f : 0.7f, ShotScenarioType(Id));
		}
		// Timed finishing presses: good ones just before the strike, early ones right after the shot starts.
		if (A && A->IsKicking() && !bFeatureFlag && Id >= 11 && Id <= 16)
		{
			const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(A->GetKickClip());
			const float ToContact = Clip.IsValid() ? (Clip.Contact - A->GetActionTime()) / FMath::Max(A->GetActionRate(), 0.1f) : 0.f;
			const bool bGood = Id <= 13;
			if ((bGood && ToContact <= 0.05f) || (!bGood && FeatureClock - FeatureAux > 0.05f))
			{
				bFeatureFlag = HumanTimedFinish(0);
			}
		}
		if (A && Ball->LastKicker == A && Ball->GetController() != A)
		{
			if (FeatureMaxZ <= 0.f)
			{
				FeatureMin = Ball->Velocity.Size(); // strike speed
				FeatureAux = FeatureClock - FeatureAux; // wind-up
				FeatureMaxZ = 1.f;
			}
			FeatureMaxZ = FMath::Max(FeatureMaxZ, BallLoc.Z);
			if (FeatureTarget.IsZero() && BallLoc.X > PK::HalfLength - 15.f)
			{
				FeatureTarget = BallLoc; // where it reached the goal line
			}
		}
		if (B && Id == 8 && (Ball->GetHolder() == B || Ball->LastKicker == B) && FeatureTarget.IsZero())
		{
			FeatureTarget = FVector(1.f, 1.f, -1.f); // keeper got it
		}
		break;
	}
	case 19:
	case 20:
	{
		HumanTeamPress(0, Id == 19);
		if (A && C)
		{
			const float D = FVector::Dist(A->GetFlatLocation(), C->GetFlatLocation());
			FeatureMin = FMath::Min(FeatureMin, D);
			if (D < 250.f && FeatureAux > 1.e8f)
			{
				FeatureAux = FeatureClock; // first time #4 got to the carrier
			}
		}
		break;
	}
	case 21:
	case 22:
	{
		// Line shape: spread of the defenders' X, and time with a Red forward in behind the last defender.
		float MinX = 1.e9f, MaxX = -1.e9f;
		// Leave out the one who has gone to engage the ball (closest to it): the line is the others.
		const APKFootballer* Engager = nullptr;
		float EngD = 1.e9f;
		for (const APKFootballer* F : Footballers)
		{
			if (F->Team == 0 && F->Role == EPKRole::Defender && FVector::Dist(F->GetFlatLocation(), BallFlat) < EngD)
			{
				EngD = FVector::Dist(F->GetFlatLocation(), BallFlat);
				Engager = F;
			}
		}
		for (const APKFootballer* F : Footballers)
		{
			if (F->Team == 0 && F->Role == EPKRole::Defender && F != Engager)
			{
				MinX = FMath::Min(MinX, F->GetActorLocation().X);
				MaxX = FMath::Max(MaxX, F->GetActorLocation().X);
			}
		}
		FeatureMaxZ += (MaxX - MinX) * Dt; // integral of the spread
		if (FMath::Fmod(FeatureClock, 0.5f) < Dt)
		{
			FString Xs;
			for (const APKFootballer* F : Footballers)
			{
				if (F->Team == 0 && F->Role == EPKRole::Defender)
				{
					Xs += FString::Printf(TEXT(" #%d %.0f%s"), F->Index, F->GetActorLocation().X, F == Engager ? TEXT("*") : TEXT(""));
				}
			}
			UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    t %.1f line %.0f |%s | ball %.0f owner %s"), FeatureClock, TeamToWorld(0, DefLine[0], 0.f).X, *Xs, BallFlat.X,
				Ball->GetOwnerPlayer() ? *FString::Printf(TEXT("%s #%d"), PK::TeamName(Ball->GetOwnerPlayer()->Team), Ball->GetOwnerPlayer()->Index) : TEXT("-"));
		}
		const bool bBehind = (B && B->GetActorLocation().X < MinX - 60.f) || (C && C->GetActorLocation().X < MinX - 60.f);
		if (bBehind && Ball->GetOwnerPlayer() == A)
		{
			FeatureMin = (FeatureMin > 1.e8f ? 0.f : FeatureMin) + Dt; // runner in behind while the passer has it
		}
		break;
	}
	case 23:
	{
		if (bStart && A)
		{
			HumanSendRun(0);
		}
		if (B)
		{
			FeatureMaxZ = FMath::Max(FeatureMaxZ, PK::Flat(B->GetVelocity()).Size()); // top speed of the run
		}
		break;
	}
	case 24:
	{
		if (bStart && A && B)
		{
			HumanPass(0, (B->GetFlatLocation() - A->GetFlatLocation()).GetSafeNormal2D(), EPKKickType::Ground, false, true);
		}
		// Control follows the pass (as in a match): the receiver is the person's player, the passer runs (AI).
		if (A && B && A->bHuman && Ball->LastKicker == A && Ball->GetController() != A)
		{
			A->bHuman = false;
			StageMakeHuman(B);
		}
		// The person plays the return as soon as the receiver has it.
		if (B && B->bHuman && Ball->GetController() == B && !bFeatureFlag && CanReachBall(B, 90.f))
		{
			HumanPass(0, FVector(1.f, 0.f, 0.f), EPKKickType::Ground);
			bFeatureFlag = true;
		}
		if (A && Ball->GetController() == A && bFeatureFlag)
		{
			FeatureCount0 += FeatureCount0 < 1000 ? 1000 : 0; // passer got it back
			FeatureMin = A->GetActorLocation().X;
		}
		if (A)
		{
			FeatureMaxZ = FMath::Max(FeatureMaxZ, A->GetActorLocation().X);
		}
		break;
	}
	case 25:
	case 26:
	{
		if (A)
		{
			A->ProtectedTimer = 1.f; // Blue keeps the ball: the comparison is about shape, not a tackle
		}
		if (FeatureClock > 2.5f)
		{
			float Sum = 0.f;
			int32 N = 0;
			for (const APKFootballer* F : Footballers)
			{
				if (F->Team == 0 && !F->IsKeeper() && F != A)
				{
					Sum += F->GetActorLocation().X;
					++N;
				}
			}
			FeatureMin += N ? Sum / N : 0.f;
			++FeatureCount0;
		}
		break;
	}
	case 27: case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36:
	{
		const float OwnX = -PK::HalfLength;
		if (Id <= 28)
		{
			HumanKeeperRush(0, Id == 27);
		}
		if (Id >= 35 && bStart && A)
		{
			// Precision cross that drops into the six-yard box.
			const FVector Drop(OwnX + 380.f, 150.f, 0.f);
			const float Dist = FVector::Dist(A->GetFlatLocation(), Drop);
			Humans[1] = A;
			HumanPrecisionPass(1, (Drop - A->GetFlatLocation()).GetSafeNormal2D(), EPKKickType::Lob, (Dist - 900.f) / 3600.f, false);
		}
		if (A && Id >= 35 && A->bHuman && Ball->LastKicker == A && Ball->GetController() != A)
		{
			A->bHuman = false;
			Humans[1] = nullptr;
		}
		if (B)
		{
			FeatureMin = FMath::Min(FeatureMin, A ? FVector::Dist(B->GetFlatLocation(), A->GetFlatLocation()) : 1.e9f);
			FeatureMaxZ = FMath::Max(FeatureMaxZ, B->GetActorLocation().X - OwnX); // furthest off his line
			if (Ball->GetHolder() == B || Ball->GetController() == B)
			{
				bFeatureFlag = true; // keeper has it
			}
		}
		if (C && Ball->LastKicker == C)
		{
			FeatureCount0 += FeatureCount0 < 1000 ? 1000 : 0; // the attacker got a touch / header
		}
		break;
	}
	case 37: case 38: case 39: case 40: case 41: case 42: case 43: case 44: case 45: case 46:
	{
		const int32 Skill = (Id - 37) % 5;
		if (A)
		{
			// Like a person would: follow the ball out of the move, then go past him on that side.
			const float Side = (Id & 1) ? 1.f : -1.f;
			const float Exit = Skill == 0 ? -Side : (Skill == 3 ? 0.f : Side);
			const FVector ToBall = BallFlat - A->GetFlatLocation();
			A->MoveInput = FeatureClock < 1.2f && ToBall.Size() > 60.f ? ToBall.GetSafeNormal2D() : FVector(1.f, 0.45f * Exit, 0.f).GetSafeNormal();
		}
		if (bStart && A)
		{
			bFeatureFlag = PerformSkill(A, Skill, (Id & 1) ? 1.f : -1.f);
			FeatureAux = 0.f;
			Humans[0] = nullptr; // the test steers him from here (bHuman still keeps the AI off him)
		}
		if (bCloseCamera && FeatureClock > 0.5f && FeatureClock - Dt <= 0.5f)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("Skill_%d.png"), Id)), true, false);
		}
		if (A && C && FMath::Fmod(FeatureClock, 0.3f) < Dt && (Id == 37 || Id == 41))
		{
			UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    t %.1f me (%.0f,%.0f) ball (%.0f,%.0f) him (%.0f,%.0f) owner %s stun %.2f steal-cd %.2f prot %.2f"), FeatureClock, A->GetActorLocation().X, A->GetActorLocation().Y,
				BallLoc.X, BallLoc.Y, C->GetActorLocation().X, C->GetActorLocation().Y, Ball->GetController() == A ? TEXT("me") : (Ball->GetController() == C ? TEXT("him") : TEXT("-")), C->StunTimer, C->StealCooldown, A->ProtectedTimer);
		}
		if (A && C && Ball->GetController() == A && A->GetActorLocation().X > C->GetActorLocation().X + 60.f)
		{
			FeatureMaxZ = 1.f; // past him with the ball
		}
		if (C && Ball->GetController() == C)
		{
			FeatureMin = 0.f; // he won it
		}
		break;
	}
	case 47:
	case 48:
	{
		if (bStart && B && C)
		{
			AwardFoul(C, B, 0.3f, TEXT("TEST"));
		}
		if (Id == 48 && C && FeatureClock > 0.8f && FeatureClock - Dt <= 0.8f)
		{
			Ball->SetController(C); // Blue gives it away straight after: no advantage after all
		}
		break;
	}
	case 49:
	{
		for (APKFootballer* R : { A, B, C })
		{
			if (R && FeatureClock > 0.3f)
			{
				R->MoveInput = FVector(1.f, 0.f, 0.f);
				R->bSprint = true;
			}
		}
		if (FeatureClock > 0.8f && FeatureClock - Dt <= 0.8f)
		{
			UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    after 0.5 s: Controlled %.0f cm, Explosive %.0f cm, Lengthy %.0f cm"), A->GetActorLocation().X + 3000.f, B->GetActorLocation().X + 3000.f, C->GetActorLocation().X + 3000.f);
		}
		if (FeatureClock > 1.3f && FeatureClock - Dt <= 1.3f)
		{
			UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    after 1.0 s: Controlled %.0f cm, Explosive %.0f cm, Lengthy %.0f cm"), A->GetActorLocation().X + 3000.f, B->GetActorLocation().X + 3000.f, C->GetActorLocation().X + 3000.f);
		}
		break;
	}
	case 18:
	{
		if (bStart && A)
		{
			FeatureTarget = BallFlat;
			HumanFakeShot(0, FVector(0.f, 1.f, 0.f));
		}
		if (C && FeatureClock > 0.35f && FeatureClock < 0.6f && C->IsStunned())
		{
			bFeatureFlag = true; // bought it
		}
		if (FeatureClock > 0.8f && FeatureClock - Dt <= 0.8f)
		{
			FeatureMin = FMath::Abs(BallLoc.Y - FeatureTarget.Y);
		}
		break;
	}
	default:
		break;
	}
}

void APKGameMode::FinishFeature(int32 Id)
{
	const TCHAR* Name = FeatureDefs[Id].Name;
	switch (Id)
	{
	case 0:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: left alone the ball would stop %.0f cm from the aimed spot; it passed within %.0f cm; receiver got it %s"), Name, FeatureAux, FeatureMin, FeatureCount0 ? TEXT("yes") : TEXT("no"));
		break;
	case 3:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: first bounce %.0f cm from the aimed spot (closest %.0f, peak %.0f cm), receiver got it %s"), Name, bFeatureFlag ? FeatureAux : -1.f, FeatureMin, FeatureMaxZ, FeatureCount0 ? TEXT("yes") : TEXT("no"));
		break;
	case 1:
	case 2:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: closest to the defender %.0f cm, peak height %.0f cm, receiver got it %s"), Name, FeatureMin, FeatureMaxZ, FeatureCount0 ? TEXT("yes") : TEXT("no"));
		break;
	case 4:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: request accepted %s, headers on demand %d, headed %s, ball came within %.0f cm of the team-mate (receiver %s)"),
			Name, bFeatureFlag ? TEXT("yes") : TEXT("no"), HeadersOnDemand - FeatureCount0 % 1000, FeatureMaxZ > 0.f ? TEXT("yes") : TEXT("no"), FeatureMin,
			FeatureCount0 >= 1000 ? TEXT("yes") : TEXT("no"));
		break;
	case 5:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: request accepted %s, headers on demand %d, headed %s, reached the goal line %s at Y %.0f Z %.0f, score now %d"),
			Name, bFeatureFlag ? TEXT("yes") : TEXT("no"), HeadersOnDemand - FeatureCount0, FeatureMaxZ > 0.f ? TEXT("yes") : TEXT("no"), FeatureTarget.IsZero() ? TEXT("no") : TEXT("yes"),
			FeatureTarget.Y, FeatureTarget.Z, Score[0]);
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    ball now (%.0f, %.0f, %.0f), held by keeper %s, closest to the goal line %.0f cm"), Ball->GetActorLocation().X, Ball->GetActorLocation().Y, Ball->GetActorLocation().Z,
			Ball->GetHolder() ? TEXT("yes") : TEXT("no"), FeatureMin);
		break;
	case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
	{
		const bool bKeeper = FeatureTarget == FVector(1.f, 1.f, -1.f);
		const bool bReached = !FeatureTarget.IsZero() && !bKeeper;
		const bool bOnTarget = bReached && FMath::Abs(FeatureTarget.Y) < PK::GoalHalfWidth && FeatureTarget.Z < 244.f;
		// Distance from where it was aimed with no error (lateral -0.8, technique height).
		const float CleanY = -0.8f * (PK::GoalHalfWidth - 50.f);
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: wind-up %.2f s, strike %.0f cm/s, peak %.0f cm | %s at Y %.0f Z %.0f (%.0f cm wide of the aimed post spot) | goal %s | timed green %d red %d"),
			Name, FeatureAux, FeatureMin, FeatureMaxZ, bKeeper ? TEXT("keeper got it") : (bReached ? (bOnTarget ? TEXT("ON TARGET") : TEXT("off target")) : TEXT("did not reach the line")),
			FeatureTarget.Y, FeatureTarget.Z, bReached ? FMath::Abs(FeatureTarget.Y - CleanY) : -1.f, Score[0] > FeatureCount0 ? TEXT("yes") : TEXT("no"), TimedGreen, TimedRed);
		break;
	}
	case 19:
	case 20:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: team-mate #4 got within %.0f cm of the carrier%s (press time %.1f s)"), Name, FeatureMin,
			FeatureAux < 1.e8f ? *FString::Printf(TEXT(", reached him after %.1f s"), FeatureAux) : TEXT(", never reached him"), TeamPressSeconds);
		HumanTeamPress(0, false);
		break;
	case 21:
	case 22:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: average line spread %.0f cm, a runner was in behind for %.1f s while the passer had it, Red shots %d"), Name,
			FeatureMaxZ / FeatureDefs[Id].Seconds, FeatureMin > 1.e8f ? 0.f : FeatureMin, Shots[1] - FeatureCount0 / 100);
		bOldLine = false;
		break;
	case 23:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: runner went from X %.0f to %.0f (offside line at 2300), top speed %.0f cm/s, runs sent %d"), Name, FeatureTarget.X,
			FeatureB.IsValid() ? FeatureB->GetActorLocation().X : 0.f, FeatureMaxZ, RunsSent);
		break;
	case 24:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: one-twos %d, return played %s, passer got it back %s at X %.0f (furthest run X %.0f)"), Name, OneTwos,
			OneTwoReturns > FeatureCount0 % 1000 ? TEXT("yes") : TEXT("no"), FeatureCount0 >= 1000 ? TEXT("yes") : TEXT("no"), FeatureMin, FeatureMaxZ);
		break;
	case 25:
	case 26:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: Blue outfield average X %.0f cm (with the ball at X 0)"), Name, FeatureCount0 ? FeatureMin / FeatureCount0 : 0.f);
		SetMentality(0, 0, false);
		break;
	case 27: case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: keeper came %.0f cm off his line, closest to the attacker %.0f cm, keeper won the ball %s, attacker touched it %s, goal %s, smothers %d"), Name,
			FeatureMaxZ, FeatureMin, bFeatureFlag ? TEXT("yes") : TEXT("no"), FeatureCount0 >= 1000 ? TEXT("yes") : TEXT("no"), Score[1] > FeatureCount0 % 1000 ? TEXT("yes") : TEXT("no"), KeeperSmothers);
		HumanKeeperRush(0, false);
		bOldKeeper = false;
		break;
	case 37: case 38: case 39: case 40: case 41: case 42: case 43: case 44: case 45: case 46:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: performed %s, defender wrong-footed %s, got past him with the ball %s, defender won it %s, still has it at the end %s"), Name,
			bFeatureFlag ? TEXT("yes") : TEXT("no"), SkillBeats > FeatureCount0 ? TEXT("yes") : TEXT("no"), FeatureMaxZ > 0.f ? TEXT("yes") : TEXT("no"), FeatureMin == 0.f ? TEXT("yes") : TEXT("no"),
			Ball->GetController() == FeatureA.Get() ? TEXT("yes") : TEXT("no"));
		break;
	case 47:
	case 48:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: advantage given %s, called back %s, free kick being taken %s"), Name,
			Advantages > FeatureCount0 / 1000 ? TEXT("yes") : TEXT("no"), AdvantageCallbacks > FeatureCount0 % 1000 ? TEXT("yes") : TEXT("no"),
			SetPiece == EPKSetPiece::FreeKick && Ball->IsPlaced() ? TEXT("yes") : TEXT("no"));
		break;
	case 49:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: after 2.7 s Controlled %.0f cm (top %.0f), Explosive %.0f cm (top %.0f), Lengthy %.0f cm (top %.0f)"), Name,
			FeatureA->GetActorLocation().X + 3000.f, FeatureA->GetVelocity().Size2D(), FeatureB->GetActorLocation().X + 3000.f, FeatureB->GetVelocity().Size2D(),
			FeatureC->GetActorLocation().X + 3000.f, FeatureC->GetVelocity().Size2D());
		break;
	case 18:
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: defender bought it %s, ball moved %.0f cm sideways, still on the ball %s"), Name, bFeatureFlag ? TEXT("yes") : TEXT("no"), FeatureMin,
			Ball->GetController() == FeatureA.Get() ? TEXT("yes") : TEXT("no"));
		break;
	default:
		break;
	}
}
