# Layer 9: 16 more PlayStyles (23 in all) with PlayStyle+ versions for elite players.
# Layer 10: AcceleRATE archetypes (Explosive / Controlled / Lengthy) from the player's attributes.
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
("""namespace PKTrait
{
	enum : uint8
	{
		Technical = 1 << 0,   // tighter dribbling touches
		Finesse = 1 << 1,     // curled, placed shots
		PowerShot = 1 << 2,   // harder shots
		TikiTaka = 1 << 3,    // accurate short passing
		Rapid = 1 << 4,       // extra top speed
		PressProven = 1 << 5, // better at winning the ball
		Anticipate = 1 << 6,  // reads passes, longer interception reach
	};
}""", """namespace PKTrait
{
	enum : uint32
	{
		Technical = 1 << 0,   // tighter dribbling touches
		Finesse = 1 << 1,     // curled, placed shots
		PowerShot = 1 << 2,   // harder shots
		TikiTaka = 1 << 3,    // accurate short passing
		Rapid = 1 << 4,       // extra top speed
		PressProven = 1 << 5, // better at winning the ball
		Anticipate = 1 << 6,  // reads passes, longer interception reach
		WhippedPass = 1 << 7, // crosses faster and more accurate
		IncisivePass = 1 << 8, // through balls
		LongBallPass = 1 << 9, // long passes
		Trickster = 1 << 10,  // skill moves
		QuickStep = 1 << 11,  // acceleration off the mark
		Relentless = 1 << 12, // stamina
		Bruiser = 1 << 13,    // physical duels
		Intercept = 1 << 14,  // cutting out passes
		Block = 1 << 15,      // blocking shots
		Aerial = 1 << 16,     // headers
		Jockey = 1 << 17,     // quicker jockeying
		Acrobatic = 1 << 18,  // volleys
		FarThrow = 1 << 19,   // long throw-ins
		DeadBall = 1 << 20,   // free kicks and corners
		ChipShot = 1 << 21,   // chips
		SlideTackle = 1 << 22, // longer slides
		Count = 23
	};
	const TCHAR* Name(int32 Bit);
	// "Finesse Shot+, Rapid" (a + marks the PlayStyle+ version). MaxNames 0 = all.
	FString Names(uint32 Traits, uint32 Plus, int32 MaxNames = 0);
}"""),
("""	bool HasTrait(uint8 Trait) const { return (Traits & Trait) != 0; }
	FString TraitNames() const;""", """	bool HasTrait(uint32 Trait) const { return (Traits & Trait) != 0; }
	// 0 without the PlayStyle, 1 with it, 1.6 with its PlayStyle+ version.
	float StyleLevel(uint32 Trait) const { return (Traits & Trait) ? ((TraitsPlus & Trait) ? 1.6f : 1.f) : 0.f; }
	FString TraitNames(int32 MaxNames = 0) const;
	// AcceleRATE: 0 Controlled, 1 Explosive (quick off the mark), 2 Lengthy (slow start, long stride at the top end).
	uint8 AccelType = 0;
	static uint8 AccelTypeFor(const FPKAttributes& A);
	static const TCHAR* AccelName(uint8 Type);
	float AccelFactor(float SpeedAlpha) const;"""),
("""	FPKAttributes Attr;
	uint8 Traits = 0;""", """	FPKAttributes Attr;
	uint32 Traits = 0;
	uint32 TraitsPlus = 0;"""),
("""	void ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint8 InTraits, int32 InNumber, const FString& InName);""",
"""	void ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint32 InTraits, int32 InNumber, const FString& InName, uint32 InPlus = 0);"""),
])

