# Kick animations fitted to the ball's speed: the clip is chosen by how hard the ball will be struck and played at
# the rate that makes the striking foot's speed fit the launch speed.
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
("""	EPKAction KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn) const;""",
"""	EPKAction KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, float LaunchSpeed) const;
	// How fast the ball will leave the foot for this request (mirrors APKGameMode::ExecuteKick).
	float EstimateLaunchSpeed(const FPKKickRequest& Request) const;
	// Play rate that makes the striking foot's speed fit the launch speed (ball / foot ratio by technique).
	static float FitKickRate(EPKAction Clip, float LaunchSpeed);"""),
])

patch("PKFootballer.cpp", [
("""EPKAction APKFootballer::KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn) const
{""", """float APKFootballer::EstimateLaunchSpeed(const FPKKickRequest& Request) const
{
	const float D = FMath::Max(FVector::Dist2D(GetActorLocation(), Request.Target), 1.f);
	switch (Request.Type)
	{
	case EPKKickType::Shot:
	{
		if (Request.ShotType == EPKShotType::Chip)
		{
			return 900.f + 500.f * Request.Power;
		}
		const float Technique = Request.ShotType == EPKShotType::Power ? 1.22f : (Request.ShotType == EPKShotType::Finesse ? 0.85f : (Request.ShotType == EPKShotType::Low ? 1.05f : 1.f));
		return (2000.f + 1900.f * Request.Power) * FMath::Lerp(0.9f, 1.05f, Attr.Shooting) * Technique;
	}
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
	{
		const float T = FMath::Clamp(D / 1300.f, 0.9f, 2.1f);
		const float H = D / T * 0.82f;
		const float V = 0.5f * PK::Gravity * T;
		return FMath::Sqrt(H * H + V * V) * (Request.Type == EPKKickType::Clearance ? 1.15f : 1.f);
	}
	default:
	{
		const float Speed = Request.bPrecision ? FMath::Sqrt(300.f * 300.f + 2.f * PK::RollDecel * D) * 1.04f : FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);
		return Speed * (Request.Type == EPKKickType::Through ? 1.1f : 1.f);
	}
	}
}

float APKFootballer::FitKickRate(EPKAction Clip, float LaunchSpeed)
{
	const FPKActionClip& Info = UPKAnimInstance::GetActionClip(Clip);
	if (!Info.IsValid() || Info.ContactSpeed < 50.f)
	{
		return 1.f;
	}
	// Ball leaves at about 1.3x the foot speed off the side foot, 1.45x off the laces, 1.6x for a full volley.
	const bool bSideFoot = Clip == EPKAction::Pass || Clip == EPKAction::PassLeft;
	const float Ratio = bSideFoot ? 1.3f : (Clip == EPKAction::Volley ? 1.6f : 1.45f);
	const float MaxRate = bSideFoot ? 1.6f : 1.55f;
	return FMath::Clamp(LaunchSpeed / (Ratio * Info.ContactSpeed), 0.7f, MaxRate);
}

EPKAction APKFootballer::KickAction(const FPKKickRequest& Request, bool bFromHands, bool bThrowIn, float LaunchSpeed) const
{"""),
("""	const float Speed = PK::Flat(GetVelocity()).Size();
	const FVector ToTarget = PK::Flat(Request.Target - GetActorLocation()).GetSafeNormal();
	const float Side = FVector::DotProduct(ToTarget, PK::Flat(GetActorRightVector()));
	switch (Request.Type)
	{
	case EPKKickType::Shot:
		if (Speed > 300.f || (Speed > 150.f && FMath::FRand() < 0.5f))
		{
			return EPKAction::RunningShot;
		}
		return EPKAction::Shot;
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
		return Speed > 330.f ? EPKAction::RunningShot : EPKAction::Shot;
	default:
		if (Speed > 330.f)
		{
			return EPKAction::RunningShot; // pass on the run
		}""", """	const float Speed = PK::Flat(GetVelocity()).Size();
	const FVector ToTarget = PK::Flat(Request.Target - GetActorLocation()).GetSafeNormal();
	const float Side = FVector::DotProduct(ToTarget, PK::Flat(GetActorRightVector()));
	// The clip has to fit how hard the ball is struck: a side-foot pass can't send it 30 m, a full strike looks
	// wrong on a 10 m pass. The penalty-kick strike tops out around 2200 cm/s; harder than that is the running strike.
	switch (Request.Type)
	{
	case EPKKickType::Shot:
		if (Speed > 300.f || LaunchSpeed > 2200.f || (Speed > 150.f && FMath::FRand() < 0.5f))
		{
			return EPKAction::RunningShot;
		}
		return EPKAction::Shot;
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
		return Speed > 330.f || LaunchSpeed > 2200.f ? EPKAction::RunningShot : EPKAction::Shot;
	default:
		if (LaunchSpeed > 1150.f || Speed > 450.f)
		{
			// Driven pass / pass on the run: struck with the laces.
			return Speed > 330.f || LaunchSpeed > 1700.f ? EPKAction::RunningShot : EPKAction::Shot;
		}"""),
("""	EPKAction Clip = KickAction(Request, bFromHands, bThrowIn);""", """	const float Launch = EstimateLaunchSpeed(Request);
	EPKAction Clip = KickAction(Request, bFromHands, bThrowIn, Launch);"""),
("""		const float WindUp = Info.IsValid() ? Info.Contact - Info.Start : 0.2f;""", """		const float WindUp = (Info.IsValid() ? Info.Contact - Info.Start : 0.2f) / FMath::Max(FitKickRate(Clip, Launch), 0.5f);"""),
("""	// A little variety in tempo so repeated kicks don't look identical.
	KickClip = Clip;
	if (!PlayAction(Clip, EPKAction::None, FMath::FRandRange(0.92f, 1.1f) * FMath::Clamp(Request.ActionRate, 0.5f, 1.6f)))""",
"""	// Tempo: the swing fits the ball's speed (a little variety on top). The power shot's longer wind-up and the quick
	// release's short one still show, within what the swing can do.
	KickClip = Clip;
	const bool bHandsOrThrow = bFromHands || bThrowIn;
	const float Fit = bHandsOrThrow ? 1.f : FitKickRate(Clip, Launch);
	const float Tempo = FMath::Lerp(1.f, FMath::Clamp(Request.ActionRate, 0.5f, 1.6f), 0.5f);
	const float Rate = bHandsOrThrow ? FMath::FRandRange(0.92f, 1.1f) * FMath::Clamp(Request.ActionRate, 0.5f, 1.6f) : FMath::Clamp(Fit * Tempo * FMath::FRandRange(0.97f, 1.03f), 0.65f, 1.7f);
	if (!PlayAction(Clip, EPKAction::None, Rate))"""),
])
print("kick fit ok")
