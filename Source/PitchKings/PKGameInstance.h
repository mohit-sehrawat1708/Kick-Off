#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "PKTeamData.h"
#include "PKSettings.h"
#include "PKGameInstance.generated.h"

// The saved team sheet (formation, style, starting XI, chosen opponent).
UCLASS()
class PITCHKINGS_API UPKSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;
	UPROPERTY()
	int32 Formation = 0;
	UPROPERTY()
	int32 Style = 0;
	UPROPERTY()
	TArray<int32> Lineup;
	UPROPERTY()
	int32 Opponent = 1;
};

// Saved user settings: key bindings, input, gameplay and HUD options.
UCLASS()
class PITCHKINGS_API UPKSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> Keys; // 2 per action, FKey names
	UPROPERTY()
	bool bInvertX = false;
	UPROPERTY()
	bool bInvertY = false;
	UPROPERTY()
	float DeadZone = 0.2f;
	UPROPERTY()
	float Rumble = 1.f;
	UPROPERTY()
	int32 MatchMinutes = 6;
	UPROPERTY()
	int32 Difficulty = 1;
	UPROPERTY()
	int32 CameraZoom = 1;
	UPROPERTY()
	bool bShowNumbers = true;
	UPROPERTY()
	bool bShowHelp = true;
	UPROPERTY()
	bool bShowRadar = true;
	UPROPERTY()
	bool bShowFPS = false;
	// 2: broadcast HUD (help panel and all-player numbers became opt-in).
	UPROPERTY()
	int32 SettingsVersion = 0;
};

// Lives for the whole session: the clubs and squads, and the player's team setup.
UCLASS()
class PITCHKINGS_API UPKGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	void Save() const;
	void SaveSettings() const;
	void LoadSettings();
	void ResetToDefaults();
	void SetFormation(int32 Index);
	void AutoPickUser();
	void SwapSlots(int32 A, int32 B);
	void PutPlayerInSlot(int32 SquadIndex, int32 Slot);
	int32 SlotOfPlayer(int32 SquadIndex) const; // INDEX_NONE = bench

	const FPKClub& UserClubData() const { return Clubs[UserClub]; }
	const FPKClub& OpponentClubData() const { return Clubs[Opponent]; }
	TArray<int32> OpponentLineup() const { return PKTeams::AutoPick(Clubs[Opponent], Clubs[Opponent].Formation); }

	TArray<FPKClub> Clubs;
	int32 UserClub = 0;
	int32 Opponent = 1;
	int32 Formation = 0;
	int32 Style = 0;
	TArray<int32> Lineup; // squad index per formation slot
	bool bSeenLanding = false;
	FPKUserSettings Settings;
};
