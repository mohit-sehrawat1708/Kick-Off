#include "PKGameMode.h"
#include "PitchKings.h"
#include "PKBall.h"
#include "PKFootballer.h"
#include "PKHUD.h"
#include "PKPitch.h"
#include "PKPlayerController.h"
#include "PKTypes.h"
#include "PKGameInstance.h"
#include "PKTeamData.h"
#include "PKAnimInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "RenderCore.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/WorldSettings.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	// 4-4-2 in team frame (x: -1 own goal line .. +1 opponent goal line, y: -1 .. +1 across).
	const FVector2D Formation[PK::PlayersPerTeam] = {
		FVector2D(-0.95f, 0.f),    // 0  GK
		FVector2D(-0.62f, -0.62f), // 1  LB
		FVector2D(-0.70f, -0.22f), // 2  LCB
		FVector2D(-0.70f, 0.22f),  // 3  RCB
		FVector2D(-0.62f, 0.62f),  // 4  RB
		FVector2D(-0.28f, -0.62f), // 5  LM
		FVector2D(-0.36f, -0.18f), // 6  LCM
		FVector2D(-0.36f, 0.18f),  // 7  RCM
		FVector2D(-0.28f, 0.62f),  // 8  RM
		FVector2D(-0.04f, -0.14f), // 9  LS
		FVector2D(0.0f, 0.14f),    // 10 RS
	};

	EPKRole RoleForIndex(int32 Index)
	{
		if (Index == 0) return EPKRole::Keeper;
		if (Index <= 4) return EPKRole::Defender;
		if (Index <= 8) return EPKRole::Midfielder;
		return EPKRole::Forward;
	}

	// Balanced, Possession, Counter, Long Ball, High Press.
	const FPKStyleParams StyleTable[(int32)EPKTeamStyle::Count] = {
		{ 3.0f, 1.0f, 4000.f, 0.4f, 0.0f, 0.25f, 0.45f, 0.50f,  0.00f, 450.f, false },
		{ 2.2f, 1.5f, 2800.f, 0.25f, 0.0f, 0.32f, 0.55f, 0.65f,  0.04f, 450.f, false },
		{ 4.5f, 0.7f, 4500.f, 0.7f, 0.2f, 0.15f, 0.30f, 0.40f, -0.08f, 380.f, false },
		{ 3.5f, 0.8f, 5500.f, 0.3f, 0.6f, 0.20f, 0.40f, 0.50f, -0.03f, 450.f, false },
		{ 3.0f, 1.0f, 4000.f, 0.4f, 0.0f, 0.25f, 0.45f, 0.50f,  0.12f, 750.f, true  },
	};

	constexpr float ControlLoseDistance = 260.f;
	constexpr float KickReach = 120.f;
	constexpr float ShootRange = 2500.f;
	constexpr float PressureDistance = 260.f;
}

APKGameMode::APKGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APKPlayerController::StaticClass();
	HUDClass = APKHUD::StaticClass();
}

void APKGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// No pawn for the player: they drive the selected footballer through a broadcast camera.
}

void APKGameMode::BeginPlay()
{
	Super::BeginPlay();

	bAutoTest = FParse::Param(FCommandLine::Get(), TEXT("PKAutoTest"));
	bCloseCamera = FParse::Param(FCommandLine::Get(), TEXT("PKCloseCam"));
	bContactShots = FParse::Param(FCommandLine::Get(), TEXT("PKContactShots"));
	bHardCollision = FParse::Param(FCommandLine::Get(), TEXT("PKHardCollision"));
	bOldDribble = FParse::Param(FCommandLine::Get(), TEXT("PKOldDribble"));
	bDribbleTest = FParse::Param(FCommandLine::Get(), TEXT("PKDribbleTest"));
	bOldLine = FParse::Param(FCommandLine::Get(), TEXT("PKOldLine"));
	bOldKeeper = FParse::Param(FCommandLine::Get(), TEXT("PKOldKeeper"));
	bKitShots = FParse::Param(FCommandLine::Get(), TEXT("PKKitShots"));
	bCelebrateShots = FParse::Param(FCommandLine::Get(), TEXT("PKCelebrateShots"));
	bKitShots |= bCelebrateShots;
	bPerfLog = bAutoTest || FParse::Param(FCommandLine::Get(), TEXT("PKPerf"));
	FString CriticFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("PKCritic="), CriticFile))
	{
		LoadCriticShots(CriticFile);
	}
	bNoReplay = FParse::Param(FCommandLine::Get(), TEXT("PKNoReplay"));
	bFeatureTest = FParse::Param(FCommandLine::Get(), TEXT("PKFeatureTest")) || FParse::Value(FCommandLine::Get(), TEXT("PKFeatureTest="), FeatureLayer);
	bSandwichTest = FParse::Param(FCommandLine::Get(), TEXT("PKSandwich"));
	bFoulTest = FParse::Param(FCommandLine::Get(), TEXT("PKFoulTest"));
	bClipSheet = FParse::Param(FCommandLine::Get(), TEXT("PKClipSheet"));
	bSpectate = FParse::Param(FCommandLine::Get(), TEXT("PKSpectate"));
	bScenario = FParse::Param(FCommandLine::Get(), TEXT("PKScenario"));
	if (bScenario)
	{
		bAutoTest = true;
		bCloseCamera = true;
	}
	bAnimTest = bClipSheet || FParse::Param(FCommandLine::Get(), TEXT("PKAnimTest"));

	SpawnEnvironmentIfMissing();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ball = GetWorld()->SpawnActor<APKBall>(APKBall::StaticClass(), FVector(0.f, 0.f, PK::BallRadius), FRotator::ZeroRotator, Params);

	SpawnTeams();
	ApplyTeamSetups();
	RestartMatch();
	if (UsesMenus())
	{
		EnterMenu();
	}
	if (bClipSheet)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game/Characters/Mannequins/Anims/Soccer"));
		Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ TEXT("/Game/Characters/Mannequins/Anims/Soccer") }, true);
		Registry.GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Asset : Assets)
		{
			if (UAnimSequence* Seq = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				SheetClips.Add(Seq);
			}
		}
		FString Frames;
		if (FParse::Value(FCommandLine::Get(), TEXT("PKClipFrames="), Frames, false))
		{
			TArray<FString> Items;
			Frames.ParseIntoArray(Items, TEXT(","));
			TArray<TObjectPtr<UAnimSequence>> Picked;
			for (const FString& Item : Items)
			{
				FString Name, Time;
				if (!Item.Split(TEXT("@"), &Name, &Time))
				{
					continue;
				}
				for (UAnimSequence* Seq : SheetClips)
				{
					if (Seq->GetName() == Name)
					{
						Picked.Add(Seq);
						SheetTimes.Add(FCString::Atof(*Time));
					}
				}
			}
			SheetClips = Picked;
		}
		UE_LOG(LogPitchKings, Log, TEXT("[ClipSheet] %d clips"), SheetClips.Num());
		SetupClipSheetGroup(0);
	}
	else if (bAnimTest)
	{
		SetupAnimTestGroup(0);
	}
	else if (bScenario)
	{
		SetupScenario(0);
	}

	UE_LOG(LogPitchKings, Log, TEXT("Pitch Kings started: %d v %d (auto-test: %s)"), PK::PlayersPerTeam, PK::PlayersPerTeam, bAutoTest ? TEXT("on") : TEXT("off"));
}

void APKGameMode::SpawnEnvironmentIfMissing()
{
	UWorld* World = GetWorld();

	// The Stadium map carries its own sky, sun and post process; only add a basic rig to empty maps.
	if (!TActorIterator<ADirectionalLight>(World))
	{
		World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator);
		World->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, -100.f), FRotator::ZeroRotator);
		if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 3000.f), FRotator(-40.f, 35.f, 0.f)))
		{
			ULightComponent* Light = Sun->GetLightComponent();
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(10.f);
			if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(Light))
			{
				DirLight->SetAtmosphereSunLight(true);
			}
			Light->MarkRenderStateDirty();
		}
		if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator))
		{
			USkyLightComponent* SkyComp = SkyLight->GetLightComponent();
			SkyComp->SetMobility(EComponentMobility::Movable);
			SkyComp->bRealTimeCapture = true;
			SkyComp->MarkRenderStateDirty();
			SkyComp->RecaptureSky();
		}
	}

	if (!TActorIterator<APKPitch>(World))
	{
		World->SpawnActor<APKPitch>(APKPitch::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	}
}

void APKGameMode::SpawnTeams()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Team = 0; Team < 2; ++Team)
	{
		for (int32 i = 0; i < PK::PlayersPerTeam; ++i)
		{
			const FVector Spot = TeamToWorld(Team, Formation[i].X, Formation[i].Y) + FVector(0.f, 0.f, 92.f);
			if (APKFootballer* P = GetWorld()->SpawnActor<APKFootballer>(APKFootballer::StaticClass(), Spot, FRotator::ZeroRotator, Params))
			{
				P->InitFootballer(Team, RoleForIndex(i), i, Formation[i]);
				Footballers.Add(P);
			}
		}
	}
}

void APKGameMode::RestartMatch()
{
	Score[0] = Score[1] = 0;
	Passes[0] = Passes[1] = 0;
	Shots[0] = Shots[1] = 0;
	Steals[0] = Steals[1] = 0;
	PossessionTime[0] = PossessionTime[1] = 0.f;
	RestoreSentOff();
	Fouls[0] = Fouls[1] = Yellows[0] = Yellows[1] = Reds[0] = Reds[1] = 0;
	Penalties[0] = Penalties[1] = Offsides[0] = Offsides[1] = 0;
	const UPKGameInstance* SettingsGI = GetGameInstance<UPKGameInstance>();
	TimeLeft = bAutoTest ? 150.f : (SettingsGI ? SettingsGI->Settings.MatchMinutes * 60.f : PK::MatchSeconds);
	MatchDuration = FMath::Max(TimeLeft, 1.f);
	MatchTotal = TimeLeft;
	Mentality[0] = Mentality[1] = 0;
	PK::SidesSwapped() = false;
	bHalfTimeDone = bHalfTimeBreak = false;
	if (SubsUsed[0] + SubsUsed[1] > 0)
	{
		ApplyTeamSetups(); // a restarted match starts with the original elevens
	}
	FirstKickoffTeam = 0;
	AdvantageTeam = -1;
	for (APKFootballer* F : Footballers)
	{
		F->Stamina = F->MaxStamina = 1.f;
	}
	TeamStyle[1] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);
	if (bAutoTest)
	{
		TeamStyle[0] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);
	}
	else if (const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())
	{
		// The team sheet's play style and the chosen opponent's own style.
		TeamStyle[0] = (EPKTeamStyle)FMath::Clamp(GI->Style, 0, (int32)EPKTeamStyle::Count - 1);
		TeamStyle[1] = (EPKTeamStyle)FMath::Clamp(GI->OpponentClubData().Style, 0, (int32)EPKTeamStyle::Count - 1);
	}
	UE_LOG(LogPitchKings, Log, TEXT("Styles: BLUE %s, RED %s"), *StyleName(TeamStyle[0]), *StyleName(TeamStyle[1]));
	SetupKickoff(0);
}

bool APKGameMode::UsesMenus()
{
	const TCHAR* Cmd = FCommandLine::Get();
	for (const TCHAR* Flag : { TEXT("PKAutoTest"), TEXT("PKSpectate"), TEXT("PKAnimTest"), TEXT("PKClipSheet"), TEXT("PKScenario"), TEXT("PKNoMenu") })
	{
		if (FParse::Param(Cmd, Flag))
		{
			return false;
		}
	}
	return true;
}

void APKGameMode::EnterMenu()
{
	// Front end: both teams lined up for kick-off behind the menu, nothing moves.
	bSpectate = false;
	bHumanTeam[0] = true;
	bHumanTeam[1] = false;
	if (Humans[1])
	{
		Humans[1]->SetHumanControlled(false);
		Humans[1] = nullptr;
	}
	RestoreSentOff();
	SetupKickoff(0);
	Phase = EPKPhase::Menu;
	Message.Empty();
	MessageTimer = 0.f;
	CardTimer = 0.f;
}

void APKGameMode::StartMatch(bool bSpectateMatch, bool bVersus)
{
	bSpectate = bSpectateMatch;
	bHumanTeam[0] = true;
	bHumanTeam[1] = bVersus && !bSpectateMatch;
	if (!bHumanTeam[1] && Humans[1])
	{
		Humans[1]->SetHumanControlled(false);
		Humans[1] = nullptr;
	}
	ApplyTeamSetups();
	RestartMatch();
	UE_LOG(LogPitchKings, Log, TEXT("Match started (%s)"), bSpectate ? TEXT("AI vs AI") : (bHumanTeam[1] ? TEXT("local 1v1: P1 BLUE, P2 RED") : TEXT("you are BLUE")));
}

void APKGameMode::ApplyTeamSetups()
{
	// Formation slots, ratings, shirt numbers and names from the team sheets (game instance).
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (!GI || GI->Clubs.Num() < 2)
	{
		return;
	}
	RestoreSentOff();
	for (int32 Team = 0; Team < 2; ++Team)
	{
		const FPKClub& Club = Team == 0 ? GI->UserClubData() : GI->OpponentClubData();
		const int32 FormationIndex = Team == 0 ? GI->Formation : Club.Formation;
		const TArray<int32> Lineup = Team == 0 ? GI->Lineup : GI->OpponentLineup();
		const FPKFormation& F = PKTeams::Formations()[FMath::Clamp(FormationIndex, 0, PKTeams::Formations().Num() - 1)];
		MatchLineup[Team] = Lineup;
		MatchFormation[Team] = FMath::Clamp(FormationIndex, 0, PKTeams::Formations().Num() - 1);
		SubbedOff[Team].Reset();
		SubsUsed[Team] = 0;
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team || !Lineup.IsValidIndex(P->Index) || !Club.Squad.IsValidIndex(Lineup[P->Index]))
			{
				continue;
			}
			const FPKFormationSlot& Slot = F.Slots[P->Index];
			const FPKSquadPlayer& Pl = Club.Squad[Lineup[P->Index]];
			FPKAttributes Attr = PKTeams::AttributesInSlot(Pl, Slot.Pos);
			ScaleForDifficulty(Team, Attr);
			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name, Pl.TraitsPlus);
			P->ApplyKit(Club.Kit, Pl.Look);
			UE_LOG(LogPitchKings, Verbose, TEXT("%s #%d %s %s OVR %d | AcceleRATE %s | PlayStyles: %s"), PK::TeamName(Team), Pl.Number, *Pl.Name, Slot.Label, Attr.Overall(),
				APKFootballer::AccelName(P->AccelType), *P->TraitNames());
		}
	}
	// Seats and fans in the clubs' colours (home club everywhere, the away end in the visitors' colours).
	if (APKPitch* Pitch = TActorIterator<APKPitch>(GetWorld()) ? *TActorIterator<APKPitch>(GetWorld()) : nullptr)
	{
		const FPKKit& Home = GI->UserClubData().Kit;
		const FPKKit& Away = GI->OpponentClubData().Kit;
		Pitch->SetClubColours(Home.Primary, Home.Trim, Away.Primary, Away.Trim);
	}
}

void APKGameMode::ScaleForDifficulty(int32 Team, FPKAttributes& Attr) const
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
		E.bKeeper = F.Slots[Slot].Pos == EPKPos::GK;
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
		E.bKeeper = Club.Squad[Sq].Pos == EPKPos::GK;
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
	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name, In.TraitsPlus);
	P->ApplyKit(Club.Kit, In.Look);
	P->Stamina = P->MaxStamina = 1.f;
	P->YellowCards = 0;
	SubbedOff[Team].Add(MatchLineup[Team][Slot]);
	MatchLineup[Team][Slot] = SquadIndex;
	++SubsUsed[Team];
	ShowMessage(FString::Printf(TEXT("SUBSTITUTION %s:  ON #%d %s   OFF #%d %s"), *TeamLabel(Team), In.Number, *In.Name, OffNumber, *OffName), 2.5f);
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
			if (P->Team == Team && !P->IsKeeper() && !P->bSentOff && P->Stamina < 0.5f && Ball->GetController() != P && Ball->GetHolder() != P
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

void APKGameMode::RecordReplay(float Dt)
{
	if (bNoReplay || bAutoTest)
	{
		return;
	}
	ReplayAccum += Dt;
	if (ReplayAccum < 1.f / ReplayHz)
	{
		return;
	}
	ReplayAccum = FMath::Fmod(ReplayAccum, 1.f / ReplayHz);
	const int32 Capacity = ReplayHz * (ReplaySeconds + 1);
	if (ReplayBuf.Num() != Capacity)
	{
		ReplayBuf.SetNum(Capacity);
		ReplayHead = ReplayCount = 0;
	}
	FPKReplayFrame& F = ReplayBuf[ReplayHead];
	F.Ball = Ball->GetActorLocation();
	F.BallSpin = Ball->GetSpinRotation();
	F.Actors.SetNum(Footballers.Num());
	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		const APKFootballer* P = Footballers[i];
		F.Actors[i] = { P->GetActorLocation(), (float)P->GetActorRotation().Yaw, P->GetVelocity(), (uint8)P->GetCurrentAction(), P->GetActionTime() };
	}
	ReplayHead = (ReplayHead + 1) % Capacity;
	ReplayCount = FMath::Min(ReplayCount + 1, Capacity);
}

FString APKGameMode::GetBroadcastClock() const
{
	const float Played = FMath::Clamp(1.f - TimeLeft / MatchDuration, 0.f, 1.f) * 90.f * 60.f;
	const int32 Seconds = FMath::FloorToInt(Played);
	return FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60);
}

void APKGameMode::CrowdCheer(int32 Team, float Amount)
{
	if (APKPitch* Pitch = TActorIterator<APKPitch>(GetWorld()) ? *TActorIterator<APKPitch>(GetWorld()) : nullptr)
	{
		Pitch->Cheer(Team, Amount);
	}
}

void APKGameMode::BeginCelebration(int32 Scorer)
{
	GoalStage = 0;
	CrowdCheer(Scorer, 1.f);
	APKFootballer* S = Ball->LastKicker;
	if (!S || S->Team != Scorer)
	{
		// Own goal or a deflection: the nearest attacker celebrates.
		float Best = 1.e9f;
		for (APKFootballer* P : Footballers)
		{
			const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(Ball->GetActorLocation()));
			if (P->Team == Scorer && !P->IsKeeper() && D < Best)
			{
				Best = D;
				S = P;
			}
		}
	}
	Celebrator = S;
	if (S)
	{
		S->bCelebrating = true;
	}
	if (!bNoReplay && !bAutoTest && ReplayCount > ReplayHz)
	{
		PhaseTimer = 2.6f + ReplaySeconds + 0.6f; // celebrate, replay, then kick off
	}
}

void APKGameMode::BeginReplay()
{
	GoalStage = 1;
	bReplaying = true;
	ReplayClock = 0.f;
	const int32 Capacity = ReplayBuf.Num();
	ReplayLength = FMath::Min(ReplayCount, ReplayHz * ReplaySeconds);
	ReplayFirst = (ReplayHead - ReplayLength + Capacity) % Capacity;
	Ball->bReplaying = true;
	for (APKFootballer* P : Footballers)
	{
		P->bReplaying = true;
		P->bCelebrating = false;
		P->GetCharacterMovement()->DisableMovement();
	}
	ShotFeedback.Reset();
}

void APKGameMode::UpdateReplay(float Dt)
{
	ReplayClock += Dt;
	const float FrameF = FMath::Clamp(ReplayClock * ReplayHz, 0.f, (float)(ReplayLength - 1));
	const int32 I0 = FMath::FloorToInt(FrameF);
	const int32 I1 = FMath::Min(I0 + 1, ReplayLength - 1);
	const float A = FrameF - I0;
	const int32 Capacity = ReplayBuf.Num();
	const FPKReplayFrame& F0 = ReplayBuf[(ReplayFirst + I0) % Capacity];
	const FPKReplayFrame& F1 = ReplayBuf[(ReplayFirst + I1) % Capacity];
	Ball->SetReplayPose(FMath::Lerp(F0.Ball, F1.Ball, A), FQuat::Slerp(F0.BallSpin, F1.BallSpin, A));
	for (int32 i = 0; i < Footballers.Num() && i < F0.Actors.Num() && i < F1.Actors.Num(); ++i)
	{
		APKFootballer* P = Footballers[i];
		const FPKReplayActor& R0 = F0.Actors[i];
		const FPKReplayActor& R1 = F1.Actors[i];
		P->SetActorLocationAndRotation(FMath::Lerp(R0.Loc, R1.Loc, A), FRotator(0.f, FMath::Lerp(R0.Yaw, R0.Yaw + FMath::FindDeltaAngleDegrees(R0.Yaw, R1.Yaw), A), 0.f));
		P->GetCharacterMovement()->Velocity = FMath::Lerp(R0.Vel, R1.Vel, A); // drives the run cycle
		// Actions (kicks, dives, headers) play from the recorded clip time.
		const EPKAction Action = (EPKAction)R0.Action;
		const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
		if (Action != EPKAction::None && Clip.IsValid())
		{
			P->DebugClip = Clip.Seq;
			P->DebugTime = R0.Action == R1.Action ? FMath::Lerp(R0.ActionTime, R1.ActionTime, A) : R0.ActionTime;
		}
		else
		{
			P->DebugClip = nullptr;
		}
	}
	if (ReplayClock > (float)ReplayLength / ReplayHz + 0.3f)
	{
		EndReplay();
	}
}

void APKGameMode::EndReplay()
{
	if (Celebrator.IsValid())
	{
		Celebrator->bCelebrating = false;
	}
	if (!bReplaying)
	{
		return;
	}
	bReplaying = false;
	Ball->bReplaying = false;
	for (APKFootballer* P : Footballers)
	{
		P->bReplaying = false;
		P->DebugClip = nullptr;
		P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		P->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
	ReplayCount = 0; // the next replay records afresh
	PhaseTimer = FMath::Min(PhaseTimer, 0.3f);
}

void APKGameMode::DebugForceGoalChance()
{
	APKFootballer* Striker = FindFootballer(0, 9);
	if (!Striker || Phase != EPKPhase::Playing)
	{
		return;
	}
	const float Dir = PK::AttackDir(0);
	Striker->Teleport2D(FVector(Dir * (PK::HalfLength - 1300.f), 200.f, 0.f), Dir > 0.f ? 0.f : 180.f);
	if (APKFootballer* GK = Keeper(1))
	{
		GK->Teleport2D(FVector(Dir * (PK::HalfLength - 150.f), 2200.f, 0.f), 0.f);
		GK->Stun(6.f);
	}
	Ball->ClearControl();
	Ball->SetActorLocation(FVector(PK::Flat(Striker->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
	Ball->Velocity = FVector::ZeroVector;
	Ball->SetController(Striker);
	RequestShot(Striker, -0.3f, 0.6f);
}

bool APKGameMode::GetCelebrationFocus(FVector& Out) const
{
	if (Phase == EPKPhase::GoalScored && GoalStage == 0 && Celebrator.IsValid() && !bHalfTimeBreak)
	{
		Out = Celebrator->GetActorLocation();
		return true;
	}
	return false;
}

void APKGameMode::SetupKickoff(int32 KickingTeam)
{
	++CameraCutStamp;
	APKFootballer* Kicker = nullptr;
	for (APKFootballer* P : Footballers)
	{
		float Fx = FMath::Min(P->Home.X, P->Team == KickingTeam ? -0.03f : -0.2f);
		float Fy = P->Home.Y;
		if (P->Team == KickingTeam && P->Index == 10)
		{
			Fx = -0.012f;
			Fy = 0.f;
			Kicker = P;
		}
		else if (P->Team == KickingTeam && P->Index == 9)
		{
			Fx = -0.02f;
			Fy = 0.12f;
		}
		P->Teleport2D(TeamToWorld(P->Team, Fx, Fy), PK::AttackDir(P->Team) > 0.f ? 0.f : 180.f);
		P->StunTimer = 0.f;
		P->Stance = EPKStance::Normal;
	}

	if (!Kicker)
	{
		Kicker = ClosestOutfield(KickingTeam, FVector::ZeroVector, nullptr);
		if (Kicker)
		{
			Kicker->Teleport2D(TeamToWorld(KickingTeam, -0.012f, 0.f), KickingTeam == 0 ? 0.f : 180.f);
		}
	}
	SetPiece = EPKSetPiece::Kickoff;
	Ball->PlaceAt(FVector::ZeroVector, Kicker);
	for (int32 T = 0; T < 2; ++T)
	{
		if (bHumanTeam[T])
		{
			SetHuman(Kicker && Kicker->Team == T ? Kicker : ClosestOutfield(T, FVector::ZeroVector, nullptr));
		}
	}

	Phase = EPKPhase::Kickoff;
	PhaseTimer = 1.5f;
	RestartTimer = 0.f;
	ShowMessage(FString::Printf(TEXT("KICK OFF - %s"), *TeamLabel(KickingTeam)), 1.8f);
}

void APKGameMode::ShowMessage(const FString& Text, float Seconds)
{
	Message = Text;
	MessageTimer = Seconds;
}

void APKGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePerfLog(DeltaSeconds);
	UpdateCriticShots(DeltaSeconds);
	{
		bool bJump = LastPositions.Num() != Footballers.Num();
		LastPositions.SetNum(Footballers.Num());
		for (int32 i = 0; i < Footballers.Num(); ++i)
		{
			const FVector L = Footballers[i]->GetActorLocation();
			bJump |= FVector::DistSquared2D(L, LastPositions[i]) > 300.f * 300.f;
			LastPositions[i] = L;
		}
		if (bJump)
		{
			++CameraCutStamp; // players were placed for a restart: no temporal ghosting of the old positions
		}
	}
	if (!StadiumPitch.IsValid())
	{
		StadiumPitch = TActorIterator<APKPitch>(GetWorld()) ? *TActorIterator<APKPitch>(GetWorld()) : nullptr;
	}
	if (APKPitch* Pitch = StadiumPitch.Get())
	{
		Pitch->SetScoreboard(Score[0], Score[1], FMath::Clamp(1.f - TimeLeft / MatchDuration, 0.f, 1.f) * 90.f * 60.f);
	}

	if (!Ball)
	{
		return;
	}

	ShotFeedbackTimer = FMath::Max(0.f, ShotFeedbackTimer - DeltaSeconds);
	MessageTimer = FMath::Max(0.f, MessageTimer - DeltaSeconds);
	RestartTimer += DeltaSeconds;
	EventShotCooldown = FMath::Max(0.f, EventShotCooldown - DeltaSeconds);
	for (int32 i = TimedShots.Num() - 1; i >= 0; --i)
	{
		TimedShots[i].Delay -= DeltaSeconds;
		if (TimedShots[i].Delay <= 0.f)
		{
			FScreenshotRequest::RequestScreenshot(TimedShots[i].File, true, false);
			TimedShots.RemoveAt(i);
			break; // one capture per frame
		}
	}
	if (bScenario)
	{
		UpdateScenario(DeltaSeconds);
	}
	if (!PendingShot.IsEmpty() && --PendingShotFrames <= 0)
	{
		FScreenshotRequest::RequestScreenshot(PendingShot, true, false);
		PendingShot.Empty();
	}
	CardTimer = FMath::Max(0.f, CardTimer - DeltaSeconds);
	if (RestoreDilationFrames > 0 && --RestoreDilationFrames == 0)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
	if (bClipSheet)
	{
		UpdateClipSheet(DeltaSeconds);
		return;
	}
	if (bKitShots)
	{
		bCelebrateShots ? UpdateCelebrateShots(DeltaSeconds) : UpdateKitShots(DeltaSeconds);
		return;
	}
	if (bAnimTest)
	{
		UpdateAnimTest(DeltaSeconds);
		return;
	}
	if (bAutoTest && !bScenario)
	{
		UpdateAutoTest(DeltaSeconds);
	}

	for (APKFootballer* P : Footballers)
	{
		P->LookTarget = Ball->GetActorLocation();
		const bool bHolding = Ball->GetHolder() == P;
		const EPKAction Hold = P->IsKeeper() ? EPKAction::GKHold : EPKAction::ThrowHold;
		if (bHolding && !P->IsInAction())
		{
			P->PlayAction(Hold); // keep the ball in the hands until it is released
			EventShot(P->IsKeeper() ? TEXT("KeeperHold") : TEXT("ThrowHold"), P);
		}
		else if (!bHolding && (P->GetCurrentAction() == EPKAction::GKHold || P->GetCurrentAction() == EPKAction::ThrowHold))
		{
			P->StopAction();
		}
	}

	UpdatePredictedTaker(DeltaSeconds);

	auto FreezeAll = [this]()
	{
		for (APKFootballer* P : Footballers)
		{
			P->MoveInput = FVector::ZeroVector;
			P->bSprint = false;
		}
	};

	switch (Phase)
	{
	case EPKPhase::Kickoff:
	case EPKPhase::Stoppage:
		FreezeAll();
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			Phase = EPKPhase::Playing;
		}
		break;

	case EPKPhase::GoalScored:
		FreezeAll();
		if (Celebrator.IsValid() && GoalStage == 0 && !bHalfTimeBreak)
		{
			// Celebration: the scorer wheels away to the corner flag, team-mates chase him.
			APKFootballer* S = Celebrator.Get();
			const FVector Flag(PK::AttackDir(S->Team) * (PK::HalfLength - 200.f), (S->GetActorLocation().Y >= 0.f ? 1.f : -1.f) * (PK::HalfWidth - 250.f), 0.f);
			MoveTo(S, Flag, true);
			for (APKFootballer* P : Footballers)
			{
				if (P != S && P->Team == S->Team && !P->IsKeeper() && FVector::Dist(P->GetFlatLocation(), S->GetFlatLocation()) < 3000.f)
				{
					MoveTo(P, S->GetFlatLocation(), FVector::Dist(P->GetFlatLocation(), S->GetFlatLocation()) > 300.f);
				}
			}
		}
		if (GoalStage == 0 && PhaseTimer <= ReplaySeconds + 0.6f && !bHalfTimeBreak && ReplayCount > ReplayHz)
		{
			BeginReplay();
		}
		if (bReplaying)
		{
			UpdateReplay(DeltaSeconds);
		}
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			EndReplay();
			if (bHalfTimeBreak)
			{
				// Second half: change ends, a breather for everyone, the other team kicks off.
				bHalfTimeBreak = false;
				PK::SidesSwapped() = !PK::SidesSwapped();
				for (APKFootballer* F : Footballers)
				{
					F->Stamina = FMath::Min(F->MaxStamina, F->Stamina + 0.2f);
				}
				ShowMessage(TEXT("SECOND HALF"), 1.5f);
				SetupKickoff(1 - FirstKickoffTeam);
			}
			else
			{
				SetupKickoff(NextKickoffTeam);
			}
		}
		break;

	case EPKPhase::FullTime:
	case EPKPhase::Menu:
		FreezeAll();
		break;

	case EPKPhase::Playing:
		TimeLeft -= DeltaSeconds;
		if (!bHalfTimeDone && TimeLeft <= MatchTotal * 0.5f && !bFeatureTest)
		{
			bHalfTimeDone = true;
			bHalfTimeBreak = true;
			Phase = EPKPhase::GoalScored; // same pause and restart path as after a goal
			PhaseTimer = 3.5f;
			Ball->ClearControl();
			ShowMessage(FString::Printf(TEXT("HALF TIME  %s %d - %d %s"), *TeamLabel(0), Score[0], Score[1], *TeamLabel(1)), 3.5f);
			{
				float Sum[2] = { 0.f, 0.f }, Low[2] = { 1.f, 1.f };
				for (const APKFootballer* F : Footballers)
				{
					Sum[F->Team] += F->Stamina;
					Low[F->Team] = FMath::Min(Low[F->Team], F->Stamina);
				}
				UE_LOG(LogPitchKings, Log, TEXT("Half time: BLUE %d - %d RED | stamina BLUE avg %.2f low %.2f, RED avg %.2f low %.2f | advantages %d (called back %d)"), Score[0], Score[1],
					Sum[0] / 11.f, Low[0], Sum[1] / 11.f, Low[1], Advantages, AdvantageCallbacks);
			}
			break;
		}
		UpdateAdvantage(DeltaSeconds);
		RecordReplay(DeltaSeconds);
		{
			// Stamina: a short match stands for 90 minutes, so players tire faster.
			const float Rate = FMath::Clamp(360.f / FMath::Max(MatchTotal, 60.f), 0.4f, 2.f);
			for (APKFootballer* F : Footballers)
			{
				F->UpdateStamina(DeltaSeconds, Rate);
			}
		}
		if (TimeLeft <= 0.f)
		{
			TimeLeft = 0.f;
			Phase = EPKPhase::FullTime;
			ShowMessage(FString::Printf(TEXT("FULL TIME  %s %d - %d %s"), *TeamLabel(0), Score[0], Score[1], *TeamLabel(1)), 9999.f);
			{
				float Sum[2] = { 0.f, 0.f }, Low[2] = { 1.f, 1.f };
				int32 N[2] = { 0, 0 };
				for (const APKFootballer* F : Footballers)
				{
					Sum[F->Team] += F->Stamina;
					Low[F->Team] = FMath::Min(Low[F->Team], F->Stamina);
					++N[F->Team];
				}
				UE_LOG(LogPitchKings, Log, TEXT("Full time stamina: BLUE avg %.2f low %.2f | RED avg %.2f low %.2f | advantages %d (called back %d)"),
					Sum[0] / FMath::Max(N[0], 1), Low[0], Sum[1] / FMath::Max(N[1], 1), Low[1], Advantages, AdvantageCallbacks);
			}
			UE_LOG(LogPitchKings, Log, TEXT("Full time: BLUE %d - %d RED | shots %d-%d | passes %d-%d | steals %d-%d | final third %.0fs-%.0fs | through %d-%d | ball airborne %.0fs (max %.1f m) | chips %d crosses %d flicks %d"), Score[0], Score[1], Shots[0], Shots[1], Passes[0], Passes[1], Steals[0], Steals[1], FinalThirdTime[0], FinalThirdTime[1], ThroughBalls[0], ThroughBalls[1], AirTime, MaxBallZ / 100.f, Chips, Crosses, Flicks);
			break;
		}
		static const bool bNoWatch = FParse::Param(FCommandLine::Get(), TEXT("PKNoWatch")); // baseline for the facing check
		for (APKFootballer* F : Footballers)
		{
			F->bWatchBall = !bNoWatch;
			// Facing check: off-ball players at walking pace should be open to the ball.
			if (!F->bHasBall && !F->IsInAction() && !F->IsKeeper() && PK::Flat(F->GetVelocity()).Size() < 200.f)
			{
				const FVector ToBall = PK::Flat(Ball->GetActorLocation() - F->GetActorLocation()).GetSafeNormal();
				const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToBall, F->GetFacing()), -1.f, 1.f)));
				WatchErrSum += Deg;
				++WatchSamples;
				WatchAway += Deg > 90.f ? 1 : 0;
			}
		}
		if (Ball->IsFree() && Ball->GetActorLocation().Z > 30.f)
		{
			AirTime += DeltaSeconds;
			MaxBallZ = FMath::Max(MaxBallZ, Ball->GetActorLocation().Z);
		}
		if (APKFootballer* BallOwner = Ball->GetOwnerPlayer())
		{
			PossessionTime[BallOwner->Team] += DeltaSeconds;
			if (WorldToFx(BallOwner->Team, Ball->GetActorLocation().X) > 0.33f)
			{
				FinalThirdTime[BallOwner->Team] += DeltaSeconds;
			}
		}
		UpdateAI(DeltaSeconds);
		UpdateBodyContacts(DeltaSeconds);
		if (APKFootballer* LooseOwner = Ball->GetController())
		{
			const bool bOutOfTouch = FVector::Dist(PK::Flat(Ball->GetActorLocation()), LooseOwner->GetFootPoint()) > 90.f && PK::Flat(LooseOwner->GetVelocity()).Size() < 80.f && !LooseOwner->IsKicking();
			StalemateTimer = bOutOfTouch ? StalemateTimer + DeltaSeconds : 0.f;
			if (StalemateTimer > 1.f)
			{
				StalemateTimer = 0.f;
				++StalemateBreaks;
				Ball->SetController(nullptr); // a loose ball both sides can play
				UE_LOG(LogPitchKings, Log, TEXT("Stalemate: %s #%d lost touch of the ball"), PK::TeamName(LooseOwner->Team), LooseOwner->Index);
			}
		}
		else
		{
			StalemateTimer = 0.f;
		}
		if (SetPiece != EPKSetPiece::None && !Ball->IsPlaced() && !Ball->GetHolder())
		{
			SetPiece = EPKSetPiece::None;
			for (APKFootballer* F : Footballers)
			{
				F->bHoldPosition = false;
			}
		}
		// A person who has not taken a restart within 6 s: the team takes it for them, so play never stalls.
		for (int32 T = 0; T < 2; ++T)
		{
			if (!bSpectate && Phase == EPKPhase::Playing && RestartTimer > 6.f && HumanOnDeadBall(T) && !Humans[T]->IsKicking())
			{
				AITakeRestart(Humans[T]);
			}
		}
		APKFootballer* Human = Humans[0];
		if (bSpectate && Human && Ball->GetOwnerPlayer() != Human)
		{
			// Auto-switch like a real player would: keep the selection on the blue player nearest the ball.
			AutoSwitchTimer -= DeltaSeconds;
			APKFootballer* CurrentOwner = Ball->GetOwnerPlayer();
			if (AutoSwitchTimer <= 0.f && !(CurrentOwner && CurrentOwner->Team == 0) && !(Ball->IntendedReceiver && Ball->IntendedReceiver->Team == 0))
			{
				AutoSwitchTimer = 0.6f;
				APKFootballer* Closest = ClosestOutfield(0, Ball->GetActorLocation(), nullptr);
				if (Closest && Closest != Human && FVector::Dist2D(Closest->GetActorLocation(), Ball->GetActorLocation()) + 300.f < FVector::Dist2D(Human->GetActorLocation(), Ball->GetActorLocation()))
				{
					SetHuman(Closest);
				}
			}
		}
		if (APKFootballer* Dribbler = Ball->GetController())
		{
			UpdateDribble(Dribbler, DeltaSeconds);
		}
		AlignBallForKicks(DeltaSeconds);
		UpdatePendingTouches(DeltaSeconds);
		UpdateBufferedKicks(DeltaSeconds);
		CheckPickups();
		CheckBallOut();
		break;
	}

	// Kicks wound up this frame release at their strike point, whatever the phase.
	if (Phase == EPKPhase::Playing || Phase == EPKPhase::Kickoff || Phase == EPKPhase::Stoppage)
	{
		ReleaseKicks();
	}
}

