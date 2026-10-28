"""Player controller: per-device input for up to two human players (local 1v1), rumble."""
import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def load(name):
    return open(os.path.join(base, name), encoding="utf-8-sig").read().replace("\r\n", "\n")


def save(name, s):
    open(os.path.join(base, name), "w", encoding="utf-8").write(s)


def rep(s, a, b, name):
    if a not in s:
        raise SystemExit("NOT FOUND in %s: %s" % (name, a[:110]))
    return s.replace(a, b, 1)


h = load("PKPlayerController.h")
h = rep(h, "\tfloat GetShotCharge() const { return bCharging ? Charge : 0.f; }",
        "\tfloat GetShotCharge(int32 Team = 0) const { return bTeamCharging[Team & 1] ? TeamCharge[Team & 1] : 0.f; }\n"
        "\t// Device each human team uses (PKDevice::Any = every device, single player).\n"
        "\tint32 GetTeamDevice(int32 Team) const { return TeamDevice[Team & 1]; }\n"
        "\tvoid RumbleTeam(int32 Team, float Strength, float Seconds);\n"
        "\tvoid MenuVersus(int32 DeviceP1, int32 DeviceP2);", "h")
h = rep(h, "\tFVector ReadMove() const;\n",
        "\tFVector ReadMove() const;\n"
        "\tbool DevDown(int32 Device, std::initializer_list<FKey> Keys) const;\n"
        "\tbool DevPressed(int32 Device, std::initializer_list<FKey> Keys) const;\n"
        "\tbool DevReleased(int32 Device, std::initializer_list<FKey> Keys) const;\n"
        "\tFVector ReadMove(int32 Device) const;\n"
        "\tvoid HandlePlayer(class APKGameMode* GM, int32 Team, float DeltaTime);\n"
        "\tvoid StartVersusSelfTest();\n", "h")
h = rep(h, "\tbool bCharging = false;\n\tfloat Charge = 0.f;\n",
        "\tbool bTeamCharging[2] = { false, false };\n\tfloat TeamCharge[2] = { 0.f, 0.f };\n"
        "\tint32 TeamDevice[2] = { -1, -1 };\n\tuint64 InputSeq = 0; // last router event this controller has read\n", "h")
save("PKPlayerController.h", h)

c = load("PKPlayerController.cpp")
c = rep(c, '#include "PKMenu.h"\n', '#include "PKMenu.h"\n#include "PKInput.h"\n#include "Misc/ScopeExit.h"\n', "cpp")
c = rep(c, "\tSetInputMode(FInputModeGameOnly());\n", "\tSetInputMode(FInputModeGameOnly());\n\tFPKInputRouter::Register();\n", "cpp")
c = rep(c, "\tMenu.Reset();\n\tSuper::EndPlay(Reason);", "\tMenu.Reset();\n\tFPKInputRouter::Unregister();\n\tSuper::EndPlay(Reason);", "cpp")
c = rep(c, "\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"PKMenuShots\")))\n\t\t{\n\t\t\tStartMenuSelfTest();\n\t\t}",
        "\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"PKMenuShots\")))\n\t\t{\n\t\t\tStartMenuSelfTest();\n\t\t}\n"
        "\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"PKVersusTest\")))\n\t\t{\n\t\t\tStartVersusSelfTest();\n\t\t}", "cpp")
c = rep(c, "void APKPlayerController::MenuKickOff()\n{\n\tHideMenu();",
        "void APKPlayerController::MenuKickOff()\n{\n\tTeamDevice[0] = TeamDevice[1] = PKDevice::Any;\n\tHideMenu();", "cpp")
c = rep(c, "void APKPlayerController::MenuSpectate()\n{",
        """void APKPlayerController::MenuVersus(int32 DeviceP1, int32 DeviceP2)
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
	FPKInputRouter::Get().Rumble(GM && GM->IsVersus() ? TeamDevice[Team & 1] : PKDevice::Any, Strength, Seconds);
}

void APKPlayerController::MenuSpectate()
{""", "cpp")

