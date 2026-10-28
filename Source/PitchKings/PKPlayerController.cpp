#include "PKPlayerController.h"
#include "PKBall.h"
#include "PKFootballer.h"
#include "PKGameMode.h"
#include "PKTypes.h"
#include "PKGameInstance.h"
#include "PKMenu.h"
#include "PKInput.h"
#include "PKSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	constexpr float ShotChargeSeconds = 1.f;
	constexpr float StickDeadZone = 0.2f;

	// Broadcast gantry: high on the near-side stand, following the ball along the touchline.
	constexpr float CameraBack = 1800.f;   // behind the near touchline (further back sits among the near-stand fans)
	constexpr float CameraHeight = 1200.f; // lower and a longer lens than before: the far stand and boards stay in frame
	constexpr float CameraFov = 31.f; // tighter: players read bigger
}

APKPlayerController::APKPlayerController()
{
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = false;
	static ConstructorHelpers::FObjectFinder<UTexture2D> Logo(TEXT("/Game/PitchKings/UI/T_PK_Logo.T_PK_Logo"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Crest(TEXT("/Game/PitchKings/UI/T_PK_Crest.T_PK_Crest"));
	LogoTexture = Logo.Object;
	CrestTexture = Crest.Object;
}

void APKPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	FPKInputRouter::Register();
	ApplyUserSettings();

	BroadcastCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(0.f, -PK::HalfWidth - CameraBack, CameraHeight), FRotator(-25.f, 90.f, 0.f));
	if (BroadcastCamera)
	{
		UCameraComponent* Cam = BroadcastCamera->GetCameraComponent();
		Cam->SetFieldOfView(CameraFov);
		Cam->bConstrainAspectRatio = false;
		SetViewTarget(BroadcastCamera);
	}

	// Normal launch: landing page first (home when coming back to the menu in the same session).
	if (APKGameMode::UsesMenus() && GEngine && GEngine->GameViewport)
	{
		SAssignNew(Menu, SPKMenu).Owner(this).Logo(LogoTexture).Crest(CrestTexture);
		GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 50);
		UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
		ShowMenu(GI && GI->bSeenLanding ? EPKMenuPage::Home : EPKMenuPage::Landing);
		if (GI)
		{
			GI->bSeenLanding = true;
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PKMenuShots")))
		{
			StartMenuSelfTest();
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PKVersusTest")))
		{
			StartVersusSelfTest();
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PKSettingsShots")))
		{
			StartSettingsSelfTest();
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PKSubsTest")))
		{
			StartSubsSelfTest();
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PKReplayTest")))
		{
			StartReplaySelfTest();
		}
	}
}

void APKPlayerController::StartMenuSelfTest()
{
	// -PKMenuShots: walk through every front-end page, screenshot each, restore the team sheet, quit.
	UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	if (!GI)
	{
		return;
	}
	struct FSaved { int32 Formation, Style, Opponent; TArray<int32> Lineup; };
	const FSaved Saved{ GI->Formation, GI->Style, GI->Opponent, GI->Lineup };
	TWeakObjectPtr<APKPlayerController> WeakThis(this);
	TSharedRef<int32> Step = MakeShared<int32>(0);
	TSharedRef<double> StartRef = MakeShared<double>(-1.0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Step, StartRef, Saved](float)
	{
#if WITH_EDITOR
		// Shaders of a fresh build compile on first use: start the walk-through once they are done.
		if (*StartRef < 0.0 && GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0)
		{
			return true;
		}
#endif
		if (*StartRef < 0.0)
		{
			*StartRef = FPlatformTime::Seconds();
		}
		const double Start = *StartRef;
		APKPlayerController* PC = WeakThis.Get();
		if (!PC || !PC->Menu.IsValid())
		{
			return false;
		}
		UPKGameInstance* G = PC->GetGameInstance<UPKGameInstance>();
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
			UE_LOG(LogTemp, Log, TEXT("[MenuShots] %s"), Name);
		};
		const double T = FPlatformTime::Seconds() - Start;
		const double Times[] = { 4.0, 4.5, 6.0, 6.5, 8.0, 8.5, 9.5, 10.0, 11.0, 11.5, 12.5, 13.0, 14.0, 14.5, 19.0, 19.5, 21.0, 21.5, 23.0, 24.0 };
		if (*Step >= UE_ARRAY_COUNT(Times) || T < Times[*Step])
		{
			return true;
		}
		switch ((*Step)++)
		{
		case 0: Shot(TEXT("Menu_01_Landing")); break;
		case 1: PC->Menu->ShowPage(EPKMenuPage::Home); break;
		case 2: Shot(TEXT("Menu_02_Home")); break;
		case 3: PC->Menu->ShowPage(EPKMenuPage::TeamSetup); break;
		case 4: Shot(TEXT("Menu_03_TeamSetup")); break;
		case 5: PC->Menu->TestClickSlot(10); break;
		case 6: Shot(TEXT("Menu_04_SlotSelected")); break;
		case 7: PC->Menu->TestClickSquadPlayer(17); break;
		case 8: Shot(TEXT("Menu_05_BenchSwapped")); break;
		case 9: PC->Menu->ShowPage(EPKMenuPage::Controls); break;
		case 10: Shot(TEXT("Menu_06_Controls")); break;
		case 11:
			if (G)
			{
				G->Formation = Saved.Formation; G->Style = Saved.Style; G->Opponent = Saved.Opponent; G->Lineup = Saved.Lineup; G->Save();
			}
			PC->MenuKickOff();
			break;
		case 12: Shot(TEXT("Menu_07_Match")); break;
		case 13: PC->SetPause(true); PC->ShowMenu(EPKMenuPage::Pause); break;
		case 14: Shot(TEXT("Menu_08_Pause")); break;
		case 15: PC->MenuMainMenu(); break;
		case 16: Shot(TEXT("Menu_09_BackHome")); break;
		case 17: PC->MenuSpectate(); break;
		case 18: Shot(TEXT("Menu_10_Spectate")); break;
		default:
			FGenericPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}));
}

void APKPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	FPKInputRouter::Unregister();
	Super::EndPlay(Reason);
}

bool APKPlayerController::IsMenuOpen() const
{
	return Menu.IsValid() && Menu->GetVisibility() == EVisibility::Visible;
}

void APKPlayerController::ShowMenu(EPKMenuPage Page)
{
	if (!Menu.IsValid())
	{
		return;
	}
	Menu->SetVisibility(EVisibility::Visible);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	Menu->ShowPage(Page); // after the input mode, so the page's first button gets focus
}

void APKPlayerController::HideMenu()
{
	if (Menu.IsValid())
	{
		Menu->SetVisibility(EVisibility::Collapsed);
	}
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	if (BroadcastCamera)
	{
		BroadcastCamera->GetCameraComponent()->SetFieldOfView(CameraFov);
	}
}

void APKPlayerController::MenuKickOff()
{
	UE_LOG(LogTemp, Log, TEXT("[Menu] MenuKickOff"));
	TeamDevice[0] = TeamDevice[1] = PKDevice::Any;
	HideMenu();
	if (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())
	{
		GM->StartMatch(false);
	}
}

void APKPlayerController::MenuVersus(int32 DeviceP1, int32 DeviceP2)
{
	TeamDevice[0] = DeviceP1;
	TeamDevice[1] = DeviceP2;
	HideMenu();
	if (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())
	{
		GM->StartMatch(false, true);
	}
	FPKInputRouter::Get().Rumble(DeviceP1, 0.4f, 0.25f); // "you are this side"
	FPKInputRouter::Get().Rumble(DeviceP2, 0.4f, 0.25f);
}

void APKPlayerController::RumbleTeam(int32 Team, float Strength, float Seconds)
{
	const APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>();
	const FPKUserSettings* S = UserSettings();
	const float Scale = S ? S->Rumble : 1.f;
	if (Scale <= 0.f)
	{
		return;
	}
	FPKInputRouter::Get().Rumble(GM && GM->IsVersus() ? TeamDevice[Team & 1] : PKDevice::Any, Strength * Scale, Seconds);
}

void APKPlayerController::MenuSpectate()
{
	HideMenu();
	if (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())
	{
		GM->StartMatch(true);
	}
}

void APKPlayerController::MenuQuit()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);
}