// ---------------------------------------------------------------------------------------------
// Team AI
// ---------------------------------------------------------------------------------------------

void APKGameMode::UpdateAI(float Dt)
{
	MarkingTimer -= Dt;
	if (MarkingTimer <= 0.f)
	{
		MarkingTimer = 0.3f;
		UpdateMarking();
	}
	APKFootballer* BallOwner = Ball->GetOwnerPlayer();
	// Dead ball = set piece or throw-in (a keeper holding the ball in open play is not a restart).
	const bool bDeadBall = Ball->IsPlaced() || (Ball->GetHolder() && !Ball->GetHolder()->IsKeeper());

	// Per team: the closest outfield player engages the ball, the second closest covers.
	UpdateDefensiveLine();
	UpdateGamePlans(Dt);
	UpdateAISubs(Dt);
	APKFootballer* Chaser[2] = { nullptr, nullptr };
	APKFootballer* Cover[2] = { nullptr, nullptr };
	for (int32 Team = 0; Team < 2; ++Team)
	{
		float Best = TNumericLimits<float>::Max();
		float Second = TNumericLimits<float>::Max();
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team || P->IsKeeper())
			{
				continue;
			}
			const float D = FVector::Dist(P->GetFlatLocation(), PredictBall(P));
			if (D < Best)
			{
				Second = Best;
				Cover[Team] = Chaser[Team];
				Best = D;
				Chaser[Team] = P;
			}
			else if (D < Second)
			{
				Second = D;
				Cover[Team] = P;
			}
		}

		// Teammate contain: the closest team-mate to the carrier (not the person's player, not the keeper) presses.
		PressMate[Team] = nullptr;
		if (bTeamPress[Team] && BallOwner && BallOwner->Team != Team)
		{
			float BestPress = TNumericLimits<float>::Max();
			for (APKFootballer* P : Footballers)
			{
				if (P->Team != Team || P->IsKeeper() || P->bHuman || P->IsStunned())
				{
					continue;
				}
				const float D = FVector::Dist(P->GetFlatLocation(), BallOwner->GetFlatLocation());
				if (D < BestPress)
				{
					BestPress = D;
					PressMate[Team] = P;
				}
			}
			TeamPressSeconds += PressMate[Team].IsValid() ? Dt : 0.f;
		}
	}

	for (APKFootballer* P : Footballers)
	{
		if (!P->bHuman && P != BallOwner)
		{
			P->Stance = EPKStance::Normal;
		}
		if (P->bHuman && !bSpectate)
		{
			// Receiver assist: a pass is coming to the selected player and the person is not steering, so he
			// moves to meet it like the AI would. Any direction input takes over at once (see HumanMove).
			if (Phase == EPKPhase::Playing && P->HeaderRequestTimer > 0.f && !BallOwner && P->HumanInput.Size() < 0.5f && !P->IsStunned())
			{
				MoveTo(P, PredictAerial(P, 110.f, 210.f), true); // asked for a header: attack the ball at head height
			}
			else if (Phase == EPKPhase::Playing && Ball->IntendedReceiver == P && !BallOwner && P->HumanInput.Size() < 0.2f && !P->IsStunned())
			{
				MoveTo(P, PredictBall(P), true);
			}
			continue;
		}
		if (P->IsStunned())
		{
			P->MoveInput = FVector::ZeroVector;
			continue;
		}
		if (P->bHoldPosition && Ball->IsPlaced() && Ball->GetSetPieceTaker() != P)
		{
			P->MoveInput = FVector::ZeroVector;
			P->bSprint = false;
			continue;
		}

		const bool bTeamHasBall = BallOwner && BallOwner->Team == P->Team;
		const bool bTeamPassing = !BallOwner && Ball->IntendedReceiver && Ball->IntendedReceiver->Team == P->Team;

		if (BallOwner == P)
		{
			if (bDeadBall)
			{
				AITakeRestart(P);
			}
			else
			{
				AICarry(P, Dt);
			}
		}
		else if (P->IsKeeper())
		{
			AIKeeper(P);
		}
		else if (P->HeaderRequestTimer > 0.f && !BallOwner)
		{
			MoveTo(P, PredictAerial(P, 110.f, 210.f), true);
		}
		else if (Ball->IntendedReceiver == P && !BallOwner)
		{
			MoveTo(P, PredictBall(P), true);
		}
		else if (bTeamHasBall || bTeamPassing)
		{
			APKFootballer* Focus = BallOwner ? BallOwner : Ball->IntendedReceiver.Get();
			const FVector Spot = AttackSupportSpot(P, Focus);
			MoveTo(P, Spot, P->RunTimer > 0.f);
		}
		else if (bDeadBall)
		{
			// Opponents give the taker 9.15 m.
			FVector Spot = SupportSpot(P, false);
			const FVector BallLoc = PK::Flat(Ball->GetActorLocation());
			const FVector Away = Spot - BallLoc;
			if (Away.Size() < PK::CentreRadius)
			{
				Spot = BallLoc + Away.GetSafeNormal2D() * PK::CentreRadius;
			}
			MoveTo(P, Spot, false);
		}
		else if (P == PressMate[P->Team].Get() && BallOwner)
		{
			AIDefend(P, BallOwner); // sent to press by the person (teammate contain)
		}
		else if (P == Chaser[P->Team])
		{
			AIDefend(P, BallOwner);
		}
		else if (P == Cover[P->Team] && BallOwner)
		{
			if (Style(P->Team).bDoublePress && FVector::Dist(P->GetFlatLocation(), BallOwner->GetFlatLocation()) < 1200.f)
			{
				AIDefend(P, BallOwner); // high press: two players hunt the ball
			}
			else
			{
				MoveTo(P, FMath::Lerp(BallOwner->GetFlatLocation(), OwnGoal(P->Team), 0.3f), false);
			}
		}
		else if (const TObjectPtr<APKFootballer>* Mark = MarkTargets.Find(P))
		{
			// Mark goal-side and slightly ball-side of the assigned opponent.
			const APKFootballer* Opp = *Mark;
			const FVector OppLoc = Opp->GetFlatLocation();
			const FVector GoalSide = (OwnGoal(P->Team) - OppLoc).GetSafeNormal2D();
			const FVector BallSide = (PK::Flat(Ball->GetActorLocation()) - OppLoc).GetSafeNormal2D();
			const FVector Spot = OppLoc + GoalSide * 250.f + BallSide * 50.f;
			MoveTo(P, Spot, FVector::Dist(Spot, P->GetFlatLocation()) > 500.f);
		}
		else
		{
			// Out of position defenders (a fullback caught upfield) sprint back into the line.
			const FVector Spot = SupportSpot(P, false);
			MoveTo(P, Spot, !bOldLine && P->Role == EPKRole::Defender && FVector::Dist(Spot, P->GetFlatLocation()) > 900.f);
		}
	}
}

void APKGameMode::UpdatePerfLog(float Dt)
{
	if (!bPerfLog)
	{
		return;
	}
	PerfClock += Dt;
	PerfFrames.Add(Dt * 1000.f);
	PerfGT += FPlatformTime::ToMilliseconds(GGameThreadTime);
	PerfRT += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	PerfGPU += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	if (PerfClock < 5.f || PerfFrames.Num() == 0)
	{
		return;
	}
	TArray<float> Sorted = PerfFrames;
	Sorted.Sort();
	const int32 N = Sorted.Num();
	float Sum = 0.f;
	for (const float F : Sorted)
	{
		Sum += F;
	}
	const float Worst1 = Sorted[FMath::Clamp(FMath::FloorToInt(N * 0.99f), 0, N - 1)];
	UE_LOG(LogPitchKings, Log, TEXT("[Perf] %.0f fps (frame %.2f ms, 1%% worst %.2f ms) | game %.2f ms, render %.2f ms, GPU %.2f ms | %dx%d scalability %d"),
		1000.f * N / Sum, Sum / N, Worst1, PerfGT / N, PerfRT / N, PerfGPU / N,
		GSystemResolution.ResX, GSystemResolution.ResY, GEngine && GEngine->GetGameUserSettings() ? GEngine->GetGameUserSettings()->GetOverallScalabilityLevel() : -1);
	PerfFrames.Reset();
	PerfGT = PerfRT = PerfGPU = 0.0;
	PerfClock = 0.f;
}

void APKGameMode::UpdateCelebrateShots(float Dt)
{
	KitClock += Dt;
	APKFootballer* S = FindFootballer(0, 9);
	if (!S)
	{
		return;
	}
	const float Warmup = WITH_EDITOR ? 35.f : 2.f;
	if (KitStep == 0 && KitClock > 1.f)
	{
		int32 k = 0;
		for (APKFootballer* F : Footballers)
		{
			F->Teleport2D(FVector(-4500.f + (k++) * 400.f, PK::HalfWidth + 400.f, 0.f), 90.f);
			F->MoveInput = FVector::ZeroVector;
		}
		Ball->ClearControl();
		Ball->SetActorLocation(FVector(0.f, PK::HalfWidth + 900.f, PK::BallRadius));
		S->Teleport2D(FVector(-4300.f, 0.f, 0.f), 0.f);
		S->bWatchBall = false;
		S->bCelebrating = true;
		KitStep = 1;
	}
	if (KitStep == 0)
	{
		return;
	}
	// Celebration run as in a match (sprinting).
	S->MoveInput = FVector(1.f, 0.f, 0.f);
	S->bSprint = true;
	S->Stamina = 1.f;
	const FVector L = S->GetActorLocation();
	const float T = KitClock - Warmup;
	FVector Cam(0.f, -520.f, 30.f);
	if (T > 1.2f && T < 2.4f)
	{
		Cam = FVector(480.f, -180.f, 40.f); // front
	}
	else if (T >= 2.4f)
	{
		Cam = FVector(-480.f, -150.f, 50.f); // back
	}
	KitCamLoc = L + Cam;
	KitCamLook = L + FVector(0.f, 0.f, 25.f);
	if (L.X > 3500.f)
	{
		S->Teleport2D(FVector(-4300.f, 0.f, 0.f), 0.f);
	}
	static const float ShotTimes[] = { 0.3f, 0.45f, 0.6f, 0.75f, 1.5f, 1.65f, 1.8f, 2.7f, 2.85f, 3.0f };
	if (KitStep - 1 < (int32)UE_ARRAY_COUNT(ShotTimes) && T >= ShotTimes[KitStep - 1])
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("Celebrate_%02d.png"), KitStep)), true, false);
		++KitStep;
	}
	if (T > 3.6f)
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}

void APKGameMode::UpdateKitShots(float Dt)
{
	// Blue in front, Red behind, everyone facing the camera (-Y); then turned round for the backs.
	KitClock += Dt;
	auto Line = [this](float Yaw)
	{
		for (APKFootballer* F : Footballers)
		{
			const float X = -1100.f + F->Index * 220.f;
			F->Teleport2D(FVector(X, F->Team == 0 ? 0.f : 650.f, 0.f), Yaw);
			F->MoveInput = FVector::ZeroVector;
			F->bWatchBall = false; // keep facing the camera
			F->StunTimer = 0.f;
		}
		Ball->ClearControl();
		Ball->SetActorLocation(FVector(0.f, 3000.f, PK::BallRadius));
	};
	auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
	};
	const float Times[] = { 2.f, 4.f, 4.5f, 6.f, 6.5f, 8.f, 8.5f, 10.f, 10.5f, 12.f, 12.5f, 14.f, 15.f };
	const float Warmup = WITH_EDITOR ? 35.f : 0.f; // the editor build compiles the kit shaders on first use
	if (KitStep >= (int32)UE_ARRAY_COUNT(Times) || KitClock < Times[KitStep] + (KitStep > 0 ? Warmup : 0.f))
	{
		return;
	}
	switch (KitStep++)
	{
	case 0: Line(-90.f); KitCamLoc = FVector(0.f, -1500.f, 260.f); KitCamLook = FVector(0.f, 300.f, 110.f); break;
	case 1: Shot(TEXT("Kit_01_Lineup")); break;
	case 2: KitCamLoc = FVector(-660.f, -420.f, 150.f); KitCamLook = FVector(-660.f, 0.f, 110.f); break; // Blue #3-#6 close
	case 3: Shot(TEXT("Kit_02_BlueClose")); break;
	case 4: KitCamLoc = FVector(660.f, 230.f, 150.f); KitCamLook = FVector(660.f, 650.f, 110.f); break;  // Red close
	case 5: Shot(TEXT("Kit_03_RedClose")); break;
	case 6: Line(90.f); KitCamLoc = FVector(-440.f, -420.f, 150.f); KitCamLook = FVector(-440.f, 0.f, 120.f); break; // backs
	case 7: Shot(TEXT("Kit_04_BlueBacks")); break;
	case 8: Line(-90.f); KitCamLoc = FVector(-1100.f, -260.f, 160.f); KitCamLook = FVector(-1100.f, 0.f, 120.f); break; // keeper
	case 9: Shot(TEXT("Kit_05_Keeper")); break;
	case 10: KitCamLoc = FVector(-880.f, -150.f, 170.f); KitCamLook = FVector(-880.f, 0.f, 165.f); break; // face / hair
	case 11: Shot(TEXT("Kit_06_Head")); break;
	default: FGenericPlatformMisc::RequestExit(false); break;
	}
}

void APKGameMode::UpdateDefensiveLine()
{
	// One line for the back four: it follows the ball up and down the pitch, never sits ahead of the ball, and
	// drops off when the opposing passer has time to play a ball in behind (so runners stay in front of it).
	const FVector BallLoc = Ball->GetActorLocation();
	const APKFootballer* Carrier = Ball->GetOwnerPlayer();
	for (int32 Team = 0; Team < 2; ++Team)
	{
		float HomeSum = 0.f;
		int32 Count = 0;
		for (const APKFootballer* P : Footballers)
		{
			if (P->Team == Team && P->Role == EPKRole::Defender)
			{
				HomeSum += P->Home.X;
				++Count;
			}
		}
		if (Count == 0)
		{
			continue;
		}
		const float BallFx = WorldToFx(Team, BallLoc.X);
		float Line = HomeSum / Count + 0.42f * BallFx - 0.06f + Style(Team).LinePush + 0.05f * Mentality[Team];
		Line = FMath::Min(Line, BallFx - 0.08f);

		// The deepest opposing outfield player (towards our goal) and whether their carrier has time.
		float DeepestOpp = 1.f;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != Team && !O->IsKeeper())
			{
				DeepestOpp = FMath::Min(DeepestOpp, WorldToFx(Team, O->GetActorLocation().X));
			}
		}
		bool bPasserFree = false;
		if (Carrier && Carrier->Team != Team)
		{
			float Near = 0.f;
			bPasserFree = !NearestOpponent(Carrier, Near) || Near > 450.f;
		}
		if (bPasserFree)
		{
			Line = FMath::Min(Line, DeepestOpp - 0.03f); // drop: nobody gets in behind
		}
		Line = FMath::Clamp(Line, -0.9f, 0.25f);
		// Move the line smoothly (a line steps up and drops together, it doesn't jump).
		DefLine[Team] = FMath::FInterpTo(DefLine[Team], Line, GetWorld()->GetDeltaSeconds(), 2.5f);

		// A fullback still far upfield after the ball is lost: the others shift across to cover his side.
		VacatedSide[Team] = 0.f;
		CaughtUpfield[Team] = nullptr;
		const bool bDefending = Carrier && Carrier->Team != Team;
		if (bDefending)
		{
			for (APKFootballer* P : Footballers)
			{
				if (P->Team == Team && P->Role == EPKRole::Defender && FMath::Abs(P->Home.Y) > 0.5f
					&& WorldToFx(Team, P->GetActorLocation().X) > DefLine[Team] + 0.25f)
				{
					VacatedSide[Team] = FMath::Sign(P->Home.Y);
					CaughtUpfield[Team] = P;
					break;
				}
			}
		}
	}
}

void APKGameMode::AIDefend(APKFootballer* P, APKFootballer* Carrier)
{
	if (!Carrier)
	{
		MoveTo(P, PredictBall(P), true);
		return;
	}

	// Close down, then jockey goal-side and pick the moment to tackle.
	const FVector BallLoc = PK::Flat(Ball->GetActorLocation());
	const float Dist = FVector::Dist(P->GetFlatLocation(), BallLoc);
	if (Dist > Style(P->Team).PressDistance * (1.f + 0.15f * Mentality[P->Team]))
	{
		MoveTo(P, PredictBall(P), true);
		return;
	}

	if (FVector::Dist(BallLoc, Carrier->GetFootPoint()) > 100.f && Dist < 400.f)
	{
		// Carrier has lost touch of it: nip in and take it.
		MoveTo(P, BallLoc, true);
		P->Stance = EPKStance::Normal;
		return;
	}
	const FVector GoalSide = BallLoc + (OwnGoal(P->Team) - BallLoc).GetSafeNormal2D() * 140.f;
	MoveTo(P, GoalSide, Dist > 220.f);
	P->Stance = EPKStance::Jockey;
	P->FaceTarget = BallLoc;
	if (Dist < 150.f && P->TackleCooldown <= 0.f && FMath::FRand() < 0.02f)
	{
		TryTackle(P, 0.f);
	}
	else if (Dist > 220.f && Dist < 380.f && P->TackleCooldown <= 0.f && Carrier->ProtectedTimer <= 0.f && FMath::FRand() < (P->YellowCards > 0 ? 0.0015f : 0.004f))
	{
		// Occasionally go to ground from range.
		P->TackleCooldown = 3.f;
		P->SetActorRotation(FRotator(0.f, (BallLoc - P->GetFlatLocation()).Rotation().Yaw, 0.f));
		P->PlaySlideTackle();
	}
}

bool APKGameMode::HumanOnDeadBall(int32 Team) const
{
	const APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	return bHumanTeam[FMath::Clamp(Team, 0, 1)] && Human && Ball && Ball->GetOwnerPlayer() == Human && (Ball->IsPlaced() || Ball->GetHolder() == Human);
}

void APKGameMode::HumanKickOff(int32 Team, const FVector& Aim)
{
	// Pushing a direction at kick-off: your kicker plays the ball himself and dribbles that way
	// (J / Space still passes it to a team-mate).
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human || SetPiece != EPKSetPiece::Kickoff || !HumanOnDeadBall(Team) || Phase != EPKPhase::Playing || Human->IsKicking() || RestartTimer < 0.3f)
	{
		return;
	}
	const FVector Dir = Aim.GetSafeNormal2D();
	Ball->ClearControl();
	Ball->SetController(Human);
	Ball->Touch(Human, Dir * 260.f);
	Human->TouchTimer = 0.3f;
	Human->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
}

