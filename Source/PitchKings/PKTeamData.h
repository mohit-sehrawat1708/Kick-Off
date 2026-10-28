#pragma once

#include "CoreMinimal.h"
#include "PKFootballer.h"

// Squad / club / formation data for the front end (team setup) and the match.
// Clubs and players are fictional and generated from fixed seeds, so every launch has the same squads.

enum class EPKPos : uint8
{
	GK,
	DEF,
	MID,
	FWD
};

struct FPKSquadPlayer
{
	FString Name;     // "L. Varga"
	int32 Number = 1; // shirt number
	EPKPos Pos = EPKPos::MID;
	FPKAttributes Attr;
	uint32 Traits = 0;
	uint32 TraitsPlus = 0; // PlayStyle+ (elite) versions
	FPKLook Look;
};

struct FPKClub
{
	FString Name;
	FString Short;
	float Quality = 0.72f; // average rating level
	int32 Style = 0;       // EPKTeamStyle
	int32 Formation = 0;   // index into PKTeams::Formations()
	TArray<FPKSquadPlayer> Squad; // 18: 2 GK, 6 DEF, 6 MID, 4 FWD
	FPKKit Kit;
};

struct FPKFormationSlot
{
	const TCHAR* Label;
	EPKPos Pos;
	FVector2D Home; // team frame: x -1 own goal .. +1 opponent goal, y +1 = left of the attacking direction
};

struct FPKFormation
{
	const TCHAR* Name;
	FPKFormationSlot Slots[11]; // slot 0 is the keeper; slots 9 and 10 take the kick-off
};

namespace PKTeams
{
	const TArray<FPKFormation>& Formations();
	TArray<FPKClub> MakeClubs();
	const TCHAR* PosName(EPKPos Pos);
	EPKRole RoleFor(EPKPos Pos);
	// How well a player of one position plays in a slot of another (1 = natural).
	float PositionFit(EPKPos Player, EPKPos Slot);
	// Rating shown in the team sheet: overall, reduced when out of position.
	int32 RatingInSlot(const FPKSquadPlayer& Player, EPKPos Slot);
	// Attributes the player actually plays with in that slot.
	FPKAttributes AttributesInSlot(const FPKSquadPlayer& Player, EPKPos Slot);
	// Best XI for a formation (squad indices per slot).
	TArray<int32> AutoPick(const FPKClub& Club, int32 Formation);
	// Average team-sheet rating of a line-up.
	int32 TeamRating(const FPKClub& Club, int32 Formation, const TArray<int32>& Lineup);
	FString TraitNames(uint32 Traits, uint32 Plus = 0);
}