void APKPlayerController::MenuResume()
{
	SetPause(false);
	HideMenu();
}

void APKPlayerController::MenuRestart()
{
	SetPause(false);
	HideMenu();
	if (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())
	{
		GM->StartMatch(GM->IsSpectating());
	}
}

void APKPlayerController::MenuMainMenu()
{
	SetPause(false);
	if (APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>())
	{
		GM->EnterMenu();
	}
	ShowMenu(EPKMenuPage::Home);
}

void APKPlayerController::UpdateMenuCamera(float DeltaTime)
{
	// Slow orbit around the stadium behind the menus.
	if (!BroadcastCamera)
	{
		return;
	}
	MenuOrbit += DeltaTime * 0.05f;
	const FVector Loc(FMath::Cos(MenuOrbit) * 5600.f, FMath::Sin(MenuOrbit) * 4000.f, 1500.f + 300.f * FMath::Sin(MenuOrbit * 0.7f));
	BroadcastCamera->GetCameraComponent()->SetFieldOfView(55.f);
	BroadcastCamera->SetActorLocationAndRotation(Loc, (FVector(0.f, 0.f, 120.f) - Loc).Rotation());
}

bool APKPlayerController::AnyDown(std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (IsInputKeyDown(Key))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::AnyPressed(std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (WasInputKeyJustPressed(Key))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::AnyReleased(std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (WasInputKeyJustReleased(Key))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::DevDown(int32 Device, std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (FPKInputRouter::Get().IsDown(Device, Key))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::DevPressed(int32 Device, std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (FPKInputRouter::Get().WasPressed(Device, Key, InputSeq))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::DevReleased(int32 Device, std::initializer_list<FKey> Keys) const
{
	for (const FKey& Key : Keys)
	{
		if (FPKInputRouter::Get().WasReleased(Device, Key, InputSeq))
		{
			return true;
		}
	}
	return false;
}

const FPKUserSettings* APKPlayerController::UserSettings() const
{
	const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>();
	return GI ? &GI->Settings : nullptr;
}

void APKPlayerController::ApplyUserSettings()
{
	if (const FPKUserSettings* S = UserSettings())
	{
		bShowHelp = S->bShowHelp;
		bShowNumbers = S->bShowNumbers;
		CameraZoomScale = S->CameraZoom == 0 ? 0.75f : (S->CameraZoom == 2 ? 1.3f : 1.f);
	}
}

void APKPlayerController::OpenSettingsFromPause()
{
	ShowMenu(EPKMenuPage::Settings);
}

TArray<FKey> APKPlayerController::ActionKeys(int32 Device, EPKInput Action) const
{
	// Keyboard keys come from the player's bindings, gamepad buttons are fixed.
	TArray<FKey> Keys;
	const FPKUserSettings* S = UserSettings();
	if (Device == PKDevice::Any || Device == PKDevice::Keyboard)
	{
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FKey Key = S ? S->Keys[(int32)Action][Slot] : (Slot == 0 ? PKInputs::Info(Action).Key1 : PKInputs::Info(Action).Key2);
			if (Key.IsValid())
			{
				Keys.Add(Key);
			}
		}
	}
	if (Device == PKDevice::Any || PKDevice::IsGamepad(Device))
	{
		const FPKInputInfo& I = PKInputs::Info(Action);
		if (I.Pad1.IsValid())
		{
			Keys.Add(I.Pad1);
		}
		if (I.Pad2.IsValid())
		{
			Keys.Add(I.Pad2);
		}
	}
	return Keys;
}

bool APKPlayerController::ActionDown(int32 Device, EPKInput Action) const
{
	for (const FKey& Key : ActionKeys(Device, Action))
	{
		if (FPKInputRouter::Get().IsDown(Device, Key))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::ActionPressed(int32 Device, EPKInput Action) const
{
	for (const FKey& Key : ActionKeys(Device, Action))
	{
		if (FPKInputRouter::Get().WasPressed(Device, Key, InputSeq))
		{
			return true;
		}
	}
	return false;
}

bool APKPlayerController::ActionReleased(int32 Device, EPKInput Action) const
{
	for (const FKey& Key : ActionKeys(Device, Action))
	{
		if (FPKInputRouter::Get().WasReleased(Device, Key, InputSeq))
		{
			return true;
		}
	}
	return false;
}

FVector APKPlayerController::ReadMove(int32 Device) const
{
	// Up / right on the screen from the key bindings and the stick...
	const FPKUserSettings* S = UserSettings();
	FVector2D Screen(0.f, 0.f);
	Screen.X += (ActionDown(Device, EPKInput::MoveRight) ? 1.f : 0.f) - (ActionDown(Device, EPKInput::MoveLeft) ? 1.f : 0.f);
	Screen.Y += (ActionDown(Device, EPKInput::MoveUp) ? 1.f : 0.f) - (ActionDown(Device, EPKInput::MoveDown) ? 1.f : 0.f);
	const FPKInputRouter& R = FPKInputRouter::Get();
	const FVector2D Stick(R.Axis(Device, EKeys::Gamepad_LeftX), R.Axis(Device, EKeys::Gamepad_LeftY));
	const float DeadZone = S ? S->DeadZone : StickDeadZone;
	if (Stick.Size() > DeadZone)
	{
		// Radial dead zone, rescaled so walking pace is still reachable.
		Screen += Stick.GetSafeNormal() * FMath::GetMappedRangeValueClamped(FVector2D(DeadZone, 1.f), FVector2D(0.f, 1.f), Stick.Size());
	}
	if (S && S->bInvertX)
	{
		Screen.X = -Screen.X;
	}
	if (S && S->bInvertY)
	{
		Screen.Y = -Screen.Y;
	}
	Screen = Screen.GetSafeNormal() * FMath::Min(1.f, Screen.Size());

	// ...turned into pitch directions through the camera, so "right" is always right on the screen.
	const float Yaw = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Yaw : 90.f;
	const FRotationMatrix M(FRotator(0.f, Yaw, 0.f));
	const FVector Forward = PK::Flat(M.GetUnitAxis(EAxis::X)).GetSafeNormal();
	const FVector Right = PK::Flat(M.GetUnitAxis(EAxis::Y)).GetSafeNormal();
	return (Right * Screen.X + Forward * Screen.Y).GetClampedToMaxSize(1.f);
}

FVector APKPlayerController::ReadRightStick(int32 Device) const
{
	const FPKUserSettings* S = UserSettings();
	const FPKInputRouter& R = FPKInputRouter::Get();
	FVector2D Stick(R.Axis(Device, EKeys::Gamepad_RightX), R.Axis(Device, EKeys::Gamepad_RightY));
	const float DeadZone = S ? S->DeadZone : StickDeadZone;
	if (Stick.Size() <= DeadZone)
	{
		return FVector::ZeroVector;
	}
	Stick = Stick.GetSafeNormal() * FMath::GetMappedRangeValueClamped(FVector2D(DeadZone, 1.f), FVector2D(0.f, 1.f), Stick.Size());
	if (S && S->bInvertX) Stick.X = -Stick.X;
	if (S && S->bInvertY) Stick.Y = -Stick.Y;
	const float Yaw = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Yaw : 90.f;
	const FRotationMatrix M(FRotator(0.f, Yaw, 0.f));
	const FVector Forward = PK::Flat(M.GetUnitAxis(EAxis::X)).GetSafeNormal();
	const FVector Right = PK::Flat(M.GetUnitAxis(EAxis::Y)).GetSafeNormal();
	return (Right * Stick.X + Forward * Stick.Y).GetClampedToMaxSize(1.f);
}

FVector APKPlayerController::ReadMove() const
{
	// The camera looks along +Y, so screen right = +X and screen up = +Y.
	FVector Move(0.f);
	Move.X += (AnyDown({ EKeys::D, EKeys::Right }) ? 1.f : 0.f) - (AnyDown({ EKeys::A, EKeys::Left }) ? 1.f : 0.f);
	Move.Y += (AnyDown({ EKeys::W, EKeys::Up }) ? 1.f : 0.f) - (AnyDown({ EKeys::S, EKeys::Down }) ? 1.f : 0.f);

	const FVector Stick(GetInputAnalogKeyState(EKeys::Gamepad_LeftX), GetInputAnalogKeyState(EKeys::Gamepad_LeftY), 0.f);
	if (Stick.Size() > StickDeadZone)
	{
		Move += Stick;
	}
	return Move.GetClampedToMaxSize(1.f);
}

void APKPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	FPKInputRouter& Router = FPKInputRouter::Get();
	ON_SCOPE_EXIT
	{
		InputSeq = Router.Sequence();
	};

	APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>();
	if (!GM || !GM->GetBall())
	{
		return;
	}

	// Front end open: only the menu camera runs.
	if (IsMenuOpen())
	{
		if (GM->Phase == EPKPhase::Menu)
		{
			UpdateMenuCamera(DeltaTime);
		}
		return;
	}
	const int32 AnyDevice = PKDevice::Any;
	if (APKGameMode::UsesMenus() && Menu.IsValid())
	{
		if (GM->Phase == EPKPhase::FullTime && DevPressed(AnyDevice, { EKeys::Escape, EKeys::M, EKeys::Gamepad_FaceButton_Right }))
		{
			MenuMainMenu();
			return;
		}
		// Either player can pause.
		if (GM->Phase != EPKPhase::FullTime && GM->Phase != EPKPhase::Menu && ActionPressed(AnyDevice, EPKInput::Pause))
		{
			SetPause(true);
			ShowMenu(EPKMenuPage::Pause);
			return;
		}
	}

	if (DevPressed(AnyDevice, { EKeys::H }))
	{
		bShowHelp = !bShowHelp;
		if (UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())
		{
			GI->Settings.bShowHelp = bShowHelp;
		}
	}
	if (DevPressed(AnyDevice, { EKeys::N }))
	{
		bShowNumbers = !bShowNumbers;
	}
	if (GM->Phase == EPKPhase::FullTime && DevPressed(AnyDevice, { EKeys::R, EKeys::Enter, EKeys::Gamepad_Special_Right }))
	{
		GM->RestartMatch();
	}

	if (GM->IsSpectating())
	{
		// Spectate: the AI plays your team; style keys and help still work.
		const FKey StyleKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
		for (int32 i = 0; i < 5; ++i)
		{
			if (Router.WasPressed(AnyDevice, StyleKeys[i], InputSeq))
			{
				GM->SetTeamStyle(0, (EPKTeamStyle)i);
			}
		}
		UpdateCamera(DeltaTime);
		return;
	}

	if (GM->IsReplaying())
	{
		if (ActionPressed(AnyDevice, EPKInput::Pass) || ActionPressed(AnyDevice, EPKInput::Shoot) || DevPressed(AnyDevice, { EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom }))
		{
			GM->SkipReplay();
		}
		UpdateCamera(DeltaTime);
		return;
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (GM->IsHumanTeam(Team))
		{
			HandlePlayer(GM, Team, DeltaTime);
		}
	}
	static const bool bInputDebug = FParse::Param(FCommandLine::Get(), TEXT("PKInputDebug"));
	static double NextDebug = 0.0;
	if (bInputDebug && FPlatformTime::Seconds() > NextDebug)
	{
		NextDebug = FPlatformTime::Seconds() + 0.1;
		const APKFootballer* H = GM->GetHuman(0);
		FString Keys;
		for (const FKey& K : { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::J })
		{
			if (Router.IsDown(PKDevice::Keyboard, K)) { Keys += K.ToString() + TEXT(" "); }
		}
		UE_LOG(LogTemp, Log, TEXT("[InputDebug] phase %d keys [%s] last device %d | human #%d input (%.2f,%.2f) vel %.0f | ball placed %d owner-is-human %d stunned %d"),
			(int32)GM->Phase, *Keys, Router.LastActiveDevice(), H ? H->ShirtNumber : -1, H ? H->MoveInput.X : 0.f, H ? H->MoveInput.Y : 0.f,
			H ? H->GetVelocity().Size2D() : 0.f, GM->GetBall()->IsPlaced() ? 1 : 0, H && GM->GetBall()->GetOwnerPlayer() == H ? 1 : 0, H && H->IsStunned() ? 1 : 0);
		if (H)
		{
			const UCharacterMovementComponent* CMC = H->GetCharacterMovement();
			UE_LOG(LogTemp, Log, TEXT("[InputDebug]    action %d lock %d kicking %d touch %d buffer %.2f hold %d mode %d maxspeed %.0f accel (%.0f,%.0f) stance %d hasball %d human %d"),
				(int32)H->GetCurrentAction(), H->IsActionLockingMovement() ? 1 : 0, H->IsKicking() ? 1 : 0, (int32)H->TouchAction, H->BufferTimer, H->bHoldPosition ? 1 : 0,
				(int32)CMC->MovementMode, CMC->MaxWalkSpeed, CMC->GetCurrentAcceleration().X, CMC->GetCurrentAcceleration().Y, (int32)H->Stance, H->bHasBall ? 1 : 0, H->bHuman ? 1 : 0);
		}
	}
	UpdateCamera(DeltaTime);
}

void APKPlayerController::HandlePlayer(APKGameMode* GM, int32 Team, float DeltaTime)
{
	// Single player: every device drives player 1. Local 1v1: each team reads only its own device.
	const int32 Dev = GM->IsVersus() ? TeamDevice[Team] : PKDevice::Any;
	const FPKInputRouter& Router = FPKInputRouter::Get();
	const FVector Move = ReadMove(Dev);
	const bool bSprint = ActionDown(Dev, EPKInput::Sprint) || Router.Axis(Dev, EKeys::Gamepad_RightTriggerAxis) > 0.3f;
	const bool bModifier = ActionDown(Dev, EPKInput::Hold) || Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	const bool bPrecision = ActionDown(Dev, EPKInput::Precision);
	GM->HumanMove(Team, Move, bSprint, bModifier, bPrecision);
	const FVector RightStick = ReadRightStick(Dev);
	// Kick-off: the taker cannot run with the ball, so a direction press takes the kick-off pass that way.
	if (Move.Size() > 0.5f && GM->GetSetPiece() == EPKSetPiece::Kickoff && GM->HumanOnDeadBall(Team))
	{
		GM->HumanKickOff(Team, Move);
	}

	bool& bCharging = bTeamCharging[Team];
	float& Charge = TeamCharge[Team];

	if (GM->HumanHasBall(Team))
	{
		GM->HumanTeamPress(Team, false);
		GM->HumanKeeperRush(Team, false);
		// Pass buttons: with Precision held, hold for power and release to play it to the spot; with Hold (LT / Ctrl)
		// held the pass is swerved around the nearest defender.
		const EPKInput PassInputs[] = { EPKInput::Pass, EPKInput::Through, EPKInput::Lob };
		const EPKKickType PassTypes[] = { EPKKickType::Ground, EPKKickType::Through, EPKKickType::Lob };
		bool bPassPressed = false;
		for (int32 i = 0; i < 3 && !bPassPressed; ++i)
		{
			if (!ActionPressed(Dev, PassInputs[i]))
			{
				continue;
			}
			bPassPressed = true;
			if (bCharging)
			{
				// Pass while charging a shot: fake shot.
				GM->HumanFakeShot(Team, Move);
				bCharging = false;
				break;
			}
			if (bPrecision)
			{
				bPassCharging[Team] = true;
				PassCharge[Team] = 0.f;
				PassChargeIdx[Team] = i;
			}
			else
			{
				const bool bOneTwo = ActionDown(Dev, EPKInput::Runs);
				bRunsUsed[Team] |= bOneTwo;
				GM->HumanPass(Team, Move, PassTypes[i], bModifier, bOneTwo);
			}
		}
		if (bPassCharging[Team])
		{
			PassCharge[Team] = FMath::Min(1.f, PassCharge[Team] + DeltaTime / 0.9f);
			if (!ActionDown(Dev, PassInputs[PassChargeIdx[Team]]) || PassCharge[Team] >= 1.f)
			{
				GM->HumanPrecisionPass(Team, Move, PassTypes[PassChargeIdx[Team]], PassCharge[Team], bModifier);
				bPassCharging[Team] = false;
			}
		}

		else if (ActionPressed(Dev, EPKInput::KnockOn))
		{
			bEffortHeld[Team] = true;
			EffortTime[Team] = 0.f;
			EffortDir[Team] = FVector::ZeroVector; // the move direction when it is let go
			bCharging = false;                     // an effort touch cancels a shot being charged
		}
		else if (bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team] && !bEffortHeld[Team])
		{
			bEffortHeld[Team] = true; // RB + right stick: effort touch that way
			EffortTime[Team] = 0.f;
			EffortDir[Team] = RightStick.GetSafeNormal2D();
			bCharging = false;
		}
		else if (!bPrecision && RightStick.Size() > 0.6f && !bRightStickFlick[Team])
		{
			GM->HumanSkill(Team, RightStick.GetSafeNormal2D()); // right stick flick: skill move
		}
		else if (ActionPressed(Dev, EPKInput::Skill))
		{
			GM->HumanSkill(Team, Move); // B + direction (B alone: step-over)
		}
		else if (ActionPressed(Dev, EPKInput::DragBack))
		{
			GM->HumanDragBack(Team);
		}
		else if (ActionPressed(Dev, EPKInput::Feint))
		{
			GM->HumanFeint(Team, Move);
		}
		else if (ActionPressed(Dev, EPKInput::FlickUp))
		{
			GM->HumanFlickUp(Team, Move);
		}

		// Runs tapped on its own (not used with a pass or shot): send a team-mate on a run.
		if (ActionDown(Dev, EPKInput::Runs))
		{
			RunsHeld[Team] = RunsHeld[Team] < 0.f ? 0.f : RunsHeld[Team] + DeltaTime;
		}
		else
		{
			if (RunsHeld[Team] >= 0.f && RunsHeld[Team] < 0.35f && !bRunsUsed[Team])
			{
				GM->HumanSendRun(Team);
			}
			RunsHeld[Team] = -1.f;
			bRunsUsed[Team] = false;
		}

		// Effort touch: short when tapped, longer the longer it is held (full after 0.4 s).
		if (bEffortHeld[Team])
		{
			EffortTime[Team] += DeltaTime;
			const bool bStickHeld = !EffortDir[Team].IsNearlyZero() && RightStick.Size() > 0.3f;
			const bool bKeyHeld = EffortDir[Team].IsNearlyZero() && ActionDown(Dev, EPKInput::KnockOn);
			if ((!bStickHeld && !bKeyHeld) || EffortTime[Team] >= 0.4f)
			{
				const FVector Dir = EffortDir[Team].IsNearlyZero() ? Move : EffortDir[Team];
				GM->HumanKnockOn(Team, Dir, FMath::Clamp(EffortTime[Team] / 0.4f, 0.f, 1.f));
				bEffortHeld[Team] = false;
			}
		}

		// Shot: hold to charge, release (or full charge) to shoot. The modifiers held at release pick the technique:
		// Precision = finesse, Runs = chip, both = power shot, Hold = low driven. Pressing shoot again during the
		// wind-up is timed finishing.
		if (ActionPressed(Dev, EPKInput::Shoot) && !GM->HumanTimedFinish(Team))
		{
			bCharging = true;
			Charge = 0.f;
		}
		if (bCharging)
		{
			Charge = FMath::Min(1.f, Charge + DeltaTime / ShotChargeSeconds);
			if (ActionReleased(Dev, EPKInput::Shoot) || !ActionDown(Dev, EPKInput::Shoot) || Charge >= 1.f)
			{
				const bool bRuns = ActionDown(Dev, EPKInput::Runs);
				bRunsUsed[Team] |= bRuns;
				const EPKShotType Type = bPrecision && bRuns ? EPKShotType::Power : (bPrecision ? EPKShotType::Finesse : (bRuns ? EPKShotType::Chip : (bModifier ? EPKShotType::Low : EPKShotType::Normal)));
				GM->HumanShoot(Team, Move, Charge, Type);
				bCharging = false;
			}
		}
	}
	else
	{
		bCharging = false;
		bEffortHeld[Team] = false;
		bPassCharging[Team] = false;
		GM->HumanTeamPress(Team, bPrecision);
		GM->HumanKeeperRush(Team, ActionDown(Dev, EPKInput::Through));
		// A ball in the air coming his way: pass / shoot ask for a header (or a volley at waist height).
		bool bHeaderAsked = false;
		if (ActionPressed(Dev, EPKInput::Pass) || ActionPressed(Dev, EPKInput::Through) || ActionPressed(Dev, EPKInput::Lob))
		{
			bHeaderAsked = GM->HumanRequestHeader(Team, Move, false);
		}
		else if (ActionPressed(Dev, EPKInput::Shoot))
		{
			bHeaderAsked = GM->HumanRequestHeader(Team, Move, true);
		}
		if (ActionPressed(Dev, EPKInput::Tackle) && !bHeaderAsked)
		{
			GM->HumanTackle(Team);
		}
		if (ActionPressed(Dev, EPKInput::Slide))
		{
			GM->HumanSlideTackle(Team, Move);
		}
		if (ActionPressed(Dev, EPKInput::Switch))
		{
			GM->HumanSwitch(Team);
		}
		if (ActionPressed(Dev, EPKInput::Barge) && !bHeaderAsked)
		{
			GM->HumanShoulder(Team);
		}
	}

	bRightStickFlick[Team] = RightStick.Size() > 0.6f; // a flick counts once until the stick comes back

	// Team play style: 1-5 on the keyboard, D-pad left / right on a gamepad.
	const FKey StyleKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 i = 0; i < 5; ++i)
	{
		if (Router.WasPressed(Dev, StyleKeys[i], InputSeq))
		{
			GM->SetTeamStyle(Team, (EPKTeamStyle)i);
		}
	}
	// Game plan mentality: 9 / 0 on the keyboard, LT + D-pad left / right on a gamepad.
	const bool bPadPlan = Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	if (Router.WasPressed(Dev, EKeys::Nine, InputSeq) || (bPadPlan && DevPressed(Dev, { EKeys::Gamepad_DPad_Left })))
	{
		GM->SetMentality(Team, GM->GetMentality(Team) - 1);
	}
	else if (Router.WasPressed(Dev, EKeys::Zero, InputSeq) || (bPadPlan && DevPressed(Dev, { EKeys::Gamepad_DPad_Right })))
	{
		GM->SetMentality(Team, GM->GetMentality(Team) + 1);
	}
	else if (DevPressed(Dev, { EKeys::Gamepad_DPad_Right }))
	{
		GM->CycleTeamStyle(Team, 1);
	}
	else if (DevPressed(Dev, { EKeys::Gamepad_DPad_Left }))
	{
		GM->CycleTeamStyle(Team, -1);
	}
}

void APKPlayerController::UpdateCamera(float DeltaTime)
{
	APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>();
	if (!BroadcastCamera || !GM || !GM->GetBall())
	{
		return;
	}

	const FVector BallLoc = PK::Flat(GM->GetBall()->GetActorLocation());
	FVector Celebration;
	const bool bReplaying = GM->IsReplaying();
	if (bReplaying && !bWasReplaying)
	{
		// the replay rewinds the ball to the start of the move: cut straight to it (easing over framed empty turf)
		CameraFocus = BallLoc;
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetGameCameraCutThisFrame();
		}
	}
	bWasReplaying = bReplaying;
	CameraFocus = FMath::VInterpTo(CameraFocus, GM->GetCelebrationFocus(Celebration) ? PK::Flat(Celebration) : BallLoc, DeltaTime, bReplaying ? 5.f : 2.2f);
	if (GM->CameraCutStamp != LastCameraCutStamp && PlayerCameraManager)
	{
		LastCameraCutStamp = GM->CameraCutStamp;
		PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	FVector EventFocus;
	if (GM->GetEventFocus(EventFocus))
	{
		if (FVector::Dist2D(CameraFocus, EventFocus) > 50.f && PlayerCameraManager)
		{
			PlayerCameraManager->SetGameCameraCutThisFrame(); // no motion blur smear on the jump
		}
		CameraFocus = PK::Flat(EventFocus); // self-test capture: frame the player making the action
	}

	FVector CamLoc;
	FVector LookAt;
	float CriticFov = 60.f;
	BroadcastCamera->GetCameraComponent()->SetFieldOfView(CameraFov); // shots below may override it
	if (GM->GetCriticCamera(CamLoc, LookAt, CriticFov))
	{
		BroadcastCamera->GetCameraComponent()->SetFieldOfView(CriticFov);
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetGameCameraCutThisFrame();
		}
	}
	else if (GM->GetAnimTestCamera(CamLoc, LookAt))
	{
		BroadcastCamera->GetCameraComponent()->SetFieldOfView(60.f);
	}
	else if (GM->IsReplaying() || GM->GetCelebrationFocus(Celebration))
	{
		// Replay: low from the side and a little behind the play, tighter than the match camera.
		CamLoc = CameraFocus + FVector(-PK::AttackDir(0) * 300.f, -1300.f, 330.f);
		LookAt = CameraFocus + FVector(0.f, 0.f, 70.f);
	}
	else if (GM->IsCloseCamera())
	{
		// Inspection camera: low and close behind the ball.
		CamLoc = CameraFocus + FVector(-600.f, -900.f, 380.f);
		LookAt = CameraFocus + FVector(0.f, 0.f, 80.f);
	}
	else
	{
		CamLoc = FVector(
			FMath::Clamp(CameraFocus.X * 0.85f, -PK::HalfLength + 1800.f, PK::HalfLength - 1800.f),
			-PK::HalfWidth - CameraBack * CameraZoomScale + CameraFocus.Y * 0.2f,
			CameraHeight * CameraZoomScale);
		// never tip down so far that the far stand leaves the frame (ball on the near touchline)
		LookAt = FVector(CameraFocus.X, FMath::Max(CameraFocus.Y * 0.7f - 150.f, -1300.f), 0.f);
		// stay clear of the near stand (its rows rise 50 cm per 80 cm from y = -4300): a low gantry ended up inside
		// the crowd when zoomed out
		const float Behind = FMath::Max(0.f, -CamLoc.Y - 4300.f);
		CamLoc.Z = FMath::Max(CamLoc.Z, 150.f + Behind * 0.625f + 450.f);
	}
	// Every camera (match, replay, inspection, critic shots) stays inside the bowl and below the roof: the world
	// outside is only a distant skyline backdrop, never a place to look from.
	CamLoc.X = FMath::Clamp(CamLoc.X, -8600.f, 8600.f);
	CamLoc.Y = FMath::Clamp(CamLoc.Y, -6900.f, 6900.f);
	CamLoc.Z = FMath::Clamp(CamLoc.Z, 30.f, 2800.f);
	BroadcastCamera->SetActorLocationAndRotation(CamLoc, (LookAt - CamLoc).Rotation());
}

void APKPlayerController::StartSettingsSelfTest()
{
	// -PKSettingsShots: every settings tab, a key rebind through the capture code, then defaults restored.
	const double Start = FPlatformTime::Seconds();
	TWeakObjectPtr<APKPlayerController> WeakThis(this);
	TSharedRef<int32> Step = MakeShared<int32>(0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Step, Start](float)
	{
		APKPlayerController* PC = WeakThis.Get();
		UPKGameInstance* GI = PC ? PC->GetGameInstance<UPKGameInstance>() : nullptr;
		if (!PC || !GI || !PC->Menu.IsValid())
		{
			return false;
		}
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
		};
		const double T = FPlatformTime::Seconds() - Start;
		const double Times[] = { 3.0, 4.0, 5.5, 6.0, 7.0, 7.5, 8.5, 9.0, 10.5, 11.0, 12.5, 13.0, 14.0, 15.5, 16.5 };
		if (*Step >= UE_ARRAY_COUNT(Times) || T < Times[*Step])
		{
			return true;
		}
		switch ((*Step)++)
		{
		case 0: PC->Menu->ShowPage(EPKMenuPage::Home); break;
		case 1: PC->Menu->ShowPage(EPKMenuPage::Settings); break;
		case 2: Shot(TEXT("Settings_01_Controls")); break;
		case 3: PC->Menu->TestStartCapture((int32)EPKInput::Pass, 1); break;
		case 4: Shot(TEXT("Settings_02_Capture")); break;
		case 5:
			PC->Menu->TestKey(EKeys::Enter);
			UE_LOG(LogTemp, Log, TEXT("[SettingsTest] Pass is now: %s"), *GI->Settings.KeyText(EPKInput::Pass));
			PC->Menu->TestStartCapture((int32)EPKInput::MoveRight, 0);
			PC->Menu->TestKey(EKeys::L); // L is Through: the two swap
			UE_LOG(LogTemp, Log, TEXT("[SettingsTest] Move right: %s, Through: %s"), *GI->Settings.KeyText(EPKInput::MoveRight), *GI->Settings.KeyText(EPKInput::Through));
			break;
		case 6: Shot(TEXT("Settings_03_Rebound")); break;
		case 7: PC->Menu->TestSettingsTab(1); break;
		case 8: Shot(TEXT("Settings_04_Graphics")); break;
		case 9: PC->Menu->TestSettingsTab(2); break;
		case 10: Shot(TEXT("Settings_05_Gameplay")); break;
		case 11:
			GI->Settings.ResetKeys();
			GI->SaveSettings();
			UE_LOG(LogTemp, Log, TEXT("[SettingsTest] restored: Move right %s, Through %s, Pass %s"), *GI->Settings.KeyText(EPKInput::MoveRight), *GI->Settings.KeyText(EPKInput::Through), *GI->Settings.KeyText(EPKInput::Pass));
			PC->Menu->ShowPage(EPKMenuPage::Home);
			break;
		case 12: PC->Menu->ShowPage(EPKMenuPage::Controls); break;
		case 13: Shot(TEXT("Settings_06_ControlsPage")); break;
		default:
			FGenericPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}));
}