void APKGameMode::AITakeRestart(APKFootballer* P)
{
	P->MoveInput = FVector::ZeroVector;
	if (RestartTimer < 1.4f || Phase != EPKPhase::Playing || P->IsKicking())
	{
		return;
	}

	if (SetPiece == EPKSetPiece::Penalty)
	{
		RequestShot(P, (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(0.45f, 0.95f), FMath::FRandRange(0.6f, 0.9f));
		return;
	}
	if (SetPiece == EPKSetPiece::FreeKick && Ball->IsPlaced())
	{
		const FVector Spot = PK::Flat(Ball->GetActorLocation());
		if (FVector::Dist(Spot, OppGoal(P->Team)) < 2900.f && FMath::Abs(Spot.Y) < 1700.f && FMath::FRand() < 0.6f)
		{
			++FreeKickShots;
			RequestShot(P, Spot.Y * PK::AttackDir(P->Team) > 0.f ? -0.85f : 0.85f, FMath::FRandRange(0.65f, 0.9f));
			return;
		}
	}
	float Score_ = 0.f;
	APKFootballer* Target = BestPassTarget(P, false, Score_);
	if (Target)
	{
		const float D = FVector::Dist(P->GetFlatLocation(), Target->GetFlatLocation());
		RequestPass(P, Target, D > 2500.f ? EPKKickType::Lob : EPKKickType::Ground);
	}
	else
	{
		RequestPassToPoint(P, TeamToWorld(P->Team, 0.1f, FMath::FRandRange(-0.5f, 0.5f)), EPKKickType::Lob);
	}
}

void APKGameMode::AICarry(APKFootballer* P, float Dt)
{
	if (Ball->GetHolder() == P)
	{
		// Keeper with the ball in hand: look up, then distribute.
		P->MoveInput = FVector::ZeroVector;
		P->HoldTimer += Dt;
		if (P->HoldTimer > 1.2f && !P->IsKicking())
		{
			float PassScore = 0.f;
			if (APKFootballer* Target = BestPassTarget(P, false, PassScore))
			{
				const float D = FVector::Dist(P->GetFlatLocation(), Target->GetFlatLocation());
				RequestPass(P, Target, D > 2200.f ? EPKKickType::Lob : EPKKickType::Ground);
			}
			else
			{
				RequestPassToPoint(P, TeamToWorld(P->Team, 0.05f, FMath::FRandRange(-0.6f, 0.6f)), EPKKickType::Lob);
			}
		}
		return;
	}

	float NearOpp = 0.f;
	const APKFootballer* Opp = NearestOpponent(P, NearOpp);
	const FVector Loc = P->GetFlatLocation();

	// One-two return: first time, into the runner's path.
	const int32 TeamIdx = P->Team;
	if (OneTwoTimer[TeamIdx] > 0.f && OneTwoReceiver[TeamIdx].Get() == P && OneTwoPasser[TeamIdx].IsValid() && !P->IsKicking() && CanReachBall(P, KickReach))
	{
		RequestPass(P, OneTwoPasser[TeamIdx].Get(), EPKKickType::Through);
		OneTwoTimer[TeamIdx] = 0.f;
		++OneTwoReturns;
		return;
	}

	P->DecisionTimer -= Dt;
	if (P->DecisionTimer <= 0.f && !P->IsKicking() && CanReachBall(P, KickReach))
	{
		const FPKStyleParams& S = Style(P->Team);
		P->DecisionTimer = FMath::FRandRange(S.DecisionMin, S.DecisionMax);

		const float DGoal = FVector::Dist(Loc, OppGoal(P->Team));
		const bool bPressured = NearOpp < PressureDistance;
		P->Stance = EPKStance::Normal;

		if (P->IsKeeper())
		{
			float PassScore = 0.f;
			APKFootballer* Target = BestPassTarget(P, false, PassScore);
			if (Target && (PassScore > 0.8f || FMath::FRand() < 0.55f))
			{
				RequestPass(P, Target, EPKKickType::Ground);
			}
			else if (FMath::FRand() < 0.6f)
			{
				// Goal kick long toward the forwards.
				RequestPassToPoint(P, TeamToWorld(P->Team, FMath::FRandRange(0.05f, 0.3f), FMath::FRandRange(-0.5f, 0.5f)), EPKKickType::Clearance);
			}
			else
			{
				RequestPassToPoint(P, TeamToWorld(P->Team, 0.1f, FMath::FRandRange(-0.6f, 0.6f)), EPKKickType::Clearance);
			}
			return;
		}

		const float Range = ShootRange + 400.f * FMath::Clamp((P->Attr.Shooting - 0.6f) / 0.35f, 0.f, 1.f);
		if (DGoal < Range || (DGoal < Range * 1.35f && !bPressured && FMath::FRand() < 0.55f))
		{
			RequestShot(P, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.55f, 1.f), false, ChooseAIShot(P, DGoal));
			return;
		}

		if (TryCross(P))
		{
			return;
		}
		if (Opp && NearOpp < 260.f && P->Attr.Control > 0.72f && FMath::FRand() < 0.12f
			&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), (OppGoal(P->Team) - Loc).GetSafeNormal2D()) > 0.6f)
		{
			FlickUp(P, (OppGoal(P->Team) - Loc).GetSafeNormal2D());
			return;
		}
		// Take a man on with a skill: technical players, defender close in front.
		if (Opp && NearOpp < 300.f && P->Attr.Control > 0.68f && FMath::FRand() < 0.1f + (P->HasTrait(PKTrait::Technical) ? 0.06f : 0.f)
			&& FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), P->GetFacing()) > 0.5f)
		{
			const float R = FMath::FRand();
			const int32 Skill = NearOpp < 210.f && R < 0.3f ? 4 : (R < 0.6f ? 0 : (R < 0.8f ? 2 : 1));
			if (PerformSkill(P, Skill, FMath::FRand() < 0.5f ? -1.f : 1.f))
			{
				return;
			}
		}

		float PassScore = 0.f;
		APKFootballer* Target = BestPassTarget(P, false, PassScore);
		// Open grass ahead: carry it forward unless a clearly better pass is on.
		bool bSpaceAhead = true;
		const FVector Fwd = (OppGoal(P->Team) - Loc).GetSafeNormal2D();
		for (APKFootballer* O : Footballers)
		{
			const FVector To = O->GetFlatLocation() - Loc;
			if (O->Team != P->Team && To.Size() < 900.f && FVector::DotProduct(To.GetSafeNormal2D(), Fwd) > 0.5f)
			{
				bSpaceAhead = false;
				break;
			}
		}
		if (Target && bSpaceAhead && !bPressured && PassScore < 1.2f && Target->RunTimer <= 0.f)
		{
			Target = nullptr;
		}
		// Final third with space: attack the goal instead of recycling the ball backwards.
		if (Target && DGoal < 3400.f && !bPressured && WorldToFx(P->Team, Target->GetActorLocation().X) < WorldToFx(P->Team, Loc.X) + 0.05f)
		{
			Target = nullptr;
		}
		if (Target && ((bPressured && PassScore > -0.6f) || (PassScore > 0.5f && FMath::FRand() < S.PassUrge) || (NearOpp < 450.f && PassScore > 0.f)))
		{
			const bool bAhead = WorldToFx(P->Team, Target->GetActorLocation().X) > WorldToFx(P->Team, Loc.X) + 0.12f;
			const float D = FVector::Dist(Loc, Target->GetFlatLocation());
			EPKKickType Type = EPKKickType::Ground;
			if (D > 2500.f || (D > 1600.f && FMath::FRand() < S.LobBias + 0.1f))
			{
				Type = EPKKickType::Lob;
			}
			else if ((bAhead && FMath::FRand() < S.ThroughChance) || Target->RunTimer > 0.f)
			{
				Type = EPKKickType::Through;
			}
			RequestPass(P, Target, Type);
			// Give-and-go: a short pass under pressure in the opponents' half, then run for the return.
			if (Type == EPKKickType::Ground && bPressured && D < 1300.f && P->Role != EPKRole::Defender && WorldToFx(P->Team, Loc.X) > 0.05f && FMath::FRand() < 0.3f)
			{
				OneTwoPasser[P->Team] = P;
				OneTwoReceiver[P->Team] = Target;
				OneTwoTimer[P->Team] = 3.f;
				SendRun(Target, P);
				++OneTwos;
			}
			// Space in behind and a forward to use it: send him (the pass can follow later).
			else if (!bPressured && FMath::FRand() < 0.15f + 0.1f * Mentality[P->Team])
			{
				SendRun(P);
			}
			return;
		}

		// Under pressure with nothing on: shield it.
		if (bPressured && Opp && FVector::DotProduct((Opp->GetFlatLocation() - Loc).GetSafeNormal2D(), (OppGoal(P->Team) - Loc).GetSafeNormal2D()) > 0.3f && FMath::FRand() < 0.35f)
		{
			P->Stance = EPKStance::Shield;
		}
		P->AIDribbleDir = ComputeDribbleDir(P);
	}

	// Dribble: head for the target direction but stay on the ball.
	const FVector ToBall = PK::Flat(Ball->GetActorLocation()) - P->GetFootPoint();
	// Ball away from the feet (behind him, knocked loose in a duel): go and get it first. Adding the dribble
	// direction here used to cancel out when the ball was behind, leaving the carrier frozen.
	FVector Move = ToBall.Size() > 70.f ? ToBall.GetSafeNormal2D() + P->AIDribbleDir * 0.2f : P->AIDribbleDir + ToBall / 120.f;
	if (P->Stance == EPKStance::Shield && Opp)
	{
		P->FaceTarget = Opp->GetActorLocation();
		Move = ToBall / 80.f;
	}
	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = NearOpp > 500.f && P->Stance == EPKStance::Normal;
	// Closing defender: keep the speed up but the ball tight.
	P->bControlledSprint = !P->bSprint && NearOpp > 180.f && P->Stance == EPKStance::Normal && P->Attr.Control > 0.55f;
}

void APKGameMode::AIKeeper(APKFootballer* P)
{
	const FVector Goal = OwnGoal(P->Team);
	const FVector BallLoc = PK::Flat(Ball->GetActorLocation());
	const float Dir = PK::AttackDir(P->Team);
	const bool bLoose = Ball->IsFree() && !Ball->GetController();
	const FVector BallVel = PK::Flat(Ball->Velocity);

	// Come and claim a loose, slow ball in the box.
	if (bLoose && InOwnBox(P, BallLoc) && BallVel.Size() < 900.f)
	{
		MoveTo(P, PredictBall(P), true);
		return;
	}

	const APKFootballer* Carrier = Ball->GetOwnerPlayer();
	const bool bOppCarrier = Carrier && Carrier->Team != P->Team && !Ball->GetHolder();
	const float CarrierToGoal = bOppCarrier ? FVector::Dist(Carrier->GetFlatLocation(), Goal) : 1.e9f;

	// Keeper rush (the person asked): charge the carrier and go down at his feet when close.
	if (!bOldKeeper && bKeeperRush[P->Team] && bOppCarrier && CarrierToGoal < 3500.f)
	{
		MoveTo(P, BallLoc, true);
		if (FVector::Dist(P->GetFlatLocation(), BallLoc) < 260.f && P->LungeTimer <= 0.f)
		{
			P->LungeTimer = 0.45f; // smother: longer reach for the pick-up
			++KeeperSmothers;
		}
		return;
	}

	if (!bOldKeeper)
	{
		// Cross into the area: come and claim it if he gets there first.
		if (bLoose && Ball->GetActorLocation().Z > 120.f && BallVel.Size() > 400.f)
		{
			const FVector Drop = PredictAerial(P, 150.f, 265.f);
			const float DropToGoal = FMath::Abs(Drop.X - Goal.X);
			if (DropToGoal < 700.f && FMath::Abs(Drop.Y) < 1100.f)
			{
				float MyT = FVector::Dist(P->GetFlatLocation(), Drop) / PK::SprintSpeed;
				bool bFirst = true;
				for (const APKFootballer* O : Footballers)
				{
					if (O->Team != P->Team && FVector::Dist(O->GetFlatLocation(), Drop) / PK::SprintSpeed < MyT - 0.25f)
					{
						bFirst = false; // an attacker is clearly there first: stay on the line
					}
				}
				if (bFirst)
				{
					MoveTo(P, Drop, true);
					return;
				}
			}
		}

		// One-on-one: an attacker through on goal with nobody goal-side of him. Narrow the angle, then smother.
		if (bOppCarrier && CarrierToGoal < 2400.f)
		{
			const FVector CarrierLoc = Carrier->GetFlatLocation();
			bool bCovered = false;
			for (const APKFootballer* D : Footballers)
			{
				if (D->Team == P->Team && D != P && !D->IsStunned())
				{
					const float Along = FVector::DotProduct(D->GetFlatLocation() - Goal, (CarrierLoc - Goal).GetSafeNormal2D());
					const float Off = FMath::Abs(FVector::DotProduct(D->GetFlatLocation() - Goal, FVector::CrossProduct(FVector::UpVector, (CarrierLoc - Goal).GetSafeNormal2D())));
					if (Along > 0.f && Along < CarrierToGoal && Off < 250.f)
					{
						bCovered = true;
						break;
					}
				}
			}
			if (!bCovered)
			{
				const float Out = FMath::Min(CarrierToGoal * 0.42f, 950.f);
				MoveTo(P, Goal + (CarrierLoc - Goal).GetSafeNormal2D() * Out, true);
				if (FVector::Dist(P->GetFlatLocation(), BallLoc) < 240.f && P->LungeTimer <= 0.f)
				{
					P->LungeTimer = 0.45f;
					++KeeperSmothers;
				}
				return;
			}
		}
	}

	// Shot coming: get across to where it will cross the line.
	if (bLoose && BallVel.X * -Dir > 700.f)
	{
		const FVector Loc = P->GetFlatLocation();
		const float T = (Loc.X - BallLoc.X) / BallVel.X;
		if (T > 0.f && T < 1.6f)
		{
			// Read the bend too: a curled shot starts wide and comes back (spin decays over the flight).
			const float Curl = bOldKeeper ? 0.f : 0.5f * Ball->Spin.Y * T * T * FMath::Max(0.f, 1.f - 0.6f * T);
			const float Y = FMath::Clamp(BallLoc.Y + BallVel.Y * T + Curl, -PK::GoalHalfWidth - 80.f, PK::GoalHalfWidth + 80.f);
			PrepareSave(P, T, BallLoc.Y + BallVel.Y * T + Curl);
			MoveTo(P, FVector(Loc.X, Y, 0.f), true);
			return;
		}
	}

	// Otherwise hold the arc between ball and goal centre, further off the line when play is far away.
	const FVector ToBall = (BallLoc - Goal).GetSafeNormal2D();
	const float Off = FMath::GetMappedRangeValueClamped(FVector2D(800.f, 4000.f), FVector2D(120.f, 600.f), FVector::Dist(BallLoc, Goal));
	FVector Target = Goal + ToBall * Off;
	Target.X = Goal.X + Dir * FMath::Max(50.f, ToBall.X * Dir * Off);
	Target.Y = FMath::Clamp(Target.Y, -PK::GoalHalfWidth, PK::GoalHalfWidth);
	MoveTo(P, Target, false);
}

void APKGameMode::MoveTo(APKFootballer* P, const FVector& Target, bool bUrgent)
{
	const FVector Loc = P->GetFlatLocation();
	const FVector Delta = PK::Flat(Target) - Loc;
	const float D = Delta.Size();
	FVector Move = D > 50.f ? Delta / D * FMath::Clamp(D / 300.f, 0.25f, 1.f) : FVector::ZeroVector;

	if (!bUrgent)
	{
		for (APKFootballer* Mate : Footballers)
		{
			if (Mate == P || Mate->Team != P->Team)
			{
				continue;
			}
			const FVector Away = Loc - Mate->GetFlatLocation();
			const float Dist = Away.Size();
			if (Dist < 600.f && Dist > 1.f)
			{
				Move += Away / Dist * (1.f - Dist / 600.f) * 0.5f;
			}
		}
	}

	P->MoveInput = Move.GetClampedToMaxSize(1.f);
	P->bSprint = bUrgent ? D > 300.f : (D > 1200.f && P->Stamina > 0.3f); // tired: save the legs unless it matters
}

FVector APKGameMode::SupportSpot(const APKFootballer* P, bool bAttacking) const
{
	const FVector BallLoc = Ball->GetActorLocation();
	const float BallFx = WorldToFx(P->Team, BallLoc.X);
	const float BallFy = PK::AttackDir(P->Team) * BallLoc.Y / PK::HalfWidth;

	float Shift = 0.5f;
	float Push = bAttacking ? 0.16f : -0.08f;
	switch (P->Role)
	{
	case EPKRole::Defender:   Shift = 0.42f; Push = bAttacking ? 0.12f : -0.06f; break;
	case EPKRole::Midfielder: Shift = 0.55f; break;
	case EPKRole::Forward:    Shift = 0.6f; Push = bAttacking ? 0.22f : -0.04f; break;
	default: break;
	}

	float Fx = P->Home.X + Shift * BallFx + Push + Style(P->Team).LinePush + 0.06f * Mentality[P->Team];
	if (bAttacking && P->Role == EPKRole::Forward)
	{
		Fx = FMath::Max(Fx, BallFx + 0.18f); // stretch the defence
	}
	if (P->Role == EPKRole::Defender)
	{
		Fx = FMath::Min(Fx, BallFx - 0.05f);
		if (!bAttacking && !bOldLine)
		{
			Fx = DefLine[P->Team]; // one flat line
		}
	}
	Fx = FMath::Clamp(Fx, -0.92f, 0.86f);

	float Fy = P->Home.Y * 0.9f + 0.28f * BallFy;
	if (!bAttacking && !bOldLine && P->Role == EPKRole::Defender && VacatedSide[P->Team] != 0.f && CaughtUpfield[P->Team].Get() != P)
	{
		Fy += 0.11f * VacatedSide[P->Team]; // cover the side a caught-upfield fullback left open
	}
	if (bAttacking && P->Role != EPKRole::Defender)
	{
		Fy += 0.06f * FMath::Sin(GetWorld()->GetTimeSeconds() * 0.6f + P->Index * 1.7f);
	}
	Fy = FMath::Clamp(Fy, -0.92f, 0.92f);

	return TeamToWorld(P->Team, Fx, Fy);
}

FVector APKGameMode::PredictBall(const APKFootballer* P) const
{
	const FVector BallLoc = PK::Flat(Ball->GetActorLocation());
	if (!Ball->IsFree())
	{
		return BallLoc;
	}
	const float Dist = FVector::Dist(BallLoc, P->GetFlatLocation());
	const float T = FMath::Clamp(Dist / 750.f, 0.f, 1.2f);
	FVector Point = BallLoc + PK::Flat(Ball->Velocity) * T;
	Point.X = FMath::Clamp(Point.X, -PK::HalfLength - 150.f, PK::HalfLength + 150.f);
	Point.Y = FMath::Clamp(Point.Y, -PK::HalfWidth - 150.f, PK::HalfWidth + 150.f);
	return Point;
}

float APKGameMode::BallArrivalTime(const FVector& Point, float* OutMissBy) const
{
	FVector Pos = Ball->GetActorLocation();
	FVector Vel = Ball->Velocity;
	const bool bRolling = Pos.Z <= PK::BallRadius + 2.f && FMath::Abs(Vel.Z) < 50.f;
	float Best = FVector::Dist(Pos, Point);
	float BestT = 0.f;
	constexpr float Step = 1.f / 60.f;
	for (float T = Step; T < 1.5f; T += Step)
	{
		if (!bRolling)
		{
			Vel.Z -= PK::Gravity * Step;
		}
		Vel *= 1.f - PK::AirDrag * Step;
		Pos += Vel * Step;
		Pos.Z = FMath::Max(Pos.Z, PK::BallRadius);
		const float D = FVector::Dist(Pos, Point);
		if (D < Best)
		{
			Best = D;
			BestT = T;
		}
	}
	if (OutMissBy)
	{
		*OutMissBy = Best;
	}
	return BestT;
}

void APKGameMode::PrepareSave(APKFootballer* GK, float SecondsToLine, float CrossY)
{
	if (bOldKeeper || GK->IsInAction() || GK->SaveKickId == Ball->KickId)
	{
		return;
	}
	const float Z = Ball->GetActorLocation().Z + Ball->Velocity.Z * SecondsToLine - 0.5f * PK::Gravity * SecondsToLine * SecondsToLine;
	if (Z > 290.f || Z < -50.f)
	{
		return; // over the bar
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, GK->GetFacing());
	const float Lateral = FVector::DotProduct(FVector(0.f, CrossY - GK->GetActorLocation().Y, 0.f), Right);
	const float Speed = Ball->Velocity.Size();
	EPKAction Action = EPKAction::GKCatch;
	const bool bDive = FMath::Abs(Lateral) > 120.f;
	if (bDive)
	{
		Action = Lateral >= 0.f ? EPKAction::GKDiveRight : EPKAction::GKDiveLeft;
	}
	else if (Z < 45.f)
	{
		Action = Speed > 1200.f ? EPKAction::GKBlock : EPKAction::GKScoop;
	}
	else if (Z < 90.f)
	{
		Action = EPKAction::GKCatchLow;
	}
	else if (Z > 190.f)
	{
		Action = EPKAction::GKCatchHigh;
	}
	const FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);
	if (!Clip.IsValid())
	{
		return;
	}
	// Start when the clip, at up to 1.6x, reaches its contact as the ball reaches him.
	const float Lead = Clip.Contact - Clip.Start;
	if (SecondsToLine > Lead / 0.9f)
	{
		return; // not yet
	}
	GK->SaveKickId = Ball->KickId;
	++TimedSaves;
	if (bDive)
	{
		GK->Dive(Lateral >= 0.f ? 1.f : -1.f, SecondsToLine);
	}
	else
	{
		GK->PlayActionTimed(Action, SecondsToLine, 1.8f);
	}
}

FVector APKGameMode::PredictAerial(const APKFootballer* P, float MinZ, float MaxZ) const
{
	FVector Pos = Ball->GetActorLocation();
	FVector Vel = Ball->Velocity;
	const FVector Loc = P->GetFlatLocation();
	constexpr float Step = 1.f / 30.f;
	for (float T = Step; T < 3.f; T += Step)
	{
		Vel.Z -= PK::Gravity * Step;
		Vel *= 1.f - PK::AirDrag * Step;
		Pos += Vel * Step;
		if (Pos.Z < PK::BallRadius)
		{
			break; // lands first: go to where it comes down
		}
		if (Pos.Z >= MinZ && Pos.Z <= MaxZ && Vel.Z < 0.f && FVector::Dist(PK::Flat(Pos), Loc) <= PK::SprintSpeed * 0.9f * T + 60.f)
		{
			return PK::Flat(Pos);
		}
	}
	return PK::Flat(Pos);
}

FVector APKGameMode::ComputeDribbleDir(const APKFootballer* P) const
{
	const FVector Loc = P->GetFlatLocation();
	const FVector ToGoal = (OppGoal(P->Team) - Loc).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToGoal);
	FVector Dir = ToGoal;
	const float Fx = WorldToFx(P->Team, Loc.X);
	if (FMath::Abs(Loc.Y) > 1100.f && Fx > 0.05f && Fx < 0.8f)
	{
		// Wide man: go down the line toward the byline to cross.
		Dir = FVector(PK::AttackDir(P->Team), 0.f, 0.f) + ToGoal * 0.25f;
	}

	for (APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const FVector D = O->GetFlatLocation() - Loc;
		const float Dist = D.Size();
		if (Dist < 600.f && FVector::DotProduct(D.GetSafeNormal2D(), ToGoal) > 0.3f)
		{
			const float Away = FVector::DotProduct(D, Side) > 0.f ? -1.f : 1.f;
			Dir += Side * Away * (1.f - Dist / 600.f) * 1.5f;
		}
	}

	if (FMath::Abs(Loc.Y) > PK::HalfWidth - 400.f)
	{
		Dir.Y -= FMath::Sign(Loc.Y) * 0.8f;
	}
	return Dir.GetSafeNormal2D();
}

APKFootballer* APKGameMode::BestPassTarget(const APKFootballer* P, bool bAllowKeeper, float& OutScore) const
{
	APKFootballer* Best = nullptr;
	OutScore = -TNumericLimits<float>::Max();
	const FVector Loc = P->GetFlatLocation();
	const float MyFx = WorldToFx(P->Team, Loc.X);
	const FPKStyleParams& S = Style(P->Team);
	const float MaxDist = S.MaxPassDist * (P->HasTrait(PKTrait::TikiTaka) ? 0.85f : 1.f);

	for (APKFootballer* T : Footballers)
	{
		if (T == P || T->Team != P->Team || (T->IsKeeper() && !bAllowKeeper) || T->StunTimer > 0.5f)
		{
			continue; // (a team-mate on the ground can't take a pass)
		}
		if (IsOffsidePosition(T, P->GetActorLocation().X) && FMath::FRand() > 0.12f) // the odd misjudged pass
		{
			continue;
		}
		const bool bRunner = T->RunTimer > 0.f && !T->AITarget.IsNearlyZero();
		const FVector TLoc = bRunner ? FMath::Lerp(T->GetFlatLocation(), PK::Flat(T->AITarget), 0.6f) : T->GetFlatLocation();
		const float D = FVector::Dist(Loc, TLoc);
		if (D < 400.f || D > MaxDist)
		{
			continue;
		}

		float Lane = TNumericLimits<float>::Max();
		float Open = TNumericLimits<float>::Max();
		for (APKFootballer* O : Footballers)
		{
			if (O->Team == P->Team)
			{
				continue;
			}
			const FVector OLoc = O->GetFlatLocation();
			Lane = FMath::Min(Lane, FMath::PointDistToSegment(OLoc, Loc, TLoc));
			Open = FMath::Min(Open, FVector::Dist(OLoc, TLoc));
		}
		if (Lane < 110.f)
		{
			continue;
		}

		const float Progress = WorldToFx(P->Team, TLoc.X) - MyFx;
		const float Safety = FMath::Clamp(Open / 700.f, 0.f, 1.f) * 0.9f + FMath::Clamp(Lane / 500.f, 0.f, 1.f) * 0.5f;
		const float Rating = Progress * S.Progress + Safety * S.Safety - D / 6000.f + (bRunner ? 0.35f : 0.f);
		if (Rating > OutScore)
		{
			OutScore = Rating;
			Best = T;
		}
	}
	return Best;
}

