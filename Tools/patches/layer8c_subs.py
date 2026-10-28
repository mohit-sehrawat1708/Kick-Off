# Layer 8c: substitutions (pause menu page, max 5), computer substitutions at stoppages, tired AI saves its legs.
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
("""	void UpdateAdvantage(float Dt);""", """	void UpdateAdvantage(float Dt);
	// Substitutions: the person's from the pause menu, the computer's own at stoppages late on.
	struct FPKSubEntry
	{
		int32 Slot = -1;   // formation slot (on the pitch) or -1 (bench)
		int32 Squad = -1;  // squad index
		int32 Number = 0;
		FString Name;
		FString Pos;
		float Stamina = 1.f;
		int32 Rating = 0;
		bool bAvailable = true; // bench: not yet used / on the pitch: not sent off
	};
	void GetSubsInfo(int32 Team, TArray<FPKSubEntry>& OnPitch, TArray<FPKSubEntry>& Bench) const;
	bool MakeSub(int32 Team, int32 Slot, int32 SquadIndex);
	int32 SubsLeft(int32 Team) const { return MaxSubs - SubsUsed[FMath::Clamp(Team, 0, 1)]; }
	static constexpr int32 MaxSubs = 5;
	int32 SubsUsed[2] = { 0, 0 };
	TArray<int32> MatchLineup[2];  // squad index per slot, as on the pitch now
	TArray<int32> SubbedOff[2];    // squad indices that have left (can't come back)
	int32 MatchFormation[2] = { 0, 0 };
	float AISubTimer = 0.f;
	void UpdateAISubs(float Dt);
	void ScaleForDifficulty(int32 Team, FPKAttributes& Attr) const;"""),
])

