import os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
base = os.path.join(root, "Source", "PitchKings")


def patch(name, pairs, folder=base):
    path = os.path.join(folder, name)
    s = open(path, encoding="utf-8-sig").read().replace("\r\n", "\n")
    for a, b in pairs:
        if a not in s:
            raise SystemExit("NOT FOUND in %s: %s" % (name, a[:100]))
        s = s.replace(a, b, 1)
    open(path, "w", encoding="utf-8").write(s)


# ---- build + config
patch("PitchKings.Build.cs", [
    ('"AIModule", "AssetRegistry" });', '"AIModule", "AssetRegistry", "Slate", "SlateCore" });'),
])
patch("DefaultEngine.ini", [
    ("GlobalDefaultGameMode=/Script/PitchKings.PKGameMode\n",
     "GlobalDefaultGameMode=/Script/PitchKings.PKGameMode\nGameInstanceClass=/Script/PitchKings.PKGameInstance\n"),
], folder=os.path.join(root, "Config"))

# ---- footballer: shirt number, name, profile from the team sheet
patch("PKFootballer.h", [
    ("\tint32 Index = 0;\n",
     "\tint32 Index = 0;        // formation slot (0 = keeper)\n"
     "\tint32 ShirtNumber = 0;  // from the squad\n"
     "\tFString PlayerName;\n"),
    ("\tvoid InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome);",
     "\tvoid InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome);\n"
     "\t// Team sheet: position in the formation and the squad player's ratings.\n"
     "\tvoid ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint8 InTraits, int32 InNumber, const FString& InName);"),
])
patch("PKFootballer.cpp", [
    ("\tTeam = InTeam;\n\tRole = InRole;\n\tIndex = InIndex;\n\tHome = InHome;\n",
     "\tTeam = InTeam;\n\tRole = InRole;\n\tIndex = InIndex;\n\tHome = InHome;\n\tShirtNumber = InIndex + 1;\n"),
    ("void APKFootballer::SetBallArrow(bool bVisible)\n",
     "void APKFootballer::ApplyProfile(EPKRole InRole, const FVector2D& InHome, const FPKAttributes& InAttr, uint8 InTraits, int32 InNumber, const FString& InName)\n{\n"
     "\tRole = InRole;\n\tHome = InHome;\n\tAttr = InAttr;\n\tTraits = InTraits;\n\tShirtNumber = InNumber;\n\tPlayerName = InName;\n"
     "\tPaceScale = FMath::Lerp(0.9f, 1.06f, Attr.Pace) * (HasTrait(PKTrait::Rapid) ? 1.04f : 1.f) * (IsKeeper() ? 0.92f : 1.f);\n}\n\n"
     "void APKFootballer::SetBallArrow(bool bVisible)\n"),
])

