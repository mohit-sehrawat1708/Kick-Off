#include "PKTeamData.h"
#include "Math/RandomStream.h"

namespace
{
	using P = EPKPos;

	// Team frame: x -1 own goal .. +1 opponent goal; y > 0 is the left side when attacking.
	const TArray<FPKFormation> GFormations = {
		{ TEXT("4-4-2"), {
			{ TEXT("GK"), P::GK, FVector2D(-0.95f, 0.f) },
			{ TEXT("RB"), P::DEF, FVector2D(-0.62f, -0.62f) },
			{ TEXT("RCB"), P::DEF, FVector2D(-0.70f, -0.22f) },
			{ TEXT("LCB"), P::DEF, FVector2D(-0.70f, 0.22f) },
			{ TEXT("LB"), P::DEF, FVector2D(-0.62f, 0.62f) },
			{ TEXT("RM"), P::MID, FVector2D(-0.28f, -0.62f) },
			{ TEXT("RCM"), P::MID, FVector2D(-0.36f, -0.18f) },
			{ TEXT("LCM"), P::MID, FVector2D(-0.36f, 0.18f) },
			{ TEXT("LM"), P::MID, FVector2D(-0.28f, 0.62f) },
			{ TEXT("RS"), P::FWD, FVector2D(-0.04f, -0.14f) },
			{ TEXT("LS"), P::FWD, FVector2D(0.0f, 0.14f) } } },
		{ TEXT("4-3-3"), {
			{ TEXT("GK"), P::GK, FVector2D(-0.95f, 0.f) },
			{ TEXT("RB"), P::DEF, FVector2D(-0.62f, -0.62f) },
			{ TEXT("RCB"), P::DEF, FVector2D(-0.70f, -0.22f) },
			{ TEXT("LCB"), P::DEF, FVector2D(-0.70f, 0.22f) },
			{ TEXT("LB"), P::DEF, FVector2D(-0.62f, 0.62f) },
			{ TEXT("RCM"), P::MID, FVector2D(-0.34f, -0.32f) },
			{ TEXT("CM"), P::MID, FVector2D(-0.44f, 0.f) },
			{ TEXT("LCM"), P::MID, FVector2D(-0.34f, 0.32f) },
			{ TEXT("RW"), P::FWD, FVector2D(-0.06f, -0.58f) },
			{ TEXT("LW"), P::FWD, FVector2D(-0.06f, 0.58f) },
			{ TEXT("ST"), P::FWD, FVector2D(0.0f, 0.f) } } },
		{ TEXT("4-2-3-1"), {
			{ TEXT("GK"), P::GK, FVector2D(-0.95f, 0.f) },
			{ TEXT("RB"), P::DEF, FVector2D(-0.62f, -0.62f) },
			{ TEXT("RCB"), P::DEF, FVector2D(-0.70f, -0.22f) },
			{ TEXT("LCB"), P::DEF, FVector2D(-0.70f, 0.22f) },
			{ TEXT("LB"), P::DEF, FVector2D(-0.62f, 0.62f) },
			{ TEXT("RDM"), P::MID, FVector2D(-0.46f, -0.18f) },
			{ TEXT("LDM"), P::MID, FVector2D(-0.46f, 0.18f) },
			{ TEXT("RAM"), P::MID, FVector2D(-0.20f, -0.55f) },
			{ TEXT("LAM"), P::MID, FVector2D(-0.20f, 0.55f) },
			{ TEXT("CAM"), P::MID, FVector2D(-0.16f, 0.f) },
			{ TEXT("ST"), P::FWD, FVector2D(0.0f, 0.f) } } },
		{ TEXT("3-5-2"), {
			{ TEXT("GK"), P::GK, FVector2D(-0.95f, 0.f) },
			{ TEXT("RCB"), P::DEF, FVector2D(-0.68f, -0.40f) },
			{ TEXT("CB"), P::DEF, FVector2D(-0.74f, 0.f) },
			{ TEXT("LCB"), P::DEF, FVector2D(-0.68f, 0.40f) },
			{ TEXT("RWB"), P::MID, FVector2D(-0.32f, -0.70f) },
			{ TEXT("RCM"), P::MID, FVector2D(-0.38f, -0.25f) },
			{ TEXT("CDM"), P::MID, FVector2D(-0.48f, 0.f) },
			{ TEXT("LCM"), P::MID, FVector2D(-0.38f, 0.25f) },
			{ TEXT("LWB"), P::MID, FVector2D(-0.32f, 0.70f) },
			{ TEXT("RS"), P::FWD, FVector2D(-0.04f, -0.14f) },
			{ TEXT("LS"), P::FWD, FVector2D(0.0f, 0.14f) } } },
		{ TEXT("5-3-2"), {
			{ TEXT("GK"), P::GK, FVector2D(-0.95f, 0.f) },
			{ TEXT("RWB"), P::DEF, FVector2D(-0.55f, -0.70f) },
			{ TEXT("RCB"), P::DEF, FVector2D(-0.70f, -0.35f) },
			{ TEXT("CB"), P::DEF, FVector2D(-0.75f, 0.f) },
			{ TEXT("LCB"), P::DEF, FVector2D(-0.70f, 0.35f) },
			{ TEXT("LWB"), P::DEF, FVector2D(-0.55f, 0.70f) },
			{ TEXT("RCM"), P::MID, FVector2D(-0.36f, -0.30f) },
			{ TEXT("CM"), P::MID, FVector2D(-0.42f, 0.f) },
			{ TEXT("LCM"), P::MID, FVector2D(-0.36f, 0.30f) },
			{ TEXT("RS"), P::FWD, FVector2D(-0.04f, -0.14f) },
			{ TEXT("LS"), P::FWD, FVector2D(0.0f, 0.14f) } } },
	};

