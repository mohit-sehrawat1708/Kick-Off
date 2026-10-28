# Layer 2 tuning: precision pass weight, precision lob flight with air drag, feature-test diagnostics.
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

patch("PKGameMode.cpp", [
("""		float Speed = FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + (Kick.bPrecision ? 150.f : 250.f), Kick.bPrecision ? 500.f : 700.f, 3000.f);""",
"""		// A precision pass arrives at the spot still rolling at ~3 m/s for the receiver to run onto.
		float Speed = Kick.bPrecision ? FMath::Clamp(FMath::Sqrt(300.f * 300.f + 2.f * PK::RollDecel * D) * 1.04f, 500.f, 3000.f)
			: FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);"""),
("""		Velocity = Dir * (D / T * 0.82f);
		Velocity.Z = 0.5f * PK::Gravity * T - (From.Z - PK::BallRadius) / T;""",
"""		Velocity = Dir * (D / T * 0.82f);
		Velocity.Z = 0.5f * PK::Gravity * T - (From.Z - PK::BallRadius) / T;
		if (Kick.bPrecision && !bThrow)
		{
			// Precision lob: lands on the spot. Exact flight with the ball's linear air drag k:
			// x(T) = v/k (1 - e^-kT),  z(T) = 0  =>  vz = g T / (1 - e^-kT) - g / k.
			const float K = PK::AirDrag;
			const float E = 1.f - FMath::Exp(-K * T);
			Velocity = Dir * (D * K / E) * SpeedScale;
			Velocity.Z = PK::Gravity * T / E - PK::Gravity / K - (From.Z - PK::BallRadius) / T;
		}"""),
])

patch("PKFeatureTest.cpp", [
("""		FeatureC->Stun(9999.f);
		StageMakeHuman(FeatureB.Get());""", """		if (Id == 5)
		{
			FeatureC->Stun(9999.f);
		}
		StageMakeHuman(FeatureB.Get());"""),
("""		if (Ball->LastKicker == A && Ball->GetController() != A && C)
		{
			FeatureMin = FMath::Min(FeatureMin, FVector::Dist(BallFlat, C->GetFlatLocation()));
			FeatureMaxZ = FMath::Max(FeatureMaxZ, BallLoc.Z);
		}""", """		if (Ball->LastKicker == A && Ball->GetController() != A && C)
		{
			FeatureMin = FMath::Min(FeatureMin, FVector::Dist(BallFlat, C->GetFlatLocation()));
			FeatureMaxZ = FMath::Max(FeatureMaxZ, BallLoc.Z);
			if (FMath::Fmod(FeatureClock, 0.25f) < Dt)
			{
				UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    t %.2f ball (%.0f, %.0f) vel (%.0f, %.0f) defender (%.0f, %.0f) controller %s"), FeatureClock, BallLoc.X, BallLoc.Y, Ball->Velocity.X, Ball->Velocity.Y,
					C->GetFlatLocation().X, C->GetFlatLocation().Y, Ball->GetController() ? *FString::Printf(TEXT("%s #%d"), PK::TeamName(Ball->GetController()->Team), Ball->GetController()->Index) : TEXT("none"));
			}
		}"""),
("""		if (!FeatureTarget.IsZero() && Ball->LastKicker == A && Ball->GetController() != A)
		{""", """		if (!FeatureTarget.IsZero() && Ball->LastKicker == A && Ball->GetController() != A)
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
			}"""),
("""		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: ball passed %.0f cm from the aimed spot, receiver got it %s"), Name, FeatureMin, FeatureCount0 ? TEXT("yes") : TEXT("no"));""",
"""		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest] %s: left alone the ball would stop %.0f cm from the aimed spot; it passed within %.0f cm; receiver got it %s"), Name, FeatureAux, FeatureMin, FeatureCount0 ? TEXT("yes") : TEXT("no"));"""),
("""			FeatureTarget.Y, FeatureTarget.Z, Score[0]);""", """			FeatureTarget.Y, FeatureTarget.Z, Score[0]);
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    ball now (%.0f, %.0f, %.0f), held by keeper %s, closest to the goal line %.0f cm"), Ball->GetActorLocation().X, Ball->GetActorLocation().Y, Ball->GetActorLocation().Z,
			Ball->GetHolder() ? TEXT("yes") : TEXT("no"), FeatureMin);"""),
])
print("fix1 ok")