void APKPlayerController::StartSubsSelfTest()
{
	// -PKSubsTest: kick off, pause, substitutions page, take a player off and bring one on, resume, screenshots.
	const double Start = FPlatformTime::Seconds();
	TWeakObjectPtr<APKPlayerController> WeakThis(this);
	TSharedRef<int32> Step = MakeShared<int32>(0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Step, Start](float)
	{
		APKPlayerController* PC = WeakThis.Get();
		APKGameMode* GM = PC ? PC->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
		if (!PC || !GM || !PC->Menu.IsValid())
		{
			return false;
		}
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
		};
		const double T = FPlatformTime::Seconds() - Start;
		const double Times[] = { 3.0, 9.0, 10.0, 11.0, 11.5, 12.5, 13.0, 14.0, 14.5, 17.0, 18.0 };
		if (*Step >= UE_ARRAY_COUNT(Times) || T < Times[*Step])
		{
			return true;
		}
		switch ((*Step)++)
		{
		case 0: PC->MenuKickOff(); break;
		case 1: PC->SetPause(true); PC->ShowMenu(EPKMenuPage::Pause); break;
		case 2: PC->Menu->ShowPage(EPKMenuPage::Subs); break;
		case 3: Shot(TEXT("Subs_01_Page")); break;
		case 4: PC->Menu->TestSubsSlot(7); break;
		case 5: Shot(TEXT("Subs_02_Picked")); break;
		case 6: UE_LOG(LogTemp, Log, TEXT("[SubsTest] sub made %d, subs left %d"), PC->Menu->TestSubsBench(0) ? 1 : 0, GM->SubsLeft(0)); break;
		case 7: Shot(TEXT("Subs_03_Done")); break;
		case 8: PC->MenuResume(); break;
		case 9: Shot(TEXT("Subs_04_Message")); break;
		default:
			FGenericPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}));
}