APKFootballer* APKGameMode::NearestOpponent(const APKFootballer* P, float& OutDist) const
{
	APKFootballer* Best = nullptr;
	OutDist = TNumericLimits<float>::Max();
	for (APKFootballer* O : Footballers)
	{
		if (O->Team != P->Team)
		{
			const float D = FVector::Dist(O->GetFlatLocation(), P->GetFlatLocation());
			if (D < OutDist)
			{
				OutDist = D;
				Best = O;
			}
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------
// Ball control: touch dribbling, kicks, tackles, pickups
// ---------------------------------------------------------------------------------------------

bool APKGameMode::CanReachBall(const APKFootballer* P, float Reach, float MaxHeight) const
{
	if (Ball->GetOwnerPlayer() == P && !Ball->IsFree())
	{
		return true;
	}
	const FVector B = Ball->GetActorLocation();
	return B.Z < MaxHeight && FVector::Dist(PK::Flat(B), P->GetFootPoint()) < Reach;
}

void APKGameMode::UpdateDribble(APKFootballer* P, float Dt)
{
	// Active-Touch-style dribbling. The ball is never pulled by an invisible force: every touch happens on a
	// real step of the animated feet, with the foot that is swinging, and sends the ball exactly far enough
	// to meet that player's foot again N steps later (rolling friction included). Between touches the player
	// lengthens or shortens his stride a little so the foot and the ball meet.
	if (bOldDribble || P->Stance == EPKStance::Shield)
	{
		UpdateDribbleLegacy(P, Dt);
		return;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Flat = PK::Flat(BallLoc);
	const FVector Body = P->GetFlatLocation();
	const float Dist = FVector::Dist(Flat, Body);
	if (Dist > ControlLoseDistance)
	{
		Ball->SetController(nullptr); // heavy touch / outran it: ball is loose
		++DribbleLosses;
		return;
	}
	const bool bSwing = P->SwingCounter != P->SeenSwingCounter;
	P->SeenSwingCounter = P->SwingCounter;
	if (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None || P->BufferTimer > 0.f)
	{
		P->DribbleSpeedScale = 1.f;
		return;
	}
	if (bSwing)
	{
		++P->StepsSinceTouch;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Vel = PK::Flat(P->GetVelocity());
	const float Speed = Vel.Size();
	const FVector Facing = P->GetFacing();
	FVector Desired = P->MoveInput.IsNearlyZero() ? Facing : P->MoveInput.GetSafeNormal2D();
	const bool bClose = P->Stance == EPKStance::CloseControl;
	const bool bSprinting = P->IsSprinting() && !bClose;
	const bool bControlled = P->bControlledSprint && !bSprinting && !bClose;

	// Touch rhythm: one touch per step in close control, every second step at jog, every third when sprinting.
	// Poor control means longer, less frequent touches; Technical players keep it tighter. With an opponent close
	// the rhythm shortens by a step (more, smaller touches), as players do in matches.
	const float Control = FMath::Clamp(P->Attr.Control * (P->HasTrait(PKTrait::Technical) ? 1.06f : 1.f), 0.f, 1.f);
	int32 StepsPerTouch = bClose ? 1 : (bSprinting ? (Control > 0.75f ? 3 : 4) : 2);
	if (bControlled)
	{
		StepsPerTouch = Control > 0.7f ? 1 : 2; // controlled sprint: a touch every step for good dribblers
	}
	float PressDist = 0.f;
	if (NearestOpponent(P, PressDist) && PressDist < 350.f)
	{
		StepsPerTouch = FMath::Max(1, StepsPerTouch - 1);
	}

	// Turning: the body can only take the ball ~40 degrees off its facing (55 with the outside of the foot, which
	// can push it across the line of the body); a sharp turn is a short touch on the next step.
	const float TurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Desired), -1.f, 1.f)));
	const bool bTurnTouch = TurnDeg > 35.f;
	const float TurnSign = FVector::CrossProduct(Facing, Desired).Z >= 0.f ? 1.f : -1.f; // +1 turning right
	if (TurnDeg > 40.f)
	{
		Desired = Facing.RotateAngleAxis(40.f * TurnSign, FVector::UpVector);
	}
	if (bTurnTouch)
	{
		StepsPerTouch = 1;
	}
	// A cut is taken with the outside of the foot on the side of the turn: the right foot when cutting right.
	const int32 OutsideFoot = TurnSign > 0.f ? 1 : 0;

	// Standing still: the ball is stopped under the sole.
	if (P->MoveInput.IsNearlyZero() && Speed < 120.f)
	{
		P->DribbleSpeedScale = 1.f;
		const FVector Sole = P->GetFootPoint();
		if (FVector::Dist(Flat, Sole) < 80.f)
		{
			const FVector Eased = FMath::VInterpTo(Flat, Sole, Dt, 4.f);
			Ball->SetActorLocation(FVector(Eased.X, Eased.Y, BallLoc.Z));
			Ball->Velocity.X *= FMath::Max(0.f, 1.f - 6.f * Dt);
			Ball->Velocity.Y *= FMath::Max(0.f, 1.f - 6.f * Dt);
		}
		return;
	}

	// Where the ball should be when a foot reaches it: just in front of the swing line.
	const FVector MeetZone = Body + Desired * 45.f;
	const float Gap = FVector::DotProduct(Flat - MeetZone, Desired); // + ahead of the zone, - behind it
	const float Lateral = FMath::Abs(FVector::DotProduct(Flat - Body, FVector::CrossProduct(FVector::UpVector, Desired)));
	// The same along the way the body is still going: on a cut the ball is "behind" the new direction long before
	// the player can actually run over it.
	const float RunGap = FVector::DotProduct(Flat - (Body + Facing * 45.f), Facing);
	const float RunLateral = FMath::Abs(FVector::DotProduct(Flat - Body, FVector::CrossProduct(FVector::UpVector, Facing)));

	// The stepping rhythm: when the next foot comes through.
	const float StepT = FMath::Clamp(P->StepInterval, 0.16f, 0.5f);
	const float SinceStep = (float)(Now - P->LastSwingTime);
	const float NextStepIn = SinceStep < 0.8f ? FMath::Max(0.f, StepT - SinceStep) : 1.f;

	bool bTouch = false;
	bool bOnStep = false;
	bool bOutside = false;
	bool bInsideCut = false;
	FVector Contact = Flat;
	float FootGap = 0.f;
	if (bSwing && P->TouchTimer <= 0.f)
	{
		const float Reach = (bClose ? 45.f : 55.f) + (bTurnTouch ? 10.f : 0.f);
		const bool bDue = P->StepsSinceTouch >= StepsPerTouch || Gap < -10.f || bTurnTouch;
		if (bTurnTouch)
		{
			// The outside foot takes the cut. The other foot only plays it (with its inside) when the ball would
			// be overrun before the outside foot comes through on the next step.
			const bool bOutsideFoot = P->LastSwingFoot == OutsideFoot;
			Contact = P->GetFootSideContactPoint(P->LastSwingFoot, TurnSign);
			FootGap = FVector::Dist(Flat, Contact);
			const bool bCanWait = RunGap > -25.f && StepT < 0.42f;
			if (FootGap < Reach && (bOutsideFoot || !bCanWait))
			{
				bTouch = bOnStep = true;
				bOutside = bOutsideFoot;
				bInsideCut = !bOutsideFoot;
			}
			else if (bOutsideFoot)
			{
				++SwingFar;
			}
		}
		else if (TurnDeg > 12.f && !bClose)
		{
			// Steering gently: the outside of the foot on that side guides the ball round. The inside foot lets it
			// run one more step unless the ball is getting under the body.
			const bool bOutsideFoot = P->LastSwingFoot == OutsideFoot;
			const bool bWait = !bOutsideFoot && RunGap > -15.f && P->StepsSinceTouch <= StepsPerTouch;
			Contact = bOutsideFoot ? P->GetFootSideContactPoint(P->LastSwingFoot, TurnSign) : P->GetFootContactPoint(P->LastSwingFoot);
			FootGap = FVector::Dist(Flat, Contact);
			if (FootGap < Reach && bDue && !bWait)
			{
				bTouch = bOnStep = true;
				bOutside = bOutsideFoot;
			}
			else if (bDue && !bWait)
			{
				++SwingFar;
			}
		}
		else
		{
			Contact = P->GetFootContactPoint(P->LastSwingFoot);
			FootGap = FVector::Dist(Flat, Contact);
			if (FootGap < Reach && bDue)
			{
				bTouch = bOnStep = true;
			}
			else if (bDue)
			{
				++SwingFar;
			}
		}
	}
	if (Speed < 60.f)
	{
		SlowDribbleTime += Dt;
	}
	// No steps seen for a while and the ball is under the body: play it from the front foot. Starting from a
	// standstill, the first step is waited for.
	if (!bTouch && SinceStep > FMath::Max(0.6f, StepT * 2.2f) && P->TouchTimer <= 0.f && FVector::Dist(Flat, P->GetFootPoint()) < 60.f && RunGap < -10.f)
	{
		bTouch = true;
		++FallbackTouches;
		UE_LOG(LogPitchKings, Verbose, TEXT("[DribbleDiag] fallback speed %.0f sinceSwing %.2f turn %.0f gap %.0f"), Speed, SinceStep, TurnDeg, Gap);
	}
	// About to run over the ball between steps: nudge it on now rather than stepping on it, unless a foot is about
	// to come through anyway.
	if (!bTouch && RunGap < -30.f && Dist < 70.f && RunLateral < 40.f && P->TouchTimer <= 0.f && (NextStepIn > 0.09f || RunGap < -42.f))
	{
		bTouch = true;
		if (SinceStep < 0.12f)
		{
			// The foot that just came through is still swinging forward: it takes it (on a cut, with its outside
			// if it is the outside foot, otherwise its inside).
			bOnStep = true;
			bOutside = bTurnTouch && P->LastSwingFoot == OutsideFoot;
			bInsideCut = bTurnTouch && !bOutside;
			Contact = bTurnTouch ? P->GetFootSideContactPoint(P->LastSwingFoot, TurnSign) : P->GetFootContactPoint(P->LastSwingFoot);
			FootGap = FVector::Dist(Flat, Contact);
		}
		else
		{
			++OverrunTouches;
		}
		UE_LOG(LogPitchKings, Verbose, TEXT("[DribbleDiag] overrun speed %.0f sinceSwing %.2f step %.2f gap %.0f steps %d/%d turn %.0f"), Speed, SinceStep, P->StepInterval, Gap, P->StepsSinceTouch, StepsPerTouch, TurnDeg);
	}

	if (bTouch)
	{
		// Plan the meeting: the player's position N steps from now, the ball rolling there with friction.
		const float T = FMath::Max(0.24f, StepT * StepsPerTouch);
		const float TopSpeed = (bClose ? PK::WalkSpeed : (bSprinting ? PK::SprintSpeed : (bControlled ? PK::ControlledSprintSpeed : PK::JogSpeed))) * 0.93f * P->PaceScale * FMath::Clamp(P->MoveInput.Size(), 0.3f, 1.f);
		// Distance the player will really cover in T: accelerate (or slow) towards his top speed.
		const float Accel = FMath::Lerp(2100.f, 900.f, FMath::Clamp(Speed / PK::SprintSpeed, 0.f, 1.f)) * 0.8f;
		const float TimeToTop = FMath::Abs(TopSpeed - Speed) / Accel;
		const float Covered = TimeToTop >= T
			? Speed * T + 0.5f * FMath::Sign(TopSpeed - Speed) * Accel * T * T
			: (Speed + TopSpeed) * 0.5f * TimeToTop + TopSpeed * (T - TimeToTop);
		// The outside of the foot cuts the ball further across; the body follows it round.
		FVector TouchDir = Desired;
		if (bOutside && TurnDeg > 40.f)
		{
			TouchDir = Facing.RotateAngleAxis(FMath::Min(TurnDeg, 55.f) * TurnSign, FVector::UpVector);
		}
		const FVector Target = Body + TouchDir * (FMath::Max(Covered, 40.f) + 45.f);
		FVector ToTarget = Target - Flat;
		const float S = FMath::Max(ToTarget.Size(), 1.f);
		ToTarget /= S;
		float V0 = S / T + 0.5f * PK::RollDecel * T;
		// Touch quality: a less controlled player's touch wanders and its weight varies.
		const float Wander = (1.f - Control) * (bSprinting ? 10.f : 6.f);
		ToTarget = ToTarget.RotateAngleAxis(FMath::FRandRange(-Wander, Wander), FVector::UpVector);
		V0 *= 1.f + FMath::FRandRange(-0.12f, 0.12f) * (1.f - Control);
		FVector NewVel = ToTarget * FMath::Clamp(V0, 60.f, 1600.f);
		if (bOnStep)
		{
			// The touching foot (its toes, or its outside / inside on a cut) is where the ball leaves from.
			const FVector Eased = FMath::Lerp(Flat, Contact, 0.4f);
			Ball->SetActorLocation(FVector(Eased.X, Eased.Y, BallLoc.Z));
			P->NoteFootTouch(P->LastSwingFoot, (bTurnTouch || bOutside) ? TurnSign * FMath::Clamp(TurnDeg / 45.f, 0.5f, 1.f) : 0.f, bOutside); // a gentle steer is a smaller movement
			TouchGapSum += FootGap;
			++StepTouches;
			OutsideTouches += bOutside ? 1 : 0;
			if (bOutside && bDribbleTest && bCloseCamera && CutShots < 4 && CutShotTimer < 0.f)
			{
				CutShotTimer = 0.07f;
			}
			InsideCuts += bInsideCut ? 1 : 0;
		}
		Ball->Touch(P, NewVel);
		P->StepsSinceTouch = 0;
		P->TouchTimer = 0.12f;
		++DribbleTouches;
	}

	// Path to the ball: if the ball is off the line of the requested direction (after a turn, a bad touch or a
	// deflection), steer the body to where the ball will be so the next step can reach it.
	const FVector BallSoon = Flat + PK::Flat(Ball->Velocity) * 0.25f;
	const FVector ToBallSoon = BallSoon - Body;
	const float SideOff = FMath::Abs(FVector::DotProduct(ToBallSoon, FVector::CrossProduct(FVector::UpVector, Desired)));
	const float AheadOff = FVector::DotProduct(ToBallSoon, Desired);
	float SteerW = FMath::Clamp((SideOff - 20.f) / 45.f, 0.f, 0.9f);
	if (AheadOff < 10.f && ToBallSoon.Size() > 40.f)
	{
		SteerW = 0.9f; // the ball is behind: turn back for it
	}
	P->DribbleSteer = ToBallSoon.GetSafeNormal2D();
	P->DribbleSteerWeight = FMath::FInterpTo(P->DribbleSteerWeight, SteerW, Dt, 10.f);

	// Stride adjustment: run a touch faster to catch a ball that is getting away, ease off before overrunning it.
	float Scale = 1.f;
	if (Gap > 70.f)
	{
		Scale = 1.f + FMath::Clamp((Gap - 70.f) / 160.f, 0.f, 0.18f);
	}
	else if (Gap < -5.f)
	{
		Scale = FMath::Clamp(1.f + Gap / 90.f, 0.72f, 1.f);
	}
	P->DribbleSpeedScale = FMath::FInterpTo(P->DribbleSpeedScale, Scale, Dt, 8.f);

	// Metrics: how far the ball is from where the foot wants it.
	const float Sep = FVector::Dist(Flat, MeetZone);
	DribbleSepSum += Sep;
	DribbleSpeedSum += Speed;
	++DribbleSepSamples;
	DribbleSepMax = FMath::Max(DribbleSepMax, Sep);
}

void APKGameMode::UpdateDribbleTest(float Dt)
{
	// -PKDribbleTest: one Blue player dribbles a fixed course alone (jog, sprint, zig-zag, close control,
	// turn back, stop); the dribbling metrics are logged per phase. Run with -PKOldDribble to compare.
	if (!bDribbleTest || Phase != EPKPhase::Playing)
	{
		return;
	}
	DribbleTestClock += Dt;
	APKFootballer* P = DribbleTestPlayer.Get();
	if (DribbleTestPhase < 0)
	{
		if (DribbleTestClock < 1.f)
		{
			return;
		}
		for (APKFootballer* F : Footballers)
		{
			if (F->Team == 0 && F->Index == 9)
			{
				P = F;
			}
		}
		if (!P)
		{
			return;
		}
		int32 k = 0;
		for (APKFootballer* F : Footballers)
		{
			if (F != P)
			{
				F->Teleport2D(FVector(-4000.f + (k++) * 380.f, PK::HalfWidth - 300.f, 0.f), -90.f);
				F->Stun(9999.f);
			}
		}
		P->Teleport2D(FVector(-3500.f, -600.f, 0.f), 0.f);
		P->bHuman = true; // keeps the AI away from him; the test drives his input
		Ball->ClearControl();
		Ball->SetActorLocation(FVector(PK::Flat(P->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
		Ball->Velocity = FVector::ZeroVector;
		Ball->SetController(P);
		DribbleTestPlayer = P;
		DribbleTestPhase = 0;
		DribbleTestClock = 0.f;
		DribbleTouches = StepTouches = DribbleSepSamples = DribbleLosses = FallbackTouches = OverrunTouches = OutsideTouches = InsideCuts = 0;
		TouchGapSum = DribbleSepSum = DribbleSpeedSum = 0.0;
		DribbleSepMax = 0.f;
		return;
	}
	if (!P)
	{
		return;
	}
	struct FPhaseDef { const TCHAR* Name; float Seconds; };
	static const FPhaseDef Phases[] = { { TEXT("jog"), 3.f }, { TEXT("sprint"), 3.f }, { TEXT("zigzag"), 4.f }, { TEXT("close control"), 3.f }, { TEXT("turn back"), 2.5f }, { TEXT("stop"), 2.f }, { TEXT("cuts"), 5.f }, { TEXT("controlled spr"), 3.f }, { TEXT("effort touch"), 4.f } };
	if (DribbleTestPhase >= (int32)UE_ARRAY_COUNT(Phases))
	{
		return;
	}
	const FPhaseDef& Ph = Phases[DribbleTestPhase];
	if (DribbleTestClock <= Dt)
	{
		// Each phase starts with the ball at his feet so a mistake in one phase does not spoil the next.
		Ball->ClearControl();
		Ball->SetActorLocation(FVector(PK::Flat(P->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
		Ball->Velocity = FVector::ZeroVector;
		Ball->SetController(P);
	}
	// Drive the input.
	P->Stance = EPKStance::Normal;
	P->bSprint = false;
	FVector Input(1.f, 0.f, 0.f);
	switch (DribbleTestPhase)
	{
	case 1: P->bSprint = true; break;
	case 2: Input = FVector(1.f, FMath::Fmod(DribbleTestClock, 1.6f) < 0.8f ? 1.f : -1.f, 0.f).GetSafeNormal(); break;
	case 3: P->Stance = EPKStance::CloseControl; Input = FVector(0.f, 1.f, 0.f); break;
	case 4: Input = FVector(-1.f, 0.f, 0.f); break;
	case 5: Input = FVector::ZeroVector; break;
	case 6: Input = FVector(1.f, 0.f, 0.f).RotateAngleAxis(FMath::Fmod(DribbleTestClock, 2.f) < 1.f ? 70.f : -70.f, FVector::UpVector); break; // sharp cuts
	case 7: P->bControlledSprint = true; break;
	case 8:
	{
		// Effort touches: short right, long forward, short left, short back.
		static const float Times[] = { 0.6f, 1.5f, 2.6f, 3.3f };
		static const FVector Dirs[] = { FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), FVector(-1.f, 0.f, 0.f) };
		static const float Strengths[] = { 0.2f, 1.f, 0.3f, 0.25f };
		for (int32 i = 0; i < 4; ++i)
		{
			if (DribbleTestClock - Dt < Times[i] && DribbleTestClock >= Times[i] && Ball->GetController() == P)
			{
				const float Before = FVector::Dist(PK::Flat(Ball->GetActorLocation()), P->GetFlatLocation());
				EffortTouch(P, Dirs[i], Strengths[i]);
				UE_LOG(LogPitchKings, Log, TEXT("[DribbleTest] effort touch %d strength %.1f ball speed %.0f (ball was %.0f cm away)"), i, Strengths[i], Ball->Velocity.Size2D(), Before);
			}
		}
		Input = Ball->GetController() == P ? (PK::Flat(Ball->GetActorLocation()) - P->GetFlatLocation()).GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
		break;
	}
	default: break;
	}
	if (DribbleTestPhase != 7)
	{
		P->bControlledSprint = false;
	}
	if (CutShotTimer >= 0.f)
	{
		CutShotTimer -= Dt;
		if (CutShotTimer < 0.f)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("Dribble_cut_%d.png"), CutShots++)), true, false);
			CutShotTimer = -1.f;
		}
	}
	P->MoveInput = Input;
	if (Ball->GetController() != P && Ball->IsFree() && FVector::Dist(PK::Flat(Ball->GetActorLocation()), P->GetFlatLocation()) < 120.f)
	{
		Ball->SetController(P); // won it back: keep the course going
	}
	if (bCloseCamera && FMath::IsNearlyEqual(DribbleTestClock, Ph.Seconds * 0.5f, Dt * 0.6f))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("Dribble_%s_%d.png"), bOldDribble ? TEXT("old") : TEXT("new"), DribbleTestPhase)), true, false);
	}
	if (DribbleTestClock >= Ph.Seconds)
	{
		UE_LOG(LogPitchKings, Log, TEXT("[DribbleTest %s] %-13s touches %2d (on steps %2d, foot-ball gap at touch %.0f cm, fallback %d overrun %d, outside %d inside %d) | ball from foot zone avg %.0f max %.0f cm | lost %d | speed %.0f | step %.2fs"),
			bOldDribble ? TEXT("OLD") : TEXT("NEW"), Ph.Name, DribbleTouches, StepTouches, StepTouches ? TouchGapSum / StepTouches : 0.0, FallbackTouches, OverrunTouches, OutsideTouches, InsideCuts,
			DribbleSepSamples ? DribbleSepSum / DribbleSepSamples : 0.0, DribbleSepMax, DribbleLosses, DribbleSepSamples ? DribbleSpeedSum / DribbleSepSamples : 0.0, P->StepInterval);
		DribbleTouches = StepTouches = DribbleSepSamples = DribbleLosses = FallbackTouches = OverrunTouches = OutsideTouches = InsideCuts = 0;
		TouchGapSum = DribbleSepSum = DribbleSpeedSum = 0.0;
		DribbleSepMax = 0.f;
		++DribbleTestPhase;
		DribbleTestClock = 0.f;
		if (DribbleTestPhase >= (int32)UE_ARRAY_COUNT(Phases))
		{
			FGenericPlatformMisc::RequestExit(false);
		}
	}
}

void APKGameMode::UpdateDribbleLegacy(APKFootballer* P, float Dt)
{
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Flat = PK::Flat(BallLoc);
	const float Dist = FVector::Dist(Flat, P->GetFlatLocation());
	if (Dist > ControlLoseDistance)
	{
		Ball->SetController(nullptr); // heavy touch / outran it: ball is loose
		++DribbleLosses;
		return;
	}
	if (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None || P->BufferTimer > 0.f)
	{
		return;
	}
	{
		const FVector Zone = P->GetFlatLocation() + (P->MoveInput.IsNearlyZero() ? P->GetFacing() : P->MoveInput.GetSafeNormal2D()) * 45.f;
		const float Sep = FVector::Dist(Flat, Zone);
		DribbleSepSum += Sep;
		DribbleSpeedSum += PK::Flat(P->GetVelocity()).Size();
		++DribbleSepSamples;
		DribbleSepMax = FMath::Max(DribbleSepMax, Sep);
	}

	const FVector Vel = PK::Flat(P->GetVelocity());
	const float Speed = Vel.Size();
	FVector Desired = P->MoveInput.IsNearlyZero() ? P->GetFacing() : P->MoveInput.GetSafeNormal2D();
	if (P->Stance == EPKStance::Shield)
	{
		Desired = P->GetFacing();
	}

	float Lead = 0.38f, Base = 120.f, Interval = 0.33f;
	switch (P->Stance)
	{
	case EPKStance::CloseControl: Lead = 0.08f; Base = 40.f; Interval = 0.22f; break;
	case EPKStance::Shield:       Lead = 0.f;   Base = 20.f; Interval = 0.25f; break;
	default:
		if (P->IsSprinting())
		{
			Lead = 0.8f; Base = 220.f; Interval = 0.5f;
		}
		break;
	}

	// Ball control rating: better control keeps touches closer and more frequent.
	Lead *= FMath::Lerp(1.35f, 0.75f, P->Attr.Control) * (P->HasTrait(PKTrait::Technical) ? 0.85f : 1.f);
	Interval *= FMath::Lerp(1.1f, 0.85f, P->Attr.Control);

	// Touch the ball where the body can take it: at most ~40 degrees off the facing, smaller touch when turning.
	if (P->Stance != EPKStance::Shield)
	{
		const FVector Facing = P->GetFacing();
		const float TurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Desired), -1.f, 1.f)));
		if (TurnDeg > 40.f)
		{
			const float SideSign = FVector::CrossProduct(Facing, Desired).Z >= 0.f ? 1.f : -1.f;
			Desired = Facing.RotateAngleAxis(40.f * SideSign, FVector::UpVector);
			Lead *= 0.5f;
			Base *= 0.6f;
		}
	}

	const FVector ToBall = Flat - P->GetFlatLocation();
	const float Ahead = FVector::DotProduct(ToBall, Desired);
	const FVector Foot = P->GetFootPoint();
	const bool bAtFeet = FVector::Dist(Flat, Foot) < 60.f || (Ahead < 20.f && Dist < 90.f);

	if (bAtFeet && P->TouchTimer <= 0.f)
	{
		FVector NewVel;
		if (P->MoveInput.IsNearlyZero() && Speed < 120.f)
		{
			NewVel = FVector::ZeroVector; // stop it under the sole
		}
		else
		{
			NewVel = Desired * (FMath::Max(Speed, 150.f) * (1.f + Lead) + Base);
			if (P->IsSprinting() && P->Stance == EPKStance::Normal)
			{
				NewVel.Z = FMath::FRandRange(70.f, 170.f); // knocked ahead, bobbling
			}
			else if (Speed > 250.f && FMath::FRand() < 0.3f)
			{
				NewVel.Z = FMath::FRandRange(40.f, 100.f);
			}
		}
		Ball->Touch(P, NewVel);
		P->TouchTimer = Interval;
		++DribbleTouches;
	}
	else if (Dist < 160.f)
	{
		// Between touches the ball drifts toward the running line (close control assist).
		const FVector BallVel = PK::Flat(Ball->Velocity);
		const float BallSpeed = BallVel.Size();
		if (BallSpeed > 10.f)
		{
			const FVector Steered = FMath::VInterpTo(BallVel, Desired * BallSpeed, Dt, P->Stance == EPKStance::CloseControl ? 6.f : 2.5f);
			Ball->Velocity.X = Steered.X;
			Ball->Velocity.Y = Steered.Y;
		}
	}
}

void APKGameMode::RequestPass(APKFootballer* P, APKFootballer* Target, EPKKickType Type, bool bSwerve)
{
	const FVector From = PK::Flat(Ball->GetActorLocation());
	const FVector TLoc = Target->GetFlatLocation();
	const FVector TVel = PK::Flat(Target->GetVelocity());
	const float Lead = FVector::Dist(From, TLoc) / 1500.f;

	FVector Point = TLoc + TVel * Lead;
	if (Type == EPKKickType::Through)
	{
		Point += FVector(PK::AttackDir(Target->Team) * 600.f, 0.f, 0.f) + TVel * 0.4f;
	}
	Point.X = FMath::Clamp(Point.X, -PK::HalfLength + 150.f, PK::HalfLength - 150.f);
	Point.Y = FMath::Clamp(Point.Y, -PK::HalfWidth + 150.f, PK::HalfWidth - 150.f);

	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;
	Kick.Power = FMath::Clamp(FVector::Dist(From, Point) / 4000.f, 0.2f, 1.f);
	Kick.Receiver = Target;
	Kick.Swerve = bSwerve ? ChooseSwerve(P, Point) : 0.f;
	if (Type == EPKKickType::Through)
	{
		++ThroughBalls[P->Team];
	}
	TryStartKick(P, Kick); // the selection moves to the receiver when the ball is struck (ExecuteKick)
}

void APKGameMode::RequestPassToPoint(APKFootballer* P, const FVector& Point, EPKKickType Type, bool bSwerve)
{
	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;
	Kick.Swerve = bSwerve ? ChooseSwerve(P, Point) : 0.f;
	Kick.Power = FMath::Clamp(FVector::Dist(P->GetFlatLocation(), PK::Flat(Point)) / 4000.f, 0.3f, 1.f);
	TryStartKick(P, Kick);
}

void APKGameMode::RequestShot(APKFootballer* P, float AimLateral, float Power, bool bAirborne, EPKShotType Type)
{
	if (bAirborne)
	{
		Type = EPKShotType::Normal; // volleys are struck as they come
	}
	// Quick release: a light tap inside the box gets the shortest wind-up.
	const bool bQuick = !bAirborne && Power < 0.3f && (Type == EPKShotType::Normal || Type == EPKShotType::Low)
		&& FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < PK::PenaltyDepth + 150.f;
	if (bQuick)
	{
		Power = 0.5f;
		++QuickShots;
	}
	const float FarPost = P->GetActorLocation().Y * PK::AttackDir(P->Team) > 0.f ? -1.f : 1.f;
	if (Type == EPKShotType::Finesse)
	{
		// Finesse: into the corner (the far one when not aiming).
		AimLateral = FMath::Abs(AimLateral) < 0.5f ? FarPost * 0.85f : FMath::Sign(AimLateral) * FMath::Max(FMath::Abs(AimLateral), 0.8f);
	}
	else if (Type == EPKShotType::Normal && P->HasTrait(PKTrait::Finesse) && FMath::Abs(AimLateral) < 0.5f)
	{
		AimLateral = FarPost * 0.85f; // Finesse PlayStyle: always looks for the far corner
	}

	// Finishing rating, technique and PlayStyles set the error.
	float Err = (50.f + 180.f * Power * Power * Power) * FMath::Lerp(1.6f, 0.7f, P->Attr.Shooting);
	switch (Type)
	{
	case EPKShotType::Finesse: Err *= 0.75f - 0.15f * P->StyleLevel(PKTrait::Finesse); break;
	case EPKShotType::Power:   Err *= 1.3f - 0.25f * P->StyleLevel(PKTrait::PowerShot); break;
	case EPKShotType::Chip:    Err *= 0.7f - 0.15f * P->StyleLevel(PKTrait::ChipShot); break;
	case EPKShotType::Low:     Err *= 0.9f; break;
	default:                   Err *= P->HasTrait(PKTrait::Finesse) ? 0.8f : (P->HasTrait(PKTrait::PowerShot) ? 1.1f : 1.f); break;
	}
	if (bAirborne)
	{
		Err *= 1.4f - 0.25f * P->StyleLevel(PKTrait::Acrobatic); // volleys are harder to keep on target
	}
	if (Ball->IsPlaced())
	{
		Err *= 1.f - 0.25f * P->StyleLevel(PKTrait::DeadBall); // free kicks
	}
	Err *= P->FatigueError();
	const float CleanY = FMath::Clamp(AimLateral, -1.f, 1.f) * (PK::GoalHalfWidth - 50.f);
	const float AimY = CleanY + FMath::FRandRange(-Err, Err);
	const float Wild = FMath::Lerp(1.4f, 0.4f, P->Attr.Shooting) * (bAirborne ? 1.5f : 1.f);
	float CleanZ = 30.f + 180.f * Power * Power;
	float AimZ = CleanZ + FMath::FRandRange(-35.f, 35.f) * Power + FMath::FRandRange(-20.f, 110.f) * Power * Power * Wild;
	switch (Type)
	{
	case EPKShotType::Finesse: CleanZ = 45.f + 110.f * Power * Power; AimZ = CleanZ + FMath::FRandRange(-25.f, 60.f) * Power * Wild; break;
	case EPKShotType::Power:   CleanZ = 50.f + 120.f * Power * Power; AimZ = CleanZ + FMath::FRandRange(-30.f, 90.f) * Power * Wild; break;
	case EPKShotType::Low:     CleanZ = 20.f; AimZ = CleanZ + FMath::FRandRange(0.f, 30.f) * Wild; break;
	case EPKShotType::Chip:    CleanZ = 190.f; AimZ = CleanZ + FMath::FRandRange(-20.f, 50.f) * Wild; break;
	default: break;
	}

	FPKKickRequest Kick;
	Kick.Type = EPKKickType::Shot;
	Kick.Target = FVector(PK::AttackDir(P->Team) * PK::HalfLength, AimY, AimZ);
	Kick.CleanTarget = FVector(Kick.Target.X, CleanY, CleanZ);
	Kick.Power = Power;
	Kick.ShotType = Type;
	Kick.ActionRate = Type == EPKShotType::Power ? 0.72f : (bQuick ? 1.35f : (Type == EPKShotType::Chip ? 0.95f : 1.f));
	if (TryStartKick(P, Kick, bAirborne))
	{
		P->ShotStartTime = GetWorld()->GetTimeSeconds();
		P->TimedShotPress = -10.0;
	}
}

EPKShotType APKGameMode::ChooseAIShot(const APKFootballer* P, float DGoal) const
{
	// Keeper off his line: chip him. Long range: power. Angled / technical: finesse. Sometimes low and hard.
	if (const APKFootballer* GK = Keeper(1 - P->Team))
	{
		const float Off = FMath::Abs(GK->GetActorLocation().X - OppGoal(P->Team).X);
		if (Off > 450.f && DGoal > 1100.f && DGoal < 3000.f && FMath::FRand() < 0.6f)
		{
			return EPKShotType::Chip;
		}
	}
	if (DGoal > 2200.f && (P->HasTrait(PKTrait::PowerShot) || FMath::FRand() < 0.3f))
	{
		return EPKShotType::Power;
	}
	if (P->HasTrait(PKTrait::Finesse) || (DGoal < 2200.f && FMath::FRand() < 0.35f))
	{
		return EPKShotType::Finesse;
	}
	return FMath::FRand() < 0.2f ? EPKShotType::Low : EPKShotType::Normal;
}

const TCHAR* APKGameMode::ShotTypeName(EPKShotType Type)
{
	switch (Type)
	{
	case EPKShotType::Finesse: return TEXT("FINESSE SHOT");
	case EPKShotType::Chip:    return TEXT("CHIP SHOT");
	case EPKShotType::Power:   return TEXT("POWER SHOT");
	case EPKShotType::Low:     return TEXT("LOW DRIVEN SHOT");
	default:                   return TEXT("SHOT");
	}
}

void APKGameMode::ReleaseKicks()
{
	for (APKFootballer* P : Footballers)
	{
		if (!P->ReadyToReleaseKick())
		{
			continue;
		}
		const FPKKickRequest Kick = P->ConsumeKick();
		const bool bOwns = Ball->GetOwnerPlayer() == P;
		const bool bVolley = P->GetCurrentAction() == EPKAction::Volley;
		if (!bOwns && !(Ball->IsFree() && CanReachBall(P, bVolley ? 170.f : KickReach, bVolley ? 220.f : 90.f)))
		{
			continue; // the ball was taken or ran away during the wind-up: air kick
		}
		const bool bFromHands = Ball->GetHolder() == P;
		if (!bFromHands && !Ball->IsPlaced() && P->GetKickClip() != EPKAction::None)
		{
			// The ball only leaves when the striking foot is on it.
			const FVector Contact = P->GetKickContactPoint();
			const float Err = FVector::Dist(Ball->GetActorLocation(), Contact);
			if (Err > (bVolley ? 110.f : 70.f))
			{
				++Whiffs;
				UE_LOG(LogPitchKings, Log, TEXT("%s #%d air kick: ball %.0f cm from the foot"), PK::TeamName(P->Team), P->Index, Err);
				continue;
			}
			Ball->SetActorLocation(Contact);
			const float BoneErr = FVector::Dist(Contact, P->GetContactBoneLocation(P->GetKickClip()));
			const FVector KickDir = PK::Flat(Kick.Target - Contact).GetSafeNormal();
			const float FacingErr = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(KickDir, P->GetFacing()), -1.f, 1.f)));
			if (bAutoTest && BoneErr > 60.f)
			{
				UE_LOG(LogPitchKings, Log, TEXT("[BoneErr] %s #%d %s bone %.0f cm, speed %.0f, action time %.2f rate %.2f, stun %d"), PK::TeamName(P->Team), P->Index, *UEnum::GetValueAsString(P->GetCurrentAction()), BoneErr, PK::Flat(P->GetVelocity()).Size(), P->GetActionTime(), P->GetActionRate(), P->IsStunned() ? 1 : 0);
			}
			++KickCount;
			ContactErrSum += Err;
			ContactErrMax = FMath::Max(ContactErrMax, Err);
			BoneErrSum += BoneErr;
			FacingErrSum += FacingErr;
			FacingErrMax = FMath::Max(FacingErrMax, FacingErr);
		}
		ExecuteKick(P, Kick);
		ContactSnap(P, *UEnum::GetValueAsString(P->GetCurrentAction()).RightChop(11));
	}
}

bool APKGameMode::PassLaneBlocked(const APKFootballer* P, const FVector& From, const FVector& To) const
{
	const FVector A = PK::Flat(From);
	const FVector B = PK::Flat(To);
	const float Len = FVector::Dist(A, B);
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team || O->IsStunned())
		{
			continue;
		}
		const FVector Loc = O->GetFlatLocation();
		const float Along = FVector::DotProduct(Loc - A, (B - A) / FMath::Max(Len, 1.f));
		if (Along < 250.f || Along > Len - 300.f)
		{
			continue; // too close to kicker or receiver to go over
		}
		if (FVector::Dist(Loc, FMath::ClosestPointOnSegment(Loc, A, B)) < 85.f)
		{
			return true;
		}
	}
	return false;
}