patch("PKGameMode.cpp", [
# remember the line-ups when they are applied; share the difficulty scaling
("""		const FPKFormation& F = PKTeams::Formations()[FMath::Clamp(FormationIndex, 0, PKTeams::Formations().Num() - 1)];
		for (APKFootballer* P : Footballers)
		{""", """		const FPKFormation& F = PKTeams::Formations()[FMath::Clamp(FormationIndex, 0, PKTeams::Formations().Num() - 1)];
		MatchLineup[Team] = Lineup;
		MatchFormation[Team] = FMath::Clamp(FormationIndex, 0, PKTeams::Formations().Num() - 1);
		SubbedOff[Team].Reset();
		SubsUsed[Team] = 0;
		for (APKFootballer* P : Footballers)
		{"""),
("""			FPKAttributes Attr = PKTeams::AttributesInSlot(Pl, Slot.Pos);
			// Difficulty scales the computer team against a single player (not in 1v1 or spectate).
			if (Team == 1 && !bHumanTeam[1] && !bSpectate && !bAutoTest)
			{
				const float Scale = GI->Settings.Difficulty == 0 ? 0.85f : (GI->Settings.Difficulty == 2 ? 1.12f : 1.f);
				for (float* V : { &Attr.Pace, &Attr.Control, &Attr.Passing, &Attr.Shooting, &Attr.Defending, &Attr.Physical })
				{
					*V = FMath::Clamp(*V * Scale, 0.3f, 0.97f);
				}
			}""", """			FPKAttributes Attr = PKTeams::AttributesInSlot(Pl, Slot.Pos);
			ScaleForDifficulty(Team, Attr);"""),
("""void APKGameMode::SetupKickoff(int32 KickingTeam)
{""", """void APKGameMode::ScaleForDifficulty(int32 Team, FPKAttributes& Attr) const
{
	// Difficulty scales the computer team against a single player (not in 1v1 or spectate).
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (GI && Team == 1 && !bHumanTeam[1] && !bSpectate && !bAutoTest)
	{
		const float Scale = GI->Settings.Difficulty == 0 ? 0.85f : (GI->Settings.Difficulty == 2 ? 1.12f : 1.f);
		for (float* V : { &Attr.Pace, &Attr.Control, &Attr.Passing, &Attr.Shooting, &Attr.Defending, &Attr.Physical })
		{
			*V = FMath::Clamp(*V * Scale, 0.3f, 0.97f);
		}
	}
}

void APKGameMode::GetSubsInfo(int32 Team, TArray<FPKSubEntry>& OnPitch, TArray<FPKSubEntry>& Bench) const
{
	OnPitch.Reset();
	Bench.Reset();
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (!GI || GI->Clubs.Num() < 2)
	{
		return;
	}
	Team = FMath::Clamp(Team, 0, 1);
	const FPKClub& Club = Team == 0 ? GI->UserClubData() : GI->OpponentClubData();
	const FPKFormation& F = PKTeams::Formations()[MatchFormation[Team]];
	for (int32 Slot = 0; Slot < MatchLineup[Team].Num() && Slot < 11; ++Slot)
	{
		const int32 Sq = MatchLineup[Team][Slot];
		if (!Club.Squad.IsValidIndex(Sq))
		{
			continue;
		}
		FPKSubEntry E;
		E.Slot = Slot;
		E.Squad = Sq;
		E.Number = Club.Squad[Sq].Number;
		E.Name = Club.Squad[Sq].Name;
		E.Pos = F.Slots[Slot].Label;
		E.Rating = PKTeams::RatingInSlot(Club.Squad[Sq], F.Slots[Slot].Pos);
		for (const APKFootballer* P : Footballers)
		{
			if (P->Team == Team && P->Index == Slot)
			{
				E.Stamina = P->Stamina;
				E.bAvailable = !P->bSentOff;
			}
		}
		OnPitch.Add(E);
	}
	for (int32 Sq = 0; Sq < Club.Squad.Num(); ++Sq)
	{
		if (MatchLineup[Team].Contains(Sq))
		{
			continue;
		}
		FPKSubEntry E;
		E.Squad = Sq;
		E.Number = Club.Squad[Sq].Number;
		E.Name = Club.Squad[Sq].Name;
		E.Pos = PKTeams::PosName(Club.Squad[Sq].Pos);
		E.Rating = Club.Squad[Sq].Attr.Overall();
		E.bAvailable = !SubbedOff[Team].Contains(Sq);
		Bench.Add(E);
	}
}

bool APKGameMode::MakeSub(int32 Team, int32 Slot, int32 SquadIndex)
{
	Team = FMath::Clamp(Team, 0, 1);
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (!GI || GI->Clubs.Num() < 2 || SubsUsed[Team] >= MaxSubs || !MatchLineup[Team].IsValidIndex(Slot))
	{
		return false;
	}
	const FPKClub& Club = Team == 0 ? GI->UserClubData() : GI->OpponentClubData();
	if (!Club.Squad.IsValidIndex(SquadIndex) || MatchLineup[Team].Contains(SquadIndex) || SubbedOff[Team].Contains(SquadIndex))
	{
		return false;
	}
	APKFootballer* P = FindFootballer(Team, Slot);
	if (!P || P->bSentOff || P->IsInAction() || Ball->GetController() == P || Ball->GetHolder() == P)
	{
		return false; // not the one on the ball / in the middle of something
	}
	const FPKFormationSlot& S = PKTeams::Formations()[MatchFormation[Team]].Slots[Slot];
	const FPKSquadPlayer& In = Club.Squad[SquadIndex];
	if ((S.Pos == EPKPos::GK) != (In.Pos == EPKPos::GK))
	{
		return false; // keepers only for keepers
	}
	const int32 OffNumber = P->ShirtNumber;
	const FString OffName = P->PlayerName;
	FPKAttributes Attr = PKTeams::AttributesInSlot(In, S.Pos);
	ScaleForDifficulty(Team, Attr);
	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name);
	P->Stamina = P->MaxStamina = 1.f;
	P->YellowCards = 0;
	SubbedOff[Team].Add(MatchLineup[Team][Slot]);
	MatchLineup[Team][Slot] = SquadIndex;
	++SubsUsed[Team];
	ShowMessage(FString::Printf(TEXT("SUBSTITUTION %s:  ON #%d %s   OFF #%d %s"), PK::TeamName(Team), In.Number, *In.Name, OffNumber, *OffName), 2.5f);
	UE_LOG(LogPitchKings, Log, TEXT("Substitution %s: #%d %s on for #%d %s (%d left)"), PK::TeamName(Team), In.Number, *In.Name, OffNumber, *OffName, MaxSubs - SubsUsed[Team]);
	return true;
}

void APKGameMode::UpdateAISubs(float Dt)
{
	// The computer freshens up tired players at a stoppage in the second half (up to three changes).
	AISubTimer -= Dt;
	const bool bStoppage = Ball->IsPlaced() || (Ball->GetHolder() && !Ball->GetHolder()->IsKeeper()) || Phase == EPKPhase::GoalScored;
	if (AISubTimer > 0.f || !bStoppage || TimeLeft > MatchTotal * 0.45f)
	{
		return;
	}
	AISubTimer = 4.f;
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (!GI || GI->Clubs.Num() < 2)
	{
		return;
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		if ((IsHumanTeam(Team) && !bSpectate) || SubsUsed[Team] >= 3)
		{
			continue;
		}
		APKFootballer* Tired = nullptr;
		for (APKFootballer* P : Footballers)
		{
			if (P->Team == Team && !P->IsKeeper() && !P->bSentOff && P->Stamina < 0.42f && Ball->GetController() != P && Ball->GetHolder() != P
				&& Ball->GetSetPieceTaker() != P && (!Tired || P->Stamina < Tired->Stamina))
			{
				Tired = P;
			}
		}
		if (!Tired)
		{
			continue;
		}
		const FPKClub& Club = Team == 0 ? GI->UserClubData() : GI->OpponentClubData();
		const EPKPos SlotPos = PKTeams::Formations()[MatchFormation[Team]].Slots[Tired->Index].Pos;
		int32 Best = -1, BestRating = -1;
		for (int32 Sq = 0; Sq < Club.Squad.Num(); ++Sq)
		{
			if (MatchLineup[Team].Contains(Sq) || SubbedOff[Team].Contains(Sq) || Club.Squad[Sq].Pos == EPKPos::GK)
			{
				continue;
			}
			const int32 R = PKTeams::RatingInSlot(Club.Squad[Sq], SlotPos);
			if (R > BestRating)
			{
				BestRating = R;
				Best = Sq;
			}
		}
		if (Best >= 0)
		{
			MakeSub(Team, Tired->Index, Best);
		}
	}
}

void APKGameMode::SetupKickoff(int32 KickingTeam)
{"""),
# AI subs tick with the game plans
("""	UpdateGamePlans(Dt);""", """	UpdateGamePlans(Dt);
	UpdateAISubs(Dt);"""),
# restart: back to the picked line-ups
("""	PK::SidesSwapped() = false;
	bHalfTimeDone = bHalfTimeBreak = false;""", """	PK::SidesSwapped() = false;
	bHalfTimeDone = bHalfTimeBreak = false;
	if (SubsUsed[0] + SubsUsed[1] > 0)
	{
		ApplyTeamSetups(); // a restarted match starts with the original elevens
	}"""),
# tired AI doesn't sprint without need
("""	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = bUrgent ? D > 300.f : D > 1200.f;
}""", """	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = bUrgent ? D > 300.f : (D > 1200.f && P->Stamina > 0.3f); // tired: save the legs unless it matters
}"""),
])