# device-filtered input helpers
c = rep(c, "FVector APKPlayerController::ReadMove() const\n",
        """bool APKPlayerController::DevDown(int32 Device, std::initializer_list<FKey> Keys) const
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

FVector APKPlayerController::ReadMove(int32 Device) const
{
	// Screen-relative for both players (one shared broadcast camera): right = +X, up = +Y.
	FVector Move(0.f);
	if (Device == PKDevice::Any || Device == PKDevice::Keyboard)
	{
		Move.X += (DevDown(Device, { EKeys::D, EKeys::Right }) ? 1.f : 0.f) - (DevDown(Device, { EKeys::A, EKeys::Left }) ? 1.f : 0.f);
		Move.Y += (DevDown(Device, { EKeys::W, EKeys::Up }) ? 1.f : 0.f) - (DevDown(Device, { EKeys::S, EKeys::Down }) ? 1.f : 0.f);
	}
	const FPKInputRouter& R = FPKInputRouter::Get();
	const FVector Stick(R.Axis(Device, EKeys::Gamepad_LeftX), R.Axis(Device, EKeys::Gamepad_LeftY), 0.f);
	if (Stick.Size() > StickDeadZone)
	{
		// Radial dead zone, rescaled so walking pace is still reachable.
		Move += Stick.GetSafeNormal() * FMath::GetMappedRangeValueClamped(FVector2D(StickDeadZone, 1.f), FVector2D(0.f, 1.f), Stick.Size());
	}
	return Move.GetClampedToMaxSize(1.f);
}

FVector APKPlayerController::ReadMove() const
""", "cpp")