patch("PKFootballer.cpp", [
("""void APKFootballer::ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint8 InTraits, int32 InNumber, const FString& InName)
{
	Role = InRole;
	Home = InHome;
	Attr = InAttr;
	Traits = InTraits;
	ShirtNumber = InNumber;
	PlayerName = InName;
	PaceScale = FMath::Lerp(0.9f, 1.06f, Attr.Pace) * (HasTrait(PKTrait::Rapid) ? 1.04f : 1.f) * (IsKeeper() ? 0.92f : 1.f);
}""", """void APKFootballer::ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint32 InTraits, int32 InNumber, const FString& InName, uint32 InPlus)
{
	Role = InRole;
	Home = InHome;
	Attr = InAttr;
	Traits = InTraits;
	TraitsPlus = InPlus & InTraits;
	ShirtNumber = InNumber;
	PlayerName = InName;
	AccelType = AccelTypeFor(Attr);
	PaceScale = FMath::Lerp(0.9f, 1.06f, Attr.Pace) * (1.f + 0.04f * StyleLevel(PKTrait::Rapid)) * (IsKeeper() ? 0.92f : 1.f)
		* (AccelType == 2 ? 1.015f : (AccelType == 1 ? 0.99f : 1.f));
}

uint8 APKFootballer::AccelTypeFor(const FPKAttributes& A)
{
	// Agile and light: Explosive. Strong with pace: Lengthy. Otherwise Controlled.
	if (A.Pace > 0.68f && A.Control - A.Physical > 0.06f)
	{
		return 1;
	}
	if (A.Pace > 0.6f && A.Physical - A.Control > 0.06f)
	{
		return 2;
	}
	return 0;
}

const TCHAR* APKFootballer::AccelName(uint8 Type)
{
	return Type == 1 ? TEXT("Explosive") : (Type == 2 ? TEXT("Lengthy") : TEXT("Controlled"));
}

float APKFootballer::AccelFactor(float SpeedAlpha) const
{
	// Acceleration across the speed range, plus Quick Step off the mark.
	float F = 1.f;
	if (AccelType == 1)
	{
		F = FMath::Lerp(1.22f, 0.9f, SpeedAlpha);
	}
	else if (AccelType == 2)
	{
		F = FMath::Lerp(0.85f, 1.15f, SpeedAlpha);
	}
	return F * (1.f + 0.12f * StyleLevel(PKTrait::QuickStep) * (1.f - SpeedAlpha));
}

namespace PKTrait
{
	const TCHAR* Name(int32 Bit)
	{
		static const TCHAR* Names[Count] = {
			TEXT("Technical"), TEXT("Finesse Shot"), TEXT("Power Shot"), TEXT("Tiki Taka"), TEXT("Rapid"), TEXT("Press Proven"), TEXT("Anticipate"),
			TEXT("Whipped Pass"), TEXT("Incisive Pass"), TEXT("Long Ball Pass"), TEXT("Trickster"), TEXT("Quick Step"), TEXT("Relentless"), TEXT("Bruiser"),
			TEXT("Intercept"), TEXT("Block"), TEXT("Aerial"), TEXT("Jockey"), TEXT("Acrobatic"), TEXT("Far Throw"), TEXT("Dead Ball"), TEXT("Chip Shot"), TEXT("Slide Tackle") };
		return Bit >= 0 && Bit < Count ? Names[Bit] : TEXT("");
	}

	FString Names(uint32 Traits, uint32 Plus, int32 MaxNames)
	{
		TArray<FString> Out;
		for (int32 Bit = 0; Bit < Count; ++Bit)
		{
			if (Traits & (1u << Bit))
			{
				Out.Add(FString(Name(Bit)) + ((Plus & (1u << Bit)) ? TEXT("+") : TEXT("")));
			}
		}
		// PlayStyle+ first: they are what the player is known for.
		Out.StableSort([](const FString& L, const FString& R) { return L.EndsWith(TEXT("+")) && !R.EndsWith(TEXT("+")); });
		if (MaxNames > 0 && Out.Num() > MaxNames)
		{
			Out.SetNum(MaxNames);
		}
		return FString::Join(Out, TEXT(", "));
	}
}"""),
])