	const TCHAR* FirstNames[] = { TEXT("Leo"), TEXT("Marco"), TEXT("Kai"), TEXT("Theo"), TEXT("Mateo"), TEXT("Luca"), TEXT("Noah"), TEXT("Rafael"),
		TEXT("Jonas"), TEXT("Ilya"), TEXT("Arjun"), TEXT("Kenji"), TEXT("Samuel"), TEXT("Diego"), TEXT("Tomas"), TEXT("Yusuf"), TEXT("Omar"),
		TEXT("Felix"), TEXT("Hugo"), TEXT("Ravi"), TEXT("Andre"), TEXT("Elias"), TEXT("Nico"), TEXT("Malik"), TEXT("Victor"), TEXT("Bruno"),
		TEXT("Emil"), TEXT("Sami"), TEXT("Joel"), TEXT("Tariq"), TEXT("Rohan"), TEXT("Iker") };
	const TCHAR* LastNames[] = { TEXT("Varga"), TEXT("Okafor"), TEXT("Lindqvist"), TEXT("Moreau"), TEXT("Castell"), TEXT("Brandt"), TEXT("Silvano"),
		TEXT("Novak"), TEXT("Haddad"), TEXT("Kowal"), TEXT("Mensah"), TEXT("Ferraro"), TEXT("Albrecht"), TEXT("Duarte"), TEXT("Petrov"),
		TEXT("Nakamura"), TEXT("Quinn"), TEXT("Rosario"), TEXT("Halden"), TEXT("Ibarra"), TEXT("Kessler"), TEXT("Lobo"), TEXT("Marin"),
		TEXT("Oyelaran"), TEXT("Pires"), TEXT("Rask"), TEXT("Sato"), TEXT("Tavares"), TEXT("Vidal"), TEXT("Wexley"), TEXT("Arora"), TEXT("Dimas"),
		TEXT("Abara"), TEXT("Adeyemi"), TEXT("Aguado"), TEXT("Ahlberg"), TEXT("Akande"), TEXT("Alcaraz"), TEXT("Almeida"), TEXT("Amari"), TEXT("Ambrose"), TEXT("Andrade"),
		TEXT("Arnaud"), TEXT("Asante"), TEXT("Aubert"), TEXT("Avelar"), TEXT("Bakker"), TEXT("Balogun"), TEXT("Barros"), TEXT("Bastos"), TEXT("Beaumont"), TEXT("Becker"),
		TEXT("Bellamy"), TEXT("Benitez"), TEXT("Bergstrom"), TEXT("Bianchi"), TEXT("Blanco"), TEXT("Boateng"), TEXT("Bonnet"), TEXT("Borges"), TEXT("Bouchard"), TEXT("Brennan"),
		TEXT("Caldwell"), TEXT("Camara"), TEXT("Campos"), TEXT("Carrasco"), TEXT("Castro"), TEXT("Cerny"), TEXT("Chevalier"), TEXT("Cisse"), TEXT("Coelho"), TEXT("Conti"),
		TEXT("Corbin"), TEXT("Correa"), TEXT("Costa"), TEXT("Dahl"), TEXT("Dalton"), TEXT("Delacroix"), TEXT("Demir"), TEXT("Diallo"), TEXT("Dimitrov"), TEXT("Dorsey"),
		TEXT("Dubois"), TEXT("Durand"), TEXT("Eriksen"), TEXT("Espinoza"), TEXT("Esteves"), TEXT("Falcao"), TEXT("Faure"), TEXT("Ferreira"), TEXT("Fischer"), TEXT("Fofana"),
		TEXT("Fontaine"), TEXT("Forde"), TEXT("Franco"), TEXT("Galvez"), TEXT("Garnier"), TEXT("Gaspar"), TEXT("Gerrard"), TEXT("Gomes"), TEXT("Grant"), TEXT("Gruber"),
		TEXT("Guerra"), TEXT("Haaland"), TEXT("Halloran"), TEXT("Hartmann"), TEXT("Hayes"), TEXT("Herrera"), TEXT("Holm"), TEXT("Horvat"), TEXT("Idowu"), TEXT("Iversen"),
		TEXT("Jansen"), TEXT("Jovic"), TEXT("Kaiser"), TEXT("Kamara"), TEXT("Keller"), TEXT("Kovac"), TEXT("Krause"), TEXT("Kruger"), TEXT("Laurent"), TEXT("Leclerc"),
		TEXT("Lehmann"), TEXT("Lindgren"), TEXT("Lopes"), TEXT("Lorenzo"), TEXT("Lundberg"), TEXT("Machado"), TEXT("Madsen"), TEXT("Maier"), TEXT("Marchetti"), TEXT("Martel"),
		TEXT("Matic"), TEXT("Medina"), TEXT("Mendes"), TEXT("Mercer"), TEXT("Meyer"), TEXT("Molina"), TEXT("Monteiro"), TEXT("Morris"), TEXT("Muller"), TEXT("Navarro"),
		TEXT("Neves"), TEXT("Nielsen"), TEXT("Nunez"), TEXT("Obi"), TEXT("Okonkwo"), TEXT("Oliveira"), TEXT("Olsen"), TEXT("Ortega"), TEXT("Osei"), TEXT("Pacheco"),
		TEXT("Pardo"), TEXT("Pavlov"), TEXT("Pereira"), TEXT("Ramos"), TEXT("Reyes"), TEXT("Ribeiro"), TEXT("Richter"), TEXT("Rios"), TEXT("Rocha"), TEXT("Romero"),
		TEXT("Rossi"), TEXT("Sabo"), TEXT("Salas"), TEXT("Santos"), TEXT("Sarr"), TEXT("Schmidt"), TEXT("Serra"), TEXT("Sousa"), TEXT("Stanic"), TEXT("Strand"),
		TEXT("Suarez"), TEXT("Sylla"), TEXT("Teixeira"), TEXT("Thorsen"), TEXT("Toure"), TEXT("Traore"), TEXT("Valdez"), TEXT("Vasquez"), TEXT("Vieira"), TEXT("Vogel"),
		TEXT("Wagner"), TEXT("Walsh"), TEXT("Weber"), TEXT("Wolff"), TEXT("Yilmaz"), TEXT("Zamora"), TEXT("Zelenko"), TEXT("Zidan")
	};

