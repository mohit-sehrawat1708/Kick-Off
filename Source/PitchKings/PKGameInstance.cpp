#include "PKGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const TCHAR* SlotName = TEXT("PitchKingsTeam");
	const TCHAR* SettingsSlot = TEXT("PitchKingsSettings");
}

void UPKGameInstance::Init()
{
	Super::Init();
	Clubs = PKTeams::MakeClubs();
	ResetToDefaults();
	LoadSettings();

	// First run on this machine: measure CPU / GPU and pick the graphics quality (a laptop typically lands on Medium or
	// High). The choice is saved with the user settings and can be changed on the GRAPHICS settings page.
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (UserSettings && !GIsEditor && UserSettings->GetLastGPUBenchmarkResult() < 0.f && !FParse::Param(FCommandLine::Get(), TEXT("PKNoBenchmark")))
	{
		UserSettings->RunHardwareBenchmark();
		UserSettings->ApplyHardwareBenchmarkResults();
		UserSettings->SaveSettings();
		UE_LOG(LogTemp, Log, TEXT("[PitchKings] first run: hardware benchmark CPU %.0f GPU %.0f -> quality %d"), UserSettings->GetLastCPUBenchmarkResult(), UserSettings->GetLastGPUBenchmarkResult(), UserSettings->GetOverallScalabilityLevel());
	}

	// Restore the saved team sheet if it still fits the squad.
	if (const UPKSaveGame* Saved = Cast<UPKSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
	{
		bool bValid = Saved->Lineup.Num() == 11 && PKTeams::Formations().IsValidIndex(Saved->Formation) && Clubs.IsValidIndex(Saved->Opponent) && Saved->Opponent != UserClub;
		TSet<int32> Seen;
		for (const int32 Index : Saved->Lineup)
		{
			bValid &= UserClubData().Squad.IsValidIndex(Index) && !Seen.Contains(Index);
			Seen.Add(Index);
		}
		if (bValid)
		{
			Formation = Saved->Formation;
			Style = FMath::Clamp(Saved->Style, 0, 4);
			Lineup = Saved->Lineup;
			Opponent = Saved->Opponent;
		}
		UserKit = Saved->UserKit;
	}
	// test flags (kit captures): -PKKit=N picks the player's kit, -PKOpponent=N the opponent club
	FParse::Value(FCommandLine::Get(), TEXT("PKKit="), UserKit);
	int32 TestOpponent = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("PKOpponent="), TestOpponent) && Clubs.IsValidIndex(TestOpponent) && TestOpponent != UserClub)
	{
		Opponent = TestOpponent;
	}
	ResolveKits();
	UE_LOG(LogTemp, Log, TEXT("[Kits] %s in %s vs %s in %s"), *UserClubData().Name, *UserClubData().Kit.Name, *OpponentClubData().Name, *OpponentClubData().Kit.Name);
}

void UPKGameInstance::SaveSettings() const
{
	UPKSettingsSave* Data = Cast<UPKSettingsSave>(UGameplayStatics::CreateSaveGameObject(UPKSettingsSave::StaticClass()));
	for (int32 i = 0; i < (int32)EPKInput::Count; ++i)
	{
		for (int32 s = 0; s < 2; ++s)
		{
			Data->Keys.Add(Settings.Keys[i][s].IsValid() ? Settings.Keys[i][s].GetFName().ToString() : FString());
		}
	}
	Data->bInvertX = Settings.bInvertX;
	Data->bInvertY = Settings.bInvertY;
	Data->DeadZone = Settings.DeadZone;
	Data->Rumble = Settings.Rumble;
	Data->MatchMinutes = Settings.MatchMinutes;
	Data->Difficulty = Settings.Difficulty;
	Data->CameraZoom = Settings.CameraZoom;
	Data->bShowNumbers = Settings.bShowNumbers;
	Data->bShowHelp = Settings.bShowHelp;
	Data->bShowRadar = Settings.bShowRadar;
	Data->bShowFPS = Settings.bShowFPS;
	Data->SettingsVersion = 2;
	UGameplayStatics::SaveGameToSlot(Data, SettingsSlot, 0);
}