# Footballer::TraitNames (whole old body replaced)
import re as _re
_p = os.path.join(SRC, "PKFootballer.cpp")
_s = open(_p, encoding="utf-8").read()
if "FString APKFootballer::TraitNames(int32 MaxNames) const" not in _s:
    _s, _n = _re.subn(r"FString APKFootballer::TraitNames\(\) const\n\{.*?\n\}\n",
        "FString APKFootballer::TraitNames(int32 MaxNames) const\n{\n\treturn PKTrait::Names(Traits, TraitsPlus, MaxNames);\n}\n", _s, count=1, flags=_re.S)
    assert _n == 1
    open(_p, "w", encoding="utf-8").write(_s)

# movement effects: AcceleRATE + Quick Step, Jockey, Relentless
patch("PKFootballer.cpp", [
("""	Move->MaxAcceleration = FMath::Lerp(2100.f, 900.f, SpeedAlpha);""",
"""	Move->MaxAcceleration = FMath::Lerp(2100.f, 900.f, SpeedAlpha) * AccelFactor(SpeedAlpha);"""),
("""	case EPKStance::Jockey:       MaxSpeed = PK::JockeySpeed; break;""",
"""	case EPKStance::Jockey:       MaxSpeed = PK::JockeySpeed * (1.f + 0.1f * StyleLevel(PKTrait::Jockey)); break;"""),
("""	const float Fitness = FMath::Lerp(1.3f, 0.75f, Attr.Physical); // strong players last longer""",
"""	const float Fitness = FMath::Lerp(1.3f, 0.75f, Attr.Physical) * (1.f - 0.2f * StyleLevel(PKTrait::Relentless)); // strong players last longer"""),
])

patch("PKTeamData.h", [
("""	uint8 Traits = 0;""", """	uint32 Traits = 0;
	uint32 TraitsPlus = 0; // PlayStyle+ (elite) versions"""),
("""	FString TraitNames(uint8 Traits);""", """	FString TraitNames(uint32 Traits, uint32 Plus = 0);"""),
])