# ---- game mode
patch("PKGameMode.h", [
    ("\tGoalScored,\n\tFullTime\n};", "\tGoalScored,\n\tFullTime,\n\tMenu // front end open: players lined up, nothing moves\n};"),
    ("\tbool IsSpectating() const { return bSpectate; }",
     "\tbool IsSpectating() const { return bSpectate; }\n"
     "\t// Front end on normal launches (off for the test / spectate command-line modes and -PKNoMenu).\n"
     "\tstatic bool UsesMenus();\n"
     "\tvoid EnterMenu();\n"
     "\tvoid StartMatch(bool bSpectateMatch);\n"
     "\tvoid ApplyTeamSetups();"),
])
patch("PKGameMode.cpp", [
    ('#include "PKTypes.h"\n', '#include "PKTypes.h"\n#include "PKGameInstance.h"\n#include "PKTeamData.h"\n'),
    ("\tSpawnTeams();\n\tRestartMatch();\n",
     "\tSpawnTeams();\n\tApplyTeamSetups();\n\tRestartMatch();\n\tif (UsesMenus())\n\t{\n\t\tEnterMenu();\n\t}\n"),
    ("\tTeamStyle[1] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n\tif (bAutoTest)\n\t{\n\t\tTeamStyle[0] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n\t}",
     "\tTeamStyle[1] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n\tif (bAutoTest)\n\t{\n\t\tTeamStyle[0] = (EPKTeamStyle)FMath::RandRange(0, (int32)EPKTeamStyle::Count - 1);\n\t}\n"
     "\telse if (const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())\n\t{\n"
     "\t\t// The team sheet's play style and the chosen opponent's own style.\n"
     "\t\tTeamStyle[0] = (EPKTeamStyle)FMath::Clamp(GI->Style, 0, (int32)EPKTeamStyle::Count - 1);\n"
     "\t\tTeamStyle[1] = (EPKTeamStyle)FMath::Clamp(GI->OpponentClubData().Style, 0, (int32)EPKTeamStyle::Count - 1);\n\t}"),
    ("\tcase EPKPhase::FullTime:\n\t\tFreezeAll();\n\t\tbreak;\n",
     "\tcase EPKPhase::FullTime:\n\tcase EPKPhase::Menu:\n\t\tFreezeAll();\n\t\tbreak;\n"),
    ("void APKGameMode::SetupKickoff(int32 KickingTeam)\n", r"""bool APKGameMode::UsesMenus()
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
	RestoreSentOff();
	SetupKickoff(0);
	Phase = EPKPhase::Menu;
	Message.Empty();
	MessageTimer = 0.f;
	CardTimer = 0.f;
}

void APKGameMode::StartMatch(bool bSpectateMatch)
{
	bSpectate = bSpectateMatch;
	ApplyTeamSetups();
	RestartMatch();
	UE_LOG(LogPitchKings, Log, TEXT("Match started (%s)"), bSpectate ? TEXT("AI vs AI") : TEXT("you are BLUE"));
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
		for (APKFootballer* P : Footballers)
		{
			if (P->Team != Team || !Lineup.IsValidIndex(P->Index) || !Club.Squad.IsValidIndex(Lineup[P->Index]))
			{
				continue;
			}
			const FPKFormationSlot& Slot = F.Slots[P->Index];
			const FPKSquadPlayer& Pl = Club.Squad[Lineup[P->Index]];
			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, PKTeams::AttributesInSlot(Pl, Slot.Pos), Pl.Traits, Pl.Number, Pl.Name);
		}
	}
}

void APKGameMode::SetupKickoff(int32 KickingTeam)
"""),
])

# ---- HUD
patch("PKHUD.cpp", [
    ("\tif (GM->IsAnimTest())\n", "\tif (GM->Phase == EPKPhase::Menu)\n\t{\n\t\treturn; // the front end is drawn by the Slate menu\n\t}\n\n\tif (GM->IsAnimTest())\n"),
    ("const FString Num = bNumbers ? FString::FromInt(P->Index + 1) : FString();",
     "const FString Num = bNumbers ? FString::FromInt(P->ShirtNumber) : FString();"),
    ("FString::Printf(TEXT(\"#%d %s  OVR %d\"), H->Index + 1,", "FString::Printf(TEXT(\"#%d %s  OVR %d\"), H->ShirtNumber,"),
    ("DrawCentered(TEXT(\"Press R / Enter / Start to play again\"), Canvas->ClipY * 0.28f + 60.f, 1.2f, FLinearColor(0.9f, 0.9f, 0.9f));",
     "DrawCentered(APKGameMode::UsesMenus() ? TEXT(\"R / Enter / Start: play again     Esc / B: main menu\") : TEXT(\"Press R / Enter / Start to play again\"), Canvas->ClipY * 0.28f + 60.f, 1.2f, FLinearColor(0.9f, 0.9f, 0.9f));"),
])