	float Rate(FRandomStream& R, float Base)
	{
		return FMath::Clamp(Base + R.FRandRange(-0.07f, 0.07f), 0.35f, 0.97f);
	}

	FPKAttributes MakeAttributes(FRandomStream& R, EPKPos Pos, float Level)
	{
		// Level: 0 = average club player, +/- shifts the whole profile.
		FPKAttributes A;
		auto Set = [&](float Pa, float Co, float Ps, float Sh, float De, float Ph)
		{
			A.Pace = Rate(R, Pa + Level); A.Control = Rate(R, Co + Level); A.Passing = Rate(R, Ps + Level);
			A.Shooting = Rate(R, Sh + Level); A.Defending = Rate(R, De + Level); A.Physical = Rate(R, Ph + Level);
		};
		switch (Pos)
		{
		case EPKPos::GK:  Set(0.55f, 0.55f, 0.60f, 0.35f, 0.80f, 0.75f); break;
		case EPKPos::DEF: Set(0.68f, 0.62f, 0.66f, 0.45f, 0.82f, 0.80f); break;
		case EPKPos::MID: Set(0.72f, 0.78f, 0.82f, 0.66f, 0.64f, 0.70f); break;
		default:          Set(0.84f, 0.80f, 0.70f, 0.84f, 0.40f, 0.72f); break;
		}
		return A;
	}

	uint32 MakeTraits(FRandomStream& R, EPKPos Pos)
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
	}

	// How good a player is at the job of a position (used to pick the best XI).
	float PositionalScore(const FPKAttributes& A, EPKPos Pos)
	{
		switch (Pos)
		{
		case EPKPos::GK:  return A.Defending * 0.45f + A.Physical * 0.3f + A.Passing * 0.15f + A.Pace * 0.1f;
		case EPKPos::DEF: return A.Defending * 0.4f + A.Physical * 0.25f + A.Pace * 0.15f + A.Passing * 0.15f + A.Control * 0.05f;
		case EPKPos::MID: return A.Passing * 0.35f + A.Control * 0.3f + A.Shooting * 0.1f + A.Defending * 0.1f + A.Pace * 0.1f + A.Physical * 0.05f;
		default:          return A.Shooting * 0.4f + A.Pace * 0.25f + A.Control * 0.2f + A.Physical * 0.1f + A.Passing * 0.05f;
		}
	}
}