patch("PKTeamData.cpp", [
("""	uint8 MakeTraits(FRandomStream& R, EPKPos Pos)
	{
		if (Pos == EPKPos::GK)
		{
			return 0;
		}
		const uint8 Pools[3][3] = {
			{ PKTrait::PressProven, PKTrait::Anticipate, PKTrait::TikiTaka },
			{ PKTrait::TikiTaka, PKTrait::Technical, PKTrait::Anticipate },
			{ PKTrait::Finesse, PKTrait::PowerShot, PKTrait::Rapid },
		};
		const int32 Pool = Pos == EPKPos::DEF ? 0 : (Pos == EPKPos::MID ? 1 : 2);
		uint8 T = Pools[Pool][R.RandRange(0, 2)];
		if (R.FRand() < 0.45f)
		{
			T |= Pools[R.RandRange(0, 2)][R.RandRange(0, 2)];
		}
		return T;
	}""", """	uint32 MakeTraits(FRandomStream& R, EPKPos Pos)
	{
		// PlayStyles by position: one or two (sometimes three), mostly from the player's own line.
		static const uint32 Def[] = { PKTrait::PressProven, PKTrait::Anticipate, PKTrait::Intercept, PKTrait::Block, PKTrait::Bruiser, PKTrait::Aerial,
			PKTrait::Jockey, PKTrait::SlideTackle, PKTrait::LongBallPass, PKTrait::FarThrow, PKTrait::TikiTaka };
		static const uint32 Mid[] = { PKTrait::TikiTaka, PKTrait::Technical, PKTrait::IncisivePass, PKTrait::LongBallPass, PKTrait::WhippedPass,
			PKTrait::Relentless, PKTrait::Trickster, PKTrait::DeadBall, PKTrait::Intercept, PKTrait::PressProven, PKTrait::Finesse };
		static const uint32 Fwd[] = { PKTrait::Finesse, PKTrait::PowerShot, PKTrait::Rapid, PKTrait::QuickStep, PKTrait::Trickster, PKTrait::ChipShot,
			PKTrait::Acrobatic, PKTrait::Aerial, PKTrait::Technical, PKTrait::WhippedPass };
		if (Pos == EPKPos::GK)
		{
			return R.FRand() < 0.3f ? (uint32)PKTrait::FarThrow : 0u; // a keeper's long throw-out
		}
		const uint32* Pool = Pos == EPKPos::DEF ? Def : (Pos == EPKPos::MID ? Mid : Fwd);
		const int32 Num = Pos == EPKPos::DEF ? UE_ARRAY_COUNT(Def) : (Pos == EPKPos::MID ? UE_ARRAY_COUNT(Mid) : UE_ARRAY_COUNT(Fwd));
		uint32 T = Pool[R.RandRange(0, Num - 1)];
		if (R.FRand() < 0.6f)
		{
			T |= Pool[R.RandRange(0, Num - 1)];
		}
		if (R.FRand() < 0.2f)
		{
			T |= Pool[R.RandRange(0, Num - 1)];
		}
		return T;
	}"""),
("""	FString TraitNames(uint8 Traits)
	{
		TArray<FString> Names;
		if (Traits & PKTrait::Technical) Names.Add(TEXT("Technical"));
		if (Traits & PKTrait::Finesse) Names.Add(TEXT("Finesse Shot"));
		if (Traits & PKTrait::PowerShot) Names.Add(TEXT("Power Shot"));
		if (Traits & PKTrait::TikiTaka) Names.Add(TEXT("Tiki Taka"));
		if (Traits & PKTrait::Rapid) Names.Add(TEXT("Rapid"));
		if (Traits & PKTrait::PressProven) Names.Add(TEXT("Press Proven"));
		if (Traits & PKTrait::Anticipate) Names.Add(TEXT("Anticipate"));
		return FString::Join(Names, TEXT(", "));
	}""", """	FString TraitNames(uint32 Traits, uint32 Plus)
	{
		return PKTrait::Names(Traits, Plus);
	}"""),
("""				Pl.Traits = MakeTraits(R, Pl.Pos);""", """				Pl.Traits = MakeTraits(R, Pl.Pos);
				// Elite players have the PlayStyle+ version of one of theirs.
				if (Pl.Attr.Overall() >= 80 && Pl.Traits != 0)
				{
					for (int32 Bit = 0; Bit < PKTrait::Count; ++Bit)
					{
						if (Pl.Traits & (1u << Bit))
						{
							Pl.TraitsPlus = 1u << Bit;
							break;
						}
					}
				}"""),
])