bool APKGameMode::TryCross(APKFootballer* P)
{
	// Wide in the final third: whip it into the box for a header or volley.
	const FVector Loc = P->GetFlatLocation();
	if (FMath::Abs(Loc.Y) < 1100.f || WorldToFx(P->Team, Loc.X) < 0.35f || FMath::FRand() > 0.8f)
	{
		return false;
	}
	const FVector Goal = OppGoal(P->Team);
	APKFootballer* Best = nullptr;
	float BestScore = -1.f;
	for (APKFootballer* T : Footballers)
	{
		if (T->Team != P->Team || T == P || T->IsKeeper())
		{
			continue;
		}
		const FVector TL = T->GetFlatLocation();
		const float DGoal = FVector::Dist(TL, Goal);
		if (DGoal > 2300.f || FMath::Abs(TL.Y) > 1600.f)
		{
			continue;
		}
		float NearestDef = 9999.f;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != P->Team)
			{
				NearestDef = FMath::Min(NearestDef, FVector::Dist(O->GetFlatLocation(), TL));
			}
		}
		const float CrossScore = NearestDef - DGoal * 0.15f;
		if (CrossScore > BestScore)
		{
			BestScore = CrossScore;
			Best = T;
		}
	}
	if (!Best)
	{
		// Nobody there yet: whip it into the corridor between keeper and defenders if a runner is arriving.
		const float SideSign = FMath::Sign(Loc.Y);
		const FVector Zone = FVector(Goal.X - PK::AttackDir(P->Team) * 750.f, -SideSign * 150.f, 0.f);
		for (APKFootballer* T : Footballers)
		{
			if (T->Team == P->Team && T != P && !T->IsKeeper() && FVector::Dist(T->GetFlatLocation(), Zone) < 1700.f
				&& FVector::DotProduct(PK::Flat(T->GetVelocity()), Zone - T->GetFlatLocation()) > 0.f)
			{
				++Crosses;
				EventShot(TEXT("Cross"), P);
				RequestPassToPoint(P, Zone + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-250.f, 250.f), 0.f), EPKKickType::Lob);
				return true;
			}
		}
		return false;
	}
	const FVector Dir = (Best->GetFlatLocation() - Loc).GetSafeNormal2D();
	const FVector Point = Best->GetFlatLocation() + Dir * 170.f + PK::Flat(Best->GetVelocity()) * 0.6f;
	++Crosses;
	EventShot(TEXT("Cross"), P);
	RequestPassToPoint(P, Point, EPKKickType::Lob);
	Ball->IntendedReceiver = Best;
	return true;
}

void APKGameMode::FlickUp(APKFootballer* P, const FVector& Dir)
{
	// Scoop the ball up and over a close challenger, then run onto it.
	if (Ball->GetController() != P || !CanReachBall(P, 90.f) || P->IsKicking())
	{
		return;
	}
	P->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	QueueTouch(P, EPKAction::Pass, Dir * (PK::Flat(P->GetVelocity()).Size() * 0.6f + 320.f) + FVector(0.f, 0.f, 520.f), false, 1.15f);
	P->TouchTimer = 1.2f;
	P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.6f);
	++Flicks;
	EventShot(TEXT("Flick"), P);
}

void APKGameMode::HumanFlickUp(int32 Team, const FVector& Aim)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human)
	{
		FlickUp(Human, Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D());
	}
}

void APKGameMode::ExecuteKick(APKFootballer* P, const FPKKickRequest& InKick)
{
	FPKKickRequest Kick = InKick;
	if (Kick.Type == EPKKickType::Shot && P->TimedShotPress > P->ShotStartTime)
	{
		// Timed finishing: the second press landed just before the strike (green) or not (red).
		const double Early = GetWorld()->GetTimeSeconds() - P->TimedShotPress;
		const bool bGreen = Early >= 0.0 && Early <= 0.11;
		Kick.Target = bGreen ? FMath::Lerp(Kick.Target, Kick.CleanTarget, 0.65f) : Kick.CleanTarget + (Kick.Target - Kick.CleanTarget) * 1.7f + FVector(0.f, 0.f, 35.f);
		bGreen ? ++TimedGreen : ++TimedRed;
		if (P->bHuman)
		{
			ShotFeedback = bGreen ? TEXT("PERFECT TIMING") : TEXT("MISTIMED");
			ShotFeedbackColor = bGreen ? FLinearColor(0.2f, 1.f, 0.3f) : FLinearColor(1.f, 0.25f, 0.2f);
			ShotFeedbackTimer = 1.4f;
		}
		P->TimedShotPress = -10.0;
	}
	const bool bThrow = Ball->GetHolder() == P && !P->IsKeeper();
	const bool bFromHands = Ball->GetHolder() == P;
	const FVector From = bFromHands ? Ball->GetActorLocation() : FVector(Ball->GetActorLocation().X, Ball->GetActorLocation().Y, PK::BallRadius);
	const FVector Delta = PK::Flat(Kick.Target - From);
	const float D = FMath::Max(Delta.Size(), 1.f);
	FVector Dir = Delta / D;
	float SpeedScale = 1.f;
	if (Kick.Type != EPKKickType::Shot && !bFromHands)
	{
		// Passing rating: small direction and weight errors (Tiki Taka halves them).
		// PlayStyles: Tiki Taka (short), Incisive Pass (through balls), Long Ball Pass, Whipped Pass (crosses), Dead Ball.
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
		const float Sloppy = (1.f - P->Attr.Passing) * FMath::Max(StyleAcc, 0.2f) * P->FatigueError();
		if (Kick.bPrecision)
		{
			// Played to a spot: the error grows with distance instead of being a fixed spray.
			const float Far = FMath::Clamp(D / 3000.f, 0.3f, 1.2f);
			Dir = Dir.RotateAngleAxis(FMath::FRandRange(-6.f, 6.f) * Sloppy * Far, FVector::UpVector);
			SpeedScale = 1.f + FMath::FRandRange(-0.08f, 0.08f) * Sloppy;
		}
		else
		{
			Dir = Dir.RotateAngleAxis(FMath::FRandRange(-10.f, 10.f) * Sloppy, FVector::UpVector);
			SpeedScale = 1.f + FMath::FRandRange(-0.15f, 0.15f) * Sloppy;
		}
	}
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);

	FVector Velocity;
	FVector Spin = FVector::ZeroVector;
	switch (Kick.Type)
	{
	case EPKKickType::Shot:
	{
		const EPKShotType ShotType = Kick.ShotType;
		float Speed = (2000.f + 1900.f * Kick.Power) * FMath::Lerp(0.9f, 1.05f, P->Attr.Shooting);
		if (P->HasTrait(PKTrait::PowerShot))
		{
			Speed *= ShotType == EPKShotType::Power ? 1.f + 0.1f * P->StyleLevel(PKTrait::PowerShot) : 1.f + 0.12f * P->StyleLevel(PKTrait::PowerShot);
		}
		switch (ShotType)
		{
		case EPKShotType::Power:   Speed *= 1.22f; break;
		case EPKShotType::Finesse: Speed *= 0.85f; break;
		case EPKShotType::Low:     Speed *= 1.05f; break;
		default:                   Speed *= P->HasTrait(PKTrait::Finesse) ? 0.9f : 1.f; break;
		}
		if (P->GetCurrentAction() == EPKAction::Volley)
		{
			Speed *= 0.85f; // a volley is rarely struck as cleanly as a ball on the ground
		}
		Speed = PK::CapShotSpeed(Speed);
		float T = D / Speed;
		Velocity = Dir * Speed;
		Velocity.Z = (Kick.Target.Z - From.Z) / T + 0.5f * PK::Gravity * T;
		Spin = Side * FMath::FRandRange(-250.f, 250.f);
		if (ShotType == EPKShotType::Chip)
		{
			// Chip: a slow, high arc that drops under the bar (air drag included).
			T = D / (800.f + 500.f * Kick.Power);
			const float K = PK::AirDrag;
			const float E = 1.f - FMath::Exp(-K * T);
			Velocity = Dir * (D * K / E);
			Velocity.Z = (Kick.Target.Z - From.Z) / T * (K * T / E) + PK::Gravity * T / E - PK::Gravity / K;
			Spin = FVector::ZeroVector;
		}
		else if (ShotType == EPKShotType::Low)
		{
			Spin *= 0.4f; // struck through the middle: little swerve
		}
		if (ShotType == EPKShotType::Finesse || (ShotType == EPKShotType::Normal && P->HasTrait(PKTrait::Finesse)))
		{
			// Start it wide of the target and curl it back in.
			const float Curl = Kick.Target.Y * FVector::DotProduct(Side, FVector(0.f, 1.f, 0.f)) > 0.f ? -1.f : 1.f;
			Spin = Side * Curl * 550.f;
			Velocity -= Side * Curl * 550.f * T * 0.5f;
		}
		++Shots[P->Team];
		++ShotTypeCount[FMath::Clamp((int32)ShotType, 0, 4)];
		CrowdCheer(P->Team, 0.35f);
		if (P->bHuman && ShotType != EPKShotType::Normal && ShotFeedbackTimer <= 0.f)
		{
			ShotFeedback = ShotTypeName(ShotType);
			ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
			ShotFeedbackTimer = 1.2f;
		}
		UE_LOG(LogPitchKings, Log, TEXT("%s #%d %s from %.0f m (power %.2f)"), PK::TeamName(P->Team), P->Index, ShotType == EPKShotType::Normal ? TEXT("shoots") : *FString(ShotTypeName(ShotType)).ToLower(), D / 100.f, Kick.Power);
		break;
	}
	case EPKKickType::Lob:
	case EPKKickType::Clearance:
	{
		// A cross into the opponents' box is whipped in: flatter and quicker than a lofted pass, so it can be
		// attacked at head height.
		const FVector OppGoalPos = OppGoal(P->Team);
		const bool bCrossIn = !bThrow && Kick.Type == EPKKickType::Lob && FMath::Abs(Kick.Target.X - OppGoalPos.X) < PK::PenaltyDepth + 100.f
			&& FMath::Abs(Kick.Target.Y) < PK::PenaltyHalfWidth && FMath::Abs(From.Y) > 900.f;
		const float T = bThrow ? FMath::Clamp(D / (1000.f * (1.f + 0.3f * P->StyleLevel(PKTrait::FarThrow))), 0.6f, 1.3f)
			: (bCrossIn ? FMath::Clamp(D / (1900.f + 150.f * P->StyleLevel(PKTrait::WhippedPass)), 0.7f, 1.6f) : FMath::Clamp(D / 1300.f, 0.9f, 2.1f));
		Velocity = Dir * (D / T * 0.82f);
		Velocity.Z = 0.5f * PK::Gravity * T - (From.Z - PK::BallRadius) / T;
		if (Kick.bPrecision && !bThrow)
		{
			// Precision lob: lands on the spot. Exact flight with the ball's linear air drag k:
			// x(T) = v/k (1 - e^-kT),  z(T) = 0  =>  vz = g T / (1 - e^-kT) - g / k.
			const float K = PK::AirDrag;
			const float E = 1.f - FMath::Exp(-K * T);
			Velocity = Dir * (D * K / E) * SpeedScale;
			Velocity.Z = PK::Gravity * T / E - PK::Gravity / K - (From.Z - PK::BallRadius) / T;
		}
		if (Kick.Type == EPKKickType::Clearance)
		{
			Velocity *= 1.15f;
			Velocity.Z *= 1.1f;
		}
		else if (!bThrow && Kick.Swerve != 0.f)
		{
			// Swerved lob: starts out on the bulge side and bends back onto the spot.
			Spin = -Side * Kick.Swerve * 420.f;
			Velocity += Side * Kick.Swerve * 420.f * T * 0.5f;
		}
		else if (!bThrow)
		{
			// Whipped in: slight curl toward the target side.
			Spin = Side * FMath::FRandRange(-180.f, 180.f);
		}
		break;
	}
	default:
	{
		if (!bFromHands && D > 900.f && !Kick.bPrecision && Kick.Swerve == 0.f && PassLaneBlocked(P, From, Kick.Target))
		{
			// Defender in the lane: dink it over him.
			const float T = FMath::Clamp(D / 950.f, 1.f, 1.9f);
			Velocity = Dir * (D / T * 0.8f) * SpeedScale;
			Velocity.Z = 0.5f * PK::Gravity * T;
			++Chips;
			EventShot(TEXT("Chip"), P);
			break;
		}
		// A precision pass arrives at the spot still rolling at ~3 m/s for the receiver to run onto.
		float Speed = Kick.bPrecision ? FMath::Clamp(FMath::Sqrt(300.f * 300.f + 2.f * PK::RollDecel * D) * 1.04f, 500.f, 3000.f)
			: FMath::Clamp(FMath::Sqrt(2.f * PK::RollDecel * D) + 250.f, 700.f, 3000.f);
		if (Kick.Type == EPKKickType::Through)
		{
			Speed *= 1.1f + 0.05f * P->StyleLevel(PKTrait::IncisivePass);
		}
		if (bThrow)
		{
			Speed = FMath::Min(Speed, 1500.f * (1.f + 0.3f * P->StyleLevel(PKTrait::FarThrow)));
		}
		Velocity = Dir * Speed * SpeedScale;
		if (bFromHands)
		{
			Velocity.Z = 120.f; // rolled/thrown out of the hands
		}
		else if (Kick.Swerve != 0.f)
		{
			// Swerved along the ground: aimed out on the bulge side, the spin bends it back (the ball curls at
			// half strength while rolling) so it still arrives at the spot.
			const float Curl = 520.f;
			const float T = D / FMath::Max(Speed * 0.7f, 1.f);
			Spin = -Side * Kick.Swerve * Curl;
			Velocity += Side * Kick.Swerve * Curl * 0.5f * T * 0.5f;
			Velocity.Z = 0.f; // a bounce would take the spin off
		}
		else if (D > 1800.f)
		{
			// Driven pass: struck through the ball, flies knee-to-waist high then skips.
			Velocity.Z = FMath::Lerp(160.f, 340.f, FMath::Clamp((D - 1800.f) / 2200.f, 0.f, 1.f)) * FMath::FRandRange(0.8f, 1.15f);
		}
		else if (D > 900.f)
		{
			Velocity.Z = FMath::FRandRange(60.f, 170.f); // skidding off the turf
		}
		break;
	}
	}

	// Pass played: the selection follows the ball to the receiver now that it has left the foot.
	if (Kick.Type != EPKKickType::Shot && bHumanTeam[P->Team] && Kick.Receiver.IsValid() && !Kick.Receiver->IsKeeper() && Kick.Receiver->Team == P->Team)
	{
		SetHuman(Kick.Receiver.Get());
	}
	if (P->bHuman)
	{
		Rumble(P->Team, Kick.Type == EPKKickType::Shot ? 0.35f + 0.5f * Kick.Power : 0.18f, Kick.Type == EPKKickType::Shot ? 0.2f : 0.1f);
	}
	if (!bFromHands)
	{
		NoteKickFit(P, Velocity.Size());
	}
	const EPKSetPiece KickedFrom = Ball->IsPlaced() || Ball->GetHolder() ? SetPiece : EPKSetPiece::None;
	if (bFeatureTest)
	{
		UE_LOG(LogPitchKings, Log, TEXT("[FeatureTest]    kick by %s #%d type %d precision %d swerve %.0f dist %.0f vel (%.0f, %.0f, %.0f)"), PK::TeamName(P->Team), P->Index, (int32)Kick.Type, Kick.bPrecision ? 1 : 0, Kick.Swerve, D, Velocity.X, Velocity.Y, Velocity.Z);
	}
	Ball->Kick(P, Velocity, Spin);
	P->HoldTimer = 0.f;
	if (Kick.Type != EPKKickType::Shot && KickedFrom != EPKSetPiece::GoalKick && KickedFrom != EPKSetPiece::Corner && KickedFrom != EPKSetPiece::ThrowIn && !bFromHands)
	{
		MarkOffside(P);
	}
	{
		static const TCHAR* Names[] = { TEXT("Pass"), TEXT("Through"), TEXT("Lob"), TEXT("Shot"), TEXT("Clear") };
		EventShot(bThrow ? TEXT("ThrowIn") : (bFromHands ? TEXT("KeeperRelease") : Names[(int32)Kick.Type]), P);
	}
	if (APKFootballer* Receiver = Kick.Receiver.Get())
	{
		Ball->IntendedReceiver = Receiver;
		++Passes[P->Team];
	}
	if (Phase == EPKPhase::Kickoff || Phase == EPKPhase::Stoppage)
	{
		Phase = EPKPhase::Playing;
	}
}

void APKGameMode::NoteKickFit(const APKFootballer* P, float LaunchSpeed)
{
	const EPKAction Clip = P->GetKickClip();
	const FPKActionClip& Info = UPKAnimInstance::GetActionClip(Clip);
	if (Clip == EPKAction::None || !Info.IsValid() || Info.ContactSpeed < 50.f)
	{
		return;
	}
	const float Foot = Info.ContactSpeed * FMath::Max(P->GetActionRate(), 0.1f);
	const float Ratio = LaunchSpeed / Foot;
	const bool bSideFoot = Clip == EPKAction::Pass || Clip == EPKAction::PassLeft;
	const bool bBad = bSideFoot ? (Ratio < 0.6f || Ratio > 1.7f) : (Ratio < 0.8f || Ratio > 2.0f);
	KickFitSum += Ratio;
	++KickFitCount;
	KickFitBad += bBad ? 1 : 0;
	UE_LOG(LogPitchKings, Verbose, TEXT("[KickFit] %s rate %.2f foot %.0f ball %.0f ratio %.2f%s"), *UEnum::GetValueAsString(Clip).RightChop(11), P->GetActionRate(), Foot, LaunchSpeed, Ratio, bBad ? TEXT("  MISFIT") : TEXT(""));
}

void APKGameMode::TryTackle(APKFootballer* Tackler, float Bonus)
{
	Tackler->TackleCooldown = Tackler->bHuman ? 0.7f : 1.4f;
	Tackler->PlayTackle();
	(void)Bonus;
}

void APKGameMode::CheckPickups()
{
	if (!Ball->IsFree() || Ball->IsInNet())
	{
		return;
	}

	const FVector BallLoc = Ball->GetActorLocation();
	const float Speed = Ball->Velocity.Size();
	APKFootballer* Controller = Ball->GetController();
	const float ControllerDist = Controller ? FVector::Dist(Controller->GetFlatLocation(), PK::Flat(BallLoc)) : 0.f;

	for (APKFootballer* P : Footballers)
	{
		if (P == Controller || P->IsStunned() || (P == Ball->LastKicker && Ball->KickerIgnoreTimer > 0.f))
		{
			continue;
		}

		const bool bKeeperHands = P->IsKeeper() && InOwnBox(P, BallLoc);
		const bool bLunging = P->LungeTimer > 0.f;
		const bool bSliding = P->IsSliding();
		float Reach = bKeeperHands ? 120.f : (bSliding ? 145.f : (bLunging ? 125.f : 75.f));
		if (!bKeeperHands && P->HasTrait(PKTrait::Anticipate))
		{
			Reach += 20.f;
		}
		const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(BallLoc));
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

		// Header / volley on demand: the person asked for it, so he goes for it whatever the situation.
		const bool bRequested = P->HeaderRequestTimer > 0.f && P->bHuman;
		if (bRequested && !P->IsInAction() && P->StealCooldown <= 0.f && D < 135.f + AerialReach && BallLoc.Z > 85.f && BallLoc.Z < 240.f + AerialReach * 0.6f)
		{
			if (CheckOffside(P))
			{
				return;
			}
			P->StealCooldown = 0.5f;
			P->HeaderRequestTimer = 0.f;
			++HeadersOnDemand;
			const FVector Aim = P->HeaderRequestAim.IsNearlyZero() ? P->GetFacing() : P->HeaderRequestAim.GetSafeNormal2D();
			if (P->bHeaderRequestShot && BallLoc.Z < 120.f)
			{
				Ball->SetController(P); // waist high: volley it
				RequestShot(P, FMath::Clamp(FVector::DotProduct(Aim, FVector(0.f, PK::AttackDir(P->Team), 0.f)), -1.f, 1.f), 0.85f, true);
				EventShot(TEXT("Volley"), P);
				continue;
			}
			FVector Vel;
			if (P->bHeaderRequestShot)
			{
				// Header at goal: down towards the corner he aims at.
				const FVector Goal = OppGoal(P->Team);
				const float Lateral = FMath::Clamp(FVector::DotProduct(Aim, FVector(0.f, PK::AttackDir(P->Team), 0.f)), -1.f, 1.f);
				const FVector Corner = Goal + FVector(0.f, Lateral * (PK::GoalHalfWidth - 60.f) * PK::AttackDir(P->Team), 0.f);
				const FVector To = PK::Flat(Corner - BallLoc);
				const float HeadSpeed = FMath::Lerp(1100.f, 1600.f, P->Attr.Shooting);
				Vel = To.GetSafeNormal() * HeadSpeed;
				Vel.Z = FMath::Clamp((60.f - BallLoc.Z) / FMath::Max(To.Size() / HeadSpeed, 0.2f) + 0.5f * PK::Gravity * To.Size() / HeadSpeed, -400.f, 200.f);
				++Shots[P->Team];
			}
			else
			{
				// Headed pass to the team-mate in that direction (or into space).
				APKFootballer* Mate = nullptr;
				float BestCos = 0.7f;
				for (APKFootballer* T : Footballers)
				{
					if (T == P || T->Team != P->Team || T->IsKeeper())
					{
						continue;
					}
					const FVector To = T->GetFlatLocation() - P->GetFlatLocation();
					const float Cos = FVector::DotProduct(To.GetSafeNormal2D(), Aim) - To.Size() / 8000.f;
					if (To.Size() < 2200.f && Cos > BestCos)
					{
						BestCos = Cos;
						Mate = T;
					}
				}
				const FVector Point = Mate ? Mate->GetFlatLocation() : P->GetFlatLocation() + Aim * 900.f;
				const float Dist = FVector::Dist(PK::Flat(BallLoc), Point);
				const float T = FMath::Clamp(Dist / 1000.f, 0.5f, 1.3f);
				Vel = (Point - PK::Flat(BallLoc)).GetSafeNormal2D() * (Dist / T * 0.85f);
				Vel.Z = 0.5f * PK::Gravity * T - (BallLoc.Z - PK::BallRadius) / T;
				if (Mate)
				{
					Ball->IntendedReceiver = Mate;
					++Passes[P->Team];
				}
			}
			P->SetActorRotation(FRotator(0.f, PK::Flat(Vel).Rotation().Yaw, 0.f));
			QueueTouch(P, EPKAction::Header, Vel, true);
			EventShot(TEXT("Header"), P);
			continue;
		}

		// Volley: an attacker meets a dropping ball at waist/chest height within shooting range.
		if (!P->IsKeeper() && !P->IsInAction() && BallLoc.Z > 70.f && BallLoc.Z < 175.f && D < 120.f + 25.f * P->StyleLevel(PKTrait::Acrobatic) && Ball->Velocity.Z < 50.f
			&& P->StealCooldown <= 0.f && FVector::Dist(P->GetFlatLocation(), OppGoal(P->Team)) < 2400.f && Ball->Velocity.Size() > 400.f)
		{
			P->StealCooldown = 0.8f;
			if (FMath::FRand() < 0.5f + 0.3f * P->Attr.Shooting)
			{
				Ball->SetController(P);
				RequestShot(P, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.7f, 1.f), true);
				EventShot(TEXT("Volley"), P);
				continue;
			}
		}

		// Headers: a dropping ball at head height is nodded on.
		if (!bKeeperHands && BallLoc.Z > 120.f && BallLoc.Z < 230.f + AerialReach * 0.6f && D < 70.f + AerialReach && Ball->Velocity.Z < 0.f && P->StealCooldown <= 0.f)
		{
			if (CheckOffside(P))
			{
				return;
			}
			P->StealCooldown = 0.5f;
			const FVector Toward = (OppGoal(P->Team) - PK::Flat(BallLoc)).GetSafeNormal2D().RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);
			const FVector HeadDir = ((-PK::Flat(Ball->Velocity)).GetSafeNormal() + Toward).GetSafeNormal2D();
			if (!HeadDir.IsNearlyZero())
			{
				P->SetActorRotation(FRotator(0.f, HeadDir.Rotation().Yaw, 0.f)); // meet the ball, nod it on
			}
			QueueTouch(P, EPKAction::Header, Toward * FMath::FRandRange(700.f, 1100.f) * (1.f + 0.1f * P->StyleLevel(PKTrait::Aerial)) + FVector(0.f, 0.f, 250.f), true);
			EventShot(TEXT("Header"), P);
			continue;
		}

		if (bKeeperHands && bOpponentBall && Speed > 900.f)
		{
			Reach = 240.f; // full-stretch dive
		}
		const float MaxZ = bKeeperHands ? 270.f : 90.f;
		if (D > Reach || BallLoc.Z > MaxZ)
		{
			continue;
		}

		// Contesting a ball an opponent is dribbling.
		if (Controller)
		{
			if (Controller->Team == P->Team)
			{
				if (ControllerDist < 220.f)
				{
					continue;
				}
			}
			else
			{
				if (P->StealCooldown > 0.f || Controller->ProtectedTimer > 0.f)
				{
					continue;
				}
				P->StealCooldown = 0.6f;
				float Chance = ControllerDist > 120.f ? 0.6f : 0.1f;
				Chance += bSliding ? 0.45f : (bLunging ? 0.3f : 0.f);
				Chance -= Controller->Stance == EPKStance::Shield ? 0.12f : 0.f;
				Chance -= Controller->Stance == EPKStance::CloseControl ? 0.06f : 0.f;
				Chance -= Controller->Stance == EPKStance::Shield ? 0.1f * Controller->Attr.Physical : 0.f;
				Chance *= FMath::Lerp(0.7f, 1.3f, P->Attr.Defending) * (P->HasTrait(PKTrait::PressProven) ? 1.15f : 1.f);
				Chance *= FMath::Lerp(1.3f, 0.75f, Controller->Attr.Control);
				// Fouls: from behind, late, or a missed lunge that takes the man.
				const bool bFromBehind = FVector::DotProduct((P->GetFlatLocation() - Controller->GetFlatLocation()).GetSafeNormal2D(), Controller->GetFacing()) < -0.4f;
				const float Care = FMath::Lerp(1.3f, 0.6f, P->Attr.Defending) * (P->YellowCards > 0 ? 0.7f : 1.f);
				const bool bWon = FMath::FRand() < Chance;
				float FoulChance = 0.f;
				if (bSliding)
				{
					FoulChance = bWon ? (bFromBehind ? 0.55f : 0.08f) : (bFromBehind ? 0.75f : 0.4f);
				}
				else if (bLunging)
				{
					FoulChance = bWon ? (bFromBehind ? 0.3f : 0.03f) : (bFromBehind ? 0.4f : 0.15f);
				}
				else if (bFromBehind)
				{
					FoulChance = bWon ? 0.12f : 0.f; // nicked from behind through the legs
				}
				if (FoulChance > 0.f && FMath::FRand() < FoulChance * Care)
				{
					const float Severity = (bSliding ? 0.55f : 0.25f) + (bFromBehind ? 0.35f : 0.f) + (bWon ? -0.15f : 0.f);
					AwardFoul(P, Controller, Severity, bSliding ? TEXT("SLIDE") : TEXT("TACKLE"));
					return;
				}
				if (bWon)
				{
					EventShot(bSliding ? TEXT("SlideWin") : (bLunging ? TEXT("TackleWin") : TEXT("Steal")), P);
					if (bSliding && FMath::FRand() < 0.6f)
					{
						Controller->PlayTrip(); // taken out by the slide
					}
					else
					{
						Controller->Stun(0.3f);
					}
					++Steals[P->Team];
					if (Controller->bHuman)
					{
						Rumble(Controller->Team, 0.55f, 0.25f); // robbed
					}
					if (P->bHuman)
					{
						Rumble(P->Team, 0.3f, 0.12f);
					}
					GainControl(P);
					return;
				}
				if (bLunging && !bSliding)
				{
					P->Stun(0.5f); // missed the tackle
				}
				continue;
			}
		}

		const bool bFast = bOpponentBall && Speed > (bKeeperHands ? 900.f : 1700.f);
		if (bFast)
		{
			if (P->LastRolledKickId == Ball->KickId)
			{
				continue;
			}
			P->LastRolledKickId = Ball->KickId;

			const float Chance = bKeeperHands ? FMath::Clamp(1.3f - Speed / 4000.f, 0.25f, 0.92f) * (D > 130.f ? 0.6f : 1.f) * FMath::Lerp(0.85f, 1.1f, P->Attr.Defending)
				: 0.25f + 0.25f * P->Attr.Defending + (P->HasTrait(PKTrait::Anticipate) ? 0.1f : 0.f);
			if (bKeeperHands && D > 130.f)
			{
				const FVector Local = P->GetActorRotation().UnrotateVector(PK::Flat(BallLoc) - P->GetFlatLocation());
				P->Dive(Local.Y >= 0.f ? 1.f : -1.f, 0.06f); // late: join the dive near the stretch
				EventShot(TEXT("Dive"), P);
			}
			if (FMath::FRand() > Chance)
			{
				if (bKeeperHands && FMath::FRand() < 0.5f)
				{
					Ball->Velocity.X *= -0.3f;
					Ball->Velocity.Y += FMath::FRandRange(-600.f, 600.f);
					Ball->Velocity.Z = 300.f;
					Ball->LastTouchTeam = P->Team;
					ShowMessage(TEXT("PARRIED!"), 1.2f);
				}
				else if (!bKeeperHands && FMath::FRand() < 0.5f)
				{
					Ball->Velocity *= 0.4f; // deflection
					Ball->Velocity.Z += 200.f;
					Ball->LastTouchTeam = P->Team;
				}
				continue;
			}
			if (bKeeperHands)
			{
				ShowMessage(TEXT("SAVE!"), 1.2f);
				UE_LOG(LogPitchKings, Log, TEXT("%s keeper saves (ball speed %.0f)"), PK::TeamName(P->Team), Speed);
			}
		}

		if (bKeeperHands && !(Ball->LastTouchTeam == P->Team && Ball->LastKicker && Ball->LastKicker != P))
		{
			if (!P->IsInAction())
			{
				// Low balls are gathered or smothered, the rest caught at the chest.
				EPKAction Catch = EPKAction::GKCatch;
				if (BallLoc.Z < 45.f)
				{
					Catch = Speed > 1200.f ? EPKAction::GKBlock : EPKAction::GKScoop;
				}
				else if (BallLoc.Z < 90.f)
				{
					Catch = EPKAction::GKCatchLow;
				}
				else if (BallLoc.Z > 190.f)
				{
					Catch = EPKAction::GKCatchHigh;
				}
				P->PlayActionTimed(Catch, 0.04f); // the ball is in his hands now: the clip joins at the catch
				EventShot(TEXT("Catch"), P);
			}
			Ball->SetHeld(P, 110.f); // catch (not from a deliberate back-pass)
			P->HoldTimer = 0.f;
			return;
		}

		GainControl(P);
		return;
	}
}