namespace PKTeams
{
	const TArray<FPKFormation>& Formations()
	{
		return GFormations;
	}

	const TCHAR* PosName(EPKPos Pos)
	{
		switch (Pos)
		{
		case EPKPos::GK:  return TEXT("GK");
		case EPKPos::DEF: return TEXT("DEF");
		case EPKPos::MID: return TEXT("MID");
		default:          return TEXT("FWD");
		}
	}

	EPKRole RoleFor(EPKPos Pos)
	{
		switch (Pos)
		{
		case EPKPos::GK:  return EPKRole::Keeper;
		case EPKPos::DEF: return EPKRole::Defender;
		case EPKPos::MID: return EPKRole::Midfielder;
		default:          return EPKRole::Forward;
		}
	}

	float PositionFit(EPKPos Player, EPKPos Slot)
	{
		if (Player == Slot)
		{
			return 1.f;
		}
		if (Player == EPKPos::GK || Slot == EPKPos::GK)
		{
			return 0.55f;
		}
		return FMath::Abs((int32)Player - (int32)Slot) == 1 ? 0.92f : 0.82f;
	}

	FPKAttributes AttributesInSlot(const FPKSquadPlayer& Player, EPKPos Slot)
	{
		const float Fit = PositionFit(Player.Pos, Slot);
		FPKAttributes A = Player.Attr;
		A.Pace *= Fit; A.Control *= Fit; A.Passing *= Fit; A.Shooting *= Fit; A.Defending *= Fit; A.Physical *= Fit;
		return A;
	}

	int32 RatingInSlot(const FPKSquadPlayer& Player, EPKPos Slot)
	{
		return AttributesInSlot(Player, Slot).Overall();
	}