# PlayerTick rewrite
start = c.index("void APKPlayerController::PlayerTick(float DeltaTime)")
end = c.index("void APKPlayerController::UpdateCamera(float DeltaTime)")
c = c[:start] + r"""void APKPlayerController::PlayerTick(float DeltaTime)
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
		if (GM->Phase != EPKPhase::FullTime && GM->Phase != EPKPhase::Menu && DevPressed(AnyDevice, { EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right }))
		{
			SetPause(true);
			ShowMenu(EPKMenuPage::Pause);
			return;
		}
	}

	if (DevPressed(AnyDevice, { EKeys::H }))
	{
		bShowHelp = !bShowHelp;
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

	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (GM->IsHumanTeam(Team))
		{
			HandlePlayer(GM, Team, DeltaTime);
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
	const bool bSprint = DevDown(Dev, { EKeys::LeftShift, EKeys::Gamepad_RightTrigger }) || Router.Axis(Dev, EKeys::Gamepad_RightTriggerAxis) > 0.3f;
	const bool bModifier = DevDown(Dev, { EKeys::LeftControl, EKeys::Gamepad_LeftTrigger }) || Router.Axis(Dev, EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	GM->HumanMove(Team, Move, bSprint, bModifier);

	const std::initializer_list<FKey> ShootKeys = { EKeys::K, EKeys::Gamepad_FaceButton_Right };
	bool& bCharging = bTeamCharging[Team];
	float& Charge = TeamCharge[Team];

	if (GM->HumanHasBall(Team))
	{
		if (DevPressed(Dev, { EKeys::J, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom }))
		{
			GM->HumanPass(Team, Move, EPKKickType::Ground);
		}
		else if (DevPressed(Dev, { EKeys::L, EKeys::Gamepad_FaceButton_Top }))
		{
			GM->HumanPass(Team, Move, EPKKickType::Through);
		}
		else if (DevPressed(Dev, { EKeys::I, EKeys::Gamepad_FaceButton_Left }))
		{
			GM->HumanPass(Team, Move, EPKKickType::Lob);
		}
		else if (DevPressed(Dev, { EKeys::F, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightThumbstick }))
		{
			GM->HumanKnockOn(Team, Move);
		}
		else if (DevPressed(Dev, { EKeys::G, EKeys::Gamepad_DPad_Down }))
		{
			GM->HumanDragBack(Team);
		}
		else if (DevPressed(Dev, { EKeys::T, EKeys::Gamepad_DPad_Up }))
		{
			GM->HumanFeint(Team, Move);
		}
		else if (DevPressed(Dev, { EKeys::V, EKeys::Gamepad_LeftThumbstick }))
		{
			GM->HumanFlickUp(Team, Move);
		}

		// Shot: hold to charge, release (or full charge) to shoot.
		if (DevPressed(Dev, ShootKeys))
		{
			bCharging = true;
			Charge = 0.f;
		}
		if (bCharging)
		{
			Charge = FMath::Min(1.f, Charge + DeltaTime / ShotChargeSeconds);
			if (DevReleased(Dev, ShootKeys) || !DevDown(Dev, ShootKeys) || Charge >= 1.f)
			{
				GM->HumanShoot(Team, Move, Charge);
				bCharging = false;
			}
		}
	}
	else
	{
		bCharging = false;
		if (DevPressed(Dev, { EKeys::E, EKeys::Gamepad_FaceButton_Right }))
		{
			GM->HumanTackle(Team);
		}
		if (DevPressed(Dev, { EKeys::C, EKeys::Gamepad_FaceButton_Left }))
		{
			GM->HumanSlideTackle(Team, Move);
		}
		if (DevPressed(Dev, { EKeys::Q, EKeys::Gamepad_LeftShoulder }))
		{
			GM->HumanSwitch(Team);
		}
		if (DevPressed(Dev, { EKeys::X, EKeys::Gamepad_RightShoulder }))
		{
			GM->HumanShoulder(Team);
		}
	}

	// Team play style: 1-5 on the keyboard, D-pad left / right on a gamepad.
	const FKey StyleKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 i = 0; i < 5; ++i)
	{
		if (Router.WasPressed(Dev, StyleKeys[i], InputSeq))
		{
			GM->SetTeamStyle(Team, (EPKTeamStyle)i);
		}
	}
	if (DevPressed(Dev, { EKeys::Gamepad_DPad_Right }))
	{
		GM->CycleTeamStyle(Team, 1);
	}
	if (DevPressed(Dev, { EKeys::Gamepad_DPad_Left }))
	{
		GM->CycleTeamStyle(Team, -1);
	}
}

""" + c[end:]
save("PKPlayerController.cpp", c)
print("versus pc ok")

# ---- self-test: two devices on one screen (-PKVersusTest)
c = load("PKPlayerController.cpp")
c += r'''
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
		const double Times[] = { 3.0, 4.0, 4.1, 5.0, 6.0, 6.1, 10.0, 12.0, 12.1, 14.0, 14.1, 15.0, 15.1, 16.0, 17.0, 18.0 };
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
		case 6: R.InjectKey(PKDevice::Keyboard, EKeys::D, true); break;          // P1 holds D (right)
		case 7: Report(TEXT("keyboard D held")); break;
		case 8: R.InjectKey(PKDevice::Keyboard, EKeys::D, false); R.InjectAxis(Pad, EKeys::Gamepad_LeftX, -1.f); break; // P2 stick left
		case 9: Report(TEXT("pad stick left")); break;
		case 10: Shot(TEXT("Versus_02_Match")); R.InjectAxis(Pad, EKeys::Gamepad_LeftX, 0.f); break;
		case 11: R.InjectKey(Pad, EKeys::Gamepad_Special_Right, true); break;    // P2 presses Start
		case 12:
			R.InjectKey(Pad, EKeys::Gamepad_Special_Right, false);
			UE_LOG(LogTemp, Log, TEXT("[VersusTest] after P2 Start: menu open %d"), PC->IsMenuOpen() ? 1 : 0);
			break;
		case 13: Shot(TEXT("Versus_03_Pause")); break;
		case 14: PC->MenuResume(); Report(TEXT("resumed")); break;
		default:
			FGenericPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}));
}
'''
save("PKPlayerController.cpp", c)
print("versus test ok")