void APKGameMode::GainControl(APKFootballer* P)
{
	// First touch: kill the pace and bring it to the feet. Poor control can mean a heavy touch;
	// the human can steer the touch with the stick, or cushion it dead with the modifier.
	if (CheckOffside(P))
	{
		return;
	}
	const float ArrivalSpeed = PK::Flat(Ball->Velocity).Size();
	const bool bHumanTouch = P->bHuman && !bSpectate; // in spectate the AI takes the touch
	FVector TouchDir = P->GetFacing();
	if (bHumanTouch && !P->MoveInput.IsNearlyZero())
	{
		TouchDir = P->MoveInput.GetSafeNormal2D();
	}
	FVector FirstTouch = PK::Flat(P->GetVelocity()) + TouchDir * 60.f;
	if (bHumanTouch && (P->Stance == EPKStance::CloseControl || P->Stance == EPKStance::Shield))
	{
		FirstTouch = PK::Flat(P->GetVelocity()) * 0.4f; // cushioned dead
	}
	else if (bHumanTouch && !P->MoveInput.IsNearlyZero())
	{
		FirstTouch = TouchDir * (PK::Flat(P->GetVelocity()).Size() + 150.f + 120.f * P->Attr.Control); // directional touch into space
	}
	const float HeavyChance = FMath::Clamp((1.f - P->Attr.Control) * 0.5f * ArrivalSpeed / 1800.f, 0.f, 0.45f) * (P->HasTrait(PKTrait::Technical) ? 0.5f : 1.f);
	if (FMath::FRand() < HeavyChance)
	{
		FirstTouch += TouchDir.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector) * FMath::FRandRange(250.f, 450.f);
		FirstTouch.Z = FMath::FRandRange(150.f, 380.f); // bounces up off the shin
		EventShot(TEXT("HeavyTouch"), P);
	}
	Ball->SetController(P);
	const float ArrivalHeight = Ball->GetActorLocation().Z;
	if (ArrivalHeight < 60.f)
	{
		Ball->Touch(P, FirstTouch);
	}
	else if (ArrivalHeight < 110.f && !P->IsInAction())
	{
		// Bouncing ball: cushion it down with the thigh.
		QueueTouch(P, EPKAction::ThighControl, FirstTouch * 0.5f + FVector(0.f, 0.f, -100.f), false, 1.5f);
		EventShot(TEXT("ThighControl"), P);
	}
	P->TouchTimer = 0.15f;
	P->HoldTimer = 0.f;
	P->DecisionTimer = FMath::FRandRange(0.2f, 0.4f);
	P->AIDribbleDir = (OppGoal(P->Team) - P->GetFlatLocation()).GetSafeNormal2D();

	if (bHumanTeam[P->Team] && !P->IsKeeper())
	{
		SetHuman(P);
	}
	const int32 Other = 1 - P->Team;
	if (bHumanTeam[Other] && Humans[Other] && FVector::Dist(Humans[Other]->GetFlatLocation(), P->GetFlatLocation()) > 1500.f)
	{
		SetHuman(ClosestOutfield(Other, P->GetFlatLocation(), nullptr)); // defending: jump to the man nearest the ball
	}
}

// ---------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------

void APKGameMode::CheckBallOut()
{
	const FVector B = Ball->GetActorLocation();

	if (Ball->IsInNet())
	{
		const int32 Scorer = B.X * PK::AttackDir(0) > 0.f ? 0 : 1;
		++Score[Scorer];
		NextKickoffTeam = 1 - Scorer;
		Phase = EPKPhase::GoalScored;
		PhaseTimer = 3.5f;
		BeginCelebration(Scorer);
		ShowMessage(FString::Printf(TEXT("GOAL!  %s"), *TeamLabel(Scorer)), 3.5f);
		if (APKFootballer* Beaten = Keeper(1 - Scorer))
		{
			if (!Beaten->IsInAction())
			{
				Beaten->PlayAction(EPKAction::GKMiss);
			}
		}
		Rumble(Scorer, 0.8f, 0.6f);
		Rumble(1 - Scorer, 0.35f, 0.3f);
		UE_LOG(LogPitchKings, Log, TEXT("GOAL %s  (BLUE %d - %d RED, %.0fs left)"), PK::TeamName(Scorer), Score[0], Score[1], TimeLeft);
		return;
	}
	if (!Ball->IsFree())
	{
		return;
	}

	if (FMath::Abs(B.X) > PK::HalfLength + PK::BallRadius)
	{
		const float Side = FMath::Sign(B.X);
		const int32 Defending = Side * PK::AttackDir(0) > 0.f ? 1 : 0;
		const int32 Attacking = 1 - Defending;
		if (Ball->LastTouchTeam == Attacking)
		{
			AwardRestart(Defending, FVector(Side * (PK::HalfLength - PK::SixDepth), B.Y >= 0.f ? 400.f : -400.f, 0.f), true, false, TEXT("GOAL KICK"));
		}
		else
		{
			const float SY = B.Y >= 0.f ? 1.f : -1.f;
			AwardRestart(Attacking, FVector(Side * (PK::HalfLength - 40.f), SY * (PK::HalfWidth - 40.f), 0.f), false, false, TEXT("CORNER"));
		}
		return;
	}

	if (FMath::Abs(B.Y) > PK::HalfWidth + PK::BallRadius)
	{
		const int32 Team = Ball->LastTouchTeam == 0 ? 1 : 0;
		const FVector Spot(FMath::Clamp(B.X, -PK::HalfLength + 100.f, PK::HalfLength - 100.f), FMath::Sign(B.Y) * (PK::HalfWidth + 20.f), 0.f);
		AwardRestart(Team, Spot, false, true, TEXT("THROW-IN"));
	}
}

void APKGameMode::AwardRestart(int32 Team, const FVector& Spot, bool bGoalKick, bool bThrowIn, const FString& Label)
{
	APKFootballer* Taker = bGoalKick ? Keeper(Team) : ClosestOutfield(Team, Spot, nullptr);
	if (!Taker)
	{
		return;
	}

	const FVector Inward = (FVector::ZeroVector - PK::Flat(Spot)).GetSafeNormal2D();
	if (bThrowIn)
	{
		Taker->Teleport2D(PK::Flat(Spot), FVector(0.f, -FMath::Sign(Spot.Y), 0.f).Rotation().Yaw);
		Ball->SetHeld(Taker, 200.f);
	}
	else
	{
		Taker->Teleport2D(PK::Flat(Spot) - Inward * 70.f, Inward.Rotation().Yaw);
		Ball->PlaceAt(Spot, Taker);
	}
	Taker->ProtectedTimer = 2.f;
	Taker->HoldTimer = 0.f;

	// Opponents back off.
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team)
		{
			continue;
		}
		const FVector Away = P->GetFlatLocation() - PK::Flat(Spot);
		if (Away.Size() < 600.f)
		{
			const FVector Dir = Away.IsNearlyZero() ? Inward : Away.GetSafeNormal2D();
			P->Teleport2D(PK::Flat(Spot) + Dir * 600.f, P->GetActorRotation().Yaw);
		}
	}

	if (bHumanTeam[Team] && !Taker->IsKeeper())
	{
		SetHuman(Taker);
	}

	SetPiece = bGoalKick ? EPKSetPiece::GoalKick : (bThrowIn ? EPKSetPiece::ThrowIn : (Label == TEXT("CORNER") ? EPKSetPiece::Corner : EPKSetPiece::FreeKick));
	Phase = EPKPhase::Stoppage;
	PhaseTimer = 1.f;
	RestartTimer = 0.f;
	ShowMessage(FString::Printf(TEXT("%s - %s"), *Label, *TeamLabel(Team)), 1.5f);
}

void APKGameMode::SetHuman(APKFootballer* P)
{
	if (bAutoTest || !P || !bHumanTeam[P->Team] || P == Humans[P->Team])
	{
		return;
	}
	if (APKFootballer* Old = Humans[P->Team])
	{
		Old->SetHumanControlled(false);
	}
	Humans[P->Team] = P;
	P->SetHumanControlled(true);
}

void APKGameMode::Rumble(int32 Team, float Strength, float Seconds)
{
	if (bSpectate || bAutoTest || !bHumanTeam[FMath::Clamp(Team, 0, 1)])
	{
		return;
	}
	if (APKPlayerController* PC = Cast<APKPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->RumbleTeam(Team, Strength, Seconds);
	}
}

APKFootballer* APKGameMode::ClosestOutfield(int32 Team, const FVector& Point, const APKFootballer* Exclude) const
{
	APKFootballer* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (APKFootballer* P : Footballers)
	{
		if (P->Team != Team || P->IsKeeper() || P == Exclude)
		{
			continue;
		}
		const float D = FVector::Dist(P->GetFlatLocation(), PK::Flat(Point));
		if (D < BestDist)
		{
			BestDist = D;
			Best = P;
		}
	}
	return Best;
}

APKFootballer* APKGameMode::Keeper(int32 Team) const
{
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team && P->IsKeeper())
		{
			return P;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Human controls
// ---------------------------------------------------------------------------------------------

bool APKGameMode::HumanHasBall(int32 Team) const
{
	const APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	return Human && Ball && Ball->GetOwnerPlayer() == Human;
}

void APKGameMode::HumanMove(int32 Team, const FVector& Direction, bool bInSprint, bool bModifier, bool bPrecision)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human)
	{
		return;
	}
	const bool bDeadBallTaker = (!Ball->IsFree() && Ball->GetOwnerPlayer() == Human) || (Human->bHoldPosition && Ball->IsPlaced());
	const bool bCanMove = Phase == EPKPhase::Playing && !Human->IsStunned() && !bDeadBallTaker;
	Human->HumanInput = bCanMove ? Direction.GetClampedToMaxSize(1.f) : FVector::ZeroVector;
	const bool bAssisted = bCanMove && !Ball->GetOwnerPlayer() && ((Direction.Size() < 0.2f && Ball->IntendedReceiver == Human) || (Direction.Size() < 0.5f && Human->HeaderRequestTimer > 0.f));
	if (!bAssisted)
	{
		Human->MoveInput = Human->HumanInput;
		// Controlled sprint (Precision held while running with the ball) replaces the full sprint.
		Human->bControlledSprint = bCanMove && bPrecision && !bModifier && Ball->GetController() == Human && Direction.Size() > 0.3f;
		Human->bSprint = bCanMove && bInSprint && !bModifier && !Human->bControlledSprint;
	}

	// Modifier (LT / Ctrl): jockey without the ball, shield or close control with it.
	Human->Stance = EPKStance::Normal;
	if (bModifier && bCanMove)
	{
		if (Ball->GetController() == Human)
		{
			float NearDist = 0.f;
			const APKFootballer* Opp = NearestOpponent(Human, NearDist);
			if (Opp && NearDist < 320.f)
			{
				Human->Stance = EPKStance::Shield;
				Human->FaceTarget = Opp->GetActorLocation();
			}
			else
			{
				Human->Stance = EPKStance::CloseControl;
			}
		}
		else
		{
			Human->Stance = EPKStance::Jockey;
			Human->FaceTarget = Ball->GetActorLocation();
		}
	}
}

void APKGameMode::HumanPass(int32 Team, const FVector& Aim, EPKKickType Type, bool bSwerve, bool bOneTwo)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}
	// The return of a one-two: pass / through within a few seconds goes to the runner.
	const int32 T2 = FMath::Clamp(Team, 0, 1);
	if (OneTwoTimer[T2] > 0.f && OneTwoReceiver[T2].Get() == Human && OneTwoPasser[T2].IsValid() && !bOneTwo)
	{
		RequestPass(Human, OneTwoPasser[T2].Get(), EPKKickType::Through, bSwerve);
		OneTwoTimer[T2] = 0.f;
		++OneTwoReturns;
		return;
	}
	if (!Ball->IsFree() && RestartTimer < 0.4f)
	{
		return;
	}

	const FVector AimDir = Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D();
	const FVector Loc = Human->GetFlatLocation();

	APKFootballer* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (APKFootballer* T : Footballers)
	{
		if (T == Human || T->Team != Human->Team)
		{
			continue;
		}
		const FVector D = T->GetFlatLocation() - Loc;
		const float Dist = D.Size();
		const float Cos = FVector::DotProduct(D / FMath::Max(Dist, 1.f), AimDir);
		if (Cos < 0.45f || Dist > 5000.f)
		{
			continue;
		}
		const float Rating = Cos * 1.6f - Dist / 4500.f;
		if (Rating > BestScore)
		{
			BestScore = Rating;
			Best = T;
		}
	}

	if (Best)
	{
		RequestPass(Human, Best, Type, bSwerve);
		if (bOneTwo)
		{
			// One-two: play it in and go. The passer runs into space for the return.
			OneTwoPasser[T2] = Human;
			OneTwoReceiver[T2] = Best;
			OneTwoTimer[T2] = 4.f;
			SendRun(Best, Human);
			++OneTwos;
		}
	}
	else
	{
		RequestPassToPoint(Human, Loc + AimDir * 1500.f, Type, bSwerve);
	}
	SwervePasses += bSwerve ? 1 : 0;
}

float APKGameMode::ChooseSwerve(const APKFootballer* P, const FVector& Target) const
{
	// Bend it around the defender nearest the line: the path bulges away from him. Nobody there: bend it the
	// way the passing foot naturally curls it (right-footed: bulges right).
	const FVector A = P->GetFlatLocation();
	const FVector B = PK::Flat(Target);
	const FVector Dir = (B - A).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
	const float Len = FVector::Dist(A, B);
	float BestDist = 350.f;
	float Bulge = 1.f;
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const FVector L = O->GetFlatLocation();
		const float Along = FVector::DotProduct(L - A, Dir);
		if (Along < 150.f || Along > Len - 150.f)
		{
			continue;
		}
		const float Off = FVector::DotProduct(L - A, Side);
		if (FMath::Abs(Off) < BestDist)
		{
			BestDist = FMath::Abs(Off);
			Bulge = Off >= 0.f ? -1.f : 1.f;
		}
	}
	return Bulge;
}

FVector APKGameMode::RunSpot(const APKFootballer* Runner) const
{
	// Into the space in behind: up to 15 m ahead, at most 6 m beyond the second-last defender (timed run).
	const int32 Team = Runner->Team;
	const float Dir = PK::AttackDir(Team);
	float SecondLast = -1.f, Last = -1.f; // team frame x of the two deepest opponents
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == Team)
		{
			continue;
		}
		const float Fx = WorldToFx(Team, O->GetActorLocation().X);
		if (Fx > Last)
		{
			SecondLast = Last;
			Last = Fx;
		}
		else if (Fx > SecondLast)
		{
			SecondLast = Fx;
		}
	}
	const float MyFx = WorldToFx(Team, Runner->GetActorLocation().X);
	float RunFx = FMath::Min(MyFx + 1500.f / PK::HalfLength, SecondLast + 600.f / PK::HalfLength);
	RunFx = FMath::Clamp(FMath::Max(RunFx, MyFx + 300.f / PK::HalfLength), -0.9f, 0.9f);
	// Bend the run toward the middle a little (between the centre-backs and full-backs).
	const float Y = Runner->GetActorLocation().Y * 0.8f;
	return FVector(Dir * RunFx * PK::HalfLength, Y, 0.f);
}

APKFootballer* APKGameMode::SendRun(APKFootballer* Carrier, APKFootballer* Runner)
{
	if (!Carrier)
	{
		return nullptr;
	}
	if (!Runner)
	{
		// The best-placed team-mate to go: forwards first, then whoever is furthest forward and not marked tight.
		float Best = -TNumericLimits<float>::Max();
		for (APKFootballer* T : Footballers)
		{
			if (T == Carrier || T->Team != Carrier->Team || T->IsKeeper() || T->IsStunned())
			{
				continue;
			}
			float Near = 0.f;
			NearestOpponent(T, Near);
			const float RunScore = WorldToFx(T->Team, T->GetActorLocation().X) * 2.f + (T->Role == EPKRole::Forward ? 0.6f : (T->Role == EPKRole::Midfielder ? 0.2f : -0.5f))
				+ FMath::Min(Near, 600.f) / 1500.f - FVector::Dist(T->GetFlatLocation(), Carrier->GetFlatLocation()) / 8000.f;
			if (RunScore > Best)
			{
				Best = RunScore;
				Runner = T;
			}
		}
	}
	if (!Runner)
	{
		return nullptr;
	}
	Runner->AITarget = RunSpot(Runner);
	Runner->AITargetTimer = 2.6f;
	Runner->RunTimer = 2.6f;
	++RunsSent;
	return Runner;
}

void APKGameMode::HumanSendRun(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human && HumanHasBall(Team))
	{
		if (APKFootballer* Runner = SendRun(Human))
		{
			ShotFeedback = FString::Printf(TEXT("RUN: #%d"), Runner->ShirtNumber);
			ShotFeedbackColor = FLinearColor(0.5f, 0.85f, 1.f);
			ShotFeedbackTimer = 1.f;
		}
	}
}

void APKGameMode::SetMentality(int32 Team, int32 Value, bool bAnnounce)
{
	const int32 T = FMath::Clamp(Team, 0, 1);
	Value = FMath::Clamp(Value, -2, 2);
	if (Mentality[T] == Value)
	{
		return;
	}
	Mentality[T] = Value;
	if (bAnnounce)
	{
		ShowMessage(FString::Printf(TEXT("%s: %s"), *TeamLabel(T), MentalityName(Value)), 1.5f);
	}
	UE_LOG(LogPitchKings, Log, TEXT("Game plan: %s now %s"), PK::TeamName(T), MentalityName(Value));
}

const TCHAR* APKGameMode::MentalityName(int32 Value)
{
	switch (Value)
	{
	case -2: return TEXT("Ultra defensive");
	case -1: return TEXT("Defensive");
	case 1:  return TEXT("Attacking");
	case 2:  return TEXT("Ultra attacking");
	default: return TEXT("Balanced");
	}
}

void APKGameMode::UpdateGamePlans(float Dt)
{
	for (int32 T = 0; T < 2; ++T)
	{
		OneTwoTimer[T] = FMath::Max(0.f, OneTwoTimer[T] - Dt);
	}
	// The computer changes its plan late on: chasing the game it goes for it, protecting a lead it sits deeper.
	PlanTimer -= Dt;
	if (PlanTimer > 0.f || Phase != EPKPhase::Playing)
	{
		return;
	}
	PlanTimer = 2.f;
	const bool bLate = TimeLeft < MatchTotal * 0.3f;
	for (int32 T = 0; T < 2; ++T)
	{
		if (IsHumanTeam(T) && !bSpectate)
		{
			continue;
		}
		const int32 Diff = Score[T] - Score[1 - T];
		const int32 Want = !bLate ? 0 : (Diff < 0 ? (Diff < -1 ? 2 : 1) : (Diff > 0 ? -1 : 0));
		SetMentality(T, Want, true);
	}
}

void APKGameMode::HumanPrecisionPass(int32 Team, const FVector& Aim, EPKKickType Type, float Charge, bool bSwerve)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}
	if (!Ball->IsFree() && RestartTimer < 0.4f)
	{
		return;
	}
	const FVector AimDir = Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D();
	const FVector Loc = Human->GetFlatLocation();
	const float C = FMath::Clamp(Charge, 0.f, 1.f);
	const float Dist = Type == EPKKickType::Lob ? FMath::Lerp(900.f, 4500.f, C) : FMath::Lerp(400.f, 4000.f, C);
	FVector Point = Loc + AimDir * Dist;
	Point.X = FMath::Clamp(Point.X, -PK::HalfLength + 100.f, PK::HalfLength - 100.f);
	Point.Y = FMath::Clamp(Point.Y, -PK::HalfWidth + 100.f, PK::HalfWidth - 100.f);
	// The team-mate best placed to get there runs onto it.
	APKFootballer* Receiver = nullptr;
	float BestT = 3.5f;
	for (APKFootballer* T : Footballers)
	{
		if (T == Human || T->Team != Human->Team || T->IsKeeper())
		{
			continue;
		}
		const float Time = FVector::Dist(T->GetFlatLocation(), Point) / PK::SprintSpeed;
		if (Time < BestT)
		{
			BestT = Time;
			Receiver = T;
		}
	}
	FPKKickRequest Kick;
	Kick.Type = Type;
	Kick.Target = Point;
	Kick.Power = FMath::Clamp(Dist / 4000.f, 0.2f, 1.f);
	Kick.Receiver = Receiver;
	Kick.bPrecision = true;
	Kick.Swerve = bSwerve ? ChooseSwerve(Human, Point) : 0.f;
	if (Type == EPKKickType::Through)
	{
		++ThroughBalls[Human->Team];
	}
	++PrecisionPasses;
	SwervePasses += bSwerve ? 1 : 0;
	TryStartKick(Human, Kick);
}

bool APKGameMode::HumanRequestHeader(int32 Team, const FVector& Aim, bool bShot)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human || Phase != EPKPhase::Playing || Ball->GetOwnerPlayer() || Ball->GetHolder() || Human->IsInAction())
	{
		return false;
	}
	// Only for a ball in the air that is coming his way.
	const FVector BallLoc = Ball->GetActorLocation();
	if (BallLoc.Z < 80.f || FVector::Dist(PK::Flat(BallLoc), Human->GetFlatLocation()) > 900.f)
	{
		return false;
	}
	Human->HeaderRequestTimer = 1.2f;
	Human->bHeaderRequestShot = bShot;
	Human->HeaderRequestAim = Aim;
	return true;
}

void APKGameMode::HumanShoot(int32 Team, const FVector& Aim, float Power, EPKShotType Type)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!HumanHasBall(Team) || Phase == EPKPhase::GoalScored || Phase == EPKPhase::FullTime || !CanReachBall(Human, KickReach))
	{
		return;
	}
	RequestShot(Human, Aim.Y * PK::AttackDir(Human->Team), FMath::Clamp(Power, 0.1f, 1.f), false, Type);
}

bool APKGameMode::HumanTimedFinish(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Human || !Human->IsKicking() || Now - Human->ShotStartTime > 1.5)
	{
		return false;
	}
	if (Human->TimedShotPress < Human->ShotStartTime)
	{
		Human->TimedShotPress = Now; // only the first press counts
	}
	return true;
}

const TCHAR* APKGameMode::SkillName(int32 Skill)
{
	static const TCHAR* Names[] = { TEXT("STEP-OVER"), TEXT("BALL ROLL"), TEXT("ROULETTE"), TEXT("RAINBOW FLICK"), TEXT("NUTMEG") };
	return Names[FMath::Clamp(Skill, 0, 4)];
}

bool APKGameMode::PerformSkill(APKFootballer* P, int32 Skill, float Side)
{
	if (!P || Ball->GetController() != P || !CanReachBall(P, 110.f) || P->IsKicking() || P->IsInAction() || P->TouchTimer > 0.15f || P->SkillSpinTimer > 0.f)
	{
		return false;
	}
	Side = Side >= 0.f ? 1.f : -1.f;
	const FVector Facing = P->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	const FVector Vel = PK::Flat(P->GetVelocity());
	const float Ctrl = FMath::Clamp(P->Attr.Control + (P->HasTrait(PKTrait::Technical) ? 0.05f : 0.f) + 0.07f * P->StyleLevel(PKTrait::Trickster), 0.f, 1.f);
	// A poor dribbler sometimes over-hits it.
	const float Botch = FMath::FRand() < (0.8f - Ctrl) * 0.5f ? FMath::FRandRange(1.3f, 1.7f) : 1.f;
	UCharacterMovementComponent* Move = P->GetCharacterMovement();

	// The defender in front (if any).
	APKFootballer* Front = nullptr;
	float FrontD = 400.f;
	for (APKFootballer* O : Footballers)
	{
		const FVector To = O->GetFlatLocation() - P->GetFlatLocation();
		if (O->Team != P->Team && !O->IsKeeper() && To.Size() < FrontD && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.5f)
		{
			FrontD = To.Size();
			Front = O;
		}
	}
	auto TryBeat = [&](float Base, float Stun)
	{
		if (Front && FMath::FRand() < Base + 0.45f * Ctrl - 0.3f * Front->Attr.Defending)
		{
			Front->Stun(Stun);
			Front->StealCooldown = FMath::Max(Front->StealCooldown, Stun + 0.3f);
			++SkillBeats;
		}
	};

	switch (Skill)
	{
	case 0: // step-over: feint towards Side, take it the other way
		P->StartSkillPose(1, Side);
		Ball->Touch(P, Vel * 0.6f - Right * Side * 190.f * Botch + Facing * 110.f);
		Move->Velocity = Vel * 0.6f - Right * Side * 260.f;
		P->TouchTimer = 0.45f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.55f);
		TryBeat(0.3f, 0.45f);
		break;
	case 1: // ball roll: roll it across with the sole
		P->StartSkillPose(2, Side);
		Ball->Touch(P, Vel * 0.3f + Right * Side * 270.f * Botch);
		Move->Velocity = Vel * 0.3f + Right * Side * 220.f;
		P->TouchTimer = 0.45f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.5f);
		TryBeat(0.2f, 0.35f);
		break;
	case 2: // roulette: spin over the ball and away past the defender
		P->StartSkillPose(3, Side);
		P->SkillSpinTimer = 0.6f;
		P->SkillSpinRate = Side * 600.f;
		Ball->Touch(P, (Facing * 0.6f + Right * Side).GetSafeNormal2D() * 230.f * Botch);
		Move->Velocity = (Facing * 0.6f + Right * Side).GetSafeNormal2D() * 260.f;
		P->TouchTimer = 0.6f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.85f);
		TryBeat(0.25f, 0.35f);
		break;
	case 3: // rainbow flick: from behind, over his own head and the defender's
		P->StartSkillPose(4, Side);
		Ball->Touch(P, Facing * (FMath::Max(0.f, FVector::DotProduct(Vel, Facing)) + 360.f * Botch) + FVector(0.f, 0.f, 560.f));
		P->TouchTimer = 1.1f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 1.f);
		if (Front)
		{
			Front->StealCooldown = FMath::Max(Front->StealCooldown, 1.f); // it goes over him
		}
		break;
	case 4: // nutmeg: through his legs and run round him
	{
		P->StartSkillPose(5, Side);
		const bool bClean = Front && FMath::FRand() < 0.4f + 0.5f * Ctrl - 0.3f * Front->Attr.Defending;
		const FVector Through = Front ? Front->GetFlatLocation() + Facing * 230.f : P->GetFlatLocation() + Facing * 300.f;
		Ball->Touch(P, (Through - PK::Flat(Ball->GetActorLocation())).GetSafeNormal2D() * 520.f * (bClean ? 1.f : 0.6f) * Botch);
		Move->Velocity = (Facing + Right * Side * 0.8f).GetSafeNormal2D() * 420.f;
		P->TouchTimer = 0.8f;
		P->ProtectedTimer = FMath::Max(P->ProtectedTimer, bClean ? 0.9f : 0.2f);
		if (bClean && Front)
		{
			Front->StealCooldown = FMath::Max(Front->StealCooldown, 1.2f); // through the legs: he can't stop it
			Front->Stun(0.5f);                                               // and has to turn round
			++SkillBeats;
		}
		break;
	}
	default:
		return false;
	}
	++SkillMoves;
	if (P->bHuman)
	{
		ShotFeedback = SkillName(Skill);
		ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
		ShotFeedbackTimer = 0.9f;
	}
	EventShot(TEXT("Feint"), P);
	return true;
}

void APKGameMode::HumanSkill(int32 Team, const FVector& Dir)
{
	// The direction (relative to the way he faces) picks the move: forward = rainbow (nutmeg when a defender is
	// right in front), diagonal = step-over, sideways = ball roll, back = roulette.
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (!Human || !HumanHasBall(Team))
	{
		return;
	}
	const FVector Facing = Human->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	if (Dir.IsNearlyZero())
	{
		PerformSkill(Human, 0, FMath::FRand() < 0.5f ? -1.f : 1.f);
		return;
	}
	const FVector D = Dir.GetSafeNormal2D();
	const float Ang = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(D, Right), FVector::DotProduct(D, Facing)));
	const float Side = Ang >= 0.f ? 1.f : -1.f;
	const float A = FMath::Abs(Ang);
	int32 Skill = 2;
	if (A < 35.f)
	{
		bool bTight = false;
		for (const APKFootballer* O : Footballers)
		{
			const FVector To = O->GetFlatLocation() - Human->GetFlatLocation();
			bTight |= O->Team != Human->Team && To.Size() < 240.f && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.8f;
		}
		Skill = bTight ? 4 : 3;
	}
	else if (A < 75.f)
	{
		Skill = 0;
	}
	else if (A < 125.f)
	{
		Skill = 1;
	}
	PerformSkill(Human, Skill, Side);
}

void APKGameMode::HumanFakeShot(int32 Team, const FVector& Aim)
{
	FakeShot(Humans[FMath::Clamp(Team, 0, 1)], Aim);
}

void APKGameMode::FakeShot(APKFootballer* P, const FVector& Aim)
{
	// Shape to shoot, then drag it across the body with the inside of the other foot.
	if (!P || Ball->GetController() != P || !CanReachBall(P, 100.f) || P->IsKicking() || P->IsInAction())
	{
		return;
	}
	const FVector Facing = P->GetFacing();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	float SideSign = FVector::DotProduct(Aim, Right);
	SideSign = FMath::Abs(SideSign) > 0.2f ? FMath::Sign(SideSign) : (FMath::FRand() < 0.5f ? -1.f : 1.f);
	Ball->Touch(P, PK::Flat(P->GetVelocity()) * 0.4f + Right * SideSign * 330.f + Facing * 60.f);
	P->GetCharacterMovement()->Velocity = PK::Flat(P->GetVelocity()) * 0.5f + Right * SideSign * 300.f;
	P->NoteFootTouch(SideSign > 0.f ? 0 : 1, SideSign, false);
	P->TouchTimer = 0.4f;
	P->ProtectedTimer = FMath::Max(P->ProtectedTimer, 0.4f);
	for (APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const FVector To = O->GetFlatLocation() - P->GetFlatLocation();
		const bool bKeeperNear = O->IsKeeper() && To.Size() < 1600.f;
		if ((To.Size() < 420.f && FVector::DotProduct(To.GetSafeNormal2D(), Facing) > 0.f) || bKeeperNear)
		{
			const float Chance = (bKeeperNear ? 0.3f : 0.45f) + 0.3f * P->Attr.Control - 0.25f * O->Attr.Defending;
			if (FMath::FRand() < Chance)
			{
				O->Stun(bKeeperNear ? 0.5f : 0.55f); // bought it: committed to the block
				O->StealCooldown = 0.9f;
			}
		}
	}
	++FakeShots;
	if (P->bHuman)
	{
		ShotFeedback = TEXT("FAKE SHOT");
		ShotFeedbackColor = FLinearColor(1.f, 0.85f, 0.2f);
		ShotFeedbackTimer = 1.f;
	}
	EventShot(TEXT("Feint"), P);
}

void APKGameMode::HumanTackle(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human && Phase == EPKPhase::Playing && !Human->IsStunned() && Human->TackleCooldown <= 0.f && !HumanHasBall(Team))
	{
		TryTackle(Human, 0.f);
	}
}