# ---- player controller: menu, pause, menu camera
patch("PKPlayerController.h", [
    ("class ACameraActor;\n", "class ACameraActor;\nclass SPKMenu;\nclass UTexture2D;\nenum class EPKMenuPage : uint8;\n"),
    ("\tvirtual void BeginPlay() override;\n",
     "\tvirtual void BeginPlay() override;\n\tvirtual void EndPlay(const EEndPlayReason::Type Reason) override;\n\n"
     "\t// Front-end actions (called by the Slate menu).\n"
     "\tvoid MenuKickOff();\n\tvoid MenuSpectate();\n\tvoid MenuQuit();\n\tvoid MenuResume();\n\tvoid MenuRestart();\n\tvoid MenuMainMenu();\n"
     "\tbool IsMenuOpen() const;\n"),
    ("\tvoid UpdateCamera(float DeltaTime);\n",
     "\tvoid UpdateCamera(float DeltaTime);\n\tvoid UpdateMenuCamera(float DeltaTime);\n\tvoid ShowMenu(EPKMenuPage Page);\n\tvoid HideMenu();\n\n"
     "\tTSharedPtr<SPKMenu> Menu;\n\tfloat MenuOrbit = 0.f;\n\n"
     "\tUPROPERTY()\n\tTObjectPtr<UTexture2D> LogoTexture;\n\tUPROPERTY()\n\tTObjectPtr<UTexture2D> CrestTexture;\n"),
])
patch("PKPlayerController.cpp", [
    ('#include "PKTypes.h"\n', '#include "PKTypes.h"\n#include "PKGameInstance.h"\n#include "PKMenu.h"\n#include "Engine/Engine.h"\n#include "Engine/GameViewportClient.h"\n#include "Engine/Texture2D.h"\n#include "Framework/Application/SlateApplication.h"\n#include "Kismet/KismetSystemLibrary.h"\n#include "UObject/ConstructorHelpers.h"\n'),
    ("\tbAutoManageActiveCameraTarget = false;\n\tbShowMouseCursor = false;\n",
     "\tbAutoManageActiveCameraTarget = false;\n\tbShowMouseCursor = false;\n"
     "\tstatic ConstructorHelpers::FObjectFinder<UTexture2D> Logo(TEXT(\"/Game/PitchKings/UI/T_PK_Logo.T_PK_Logo\"));\n"
     "\tstatic ConstructorHelpers::FObjectFinder<UTexture2D> Crest(TEXT(\"/Game/PitchKings/UI/T_PK_Crest.T_PK_Crest\"));\n"
     "\tLogoTexture = Logo.Object;\n\tCrestTexture = Crest.Object;\n"),
    ("\t\tCam->bConstrainAspectRatio = false;\n\t\tSetViewTarget(BroadcastCamera);\n\t}\n}",
     "\t\tCam->bConstrainAspectRatio = false;\n\t\tSetViewTarget(BroadcastCamera);\n\t}\n\n"
     "\t// Normal launch: landing page first (home when coming back to the menu in the same session).\n"
     "\tif (APKGameMode::UsesMenus() && GEngine && GEngine->GameViewport)\n\t{\n"
     "\t\tSAssignNew(Menu, SPKMenu).Owner(this).Logo(LogoTexture).Crest(CrestTexture);\n"
     "\t\tGEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 50);\n"
     "\t\tUPKGameInstance* GI = GetGameInstance<UPKGameInstance>();\n"
     "\t\tShowMenu(GI && GI->bSeenLanding ? EPKMenuPage::Home : EPKMenuPage::Landing);\n"
     "\t\tif (GI)\n\t\t{\n\t\t\tGI->bSeenLanding = true;\n\t\t}\n\t}\n}\n\n"
     "void APKPlayerController::EndPlay(const EEndPlayReason::Type Reason)\n{\n"
     "\tif (Menu.IsValid() && GEngine && GEngine->GameViewport)\n\t{\n\t\tGEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());\n\t}\n"
     "\tMenu.Reset();\n\tSuper::EndPlay(Reason);\n}\n\n"
     "bool APKPlayerController::IsMenuOpen() const\n{\n\treturn Menu.IsValid() && Menu->GetVisibility() == EVisibility::Visible;\n}\n\n"
     "void APKPlayerController::ShowMenu(EPKMenuPage Page)\n{\n"
     "\tif (!Menu.IsValid())\n\t{\n\t\treturn;\n\t}\n"
     "\tMenu->SetVisibility(EVisibility::Visible);\n"
     "\tFInputModeUIOnly Mode;\n\tMode.SetWidgetToFocus(Menu);\n\tMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);\n\tSetInputMode(Mode);\n"
     "\tbShowMouseCursor = true;\n"
     "\tMenu->ShowPage(Page); // after the input mode, so the page's first button gets focus\n}\n\n"
     "void APKPlayerController::HideMenu()\n{\n"
     "\tif (Menu.IsValid())\n\t{\n\t\tMenu->SetVisibility(EVisibility::Collapsed);\n\t}\n"
     "\tSetInputMode(FInputModeGameOnly());\n\tbShowMouseCursor = false;\n"
     "\tFSlateApplication::Get().SetAllUserFocusToGameViewport();\n"
     "\tif (BroadcastCamera)\n\t{\n\t\tBroadcastCamera->GetCameraComponent()->SetFieldOfView(CameraFov);\n\t}\n}\n\n"
     "void APKPlayerController::MenuKickOff()\n{\n\tHideMenu();\n\tif (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())\n\t{\n\t\tGM->StartMatch(false);\n\t}\n}\n\n"
     "void APKPlayerController::MenuSpectate()\n{\n\tHideMenu();\n\tif (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())\n\t{\n\t\tGM->StartMatch(true);\n\t}\n}\n\n"
     "void APKPlayerController::MenuQuit()\n{\n\tUKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);\n}\n\n"
     "void APKPlayerController::MenuResume()\n{\n\tSetPause(false);\n\tHideMenu();\n}\n\n"
     "void APKPlayerController::MenuRestart()\n{\n\tSetPause(false);\n\tHideMenu();\n\tif (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())\n\t{\n\t\tGM->StartMatch(GM->IsSpectating());\n\t}\n}\n\n"
     "void APKPlayerController::MenuMainMenu()\n{\n\tSetPause(false);\n\tif (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())\n\t{\n\t\tGM->EnterMenu();\n\t}\n\tShowMenu(EPKMenuPage::Home);\n}\n\n"
     "void APKPlayerController::UpdateMenuCamera(float DeltaTime)\n{\n"
     "\t// Slow orbit around the stadium behind the menus.\n"
     "\tif (!BroadcastCamera)\n\t{\n\t\treturn;\n\t}\n"
     "\tMenuOrbit += DeltaTime * 0.05f;\n"
     "\tconst FVector Loc(FMath::Cos(MenuOrbit) * 5600.f, FMath::Sin(MenuOrbit) * 4000.f, 1500.f + 300.f * FMath::Sin(MenuOrbit * 0.7f));\n"
     "\tBroadcastCamera->GetCameraComponent()->SetFieldOfView(55.f);\n"
     "\tBroadcastCamera->SetActorLocationAndRotation(Loc, (FVector(0.f, 0.f, 120.f) - Loc).Rotation());\n}"),
    ("\tconst FVector Move = ReadMove();\n",
     "\t// Front end open: only the menu camera runs.\n"
     "\tif (IsMenuOpen())\n\t{\n\t\tif (GM->Phase == EPKPhase::Menu)\n\t\t{\n\t\t\tUpdateMenuCamera(DeltaTime);\n\t\t}\n\t\treturn;\n\t}\n"
     "\tif (APKGameMode::UsesMenus() && Menu.IsValid())\n\t{\n"
     "\t\tif (GM->Phase == EPKPhase::FullTime && AnyPressed({ EKeys::Escape, EKeys::M, EKeys::Gamepad_FaceButton_Right }))\n\t\t{\n\t\t\tMenuMainMenu();\n\t\t\treturn;\n\t\t}\n"
     "\t\tif (GM->Phase != EPKPhase::FullTime && GM->Phase != EPKPhase::Menu && AnyPressed({ EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right }))\n\t\t{\n"
     "\t\t\tSetPause(true);\n\t\t\tShowMenu(EPKMenuPage::Pause);\n\t\t\treturn;\n\t\t}\n\t}\n\n"
     "\tconst FVector Move = ReadMove();\n"),
])
print("menu ok")