patch("PKGameMode.cpp", [
("""			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name);""",
"""			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name, Pl.TraitsPlus);"""),
("""	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name);""",
"""	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name, In.TraitsPlus);"""),
# passing styles
("""		const float Sloppy = (1.f - P->Attr.Passing) * (P->HasTrait(PKTrait::TikiTaka) ? 0.5f : 1.f) * P->FatigueError();""",
"""		// PlayStyles: Tiki Taka (short), Incisive Pass (through balls), Long Ball Pass, Whipped Pass (crosses), Dead Ball.
		float StyleAcc = 1.f - 0.5f * FMath::Min(P->StyleLevel(PKTrait::TikiTaka), 1.3f);
		if (Kick.Type == EPKKickType::Through)
		{
			StyleAcc -= 0.25f * P->StyleLevel(PKTrait::IncisivePass);
		}
		if ((Kick.Type == EPKKickType::Lob || Kick.Type == EPKKickType::Clearance) && D > 2500.f)
		{
			StyleAcc -= 0.25f * P->StyleLevel(PKTrait::LongBallPass);
		}
		if (Kick.Type == EPKKickType::Lob && FMath::Abs(Kick.Target.X - OppGoal(P->Team).X) < PK::PenaltyDepth + 100.f)
		{
			StyleAcc -= 0.25f * P->StyleLevel(PKTrait::WhippedPass);
		}
		if (Ball->IsPlaced())
		{
			StyleAcc -= 0.25f * P->StyleLevel(PKTrait::DeadBall);
		}
		const float Sloppy = (1.f - P->Attr.Passing) * FMath::Max(StyleAcc, 0.2f) * P->FatigueError();"""),
("""		const float T = bThrow ? FMath::Clamp(D / 1000.f, 0.6f, 1.3f) : (bCrossIn ? FMath::Clamp(D / 1900.f, 0.8f, 1.6f) : FMath::Clamp(D / 1300.f, 0.9f, 2.1f));""",
"""		const float T = bThrow ? FMath::Clamp(D / (1000.f * (1.f + 0.3f * P->StyleLevel(PKTrait::FarThrow))), 0.6f, 1.3f)
			: (bCrossIn ? FMath::Clamp(D / (1900.f + 150.f * P->StyleLevel(PKTrait::WhippedPass)), 0.7f, 1.6f) : FMath::Clamp(D / 1300.f, 0.9f, 2.1f));"""),
("""		if (Kick.Type == EPKKickType::Through)
		{
			Speed *= 1.1f;
		}
		if (bThrow)
		{
			Speed = FMath::Min(Speed, 1500.f);
		}""", """		if (Kick.Type == EPKKickType::Through)
		{
			Speed *= 1.1f + 0.05f * P->StyleLevel(PKTrait::IncisivePass);
		}
		if (bThrow)
		{
			Speed = FMath::Min(Speed, 1500.f * (1.f + 0.3f * P->StyleLevel(PKTrait::FarThrow)));
		}"""),
# shooting styles
("""	case EPKShotType::Finesse: Err *= P->HasTrait(PKTrait::Finesse) ? 0.6f : 0.75f; break;
	case EPKShotType::Power:   Err *= P->HasTrait(PKTrait::PowerShot) ? 1.05f : 1.3f; break;
	case EPKShotType::Chip:    Err *= 0.7f; break;""", """	case EPKShotType::Finesse: Err *= 0.75f - 0.15f * P->StyleLevel(PKTrait::Finesse); break;
	case EPKShotType::Power:   Err *= 1.3f - 0.25f * P->StyleLevel(PKTrait::PowerShot); break;
	case EPKShotType::Chip:    Err *= 0.7f - 0.15f * P->StyleLevel(PKTrait::ChipShot); break;"""),
("""	if (bAirborne)
	{
		Err *= 1.4f; // volleys are harder to keep on target
	}
	Err *= P->FatigueError();""", """	if (bAirborne)
	{
		Err *= 1.4f - 0.25f * P->StyleLevel(PKTrait::Acrobatic); // volleys are harder to keep on target
	}
	if (Ball->IsPlaced())
	{
		Err *= 1.f - 0.25f * P->StyleLevel(PKTrait::DeadBall); // free kicks
	}
	Err *= P->FatigueError();"""),
("""			Speed *= ShotType == EPKShotType::Power ? 1.1f : 1.12f;""", """			Speed *= ShotType == EPKShotType::Power ? 1.f + 0.1f * P->StyleLevel(PKTrait::PowerShot) : 1.f + 0.12f * P->StyleLevel(PKTrait::PowerShot);"""),
# skills
("""	const float Ctrl = FMath::Clamp(P->Attr.Control + (P->HasTrait(PKTrait::Technical) ? 0.05f : 0.f), 0.f, 1.f);
	// A poor dribbler sometimes over-hits it.""", """	const float Ctrl = FMath::Clamp(P->Attr.Control + (P->HasTrait(PKTrait::Technical) ? 0.05f : 0.f) + 0.07f * P->StyleLevel(PKTrait::Trickster), 0.f, 1.f);
	// A poor dribbler sometimes over-hits it."""),
# duels
("""	const float CarrierPower = (0.6f + 0.8f * Carrier->Attr.Physical + (Carrier->Stance == EPKStance::Shield ? 0.3f : 0.f)) * FMath::FRandRange(0.6f, 1.2f);
	const float ChallengerPower = (0.6f + 0.8f * Challenger->Attr.Physical) * FMath::Lerp(0.85f, 1.15f, Challenger->Attr.Defending) * (bBarge ? 1.15f : 1.f) * FMath::FRandRange(0.6f, 1.2f);""",
"""	const float CarrierPower = (0.6f + 0.8f * Carrier->Attr.Physical + 0.15f * Carrier->StyleLevel(PKTrait::Bruiser) + (Carrier->Stance == EPKStance::Shield ? 0.3f : 0.f)) * FMath::FRandRange(0.6f, 1.2f);
	const float ChallengerPower = (0.6f + 0.8f * Challenger->Attr.Physical + 0.15f * Challenger->StyleLevel(PKTrait::Bruiser)) * FMath::Lerp(0.85f, 1.15f, Challenger->Attr.Defending) * (bBarge ? 1.15f : 1.f) * FMath::FRandRange(0.6f, 1.2f);"""),
# pickups: intercept, block, slide; aerial / acrobatic reach
("""		const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(BallLoc));
		const bool bOpponentBall = Ball->LastTouchTeam != P->Team;
""", """		const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(BallLoc));
		const bool bOpponentBall = Ball->LastTouchTeam != P->Team;
		if (!bKeeperHands && bOpponentBall)
		{
			Reach += Speed > 600.f ? 15.f * P->StyleLevel(PKTrait::Intercept) : 0.f;                       // cuts out passes
			Reach += Speed > 1500.f && BallLoc.Z < 140.f ? 25.f * P->StyleLevel(PKTrait::Block) : 0.f;     // throws himself in front of shots
		}
		if (bSliding)
		{
			Reach += 15.f * P->StyleLevel(PKTrait::SlideTackle);
		}
		const float AerialReach = 25.f * P->StyleLevel(PKTrait::Aerial);
"""),
("""		if (bRequested && !P->IsInAction() && P->StealCooldown <= 0.f && D < 135.f && BallLoc.Z > 85.f && BallLoc.Z < 240.f)""",
"""		if (bRequested && !P->IsInAction() && P->StealCooldown <= 0.f && D < 135.f + AerialReach && BallLoc.Z > 85.f && BallLoc.Z < 240.f + AerialReach * 0.6f)"""),
("""		if (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f && Ball->Velocity.Z < 50.f""",
"""		if (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f + 25.f * P->StyleLevel(PKTrait::Acrobatic) && Ball->Velocity.Z < 50.f"""),
("""		if (!bKeeperHands && BallLoc.Z > 120.f && BallLoc.Z < 230.f && D < 70.f && Ball->Velocity.Z < 0.f && P->StealCooldown <= 0.f)""",
"""		if (!bKeeperHands && BallLoc.Z > 120.f && BallLoc.Z < 230.f + AerialReach * 0.6f && D < 70.f + AerialReach && Ball->Velocity.Z < 0.f && P->StealCooldown <= 0.f)"""),
("""			QueueTouch(P, EPKAction::Header, Toward * FMath::FRandRange(700.f, 1100.f) + FVector(0.f, 0.f, 250.f), true);""",
"""			QueueTouch(P, EPKAction::Header, Toward * FMath::FRandRange(700.f, 1100.f) * (1.f + 0.1f * P->StyleLevel(PKTrait::Aerial)) + FVector(0.f, 0.f, 250.f), true);"""),
])

# Player card keeps its layout: at most three PlayStyles on the line.
patch("PKHUD.cpp", [
("""		const FString Traits = H->TraitNames();""", """		const FString Traits = H->TraitNames(3);"""),
])

# Team sheet detail: PlayStyles (+) and AcceleRATE
patch("PKMenu.cpp", [
("""	const FString Traits = PKTeams::TraitNames(Pl.Traits);""",
"""	const FString Traits = PKTeams::TraitNames(Pl.Traits, Pl.TraitsPlus) + (Pl.Pos == EPKPos::GK ? FString() : FString::Printf(TEXT("%sAcceleRATE: %s"), Pl.Traits ? TEXT("   |   ") : TEXT(""), APKFootballer::AccelName(APKFootballer::AccelTypeFor(Pl.Attr))));"""),
])
print("layer9 ok")