void UPKGameInstance::LoadSettings()
{
	Settings = FPKUserSettings();
	const UPKSettingsSave* Data = Cast<UPKSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
	if (!Data)
	{
		return;
	}
	// Saves from older versions have fewer actions; the new ones keep their defaults.
	if (Data->Keys.Num() % 2 == 0 && Data->Keys.Num() <= 2 * (int32)EPKInput::Count)
	{
		for (int32 i = 0; i < Data->Keys.Num() / 2; ++i)
		{
			for (int32 s = 0; s < 2; ++s)
			{
				const FString& Name = Data->Keys[i * 2 + s];
				Settings.Keys[i][s] = Name.IsEmpty() ? FKey() : FKey(*Name);
			}
		}
	}
	Settings.bInvertX = Data->bInvertX;
	Settings.bInvertY = Data->bInvertY;
	Settings.DeadZone = FMath::Clamp(Data->DeadZone, 0.05f, 0.5f);
	Settings.Rumble = FMath::Clamp(Data->Rumble, 0.f, 1.f);
	Settings.MatchMinutes = FMath::Clamp(Data->MatchMinutes, 2, 20);
	Settings.Difficulty = FMath::Clamp(Data->Difficulty, 0, 2);
	Settings.CameraZoom = FMath::Clamp(Data->CameraZoom, 0, 2);
	if (Data->SettingsVersion >= 2)
	{
		Settings.bShowNumbers = Data->bShowNumbers;
		Settings.bShowHelp = Data->bShowHelp;
	}
	Settings.bShowRadar = Data->bShowRadar;
	Settings.bShowFPS = Data->bShowFPS;
}

void UPKGameInstance::ResetToDefaults()
{
	Formation = UserClubData().Formation;
	Style = UserClubData().Style;
	Opponent = 1;
	Lineup = PKTeams::AutoPick(UserClubData(), Formation);
}

void UPKGameInstance::ResolveKits()
{
	if (!Clubs.IsValidIndex(UserClub))
	{
		return;
	}
	for (FPKClub& Club : Clubs)
	{
		if (Club.Kits.Num())
		{
			Club.Kit = Club.Kits[0];
		}
	}
	FPKClub& User = Clubs[UserClub];
	UserKit = User.Kits.Num() ? FMath::Clamp(UserKit, 0, User.Kits.Num() - 1) : 0;
	if (User.Kits.Num())
	{
		User.Kit = User.Kits[UserKit];
	}
	if (Clubs.IsValidIndex(Opponent) && Opponent != UserClub && Clubs[Opponent].Kits.Num())
	{
		Clubs[Opponent].Kit = Clubs[Opponent].Kits[OpponentKitIndex()];
	}
}

int32 UPKGameInstance::OpponentKitIndex() const
{
	return Clubs.IsValidIndex(Opponent) && Clubs.IsValidIndex(UserClub) ? PKTeams::ChooseKit(Clubs[Opponent], Clubs[UserClub].Kit) : 0;
}

void UPKGameInstance::Save()
{
	ResolveKits();
	UPKSaveGame* Data = Cast<UPKSaveGame>(UGameplayStatics::CreateSaveGameObject(UPKSaveGame::StaticClass()));
	Data->Formation = Formation;
	Data->Style = Style;
	Data->Lineup = Lineup;
	Data->Opponent = Opponent;
	Data->UserKit = UserKit;
	UGameplayStatics::SaveGameToSlot(Data, SlotName, 0);
}

void UPKGameInstance::SetFormation(int32 Index)
{
	const int32 Count = PKTeams::Formations().Num();
	Formation = (Index % Count + Count) % Count;
}

void UPKGameInstance::AutoPickUser()
{
	Lineup = PKTeams::AutoPick(UserClubData(), Formation);
}

void UPKGameInstance::SwapSlots(int32 A, int32 B)
{
	if (Lineup.IsValidIndex(A) && Lineup.IsValidIndex(B))
	{
		Lineup.Swap(A, B);
	}
}

void UPKGameInstance::PutPlayerInSlot(int32 SquadIndex, int32 Slot)
{
	if (!Lineup.IsValidIndex(Slot) || !UserClubData().Squad.IsValidIndex(SquadIndex))
	{
		return;
	}
	const int32 Current = SlotOfPlayer(SquadIndex);
	if (Current != INDEX_NONE)
	{
		Lineup.Swap(Current, Slot); // already starting: swap positions
	}
	else
	{
		Lineup[Slot] = SquadIndex; // from the bench: the starter goes to the bench
	}
}

int32 UPKGameInstance::SlotOfPlayer(int32 SquadIndex) const
{
	return Lineup.IndexOfByKey(SquadIndex);
}