void APKPlayerController::StartReplaySelfTest()
{
	// -PKReplayTest: kick off, play a few seconds, force a goal chance; screenshots of the celebration and replay.
	const double Start = FPlatformTime::Seconds();
	TWeakObjectPtr<APKPlayerController> WeakThis(this);
	TSharedRef<int32> Step = MakeShared<int32>(0);
	TSharedRef<double> GoalAt = MakeShared<double>(0.0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Step, Start, GoalAt](float)
	{
		APKPlayerController* PC = WeakThis.Get();
		APKGameMode* GM = PC ? PC->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
		if (!PC || !GM)
		{
			return false;
		}
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
		};
		const double T = FPlatformTime::Seconds() - Start;
		switch (*Step)
		{
		case 0: if (T > 3.0) { PC->MenuKickOff(); ++*Step; } break;
		case 1: if (T > 13.0) { GM->DebugForceGoalChance(); ++*Step; } break;
		case 2:
			if (GM->Phase == EPKPhase::GoalScored) { *GoalAt = T; ++*Step; UE_LOG(LogTemp, Log, TEXT("[ReplayTest] goal at %.1f"), T); }
			else if (T > 22.0) { UE_LOG(LogTemp, Log, TEXT("[ReplayTest] no goal")); FGenericPlatformMisc::RequestExit(false); return false; }
			break;
		case 3: if (T > *GoalAt + 1.6) { Shot(TEXT("Replay_01_Celebration")); ++*Step; } break;
		case 4: if (GM->IsReplaying()) { UE_LOG(LogTemp, Log, TEXT("[ReplayTest] replay started at +%.1f"), T - *GoalAt); ++*Step; } else if (T > *GoalAt + 6.0) { UE_LOG(LogTemp, Log, TEXT("[ReplayTest] replay did not start")); *Step = 7; } break;
		case 5: if (T > *GoalAt + 4.6) { Shot(TEXT("Replay_02_Replay")); ++*Step; } break;
		case 6: if (T > *GoalAt + 6.6) { Shot(TEXT("Replay_03_ReplayEnd")); ++*Step; } break;
		case 7: if (GM->Phase == EPKPhase::Kickoff || GM->Phase == EPKPhase::Playing) { UE_LOG(LogTemp, Log, TEXT("[ReplayTest] back to kick-off at +%.1f, replaying %d"), T - *GoalAt, GM->IsReplaying() ? 1 : 0); Shot(TEXT("Replay_04_Kickoff")); ++*Step; } break;
		default:
			if (T > *GoalAt + 12.0) { FGenericPlatformMisc::RequestExit(false); return false; }
			break;
		}
		return true;
	}));
}