# Menu: a SUBSTITUTIONS page reached from the pause menu.
patch("PKMenu.h", [
("""	Settings   // controls (key bindings), graphics, gameplay
};""", """	Settings,  // controls (key bindings), graphics, gameplay
	Subs       // substitutions (from the pause menu)
};"""),
("""	void RebuildSettings();""", """	void RebuildSettings();
	void RebuildSubs();
	TSharedPtr<SBox> SubsBox;
	int32 SubsTeam = 0;
	int32 SubsSlot = INDEX_NONE; // on-pitch player picked to come off
	int32 SubsFocus = 0;"""),
])

patch("PKMenu.cpp", [
("""			+ SWidgetSwitcher::Slot()[ SAssignNew(SettingsBox, SBox) ]""", """			+ SWidgetSwitcher::Slot()[ SAssignNew(SettingsBox, SBox) ]
			+ SWidgetSwitcher::Slot()[ SAssignNew(SubsBox, SBox) ]"""),
("""	case EPKMenuPage::Settings:
		RebuildSettings();
		break;
	case EPKMenuPage::Pause:""", """	case EPKMenuPage::Settings:
		RebuildSettings();
		break;
	case EPKMenuPage::Subs:
		SubsSlot = INDEX_NONE;
		SubsFocus = 0;
		RebuildSubs();
		break;
	case EPKMenuPage::Pause:"""),
("""	case EPKMenuPage::Pause:
		if (Owner.IsValid())
		{
			Owner->MenuResume();
		}
		break;
	default:""", """	case EPKMenuPage::Pause:
		if (Owner.IsValid())
		{
			Owner->MenuResume();
		}
		break;
	case EPKMenuPage::Subs:
		if (SubsSlot != INDEX_NONE)
		{
			SubsSlot = INDEX_NONE;
			RebuildSubs();
			return;
		}
		ShowPage(EPKMenuPage::Pause);
		break;
	default:"""),
("""				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("RESTART MATCH"), FOnClicked::CreateLambda([this]()""", """				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("SUBSTITUTIONS"), FOnClicked::CreateLambda([this]()
					{
						ShowPage(EPKMenuPage::Subs);
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("RESTART MATCH"), FOnClicked::CreateLambda([this]()"""),
])
print("layer8c ok")