	TArray<int32> AutoPick(const FPKClub& Club, int32 FormationIndex)
	{
		const FPKFormation& F = GFormations[FMath::Clamp(FormationIndex, 0, GFormations.Num() - 1)];
		TArray<int32> Lineup;
		Lineup.Init(INDEX_NONE, 11);
		TArray<bool> Used;
		Used.Init(false, Club.Squad.Num());
		// Fill the slots in order of how specialised they are: keeper, then the rest.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 s = 0; s < 11; ++s)
			{
				if (Lineup[s] != INDEX_NONE || (Pass == 0 && F.Slots[s].Pos != EPKPos::GK))
				{
					continue;
				}
				int32 Best = INDEX_NONE;
				float BestScore = -1.f;
				for (int32 i = 0; i < Club.Squad.Num(); ++i)
				{
					if (Used[i])
					{
						continue;
					}
					const float Score = PositionalScore(Club.Squad[i].Attr, F.Slots[s].Pos) * PositionFit(Club.Squad[i].Pos, F.Slots[s].Pos);
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = i;
					}
				}
				Lineup[s] = Best;
				if (Best != INDEX_NONE)
				{
					Used[Best] = true;
				}
			}
		}
		return Lineup;
	}

	int32 TeamRating(const FPKClub& Club, int32 FormationIndex, const TArray<int32>& Lineup)
	{
		const FPKFormation& F = GFormations[FMath::Clamp(FormationIndex, 0, GFormations.Num() - 1)];
		int32 Sum = 0, Count = 0;
		for (int32 s = 0; s < 11 && s < Lineup.Num(); ++s)
		{
			if (Club.Squad.IsValidIndex(Lineup[s]))
			{
				Sum += RatingInSlot(Club.Squad[Lineup[s]], F.Slots[s].Pos);
				++Count;
			}
		}
		return Count ? FMath::RoundToInt((float)Sum / Count) : 0;
	}

	FString TraitNames(uint32 Traits, uint32 Plus)
	{
		return PKTrait::Names(Traits, Plus);
	}

	FLinearColor Lin(float R, float G, float B)
	{
		return FLinearColor(FColor((uint8)(R * 255.f), (uint8)(G * 255.f), (uint8)(B * 255.f)));
	}

	// Appearance from the player's own seed: skin, hair, beard, boots, and a build that follows his strength.
	// Kits: the player's club has eight to choose from; every opponent has home, away and third kits in different
	// colours and designs (patterns: 0 plain, 1 stripes, 2 hoops, 3 sash, 4 halves, 5 pinstripes, 6 chevron,
	// 7 quarters, 8 chest band, 9 fade, 10 contrast shoulders, 11 centre stripe).
	struct FKitDef
	{
		const TCHAR* Name;
		int32 Pattern;
		float P[3], S[3], T[3], Sh[3], So[3], N[3];
		float K[3], KS[3], KSh[3];
	};

	FPKKit FromDef(const FKitDef& D)
	{
		FPKKit K;
		K.Name = D.Name;
		K.Pattern = D.Pattern;
		K.Primary = Lin(D.P[0], D.P[1], D.P[2]);
		K.Secondary = Lin(D.S[0], D.S[1], D.S[2]);
		K.Trim = Lin(D.T[0], D.T[1], D.T[2]);
		K.Shorts = Lin(D.Sh[0], D.Sh[1], D.Sh[2]);
		K.Socks = Lin(D.So[0], D.So[1], D.So[2]);
		K.Number = Lin(D.N[0], D.N[1], D.N[2]);
		K.KeeperPrimary = Lin(D.K[0], D.K[1], D.K[2]);
		K.KeeperSecondary = Lin(D.KS[0], D.KS[1], D.KS[2]);
		K.KeeperShorts = Lin(D.KSh[0], D.KSh[1], D.KSh[2]);
		return K;
	}

	TArray<FPKKit> MakeKits(int32 Club)
	{
		// name, pattern, shirt, second colour, trim, shorts, socks, number, keeper shirt / second / shorts
		static const FKitDef Kings[] = {
			{ TEXT("Home: royal pinstripes"), 5, {0.08f, 0.30f, 0.85f}, {0.03f, 0.10f, 0.40f}, {0.95f, 0.95f, 0.95f}, {0.95f, 0.95f, 0.97f}, {0.08f, 0.30f, 0.85f}, {0.97f, 0.97f, 0.97f}, {0.15f, 0.75f, 0.25f}, {0.05f, 0.35f, 0.10f}, {0.08f, 0.08f, 0.10f} },
			{ TEXT("Away: white chevron"), 6, {0.96f, 0.96f, 0.96f}, {0.08f, 0.30f, 0.85f}, {0.05f, 0.10f, 0.35f}, {0.05f, 0.10f, 0.35f}, {0.96f, 0.96f, 0.96f}, {0.05f, 0.10f, 0.35f}, {0.15f, 0.75f, 0.25f}, {0.05f, 0.35f, 0.10f}, {0.08f, 0.08f, 0.10f} },
			{ TEXT("Third: black and volt"), 8, {0.08f, 0.08f, 0.09f}, {0.72f, 0.95f, 0.15f}, {0.72f, 0.95f, 0.15f}, {0.08f, 0.08f, 0.09f}, {0.08f, 0.08f, 0.09f}, {0.72f, 0.95f, 0.15f}, {0.95f, 0.45f, 0.10f}, {0.40f, 0.15f, 0.02f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Sky blue quarters"), 7, {0.45f, 0.72f, 0.95f}, {0.05f, 0.12f, 0.38f}, {0.96f, 0.96f, 0.96f}, {0.05f, 0.12f, 0.38f}, {0.45f, 0.72f, 0.95f}, {0.96f, 0.96f, 0.96f}, {0.15f, 0.75f, 0.25f}, {0.05f, 0.35f, 0.10f}, {0.08f, 0.08f, 0.10f} },
			{ TEXT("Teal fade"), 9, {0.00f, 0.55f, 0.55f}, {0.00f, 0.22f, 0.32f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.00f, 0.55f, 0.55f}, {0.96f, 0.96f, 0.96f}, {0.95f, 0.45f, 0.10f}, {0.40f, 0.15f, 0.02f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Purple and gold"), 11, {0.38f, 0.15f, 0.62f}, {0.95f, 0.75f, 0.20f}, {0.95f, 0.75f, 0.20f}, {0.20f, 0.07f, 0.35f}, {0.38f, 0.15f, 0.62f}, {0.95f, 0.75f, 0.20f}, {0.15f, 0.75f, 0.25f}, {0.05f, 0.35f, 0.10f}, {0.08f, 0.08f, 0.10f} },
			{ TEXT("Sunset orange"), 10, {0.98f, 0.48f, 0.12f}, {0.05f, 0.10f, 0.30f}, {0.96f, 0.96f, 0.96f}, {0.05f, 0.10f, 0.30f}, {0.98f, 0.48f, 0.12f}, {0.05f, 0.10f, 0.30f}, {0.15f, 0.75f, 0.25f}, {0.05f, 0.35f, 0.10f}, {0.08f, 0.08f, 0.10f} },
			{ TEXT("Retro green hoops"), 2, {0.05f, 0.50f, 0.22f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.05f, 0.50f, 0.22f}, {0.05f, 0.08f, 0.20f}, {0.95f, 0.45f, 0.10f}, {0.40f, 0.15f, 0.02f}, {0.08f, 0.08f, 0.08f} },
		};
		static const FKitDef Rovers[] = {
			{ TEXT("Home: red and white hoops"), 2, {0.80f, 0.06f, 0.08f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.80f, 0.06f, 0.08f}, {0.08f, 0.08f, 0.10f}, {0.98f, 0.82f, 0.10f}, {0.40f, 0.32f, 0.02f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Away: navy, red band"), 8, {0.06f, 0.09f, 0.25f}, {0.80f, 0.06f, 0.08f}, {0.80f, 0.06f, 0.08f}, {0.06f, 0.09f, 0.25f}, {0.06f, 0.09f, 0.25f}, {0.96f, 0.96f, 0.96f}, {0.98f, 0.82f, 0.10f}, {0.40f, 0.32f, 0.02f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Third: lime fade"), 9, {0.62f, 0.88f, 0.15f}, {0.10f, 0.45f, 0.15f}, {0.08f, 0.08f, 0.09f}, {0.08f, 0.08f, 0.09f}, {0.62f, 0.88f, 0.15f}, {0.08f, 0.08f, 0.09f}, {0.45f, 0.15f, 0.65f}, {0.15f, 0.04f, 0.25f}, {0.08f, 0.08f, 0.08f} },
		};
		static const FKitDef Crimson[] = {
			{ TEXT("Home: crimson and black stripes"), 1, {0.62f, 0.03f, 0.10f}, {0.07f, 0.07f, 0.08f}, {0.95f, 0.75f, 0.20f}, {0.07f, 0.07f, 0.08f}, {0.07f, 0.07f, 0.08f}, {0.95f, 0.78f, 0.25f}, {0.20f, 0.75f, 0.85f}, {0.05f, 0.30f, 0.40f}, {0.05f, 0.20f, 0.30f} },
			{ TEXT("Away: white, crimson chevron"), 6, {0.96f, 0.96f, 0.96f}, {0.62f, 0.03f, 0.10f}, {0.95f, 0.75f, 0.20f}, {0.96f, 0.96f, 0.96f}, {0.96f, 0.96f, 0.96f}, {0.62f, 0.03f, 0.10f}, {0.20f, 0.75f, 0.85f}, {0.05f, 0.30f, 0.40f}, {0.05f, 0.20f, 0.30f} },
			{ TEXT("Third: grey and gold quarters"), 7, {0.45f, 0.47f, 0.50f}, {0.10f, 0.10f, 0.12f}, {0.95f, 0.75f, 0.20f}, {0.12f, 0.12f, 0.14f}, {0.45f, 0.47f, 0.50f}, {0.95f, 0.78f, 0.25f}, {0.20f, 0.75f, 0.85f}, {0.05f, 0.30f, 0.40f}, {0.05f, 0.20f, 0.30f} },
		};
		static const FKitDef Rojo[] = {
			{ TEXT("Home: red, white sash"), 3, {0.85f, 0.12f, 0.12f}, {0.97f, 0.97f, 0.97f}, {0.97f, 0.97f, 0.97f}, {0.85f, 0.12f, 0.12f}, {0.97f, 0.97f, 0.97f}, {0.97f, 0.97f, 0.97f}, {0.45f, 0.15f, 0.65f}, {0.15f, 0.04f, 0.25f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Away: black, red sash"), 3, {0.08f, 0.08f, 0.09f}, {0.85f, 0.12f, 0.12f}, {0.85f, 0.12f, 0.12f}, {0.08f, 0.08f, 0.09f}, {0.08f, 0.08f, 0.09f}, {0.97f, 0.97f, 0.97f}, {0.45f, 0.15f, 0.65f}, {0.15f, 0.04f, 0.25f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Third: sky, white centre stripe"), 11, {0.50f, 0.75f, 0.95f}, {0.97f, 0.97f, 0.97f}, {0.97f, 0.97f, 0.97f}, {0.97f, 0.97f, 0.97f}, {0.50f, 0.75f, 0.95f}, {0.05f, 0.10f, 0.30f}, {0.45f, 0.15f, 0.65f}, {0.15f, 0.04f, 0.25f}, {0.08f, 0.08f, 0.08f} },
		};
		static const FKitDef Scarlet[] = {
			{ TEXT("Home: scarlet and maroon halves"), 4, {0.95f, 0.18f, 0.12f}, {0.40f, 0.05f, 0.08f}, {0.98f, 0.85f, 0.75f}, {0.40f, 0.05f, 0.08f}, {0.95f, 0.18f, 0.12f}, {0.98f, 0.95f, 0.90f}, {0.65f, 0.90f, 0.15f}, {0.25f, 0.40f, 0.05f}, {0.10f, 0.10f, 0.10f} },
			{ TEXT("Away: yellow, maroon shoulders"), 10, {0.98f, 0.82f, 0.15f}, {0.40f, 0.05f, 0.08f}, {0.40f, 0.05f, 0.08f}, {0.40f, 0.05f, 0.08f}, {0.98f, 0.82f, 0.15f}, {0.40f, 0.05f, 0.08f}, {0.45f, 0.15f, 0.65f}, {0.15f, 0.04f, 0.25f}, {0.08f, 0.08f, 0.08f} },
			{ TEXT("Third: teal pinstripes"), 5, {0.00f, 0.50f, 0.50f}, {0.95f, 0.95f, 0.95f}, {0.95f, 0.95f, 0.95f}, {0.00f, 0.25f, 0.28f}, {0.00f, 0.50f, 0.50f}, {0.95f, 0.95f, 0.95f}, {0.65f, 0.90f, 0.15f}, {0.25f, 0.40f, 0.05f}, {0.10f, 0.10f, 0.10f} },
		};
		TArray<FPKKit> Kits;
		auto Add = [&Kits](const FKitDef* Defs, int32 Num)
		{
			for (int32 i = 0; i < Num; ++i)
			{
				Kits.Add(FromDef(Defs[i]));
			}
		};
		switch (Club)
		{
		case 0: Add(Kings, UE_ARRAY_COUNT(Kings)); break;
		case 1: Add(Rovers, UE_ARRAY_COUNT(Rovers)); break;
		case 2: Add(Crimson, UE_ARRAY_COUNT(Crimson)); break;
		case 3: Add(Rojo, UE_ARRAY_COUNT(Rojo)); break;
		default: Add(Scarlet, UE_ARRAY_COUNT(Scarlet)); break;
		}
		return Kits;
	}

	FPKLook MakeLook(FRandomStream& R, const FPKAttributes& A)
	{
		static const FLinearColor Skins[] = { Lin(0.96f, 0.80f, 0.69f), Lin(0.90f, 0.72f, 0.58f), Lin(0.80f, 0.60f, 0.45f), Lin(0.68f, 0.48f, 0.33f),
			Lin(0.55f, 0.37f, 0.25f), Lin(0.42f, 0.27f, 0.18f), Lin(0.30f, 0.19f, 0.13f) };
		static const FLinearColor Hairs[] = { Lin(0.05f, 0.04f, 0.035f), Lin(0.12f, 0.08f, 0.05f), Lin(0.25f, 0.16f, 0.09f), Lin(0.42f, 0.28f, 0.15f),
			Lin(0.78f, 0.62f, 0.38f), Lin(0.48f, 0.20f, 0.09f) };
		static const FLinearColor BootColors[] = { Lin(0.06f, 0.06f, 0.06f), Lin(0.92f, 0.92f, 0.92f), Lin(0.75f, 0.95f, 0.15f), Lin(1.f, 0.42f, 0.1f),
			Lin(0.15f, 0.45f, 0.95f), Lin(0.9f, 0.12f, 0.2f), Lin(0.95f, 0.85f, 0.2f) };
		FPKLook L;
		const int32 SkinI = R.RandRange(0, UE_ARRAY_COUNT(Skins) - 1);
		L.Skin = Skins[SkinI];
		// Dark hair is the most common; light hair goes with lighter skin.
		int32 HairI = R.RandRange(0, 3);
		if (SkinI <= 2 && R.FRand() < 0.3f)
		{
			HairI = R.RandRange(3, 5);
		}
		L.Hair = Hairs[HairI];
		const float S = R.FRand();
		L.HairStyle = S < 0.f ? 0 : (S < 0.18f ? 1 : (S < 0.63f ? 2 : (S < 0.88f ? 3 : 4)));
		const float B = R.FRand();
		L.Beard = B < 0.55f ? 0.f : (B < 0.8f ? 0.45f : 0.9f);
		L.Boots = BootColors[R.RandRange(0, UE_ARRAY_COUNT(BootColors) - 1)];
		L.bBootFlash = R.FRand() < 0.7f;
		L.Height = FMath::Clamp(0.965f + 0.06f * R.FRand() + 0.02f * (A.Physical - 0.7f), 0.95f, 1.04f);
		L.Build = FMath::Clamp(0.95f + 0.1f * A.Physical + R.FRandRange(-0.02f, 0.02f), 0.95f, 1.06f);
		return L;
	}

	TArray<FPKClub> MakeClubs()
	{
		struct FDef { const TCHAR* Name; const TCHAR* Short; float Quality; int32 Style; int32 Formation; };
		// Style: 0 Balanced, 1 Possession, 2 Counter, 3 Long Ball, 4 High Press.
		const FDef Defs[] = {
			{ TEXT("Pitch Kings FC"), TEXT("PKF"), 0.00f, 0, 0 },
			{ TEXT("Red Rovers"), TEXT("RRV"), -0.02f, 2, 0 },
			{ TEXT("Crimson Athletic"), TEXT("CRA"), 0.03f, 1, 1 },
			{ TEXT("Union Rojo"), TEXT("URO"), -0.05f, 3, 4 },
			{ TEXT("Scarlet City"), TEXT("SCC"), 0.06f, 4, 2 },
		};
		// Squad shape and shirt numbers: 2 GK, 6 DEF, 6 MID, 4 FWD.
		const EPKPos Shape[18] = { P::GK, P::GK, P::DEF, P::DEF, P::DEF, P::DEF, P::DEF, P::DEF,
			P::MID, P::MID, P::MID, P::MID, P::MID, P::MID, P::FWD, P::FWD, P::FWD, P::FWD };
		const int32 Numbers[18] = { 1, 13, 2, 3, 4, 5, 12, 15, 6, 8, 10, 14, 16, 18, 7, 9, 11, 17 };
		// First choices are a little better than the bench.
		const float Depth[18] = { 0.03f, -0.05f, 0.03f, 0.02f, 0.02f, 0.01f, -0.04f, -0.05f,
			0.03f, 0.03f, 0.02f, 0.01f, -0.04f, -0.05f, 0.03f, 0.03f, -0.03f, -0.05f };

		TArray<FPKClub> Clubs;
		TSet<FString> Surnames;
		int32 Next = 0;
		for (int32 c = 0; c < UE_ARRAY_COUNT(Defs); ++c)
		{
			FRandomStream R(9173 + c * 101);
			FPKClub Club;
			Club.Name = Defs[c].Name;
			Club.Short = Defs[c].Short;
			Club.Quality = Defs[c].Quality;
			Club.Style = Defs[c].Style;
			Club.Formation = Defs[c].Formation;
			Club.Kits = MakeKits(c);
			Club.Kit = Club.Kits[0];
			TSet<FString> Taken;
			for (int32 i = 0; i < 18; ++i)
			{
				FPKSquadPlayer Pl;
				do
				{
					Pl.Name = FString::Printf(TEXT("%c. %s"), FirstNames[R.RandRange(0, UE_ARRAY_COUNT(FirstNames) - 1)][0], LastNames[R.RandRange(0, UE_ARRAY_COUNT(LastNames) - 1)]);
				} while (Taken.Contains(Pl.Name));
				Taken.Add(Pl.Name);
				Pl.Number = Numbers[i];
				Pl.Pos = Shape[i];
				Pl.Attr = MakeAttributes(R, Pl.Pos, Club.Quality + Depth[i]);
				{
					FRandomStream LookSeed(R.GetCurrentSeed() ^ (i * 7919));
					Pl.Look = MakeLook(LookSeed, Pl.Attr);
				}
				Pl.Traits = MakeTraits(R, Pl.Pos);
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
				}
				Club.Squad.Add(Pl);
			}
			// One player per surname across all the clubs: later duplicates take the next unused surname (no random draws,
			// so the ratings generated above are unchanged).
			{
				for (FPKSquadPlayer& Pl : Club.Squad)
				{
					FString Surname = Pl.Name.Mid(3);
					while (Surnames.Contains(Surname) && Next < UE_ARRAY_COUNT(LastNames))
					{
						Surname = LastNames[Next++];
					}
					Surnames.Add(Surname);
					Pl.Name = Pl.Name.Left(3) + Surname;
				}
			}
			Clubs.Add(Club);
		}
		return Clubs;
	}

	float KitClash(const FPKKit& A, const FPKKit& B)
	{
		// the colour a viewer reads as the shirt's: the first colour, plus the second in proportion to its share of the design
		auto Main = [](const FPKKit& K)
		{
			static const float Share[] = { 0.f, 0.5f, 0.5f, 0.2f, 0.5f, 0.1f, 0.2f, 0.5f, 0.25f, 0.45f, 0.3f, 0.25f };
			const float W = Share[FMath::Clamp(K.Pattern, 0, (int32)UE_ARRAY_COUNT(Share) - 1)];
			return FMath::Lerp(K.Primary.ToFColor(true).ReinterpretAsLinear(), K.Secondary.ToFColor(true).ReinterpretAsLinear(), W);
		};
		auto Dist = [](const FLinearColor& X, const FLinearColor& Y)
		{
			return FMath::Sqrt(FMath::Square(X.R - Y.R) + FMath::Square(X.G - Y.G) + FMath::Square(X.B - Y.B));
		};
		const float Shirt = Dist(Main(A), Main(B));
		const float Shorts = Dist(A.Shorts.ToFColor(true).ReinterpretAsLinear(), B.Shorts.ToFColor(true).ReinterpretAsLinear());
		// shirts decide it; similar shorts only tip a close call
		return FMath::Clamp(1.f - Shirt / 0.6f, 0.f, 1.f) * 0.8f + FMath::Clamp(1.f - Shorts / 0.6f, 0.f, 1.f) * 0.2f;
	}

	int32 ChooseKit(const FPKClub& Club, const FPKKit& Against)
	{
		int32 Best = 0;
		float BestClash = 2.f;
		for (int32 i = 0; i < Club.Kits.Num(); ++i)
		{
			const float C = KitClash(Club.Kits[i], Against);
			if (C < 0.45f)
			{
				return i; // the first that reads clearly different
			}
			if (C < BestClash)
			{
				Best = i;
				BestClash = C;
			}
		}
		return Best;
	}
}