void APKPlayerController::StartVersusSelfTest()
{
	// -PKVersusTest: keyboard (P1) vs a synthetic gamepad (P2) through the real router, setup page and match.
	const double Start = FPlatformTime::Seconds();
	TWeakObjectPtr<APKPlayerController> WeakThis(this);
	TSharedRef<int32> Step = MakeShared<int32>(0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Step, Start](float)
	{
		APKPlayerController* PC = WeakThis.Get();
		APKGameMode* GM = PC ? PC->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
		if (!PC || !GM || !PC->Menu.IsValid())
		{
			return false;
		}
		FPKInputRouter& R = FPKInputRouter::Get();
		const int32 Pad = PKDevice::FromGamepadIndex(0);
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FString::Printf(TEXT("%s.png"), Name)), true, false);
			UE_LOG(LogTemp, Log, TEXT("[VersusTest] shot %s"), Name);
		};
		auto Report = [GM](const TCHAR* Label)
		{
			for (int32 T = 0; T < 2; ++T)
			{
				const APKFootballer* H = GM->GetHuman(T);
				UE_LOG(LogTemp, Log, TEXT("[VersusTest] %s: P%d %s #%d vel (%.0f, %.0f) input (%.2f, %.2f)"), Label, T + 1, H ? TEXT("controls") : TEXT("NONE"),
					H ? H->ShirtNumber : -1, H ? H->GetVelocity().X : 0.f, H ? H->GetVelocity().Y : 0.f, H ? H->MoveInput.X : 0.f, H ? H->MoveInput.Y : 0.f);
			}
		};
		const double T = FPlatformTime::Seconds() - Start;
		const double Times[] = { 3.0, 4.0, 4.1, 5.0, 6.0, 6.1, 9.0, 9.1, 10.0, 12.0, 12.1, 14.0, 14.1, 15.0, 15.1, 16.0, 17.0, 18.0, 19.0 };
		if (*Step >= UE_ARRAY_COUNT(Times) || T < Times[*Step])
		{
			return true;
		}
		switch ((*Step)++)
		{
		case 0: PC->Menu->ShowPage(EPKMenuPage::Versus); break;
		case 1: R.InjectKey(Pad, EKeys::Gamepad_FaceButton_Top, true); break;   // a gamepad says hello
		case 2: R.InjectKey(Pad, EKeys::Gamepad_FaceButton_Top, false); break;
		case 3: Shot(TEXT("Versus_01_Setup")); break;
		case 4: R.InjectKey(Pad, EKeys::Gamepad_FaceButton_Bottom, true); break; // P2 presses A: kick off
		case 5:
			R.InjectKey(Pad, EKeys::Gamepad_FaceButton_Bottom, false);
			UE_LOG(LogTemp, Log, TEXT("[VersusTest] versus=%d P1 device %s, P2 device %s"), GM->IsVersus() ? 1 : 0,
				*FPKInputRouter::DeviceName(PC->GetTeamDevice(0)), *FPKInputRouter::DeviceName(PC->GetTeamDevice(1)));
			break;
		case 6: R.InjectKey(PKDevice::Keyboard, EKeys::J, true); break;          // P1 takes the kick-off (pass)
		case 7: R.InjectKey(PKDevice::Keyboard, EKeys::J, false); Report(TEXT("P1 kicked off")); break;
		case 8: R.InjectKey(PKDevice::Keyboard, EKeys::D, true); break;          // P1 holds D (right)
		case 9: Report(TEXT("keyboard D held")); break;
		case 10: R.InjectKey(PKDevice::Keyboard, EKeys::D, false); R.InjectAxis(Pad, EKeys::Gamepad_LeftX, -1.f); break; // P2 stick left
		case 11: Report(TEXT("pad stick left")); break;
		case 12: Shot(TEXT("Versus_02_Match")); R.InjectAxis(Pad, EKeys::Gamepad_LeftX, 0.f); break;
		case 13: R.InjectKey(Pad, EKeys::Gamepad_Special_Right, true); break;    // P2 presses Start
		case 14:
			R.InjectKey(Pad, EKeys::Gamepad_Special_Right, false);
			UE_LOG(LogTemp, Log, TEXT("[VersusTest] after P2 Start: menu open %d"), PC->IsMenuOpen() ? 1 : 0);
			break;
		case 15: Shot(TEXT("Versus_03_Pause")); break;
		case 16: PC->MenuResume(); Report(TEXT("resumed")); break;
		default:
			FGenericPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}));
}