void APKGameMode::HumanSlideTackle(int32 Team, const FVector& Aim)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human && Phase == EPKPhase::Playing && !Human->IsStunned() && Human->TackleCooldown <= 0.f && !HumanHasBall(Team))
	{
		Human->TackleCooldown = 1.5f;
		const FVector Dir = Aim.IsNearlyZero() ? Human->GetFacing() : Aim.GetSafeNormal2D();
		Human->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		Human->PlaySlideTackle();
	}
}

void APKGameMode::HumanSwitch(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	if (Human && !HumanHasBall(Team))
	{
		SetHuman(ClosestOutfield(Human->Team, Ball->GetActorLocation(), Human));
	}
}

void APKGameMode::HumanKnockOn(int32 Team, const FVector& Aim, float Strength)
{
	EffortTouch(Humans[FMath::Clamp(Team, 0, 1)], Aim, Strength);
}

void APKGameMode::EffortTouch(APKFootballer* Human, const FVector& Aim, float Strength)
{
	// Effort dribble touch: knock the ball any way at once. A tap is a short touch that stays under control,
	// a held press pushes it into space to run onto.
	if (!Human || Ball->GetController() != Human || !CanReachBall(Human, 100.f) || Human->IsKicking())
	{
		return;
	}
	const float S = FMath::Clamp(Strength, 0.2f, 1.f);
	const FVector Facing = Human->GetFacing();
	const FVector Dir = Aim.IsNearlyZero() ? Facing : Aim.GetSafeNormal2D();
	const float Along = FMath::Max(0.f, FVector::DotProduct(PK::Flat(Human->GetVelocity()), Dir));
	FVector V = Dir * (Along + FMath::Lerp(260.f, 900.f, S));
	V.Z = S > 0.7f ? 160.f * (S - 0.7f) / 0.3f : 0.f; // a long knock skips off the grass
	Ball->Touch(Human, V);
	Human->TouchTimer = FMath::Lerp(0.2f, 0.6f, S);
	Human->bSprint = S > 0.6f;
	// Sideways knocks go with the outside of the foot on that side; forward ones with the laces.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	const float Lat = FVector::DotProduct(Dir, Right);
	if (FMath::Abs(Lat) > 0.3f)
	{
		Human->NoteFootTouch(Lat > 0.f ? 1 : 0, FMath::Sign(Lat) * FMath::Min(1.f, FMath::Abs(Lat) + 0.3f), true);
	}
	++EffortTouches;
}

void APKGameMode::HumanDragBack(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
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

void APKGameMode::HumanFeint(int32 Team, const FVector& Aim)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
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
	ShowMessage(FString::Printf(TEXT("%s: %s"), *TeamLabel(Team), *StyleName(InStyle)), 1.5f);
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
	// During a wind-up the ball is brought onto the striking foot so that it is exactly where the
	// foot will be on the contact frame: it leaves from the foot, in the direction the body faces.
	if (!Ball->IsFree() || Ball->IsPlaced())
	{
		return;
	}
	APKFootballer* P = Ball->GetController();
	if (!P || !P->IsKicking())
	{
		P = nullptr;
		float Best = 170.f;
		for (APKFootballer* F : Footballers)
		{
			if (F->IsKicking() && F->GetKickClip() != EPKAction::None)
			{
				const float D = FVector::Dist(PK::Flat(Ball->GetActorLocation()), F->GetFlatLocation());
				if (D < Best)
				{
					Best = D;
					P = F;
				}
			}
		}
	}
	if (!P || P->GetKickClip() == EPKAction::None || P->GetCurrentAction() != P->GetKickClip())
	{
		return;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Contact = P->GetKickContactPoint();
	const bool bVolley = P->GetCurrentAction() == EPKAction::Volley;
	if (FVector::Dist(BallLoc, Contact) > (bVolley ? 260.f : 170.f) || (!bVolley && BallLoc.Z > 60.f))
	{
		return;
	}
	const float Remaining = P->TimeToContact();
	const float Alpha = Remaining > Dt ? FMath::Clamp(Dt / Remaining, 0.f, 1.f) : 1.f;
	Ball->SetActorLocation(FMath::Lerp(BallLoc, Contact, Alpha));
	Ball->Velocity = FVector::ZeroVector;
}

bool APKGameMode::BallReadyForKick(const APKFootballer* P) const
{
	if (Ball->GetHolder() == P || Ball->IsPlaced())
	{
		return true;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Rel = PK::Flat(Ball->Velocity - P->GetVelocity());
	return BallLoc.Z < 60.f && FVector::Dist(PK::Flat(BallLoc), P->GetFootPoint()) < 70.f && FVector::DotProduct(Rel, P->GetFacing()) < 350.f;
}

bool APKGameMode::TryStartKick(APKFootballer* P, const FPKKickRequest& Kick, bool bAirborne)
{
	// Only wind up when the ball is at the feet; otherwise keep the decision a moment while the
	// player catches the ball up (no more balls flying off from a metre away).
	if (!bAirborne && !BallReadyForKick(P))
	{
		if (!P->IsKicking())
		{
			P->BufferedKick = Kick;
			P->BufferTimer = 0.7f;
		}
		return false;
	}
	P->BufferTimer = 0.f;
	const bool bHands = Ball->GetHolder() == P;
	if (!P->StartKick(Kick, bHands && P->IsKeeper(), bHands && !P->IsKeeper(), bAirborne))
	{
		return false;
	}
	AfterKickStart(P);
	if (bContactShots && bAutoTest && ContactShotCount < 30 && EventShotCooldown <= 0.f && RestoreDilationFrames == 0)
	{
		ContactWatch = P;
		EventFocusActor = P;
		EventShotCooldown = 1.5f;
	}
	return true;
}

void APKGameMode::UpdateBufferedKicks(float Dt)
{
	for (APKFootballer* P : Footballers)
	{
		if (P->BufferTimer <= 0.f)
		{
			continue;
		}
		P->BufferTimer -= Dt;
		if (Ball->GetOwnerPlayer() != P || P->IsKicking() || P->IsStunned())
		{
			P->BufferTimer = 0.f;
			continue;
		}
		// Run onto the ball.
		if (!P->bHuman || bSpectate)
		{
			P->MoveInput = (PK::Flat(Ball->GetActorLocation()) - P->GetFootPoint()).GetSafeNormal2D() * 0.8f;
			P->bSprint = false;
		}
		if (BallReadyForKick(P))
		{
			const FPKKickRequest Kick = P->BufferedKick;
			P->BufferTimer = 0.f;
			TryStartKick(P, Kick);
		}
		else if (P->BufferTimer <= 0.f)
		{
			++BufferedDropped;
		}
	}
}

void APKGameMode::ContactSnap(APKFootballer* P, const TCHAR* Label)
{
	if (!bContactShots || ContactWatch.Get() != P)
	{
		return;
	}
	ContactWatch = nullptr;
	++ContactShotCount;
	UGameplayStatics::SetGlobalTimeDilation(this, 0.03f);
	RestoreDilationFrames = 6;
	PendingShot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKContact_%02d_%s.png"), ContactShotCount, Label));
	PendingShotFrames = 1;
	UE_LOG(LogPitchKings, Log, TEXT("[Contact] %d %s #%d %s"), ContactShotCount, PK::TeamName(P->Team), P->Index, Label);
}

void APKGameMode::UpdateBodyContacts(float Dt)
{
	// Soft body contact: overlapping players are pushed apart, the stronger / faster one moving less.
	// Nobody can be wedged between two opponents any more, and contact with the ball carrier becomes
	// a shoulder challenge (legal) or a push from behind (foul).
	for (APKFootballer* P : Footballers)
	{
		P->ShoulderCooldown = FMath::Max(0.f, P->ShoulderCooldown - Dt);
		// Stuck metric: wants to move, is not moving, nothing else explains it.
		const bool bWantsMove = P->MoveInput.Size() > 0.5f && !P->IsInAction() && !P->IsStunned() && !P->bHoldPosition && Phase == EPKPhase::Playing;
		P->StuckTimer = bWantsMove && PK::Flat(P->GetVelocity()).Size() < 60.f ? P->StuckTimer + Dt : 0.f;
		if (P->StuckTimer > 0.6f)
		{
			StuckSeconds += Dt;
		}
	}
	if (bHardCollision)
	{
		return;
	}

	auto Strength = [this](const APKFootballer* F, const FVector& TowardOther)
	{
		float S = 0.7f + 0.6f * F->Attr.Physical;
		S += FMath::Max(0.f, FVector::DotProduct(PK::Flat(F->GetVelocity()), TowardOther)) * 0.0006f; // momentum
		S += F->Stance == EPKStance::Shield ? 0.35f : 0.f;
		S += F->IsKeeper() ? 0.3f : 0.f;
		S *= F->IsStunned() ? 0.4f : 1.f;
		return S;
	};

	const float MinDist = 80.f; // shoulders and swinging arms no longer pass through each other (the MetaHuman shirts are bulkier)
	APKFootballer* Carrier = Ball->GetController();
	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		APKFootballer* A = Footballers[i];
		for (int32 j = i + 1; j < Footballers.Num(); ++j)
		{
			APKFootballer* B = Footballers[j];
			FVector Delta = B->GetFlatLocation() - A->GetFlatLocation();
			const float D = Delta.Size();
			if (D >= MinDist + 25.f)
			{
				continue;
			}
			const FVector N = D > 1.f ? Delta / D : A->GetActorRightVector();

			// Physical duel with the ball carrier.
			if (A->Team != B->Team && (A == Carrier || B == Carrier) && Phase == EPKPhase::Playing)
			{
				APKFootballer* C = A == Carrier ? A : B;
				APKFootballer* O = A == Carrier ? B : A;
				if (O->ShoulderCooldown <= 0.f && !O->IsInAction() && !C->IsInAction() && !O->bHuman)
				{
					ShoulderChallenge(C, O, false);
					if (Phase != EPKPhase::Playing)
					{
						return; // foul given: the restart has rearranged the players
					}
				}
			}

			if (D >= MinDist)
			{
				continue;
			}
			const float Overlap = MinDist - D;
			const float SA = Strength(A, N);
			const float SB = Strength(B, -N);
			const float ShareA = SB / (SA + SB); // A moves by the other's strength share
			const float ShareB = 1.f - ShareA;
			A->AddActorWorldOffset(-N * Overlap * ShareA, false, nullptr, ETeleportType::None);
			B->AddActorWorldOffset(N * Overlap * ShareB, false, nullptr, ETeleportType::None);
			// Bodies absorb some of the closing speed.
			UCharacterMovementComponent* MA = A->GetCharacterMovement();
			UCharacterMovementComponent* MB = B->GetCharacterMovement();
			const float Closing = FVector::DotProduct(PK::Flat(MA->Velocity - MB->Velocity), N);
			if (Closing > 0.f)
			{
				MA->Velocity -= N * Closing * ShareA * 0.8f;
				MB->Velocity += N * Closing * ShareB * 0.8f;
			}
		}
	}
}

bool APKGameMode::ShoulderChallenge(APKFootballer* Carrier, APKFootballer* Challenger, bool bBarge)
{
	// Side by side: a legal shoulder-to-shoulder duel. From behind: a push, usually a foul.
	const FVector ToChallenger = (Challenger->GetFlatLocation() - Carrier->GetFlatLocation()).GetSafeNormal2D();
	const float Along = FVector::DotProduct(ToChallenger, Carrier->GetFacing());
	const bool bBehind = Along < -0.5f;
	const bool bAlongside = FMath::Abs(Along) < 0.6f;
	const float CarrierSpeed = PK::Flat(Carrier->GetVelocity()).Size();
	if (!bBarge && (!bAlongside || CarrierSpeed < 250.f) && !bBehind)
	{
		return false;
	}
	if (!bBarge && bBehind && FVector::DotProduct(PK::Flat(Challenger->GetVelocity()), -ToChallenger) < 250.f)
	{
		return false; // trailing, not pushing
	}
	Challenger->ShoulderCooldown = bBarge ? 0.8f : 1.4f;

	if (bBehind)
	{
		const float PushFoul = (bBarge ? 0.7f : 0.3f) * FMath::Lerp(1.3f, 0.6f, Challenger->Attr.Defending);
		if (FMath::FRand() < PushFoul)
		{
			AwardFoul(Challenger, Carrier, 0.3f, TEXT("PUSH"));
			return true;
		}
	}
	const float CarrierPower = (0.6f + 0.8f * Carrier->Attr.Physical + 0.15f * Carrier->StyleLevel(PKTrait::Bruiser) + (Carrier->Stance == EPKStance::Shield ? 0.3f : 0.f)) * FMath::FRandRange(0.6f, 1.2f);
	const float ChallengerPower = (0.6f + 0.8f * Challenger->Attr.Physical + 0.15f * Challenger->StyleLevel(PKTrait::Bruiser)) * FMath::Lerp(0.85f, 1.15f, Challenger->Attr.Defending) * (bBarge ? 1.15f : 1.f) * FMath::FRandRange(0.6f, 1.2f);
	if (ChallengerPower > CarrierPower * 1.1f)
	{
		// Knocked off the ball: the carrier stumbles sideways and the ball runs loose.
		Carrier->Stun(0.45f);
		if (Carrier->bHuman)
		{
			Rumble(Carrier->Team, 0.45f, 0.18f);
		}
		Carrier->LaunchCharacter(-ToChallenger * 280.f, false, false);
		Ball->SetController(nullptr);
		Ball->LastTouchTeam = Challenger->Team;
		Challenger->StealCooldown = 0.f;
		++ShoulderWins;
		EventShot(TEXT("Shoulder"), Challenger);
		return true;
	}
	if (CarrierPower > ChallengerPower * 1.1f)
	{
		// Bounced off: the challenger loses a stride.
		Challenger->Stun(0.35f);
		Challenger->LaunchCharacter(ToChallenger * 220.f, false, false);
		Carrier->ProtectedTimer = FMath::Max(Carrier->ProtectedTimer, 0.4f);
		++ShoulderLosses;
		EventShot(TEXT("ShoulderHold"), Carrier);
	}
	return false;
}

void APKGameMode::HumanShoulder(int32 Team)
{
	APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];
	// X / RB without the ball: barge the carrier (fair from the side, a foul from behind).
	APKFootballer* Carrier = Ball->GetController();
	if (!Human || !Carrier || Carrier->Team == Human->Team || Human->ShoulderCooldown > 0.f || Human->IsInAction() || Human->IsStunned())
	{
		return;
	}
	if (FVector::Dist(Human->GetFlatLocation(), Carrier->GetFlatLocation()) > 120.f)
	{
		return;
	}
	ShoulderChallenge(Carrier, Human, true);
}

bool APKGameMode::InPenaltyArea(int32 DefendingTeam, const FVector& Point) const
{
	const float GoalX = -PK::AttackDir(DefendingTeam) * PK::HalfLength;
	return FMath::Abs(Point.X - GoalX) < PK::PenaltyDepth && FMath::Abs(Point.Y) < PK::PenaltyHalfWidth && FMath::Abs(Point.X) <= PK::HalfLength;
}

void APKGameMode::AwardFoul(APKFootballer* Fouler, APKFootballer* Victim, float Severity, const TCHAR* Label)
{
	if (Phase != EPKPhase::Playing || !Fouler || !Victim)
	{
		return;
	}
	const int32 Team = Victim->Team;
	const FVector Spot = Victim->GetFlatLocation();
	++Fouls[Fouler->Team];
	if (Victim->bHuman)
	{
		Rumble(Team, 0.6f, 0.3f);
	}
	if (!Victim->IsInAction())
	{
		Victim->PlayTrip(); // brought down
	}
	EventShot(TEXT("Foul"), Victim);

	// Discipline. Denying an obvious goal-scoring chance is a red card.
	bool bClearChance = false;
	if (FVector::Dist(Spot, OppGoal(Team)) < 2600.f && !Fouler->IsKeeper())
	{
		bClearChance = true;
		for (const APKFootballer* O : Footballers)
		{
			if (O->Team != Team && O != Fouler && !O->IsKeeper()
				&& FVector::Dist(O->GetFlatLocation(), OppGoal(Team)) < FVector::Dist(Spot, OppGoal(Team)) + 100.f)
			{
				bClearChance = false;
				break;
			}
		}
	}
	bool bRed = false;
	bool bYellow = false;
	if (bClearChance && Severity >= 0.4f)
	{
		bRed = true;
	}
	else if (Severity > 0.85f && FMath::FRand() < 0.15f)
	{
		bRed = true;
	}
	else if (FMath::FRand() < FMath::Clamp(Severity - 0.2f, 0.f, 1.f))
	{
		bYellow = true;
		if (++Fouler->YellowCards >= 2)
		{
			bRed = true;
		}
	}
	if (Fouler->IsKeeper() && bRed)
	{
		bRed = false; // no substitute keepers yet: keepers only get booked
		bYellow = true;
	}
	if (bYellow || bRed)
	{
		const bool bSecond = bRed && Fouler->YellowCards >= 2;
		CardText = FString::Printf(TEXT("%s  %s #%d%s"), bRed ? TEXT("RED CARD") : TEXT("YELLOW CARD"), *TeamLabel(Fouler->Team), Fouler->Index + 1, bSecond ? TEXT("  (second yellow)") : TEXT(""));
		CardColor = bRed ? FLinearColor(0.9f, 0.05f, 0.05f) : FLinearColor(1.f, 0.85f, 0.f);
		CardTimer = 3.f;
		if (bRed)
		{
			++Reds[Fouler->Team];
		}
		else
		{
			++Yellows[Fouler->Team];
		}
	}
	UE_LOG(LogPitchKings, Log, TEXT("FOUL (%s) by %s #%d on %s #%d at (%.0f,%.0f) severity %.2f %s"), Label, PK::TeamName(Fouler->Team), Fouler->Index, PK::TeamName(Team), Victim->Index,
		Spot.X, Spot.Y, Severity, bRed ? TEXT("RED") : (bYellow ? TEXT("YELLOW") : TEXT("")));

	const bool bPenalty = InPenaltyArea(Fouler->Team, Spot);
	if (bRed)
	{
		SendOff(Fouler);
	}
	// Advantage: the fouled team still has the ball (or a team-mate is first to it) in the opponents' half.
	if (!bPenalty && !bRed && WorldToFx(Team, Spot.X) > -0.1f && TeamHasBall(Team))
	{
		AdvantageTeam = Team;
		AdvantageTimer = 2.5f;
		AdvantageSpot = Spot;
		AdvantageLabel = FString::Printf(TEXT("FOUL (%s)  FREE KICK"), Label);
		++Advantages;
		ShowMessage(TEXT("ADVANTAGE"), 1.2f);
		UE_LOG(LogPitchKings, Log, TEXT("Advantage %s"), PK::TeamName(Team));
		return;
	}
	if (bPenalty)
	{
		AwardPenalty(Team);
	}
	else
	{
		AwardFreeKick(Team, Spot, FString::Printf(TEXT("FOUL (%s)  FREE KICK"), Label));
	}
}

bool APKGameMode::TeamHasBall(int32 Team) const
{
	const APKFootballer* C = Ball->GetController();
	if (C && !C->IsStunned())
	{
		return C->Team == Team;
	}
	// Loose: whoever is first to it.
	const FVector B = PK::Flat(Ball->GetActorLocation());
	float Mine = 1.e9f, Theirs = 1.e9f;
	for (const APKFootballer* P : Footballers)
	{
		if (P->IsStunned())
		{
			continue;
		}
		const float D = FVector::Dist(P->GetFlatLocation(), B);
		(P->Team == Team ? Mine : Theirs) = FMath::Min(P->Team == Team ? Mine : Theirs, D);
	}
	return Mine < 400.f && Mine + 150.f < Theirs;
}

void APKGameMode::UpdateAdvantage(float Dt)
{
	if (AdvantageTeam < 0)
	{
		return;
	}
	AdvantageTimer -= Dt;
	const APKFootballer* C = Ball->GetController();
	if (C && C->Team != AdvantageTeam && AdvantageTimer > 1.f)
	{
		// Lost it straight away: no advantage after all, back for the free kick.
		const int32 Team = AdvantageTeam;
		AdvantageTeam = -1;
		++AdvantageCallbacks;
		UE_LOG(LogPitchKings, Log, TEXT("Advantage called back: free kick %s"), PK::TeamName(Team));
		AwardFreeKick(Team, AdvantageSpot, AdvantageLabel);
		return;
	}
	if (AdvantageTimer <= 0.f)
	{
		AdvantageTeam = -1; // advantage played
	}
}

void APKGameMode::AwardFreeKick(int32 Team, FVector Spot, const FString& Label)
{
	Spot.X = FMath::Clamp(Spot.X, -PK::HalfLength + 120.f, PK::HalfLength - 120.f);
	Spot.Y = FMath::Clamp(Spot.Y, -PK::HalfWidth + 120.f, PK::HalfWidth - 120.f);
	Spot.Z = 0.f;
	// Never inside the defenders' box for an indirect offence: push it to the edge.
	if (InPenaltyArea(1 - Team, Spot))
	{
		Spot.X = -PK::AttackDir(1 - Team) * (PK::HalfLength - PK::PenaltyDepth - 30.f);
	}
	AwardRestart(Team, Spot, false, false, Label);
	SetPiece = EPKSetPiece::FreeKick;

	// Taker faces the goal; opponents 9.15 m away; a wall when it is within shooting range.
	APKFootballer* Taker = Ball->GetSetPieceTaker();
	const FVector ToGoal = (OppGoal(Team) - Spot).GetSafeNormal2D();
	if (Taker)
	{
		Taker->Teleport2D(Spot - ToGoal * 70.f, ToGoal.Rotation().Yaw);
	}
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team || P->IsKeeper())
		{
			continue;
		}
		const FVector Away = P->GetFlatLocation() - Spot;
		if (Away.Size() < 915.f)
		{
			const FVector Dir = Away.IsNearlyZero() ? -ToGoal : Away.GetSafeNormal2D();
			P->Teleport2D(Spot + Dir * 930.f, (-Dir).Rotation().Yaw);
		}
	}
	const float DGoal = FVector::Dist(Spot, OppGoal(Team));
	if (DGoal < 3200.f)
	{
		TArray<APKFootballer*> Defenders;
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team && !P->IsKeeper())
			{
				Defenders.Add(P);
			}
		}
		Defenders.Sort([&](const APKFootballer& L, const APKFootballer& R)
		{
			return FVector::DistSquared(L.GetFlatLocation(), Spot) < FVector::DistSquared(R.GetFlatLocation(), Spot);
		});
		const int32 WallSize = FMath::Min(Defenders.Num(), DGoal < 2200.f ? 4 : 3);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, ToGoal);
		const FVector WallCentre = Spot + ToGoal * 915.f;
		for (int32 k = 0; k < WallSize; ++k)
		{
			const FVector At = WallCentre + Side * ((k - (WallSize - 1) * 0.5f) * 55.f);
			Defenders[k]->Teleport2D(At, (-ToGoal).Rotation().Yaw);
			Defenders[k]->bHoldPosition = true;
		}
	}
	UE_LOG(LogPitchKings, Log, TEXT("Free kick %s at (%.0f,%.0f), %.0f m from goal"), PK::TeamName(Team), Spot.X, Spot.Y, DGoal / 100.f);
}

void APKGameMode::AwardPenalty(int32 Team)
{
	const float Dir = PK::AttackDir(Team);
	const FVector Spot(Dir * (PK::HalfLength - PK::PenaltySpot), 0.f, 0.f);
	APKFootballer* Taker = nullptr;
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Team && !P->IsKeeper() && !P->IsStunned() && (!Taker || P->Attr.Shooting > Taker->Attr.Shooting))
		{
			Taker = P;
		}
	}
	if (!Taker)
	{
		return;
	}
	// Everyone else outside the box and behind the ball; the keeper on the line.
	int32 Slot = 0;
	for (APKFootballer* P : Footballers)
	{
		P->StopAction();
		P->bHoldPosition = true;
		if (P == Taker)
		{
			continue;
		}
		if (P->IsKeeper() && P->Team != Team)
		{
			P->Teleport2D(FVector(Dir * (PK::HalfLength - 20.f), 0.f, 0.f), (-FVector(Dir, 0.f, 0.f)).Rotation().Yaw);
			continue;
		}
		if (P->IsKeeper())
		{
			continue;
		}
		const float X = Dir * (PK::HalfLength - PK::PenaltyDepth - 150.f - (Slot % 3) * 140.f);
		const float Y = ((Slot / 3) - 3) * 420.f + (Slot % 2) * 120.f;
		P->Teleport2D(FVector(X, Y, 0.f), FVector(Dir, 0.f, 0.f).Rotation().Yaw);
		++Slot;
	}
	Taker->Teleport2D(Spot - FVector(Dir * 110.f, 0.f, 0.f), FVector(Dir, 0.f, 0.f).Rotation().Yaw);
	Ball->PlaceAt(Spot, Taker);
	Taker->ProtectedTimer = 3.f;
	Taker->bHoldPosition = false;
	if (bHumanTeam[Team])
	{
		SetHuman(Taker);
	}
	++Penalties[Team];
	SetPiece = EPKSetPiece::Penalty;
	Phase = EPKPhase::Stoppage;
	PhaseTimer = 1.5f;
	RestartTimer = 0.f;
	ShowMessage(FString::Printf(TEXT("PENALTY - %s"), *TeamLabel(Team)), 2.5f);
	EventShot(TEXT("Penalty"), Taker);
	UE_LOG(LogPitchKings, Log, TEXT("PENALTY to %s, taker #%d"), PK::TeamName(Team), Taker->Index);
}

void APKGameMode::SendOff(APKFootballer* P)
{
	// Off the pitch for the rest of the match: the team plays a man down.
	if (Ball->GetController() == P || Ball->GetHolder() == P)
	{
		Ball->ClearControl();
	}
	P->bSentOff = true;
	P->StopAction();
	P->SetBallArrow(false);
	if (Humans[P->Team] == P)
	{
		P->SetHumanControlled(false);
		Humans[P->Team] = nullptr;
	}
	Footballers.Remove(P);
	SentOff.Add(P);
	P->Teleport2D(FVector(0.f, (P->Team == 0 ? -1.f : 1.f) * (PK::HalfWidth + 900.f), 0.f), 0.f);
	P->SetActorHiddenInGame(true);
	P->SetActorEnableCollision(false);
	P->SetActorTickEnabled(false);
	if (bHumanTeam[P->Team] && !Humans[P->Team])
	{
		SetHuman(ClosestOutfield(P->Team, Ball->GetActorLocation(), nullptr));
	}
	UE_LOG(LogPitchKings, Log, TEXT("SENT OFF: %s #%d (%d left)"), PK::TeamName(P->Team), P->Index, Footballers.FilterByPredicate([P](const APKFootballer* F) { return F->Team == P->Team; }).Num());
}

void APKGameMode::RestoreSentOff()
{
	for (APKFootballer* P : SentOff)
	{
		P->bSentOff = false;
		P->SetActorHiddenInGame(false);
		P->SetActorEnableCollision(true);
		P->SetActorTickEnabled(true);
		Footballers.Add(P);
	}
	SentOff.Empty();
	for (APKFootballer* P : Footballers)
	{
		P->YellowCards = 0;
		P->bHoldPosition = false;
	}
}

bool APKGameMode::IsOffsidePosition(const APKFootballer* P, float BallX) const
{
	// In the opponents' half, ahead of the ball and of the second-last opponent.
	const float Fx = WorldToFx(P->Team, P->GetActorLocation().X);
	if (Fx <= 0.f || Fx <= WorldToFx(P->Team, BallX))
	{
		return false;
	}
	float Last = -2.f, SecondLast = -2.f;
	for (const APKFootballer* O : Footballers)
	{
		if (O->Team == P->Team)
		{
			continue;
		}
		const float OFx = WorldToFx(P->Team, O->GetActorLocation().X);
		if (OFx > Last)
		{
			SecondLast = Last;
			Last = OFx;
		}
		else if (OFx > SecondLast)
		{
			SecondLast = OFx;
		}
	}
	return Fx > SecondLast + 30.f / PK::HalfLength;
}

void APKGameMode::MarkOffside(const APKFootballer* Kicker)
{
	const float BallX = Ball->GetActorLocation().X;
	for (APKFootballer* P : Footballers)
	{
		if (P->Team == Kicker->Team && P != Kicker)
		{
			P->OffsideKickId = IsOffsidePosition(P, BallX) ? Ball->KickId : -1;
		}
	}
}

bool APKGameMode::CheckOffside(APKFootballer* Receiver)
{
	if (Phase != EPKPhase::Playing || Receiver->OffsideKickId != Ball->KickId || !Ball->LastKicker || Ball->LastKicker->Team != Receiver->Team)
	{
		return false;
	}
	Receiver->OffsideKickId = -1;
	++Offsides[Receiver->Team];
	UE_LOG(LogPitchKings, Log, TEXT("OFFSIDE %s #%d at (%.0f,%.0f)"), PK::TeamName(Receiver->Team), Receiver->Index, Receiver->GetActorLocation().X, Receiver->GetActorLocation().Y);
	EventShot(TEXT("Offside"), Receiver);
	AwardFreeKick(1 - Receiver->Team, Receiver->GetFlatLocation(), FString::Printf(TEXT("OFFSIDE  %s #%d"), *TeamLabel(Receiver->Team), Receiver->Index + 1));
	return true;
}

void APKGameMode::QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate)
{
	bool bPlaying = false;
	if (Action == EPKAction::Header || Action == EPKAction::ThighControl)
	{
		// Meet the ball when it gets there: the head / knee reaches the contact point on the ball's own time.
		const float Arrival = BallArrivalTime(P->GetActionContactPoint(Action, P->GetActorRotation().Yaw));
		bPlaying = P->PlayActionTimed(Action, Arrival, 2.4f);
	}
	else
	{
		bPlaying = P->PlayAction(Action, EPKAction::None, Rate);
	}
	P->TouchQueuedSpeed = Ball->Velocity.Size();
	if (!bPlaying)
	{
		// No clip: touch now.
		bKick ? Ball->Kick(P, Velocity) : Ball->Touch(P, Velocity);
		return;
	}
	if (bContactShots && bAutoTest && ContactShotCount < 30 && EventShotCooldown <= 0.f && RestoreDilationFrames == 0)
	{
		ContactWatch = P;
		EventFocusActor = P;
		EventShotCooldown = 1.5f;
	}
	P->TouchAction = Action;
	P->TouchVelocity = Velocity;
	P->bTouchIsKick = bKick;
	++DeferredTouches;
}

void APKGameMode::UpdatePendingTouches(float Dt)
{
	// Headers, thigh control and flicks: the ball moves only when the head / knee / foot reaches it.
	for (APKFootballer* P : Footballers)
	{
		if (P->TouchAction == EPKAction::None)
		{
			continue;
		}
		if (P->GetCurrentAction() != P->TouchAction || P->IsStunned() || !Ball->IsFree())
		{
			P->TouchAction = EPKAction::None;
			continue;
		}
		const FVector Contact = P->GetActionContactPoint(P->TouchAction, P->GetActorRotation().Yaw)
			+ (P->TouchAction == EPKAction::Pass ? P->GetFacing() * 6.f : (P->TouchAction == EPKAction::Header ? P->GetFacing() * 10.f : P->GetFacing() * 12.f));
		const FVector Target(Contact.X, Contact.Y, FMath::Max(PK::BallRadius, Contact.Z));
		const FVector BallLoc = Ball->GetActorLocation();
		const float Remaining = P->TimeToContact();
		if (Remaining > 0.f)
		{
			if (FVector::Dist(BallLoc, Target) < 220.f)
			{
				// The ball keeps flying; it is only nudged onto the head / knee so they meet at the contact frame.
				const FVector Need = (Target - BallLoc) / FMath::Max(Remaining, Dt);
				Ball->Velocity = FMath::Lerp(Ball->Velocity, Need, FMath::Clamp(Dt * 12.f, 0.f, 1.f));
			}
			continue;
		}
		const EPKAction Action = P->TouchAction;
		P->TouchAction = EPKAction::None;
		if (FVector::Dist(BallLoc, Target) > 90.f)
		{
			++MissedTouches;
			continue;
		}
		if (P->TouchQueuedSpeed > 300.f && (Action == EPKAction::Header || Action == EPKAction::ThighControl))
		{
			PendingSlowSum += FMath::Min(Ball->Velocity.Size() / P->TouchQueuedSpeed, 2.f); // speed kept up to contact
			++PendingSlowCount;
		}
		Ball->SetActorLocation(Target);
		ContactSnap(P, *UEnum::GetValueAsString(Action).RightChop(11));
		if (P->bTouchIsKick)
		{
			Ball->Kick(P, P->TouchVelocity);
		}
		else
		{
			Ball->Touch(P, P->TouchVelocity);
		}
		UE_LOG(LogPitchKings, Verbose, TEXT("%s #%d touch %d at contact"), PK::TeamName(P->Team), P->Index, (int32)Action);
	}
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
	const bool bSuppress = bSpectate || bReplaying || IsCriticHudHidden();
	Ball->SetMarkersSuppressed(bSuppress);
	for (APKFootballer* P : Footballers)
	{
		P->SetBallArrow(bShow && P == PredictedTaker);
		P->SetMarkersSuppressed(bSuppress);
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
			else if (P->Team == Team && !P->IsKeeper() && (!P->bHuman || bSpectate)
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
			if (FVector::Dist2D(Opp->GetActorLocation(), Goal) > PK::HalfLength * 1.05f)
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
	P->RunTimer = FMath::Max(0.f, P->RunTimer - GetWorld()->GetDeltaSeconds());
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

	// Winger wide in the final third: forwards attack the near and far post, a midfielder arrives at the edge.
	if (FMath::Abs(CarrierLoc.Y) > 1100.f && WorldToFx(P->Team, CarrierLoc.X) > 0.35f && P != Carrier
		&& (P->Role == EPKRole::Forward || (P->Role == EPKRole::Midfielder && FMath::Abs(P->GetActorLocation().Y) < 1200.f)))
	{
		const float SideSign = FMath::Sign(CarrierLoc.Y);
		const float GoalX = Dir * PK::HalfLength;
		FVector Spot;
		if (P->Role == EPKRole::Midfielder)
		{
			Spot = FVector(GoalX - Dir * 1800.f, -SideSign * 200.f, 0.f);
		}
		else if (P->Index % 2 == 0)
		{
			Spot = FVector(GoalX - Dir * 650.f, SideSign * 250.f, 0.f); // near post
		}
		else
		{
			Spot = FVector(GoalX - Dir * 900.f, -SideSign * 350.f, 0.f); // far post
		}
		P->AITarget = Spot + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 0.f);
		P->RunTimer = 1.2f;
		return P->AITarget;
	}

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
			const float SpotScore = FMath::Min(Lane, 600.f) / 600.f + FMath::Min(Space, 900.f) / 900.f - FVector::Dist2D(Spot, Base) / 3000.f;
			if (SpotScore > BestScore)
			{
				BestScore = SpotScore;
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

FVector APKGameMode::OppGoal(int32 Team) const
{
	return FVector(PK::AttackDir(Team) * PK::HalfLength, 0.f, 0.f);
}

FVector APKGameMode::OwnGoal(int32 Team) const
{
	return FVector(-PK::AttackDir(Team) * PK::HalfLength, 0.f, 0.f);
}

FVector APKGameMode::TeamToWorld(int32 Team, float Fx, float Fy) const
{
	const float Dir = PK::AttackDir(Team);
	return FVector(Dir * Fx * PK::HalfLength, Dir * Fy * PK::HalfWidth, 0.f);
}

float APKGameMode::WorldToFx(int32 Team, float X) const
{
	return PK::AttackDir(Team) * X / PK::HalfLength;
}

bool APKGameMode::InOwnBox(const APKFootballer* P, const FVector& Point) const
{
	const FVector Goal = OwnGoal(P->Team);
	const float Depth = (Point.X - Goal.X) * PK::AttackDir(P->Team);
	return Depth > -50.f && Depth < PK::PenaltyDepth && FMath::Abs(Point.Y) < PK::PenaltyHalfWidth;
}

void APKGameMode::UpdateRulesTest(float Dt)
{
	auto Find = [this](int32 Team, int32 Index) -> APKFootballer*
	{
		for (APKFootballer* F : Footballers)
		{
			if (F->Team == Team && F->Index == Index)
			{
				return F;
			}
		}
		return nullptr;
	};

	// -PKSandwich: a carrier wedged between two touching opponents, 12 times. Does anyone get stuck?
	if (bSandwichTest && Phase == EPKPhase::Playing && SandwichTrial < 12)
	{
		SandwichClock += Dt;
		APKFootballer* C = SandwichPlayers[0].Get();
		if (SandwichClock > 3.f && C)
		{
			float MaxMove = 0.f;
			for (int32 k = 0; k < 3; ++k)
			{
				if (SandwichPlayers[k].IsValid())
				{
					MaxMove = FMath::Max(MaxMove, FVector::Dist(SandwichPlayers[k]->GetFlatLocation(), SandwichStart[k]));
				}
			}
			const float CarrierMove = FVector::Dist(C->GetFlatLocation(), SandwichStart[0]);
			const bool bStuck = MaxMove < 150.f && FVector::Dist(PK::Flat(Ball->GetActorLocation()), SandwichStart[0]) < 200.f;
			SandwichStuck += bStuck ? 1 : 0;
			UE_LOG(LogPitchKings, Log, TEXT("[Sandwich %d] carrier moved %.0f cm, trio max %.0f cm, ball owner %s -> %s (stuck total %d)"), SandwichTrial, CarrierMove, MaxMove,
				Ball->GetOwnerPlayer() ? PK::TeamName(Ball->GetOwnerPlayer()->Team) : TEXT("none"), bStuck ? TEXT("STUCK") : TEXT("free"), SandwichStuck);
			SandwichPlayers[0] = nullptr;
			++SandwichTrial;
			SandwichClock = 0.f;
		}
		else if (!C && SandwichClock > 1.f)
		{
			APKFootballer* Carrier = Find(0, 9);
			APKFootballer* D1 = Find(1, 4);
			APKFootballer* D2 = Find(1, 5);
			if (Carrier && D1 && D2)
			{
				const FVector At(800.f, (SandwichTrial % 3 - 1) * 900.f, 0.f);
				const float Yaw = (SandwichTrial % 2) ? 0.f : 180.f;
				Carrier->Teleport2D(At, Yaw);
				D1->Teleport2D(At + FVector(0.f, 56.f, 0.f), -90.f);
				D2->Teleport2D(At - FVector(0.f, 56.f, 0.f), 90.f);
				Ball->ClearControl();
				Ball->SetActorLocation(FVector(PK::Flat(Carrier->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
				Ball->Velocity = FVector::ZeroVector;
				Ball->SetController(Carrier);
				SandwichPlayers[0] = Carrier;
				SandwichPlayers[1] = D1;
				SandwichPlayers[2] = D2;
				SandwichStart[0] = Carrier->GetFlatLocation();
				SandwichStart[1] = D1->GetFlatLocation();
				SandwichStart[2] = D2->GetFlatLocation();
				SandwichClock = 0.f;
			}
		}
	}

	// -PKFoulTest: stage a foul in the box (penalty), a foul near the box (free kick + wall) and a
	// professional foul (red card), so the restarts and cards can be checked in screenshots.
	if (bFoulTest && Phase == EPKPhase::Playing)
	{
		const float At[] = { 8.f, 40.f, 75.f, 110.f };
		if (RulesTestStep < UE_ARRAY_COUNT(At) && AutoTestClock > At[RulesTestStep])
		{
			APKFootballer* Victim = Find(0, 9);
			APKFootballer* Fouler = Find(1, RulesTestStep == 2 ? 2 : 4);
			if (Victim && Fouler)
			{
				FVector Spot = FVector(3200.f, 350.f, 0.f);
				float Severity = 0.7f;
				if (RulesTestStep == 0 || RulesTestStep == 3)
				{
					Spot = FVector(PK::HalfLength - 900.f, -300.f, 0.f); // in the box
					Severity = 0.45f;
				}
				else if (RulesTestStep == 2)
				{
					Spot = FVector(PK::HalfLength - 2000.f, 0.f, 0.f); // through on goal
					Severity = 0.9f;
					// Clear the way: the other defenders are behind the play.
					for (APKFootballer* F : Footballers)
					{
						if (F->Team == 1 && !F->IsKeeper() && F != Fouler)
						{
							F->Teleport2D(FVector(F->GetActorLocation().X - 3000.f, F->GetActorLocation().Y, 0.f), F->GetActorRotation().Yaw);
						}
					}
				}
				Victim->Teleport2D(Spot, 0.f);
				Fouler->Teleport2D(Spot - FVector(70.f, 0.f, 0.f), 0.f);
				Ball->ClearControl();
				Ball->SetActorLocation(FVector(PK::Flat(Victim->GetFootPoint())) + FVector(0.f, 0.f, PK::BallRadius));
				Ball->Velocity = FVector::ZeroVector;
				Ball->SetController(Victim);
				UE_LOG(LogPitchKings, Log, TEXT("[FoulTest] step %d"), RulesTestStep);
				AwardFoul(Fouler, Victim, Severity, TEXT("TEST"));
			}
			++RulesTestStep;
		}
	}
}

void APKGameMode::UpdateAutoTest(float Dt)
{
	// -PKAutoTest: both teams AI-controlled, stats logged, screenshots taken, then quit.
	AutoTestClock += Dt;
	UpdateRulesTest(Dt);
	UpdateDribbleTest(Dt);
	UpdateFeatureTest(Dt);
	const float Marks[] = { 3.f, 8.f, 15.f, 30.f, 45.f, 60.f, 75.f, 90.f, 105.f, 120.f, 135.f, 150.f, 162.f, 175.f, 190.f };
	if (AutoTestStep >= UE_ARRAY_COUNT(Marks) || AutoTestClock < Marks[AutoTestStep])
	{
		return;
	}

	UE_LOG(LogPitchKings, Log, TEXT("[AutoTest %.0fs] phase=%d BLUE %d - %d RED | shots %d-%d | passes %d-%d | steals %d-%d | possession %.0fs-%.0fs | air %.0fs max %.1fm chips %d crosses %d flicks %d | kicks %d whiffs %d contact err avg %.0f max %.0f bone %.0f cm facing avg %.0f max %.0f deg | touches %d missed %d buffered-dropped %d | off-ball facing avg %.0f deg, back to ball %.0f%% | fouls %d-%d yellow %d-%d red %d-%d pens %d-%d offside %d-%d fk-shots %d | shoulder won %d lost %d | stuck %.1fs stalemates %d | dribble touches %d (steps %d, gap %.0f, fallback %d, overrun %d, far %d, slow %.1fs, outside %d inside %d) zone %.0f lost %d | kick fit avg %.2f misfit %d/%d | timed saves %d | ball kept %.2f of its speed waiting for a header"),
		AutoTestClock, (int32)Phase, Score[0], Score[1], Shots[0], Shots[1], Passes[0], Passes[1], Steals[0], Steals[1], PossessionTime[0], PossessionTime[1], AirTime, MaxBallZ / 100.f, Chips, Crosses, Flicks,
		KickCount, Whiffs, KickCount ? ContactErrSum / KickCount : 0.f, ContactErrMax, KickCount ? BoneErrSum / KickCount : 0.f, KickCount ? FacingErrSum / KickCount : 0.f, FacingErrMax,
		DeferredTouches, MissedTouches, BufferedDropped, WatchSamples ? WatchErrSum / WatchSamples : 0.0, WatchSamples ? 100.0 * WatchAway / WatchSamples : 0.0,
		Fouls[0], Fouls[1], Yellows[0], Yellows[1], Reds[0], Reds[1], Penalties[0], Penalties[1], Offsides[0], Offsides[1], FreeKickShots, ShoulderWins, ShoulderLosses, StuckSeconds, StalemateBreaks,
		DribbleTouches, StepTouches, StepTouches ? TouchGapSum / StepTouches : 0.0, FallbackTouches, OverrunTouches, SwingFar, SlowDribbleTime, OutsideTouches, InsideCuts, DribbleSepSamples ? DribbleSepSum / DribbleSepSamples : 0.0, DribbleLosses,
		KickFitCount ? KickFitSum / KickFitCount : 0.0, KickFitBad, KickFitCount, TimedSaves, PendingSlowCount ? PendingSlowSum / PendingSlowCount : 1.0);

	if (APKFootballer* O = Ball->GetOwnerPlayer())
	{
		float NearD = 0.f;
		const APKFootballer* Near = NearestOpponent(O, NearD);
		UE_LOG(LogPitchKings, Log, TEXT("   owner %s #%d at (%.0f,%.0f) ball %.0f cm from foot z %.0f | vel %.0f input %.2f stance %d action %d kicking %d buffer %.2f touchact %d decision %.2f stun %d | nearest opp #%d %.0f cm"),
			PK::TeamName(O->Team), O->Index, O->GetActorLocation().X, O->GetActorLocation().Y, FVector::Dist(PK::Flat(Ball->GetActorLocation()), O->GetFootPoint()), Ball->GetActorLocation().Z,
			PK::Flat(O->GetVelocity()).Size(), O->MoveInput.Size(), (int32)O->Stance, (int32)O->GetCurrentAction(), O->IsKicking() ? 1 : 0, O->BufferTimer, (int32)O->TouchAction, O->DecisionTimer, O->IsStunned() ? 1 : 0,
			Near ? Near->Index : -1, NearD);
	}
	if (AutoTestStep == 1 || AutoTestStep == 2 || AutoTestStep == 4)
	{
		const FString Shot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKAutoTest_%02d.png"), AutoTestStep));
		FScreenshotRequest::RequestScreenshot(Shot, true, false);
	}
	if (AutoTestStep == UE_ARRAY_COUNT(Marks) - 1 && !bFeatureTest && !bDribbleTest)
	{
		FGenericPlatformMisc::RequestExit(false); // the staged tests end themselves
	}
	++AutoTestStep;
}

// ---------------------------------------------------------------------------------------------
// Animation test (-PKAnimTest): every action clip side-on in groups of six, screenshotted.
// ---------------------------------------------------------------------------------------------

namespace
{
	constexpr int32 AnimPerGroup = 6;
	constexpr float AnimSpacing = 260.f;
	const float AnimShotTimes[] = { 0.2f, 0.45f, 0.8f, 1.3f };
}

void APKGameMode::SetupAnimTestGroup(int32 Group)
{
	AnimGroup = Group;
	AnimShot = 0;
	AnimClock = 0.f;
	AnimAssignments.Reset();
	Ball->PlaceAt(FVector(0.f, -3000.f, 0.f), nullptr);
	Phase = EPKPhase::FullTime;
	MessageTimer = 0.f;

	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		APKFootballer* P = Footballers[i];
		const int32 ActionIndex = Group * AnimPerGroup + i + 1;
		if (i < AnimPerGroup && ActionIndex < (int32)EPKAction::Count)
		{
			P->Teleport2D(FVector(i * AnimSpacing, 0.f, 0.f), 0.f);
			const EPKAction Action = (EPKAction)ActionIndex;
			AnimAssignments.Add(P, Action);
			P->PlayAction(Action);
		}
		else
		{
			P->Teleport2D(FVector(-4000.f - i * 100.f, 2500.f, 0.f), 0.f); // park off camera
		}
	}
}

void APKGameMode::UpdateAnimTest(float Dt)
{
	AnimClock += Dt;
	for (auto& Pair : AnimAssignments)
	{
		Pair.Key->MoveInput = FVector::ZeroVector;
		Pair.Key->LookTarget = Pair.Key->GetActorLocation() + FVector(0.f, -500.f, 0.f);
	}

	if (AnimShot < UE_ARRAY_COUNT(AnimShotTimes) && AnimClock >= AnimShotTimes[AnimShot] && AnimClock < AnimShotTimes[AnimShot] + 0.5f)
	{
		const FString Shot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKAnim_G%d_%d.png"), AnimGroup, AnimShot));
		FScreenshotRequest::RequestScreenshot(Shot, true, false);
		UE_LOG(LogPitchKings, Log, TEXT("[AnimTest] group %d shot %d at %.2fs"), AnimGroup, AnimShot, AnimClock);
		++AnimShot;
		// Freeze the clips for the capture so the frame matches the time label.
		GetWorld()->GetWorldSettings()->SetTimeDilation(0.0001f);
		AnimFreeze = 0.6f;
	}

	if (AnimFreeze > 0.f)
	{
		AnimFreeze -= FApp::GetDeltaTime();
		if (AnimFreeze <= 0.f)
		{
			GetWorld()->GetWorldSettings()->SetTimeDilation(1.f);
		}
		return;
	}

	if (AnimShot >= UE_ARRAY_COUNT(AnimShotTimes) && AnimClock > 2.5f)
	{
		const int32 Next = AnimGroup + 1;
		if (Next * AnimPerGroup + 1 < (int32)EPKAction::Count)
		{
			SetupAnimTestGroup(Next);
		}
		else
		{
			FGenericPlatformMisc::RequestExit(false);
		}
	}
}

bool APKGameMode::GetAnimTestCamera(FVector& OutLocation, FVector& OutLookAt) const
{
	if (bKitShots && !KitCamLoc.IsZero())
	{
		OutLocation = KitCamLoc;
		OutLookAt = KitCamLook;
		return true;
	}
	if (!bAnimTest)
	{
		return false;
	}
	const float CenterX = (AnimPerGroup - 1) * AnimSpacing * 0.5f;
	OutLocation = FVector(CenterX, -1250.f, 260.f);
	OutLookAt = FVector(CenterX, 0.f, 95.f);
	return true;
}

FString APKGameMode::GetAnimTestLabel(const APKFootballer* P) const
{
	if (bClipSheet)
	{
		return P->DebugClip ? FString::Printf(TEXT("%s %.2f"), *P->DebugClip->GetName().RightChop(3), P->DebugTime) : FString();
	}
	for (const auto& Pair : AnimAssignments)
	{
		if (Pair.Key == P)
		{
			const FString Name = StaticEnum<EPKAction>()->GetNameStringByValue((int64)Pair.Value);
			return FString::Printf(TEXT("%s  t=%.2f"), *Name, P->GetActionTime());
		}
	}
	return FString();
}
namespace
{
	const float SheetFractions[] = { 0.25f, 0.5f, 0.8f };
}

void APKGameMode::SetupClipSheetGroup(int32 Group)
{
	AnimGroup = Group;
	AnimShot = 0;
	AnimClock = Group == 0 ? -1.5f : 0.f; // first group: let streaming and anim init settle
	Ball->PlaceAt(FVector(0.f, -3000.f, 0.f), nullptr);
	Phase = EPKPhase::FullTime;
	MessageTimer = 0.f;
	for (int32 i = 0; i < Footballers.Num(); ++i)
	{
		APKFootballer* P = Footballers[i];
		const int32 ClipIndex = Group * AnimPerGroup + i;
		P->StopAction();
		if (i < AnimPerGroup && SheetClips.IsValidIndex(ClipIndex))
		{
			P->Teleport2D(FVector(i * AnimSpacing, 0.f, 0.f), 0.f);
			P->DebugClip = SheetClips[ClipIndex];
		}
		else
		{
			P->DebugClip = nullptr;
			P->Teleport2D(FVector(-4000.f - i * 100.f, 2500.f, 0.f), 0.f);
		}
	}
}

void APKGameMode::UpdateClipSheet(float Dt)
{
	AnimClock += Dt;
	const int32 Shot = FMath::Max(0, FMath::FloorToInt(AnimClock / 0.8f));
	const int32 ShotCount = SheetTimes.Num() ? 1 : UE_ARRAY_COUNT(SheetFractions);
	if (SheetTimes.Num())
	{
		for (int32 i = 0; i < AnimPerGroup && i < Footballers.Num(); ++i)
		{
			const int32 Slot = AnimGroup * AnimPerGroup + i;
			if (SheetTimes.IsValidIndex(Slot) && Footballers[i]->DebugClip)
			{
				Footballers[i]->DebugTime = SheetTimes[Slot];
			}
		}
	}
	if (Shot >= ShotCount)
	{
		const int32 Next = AnimGroup + 1;
		if (Next * AnimPerGroup < SheetClips.Num())
		{
			SetupClipSheetGroup(Next);
		}
		else
		{
			FGenericPlatformMisc::RequestExit(false);
		}
		return;
	}
	for (APKFootballer* P : Footballers)
	{
		P->MoveInput = FVector::ZeroVector;
		P->LookTarget = P->GetActorLocation() + FVector(0.f, -500.f, 0.f);
		if (P->DebugClip && SheetTimes.Num() == 0)
		{
			P->DebugTime = P->DebugClip->GetPlayLength() * SheetFractions[Shot];
		}
	}
	if (AnimClock > 0.f && Shot == AnimShot && AnimClock - Shot * 0.8f > 0.4f)
	{
		const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKSheet_%02d_%d.png"), AnimGroup, Shot));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		++AnimShot;
	}
}
bool APKGameMode::GetEventFocus(FVector& OutFocus) const
{
	if (const AActor* Focus = EventFocusActor.Get())
	{
		OutFocus = Focus->GetActorLocation();
		return EventShotCooldown > 0.6f;
	}
	return false;
}

void APKGameMode::EventShot(const TCHAR* Label, const AActor* Focus)
{
	// Close-camera self-test: capture the moment of each kind of action (a few of each).
	if (!bAutoTest || !bCloseCamera || bContactShots || EventShotCooldown > 0.f || EventShots >= 60)
	{
		return;
	}
	if (bScenario)
	{
		// Burst: three frames through the action.
		EventFocusActor = Focus;
		EventShotCooldown = 0.35f;
		++EventShots;
		for (int32 k = 0; k < 3; ++k)
		{
			TimedShots.Add({ FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKScen_%d_%s_r%d_%d.png"), ScenarioIndex, Label, ScenarioRound, k)), 0.1f + 0.3f * k });
		}
		UE_LOG(LogPitchKings, Log, TEXT("[Scenario] %d %s"), ScenarioIndex, Label);
		return;
	}
	int32& Count = EventShotCounts.FindOrAdd(Label);
	if (Count >= 3)
	{
		return;
	}
	++Count;
	++EventShots;
	EventShotCooldown = 1.5f;
	EventFocusActor = Focus;
	// Let the camera snap to the player first, then capture a few frames later (mid-action).
	PendingShot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKEvent_%s_%d.png"), Label, Count));
	PendingShotFrames = 6;
	UE_LOG(LogPitchKings, Log, TEXT("[Event] %s %d"), Label, Count);
}
// ---------------------------------------------------------------------------------------------
// Scenario test (-PKScenario): stage each rare action and capture it.
// ---------------------------------------------------------------------------------------------

namespace
{
	constexpr int32 ScenarioCount = 9;
	constexpr int32 ScenarioRounds = 2;
}

void APKGameMode::SetupScenario(int32 Index)
{
	ScenarioIndex = Index;
	ScenarioClock = 0.f;
	Phase = EPKPhase::Playing;
	RestartTimer = 10.f;
	MessageTimer = 0.f;

	// Everyone parked and frozen except the players a scenario uses.
	for (APKFootballer* P : Footballers)
	{
		P->StopAction();
		P->Teleport2D(TeamToWorld(P->Team, P->Home.X * 0.3f + 0.4f, P->Home.Y), P->Team == 0 ? 0.f : 180.f);
		P->StunTimer = 0.f;
	}
	APKFootballer* BlueKeeper = Keeper(0);
	APKFootballer* RedStriker = Footballers[PK::PlayersPerTeam + 10];
	APKFootballer* RedDefender = Footballers[PK::PlayersPerTeam + 3];
	APKFootballer* BlueForward = Footballers[9];
	const FVector BlueGoal = OwnGoal(0);

	BlueKeeper->Teleport2D(BlueGoal + FVector(120.f, 0.f, 0.f), 0.f);
	switch (Index)
	{
	case 0: // wide shot -> keeper dive
	case 1: // shot at the keeper -> catch
	case 2: // low shot -> block / scoop
	{
		RedStriker->Teleport2D(BlueGoal + FVector(1600.f, Index == 0 ? 200.f : 0.f, 0.f), 180.f);
		Ball->PlaceAt(RedStriker->GetFootPoint(), nullptr);
		Ball->ClearControl(); // open play, not a set piece
		GainControl(RedStriker);
		RedStriker->DecisionTimer = 5.f; // the scenario decides the kick
		break;
	}
	case 3: // slide tackle from the side -> trip
	{
		BlueForward->Teleport2D(FVector(0.f, 0.f, 0.f), 0.f);
		Ball->PlaceAt(BlueForward->GetFootPoint(), nullptr);
		Ball->ClearControl();
		GainControl(BlueForward);
		RedDefender->Teleport2D(FVector(-250.f, 260.f, 0.f), -45.f);
		break;
	}
	case 4: // header
	{
		RedStriker->Teleport2D(BlueGoal + FVector(1100.f, 0.f, 0.f), 180.f);
		Ball->PlaceAt(FVector::ZeroVector, nullptr);
		Ball->SetActorLocation(RedStriker->GetActorLocation() + FVector(-30.f, 0.f, 260.f));
		Ball->Kick(nullptr, FVector(0.f, 0.f, -150.f));
		Ball->LastTouchTeam = 0;
		break;
	}
	case 5: // throw-in
		AwardRestart(0, FVector(-800.f, -PK::HalfWidth - 20.f, 0.f), false, true, TEXT("THROW-IN"));
		Phase = EPKPhase::Playing;
		RestartTimer = 10.f;
		break;
	case 6: // keeper with the ball in hand distributes
		Ball->SetHeld(BlueKeeper, 110.f);
		BlueKeeper->HoldTimer = 0.f;
		break;
	case 8: // striker dribbles across the box -> keeper shuffles sideways
		RedStriker->Teleport2D(BlueGoal + FVector(1500.f, -1100.f, 0.f), 90.f);
		Ball->PlaceAt(RedStriker->GetFootPoint(), nullptr);
		Ball->ClearControl();
		GainControl(RedStriker);
		RedStriker->DecisionTimer = 9.f;
		RedStriker->AIDribbleDir = FVector(0.f, 1.f, 0.f);
		break;
	case 7: // bouncing ball at thigh height -> thigh control
		BlueForward->Teleport2D(FVector(0.f, 0.f, 0.f), 180.f);
		Ball->PlaceAt(FVector(-250.f, 0.f, 0.f), nullptr);
		Ball->SetActorLocation(FVector(-260.f, 0.f, 140.f));
		Ball->Kick(nullptr, FVector(420.f, 0.f, -60.f));
		Ball->LastTouchTeam = 0;
		break;
	}

	// Freeze bystanders.
	for (APKFootballer* P : Footballers)
	{
		const bool bActive = P == BlueKeeper || P == RedStriker || P == RedDefender || P == BlueForward || Ball->GetOwnerPlayer() == P;
		if (!bActive)
		{
			P->StunTimer = 99.f;
		}
	}
	UE_LOG(LogPitchKings, Log, TEXT("[Scenario] setup %d round %d"), Index, ScenarioRound);
}

void APKGameMode::UpdateScenario(float Dt)
{
	ScenarioClock += Dt;
	APKFootballer* RedStriker = Footballers[PK::PlayersPerTeam + 10];
	APKFootballer* RedDefender = Footballers[PK::PlayersPerTeam + 3];

	if (ScenarioClock > 0.4f && ScenarioClock - Dt <= 0.4f)
	{
		switch (ScenarioIndex)
		{
		case 0: RequestShot(RedStriker, 1.f, 0.75f); break;
		case 1: RequestShot(RedStriker, 0.f, 0.35f); break;
		case 2:
		{
			FPKKickRequest Kick;
			Kick.Type = EPKKickType::Ground;
			Kick.Target = OwnGoal(0) + FVector(0.f, 40.f, 0.f);
			Kick.Power = 1.f;
			RedStriker->StartKick(Kick, false, false);
			break;
		}
		case 8:
			TimedShots.Add({ FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKScen_8_Shuffle_r%d_0.png"), ScenarioRound)), 0.6f });
			TimedShots.Add({ FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("PKScen_8_Shuffle_r%d_1.png"), ScenarioRound)), 1.0f });
			EventFocusActor = Keeper(0);
			EventShotCooldown = 2.f;
			break;
		case 3:
			RedDefender->SetActorRotation(FRotator(0.f, (Ball->GetActorLocation() - RedDefender->GetActorLocation()).Rotation().Yaw, 0.f));
			RedDefender->PlaySlideTackle();
			EventShot(TEXT("Slide"), RedDefender);
			break;
		default:
			break;
		}
	}

	if (ScenarioClock > 4.5f)
	{
		int32 Next = ScenarioIndex + 1;
		if (Next >= ScenarioCount)
		{
			Next = 0;
			if (++ScenarioRound >= ScenarioRounds)
			{
				FGenericPlatformMisc::RequestExit(false);
				return;
			}
		}
		SetupScenario(Next);
	}
}
FString APKGameMode::TeamLabel(int32 Team) const
{
	// the club's short name on screen (PKF, CRA...), the colour name when no clubs are set up
	if (const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())
	{
		if (GI->Clubs.Num() >= 2)
		{
			const FPKClub& Club = Team == 0 ? GI->UserClubData() : GI->OpponentClubData();
			return Club.Short.IsEmpty() ? Club.Name.Left(3).ToUpper() : Club.Short.ToUpper();
		}
	}
	return PK::TeamName(Team);
}
