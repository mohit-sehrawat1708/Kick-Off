"""Player controller: key bindings from the settings, camera-relative movement (fixes mirrored left/right)."""
import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def load(p):
    return open(os.path.join(base, p), encoding="utf-8").read()


def save(p, s):
    open(os.path.join(base, p), "w", encoding="utf-8").write(s)


def rep(s, a, b):
    if a not in s:
        raise SystemExit("NOT FOUND: " + a[:100])
    return s.replace(a, b, 1)


h = load("PKPlayerController.h")
h = rep(h, "\tvoid MenuVersus(int32 DeviceP1, int32 DeviceP2);",
        "\tvoid MenuVersus(int32 DeviceP1, int32 DeviceP2);\n"
        "\t// Re-read the saved settings (HUD toggles, camera zoom) after the settings page changes them.\n"
        "\tvoid ApplyUserSettings();\n"
        "\tvoid OpenSettingsFromPause();")
h = rep(h, "\tFVector ReadMove(int32 Device) const;\n",
        "\tFVector ReadMove(int32 Device) const;\n"
        "\tTArray<FKey> ActionKeys(int32 Device, enum class EPKInput Action) const;\n"
        "\tbool ActionDown(int32 Device, EPKInput Action) const;\n"
        "\tbool ActionPressed(int32 Device, EPKInput Action) const;\n"
        "\tbool ActionReleased(int32 Device, EPKInput Action) const;\n"
        "\tconst struct FPKUserSettings* UserSettings() const;\n")
h = rep(h, "\tuint64 InputSeq = 0;", "\tuint64 InputSeq = 0;\n\tfloat CameraZoomScale = 1.f;")
save("PKPlayerController.h", h)

c = load("PKPlayerController.cpp")
c = rep(c, '#include "PKInput.h"\n', '#include "PKInput.h"\n#include "PKSettings.h"\n')
c = rep(c, "\tFPKInputRouter::Get().Rumble(GM && GM->IsVersus() ? TeamDevice[Team & 1] : PKDevice::Any, Strength, Seconds);",
        "\tconst FPKUserSettings* S = UserSettings();\n\tconst float Scale = S ? S->Rumble : 1.f;\n\tif (Scale <= 0.f)\n\t{\n\t\treturn;\n\t}\n"
        "\tFPKInputRouter::Get().Rumble(GM && GM->IsVersus() ? TeamDevice[Team & 1] : PKDevice::Any, Strength * Scale, Seconds);")

# replace the old ReadMove(int32) body with the binding / camera-relative one
start = c.index("FVector APKPlayerController::ReadMove(int32 Device) const\n")
end = c.index("FVector APKPlayerController::ReadMove() const")
c = c[:start] + r"""const FPKUserSettings* APKPlayerController::UserSettings() const
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

""" + c[end:]

c = rep(c, "\t\tif (GM->Phase != EPKPhase::FullTime && GM->Phase != EPKPhase::Menu && DevPressed(AnyDevice, { EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right }))",
        "\t\tif (GM->Phase != EPKPhase::FullTime && GM->Phase != EPKPhase::Menu && ActionPressed(AnyDevice, EPKInput::Pause))")
c = rep(c, "\tif (DevPressed(AnyDevice, { EKeys::H }))\n\t{\n\t\tbShowHelp = !bShowHelp;\n\t}",
        "\tif (DevPressed(AnyDevice, { EKeys::H }))\n\t{\n\t\tbShowHelp = !bShowHelp;\n"
        "\t\tif (UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())\n\t\t{\n\t\t\tGI->Settings.bShowHelp = bShowHelp;\n\t\t}\n\t}")
pairs = [
    ("const bool bSprint = DevDown(Dev, { EKeys::LeftShift, EKeys::Gamepad_RightTrigger }) ||", "const bool bSprint = ActionDown(Dev, EPKInput::Sprint) ||"),
    ("const bool bModifier = DevDown(Dev, { EKeys::LeftControl, EKeys::Gamepad_LeftTrigger }) ||", "const bool bModifier = ActionDown(Dev, EPKInput::Hold) ||"),
    ("\tconst std::initializer_list<FKey> ShootKeys = { EKeys::K, EKeys::Gamepad_FaceButton_Right };\n", ""),
    ("DevPressed(Dev, { EKeys::J, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })", "ActionPressed(Dev, EPKInput::Pass)"),
    ("DevPressed(Dev, { EKeys::L, EKeys::Gamepad_FaceButton_Top })", "ActionPressed(Dev, EPKInput::Through)"),
    ("DevPressed(Dev, { EKeys::I, EKeys::Gamepad_FaceButton_Left })", "ActionPressed(Dev, EPKInput::Lob)"),
    ("DevPressed(Dev, { EKeys::F, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightThumbstick })", "ActionPressed(Dev, EPKInput::KnockOn)"),
    ("DevPressed(Dev, { EKeys::G, EKeys::Gamepad_DPad_Down })", "ActionPressed(Dev, EPKInput::DragBack)"),
    ("DevPressed(Dev, { EKeys::T, EKeys::Gamepad_DPad_Up })", "ActionPressed(Dev, EPKInput::Feint)"),
    ("DevPressed(Dev, { EKeys::V, EKeys::Gamepad_LeftThumbstick })", "ActionPressed(Dev, EPKInput::FlickUp)"),
    ("\t\tif (DevPressed(Dev, ShootKeys))", "\t\tif (ActionPressed(Dev, EPKInput::Shoot))"),
    ("\t\t\tif (DevReleased(Dev, ShootKeys) || !DevDown(Dev, ShootKeys) || Charge >= 1.f)", "\t\t\tif (ActionReleased(Dev, EPKInput::Shoot) || !ActionDown(Dev, EPKInput::Shoot) || Charge >= 1.f)"),
    ("DevPressed(Dev, { EKeys::E, EKeys::Gamepad_FaceButton_Right })", "ActionPressed(Dev, EPKInput::Tackle)"),
    ("DevPressed(Dev, { EKeys::C, EKeys::Gamepad_FaceButton_Left })", "ActionPressed(Dev, EPKInput::Slide)"),
    ("DevPressed(Dev, { EKeys::Q, EKeys::Gamepad_LeftShoulder })", "ActionPressed(Dev, EPKInput::Switch)"),
    ("DevPressed(Dev, { EKeys::X, EKeys::Gamepad_RightShoulder })", "ActionPressed(Dev, EPKInput::Barge)"),
]
for a, b in pairs:
    c = rep(c, a, b)
c = rep(c, "\t\t\t-PK::HalfWidth - CameraBack + CameraFocus.Y * 0.2f,\n\t\t\tCameraHeight);",
        "\t\t\t-PK::HalfWidth - CameraBack * CameraZoomScale + CameraFocus.Y * 0.2f,\n\t\t\tCameraHeight * CameraZoomScale);")
c = rep(c, "\tSetInputMode(FInputModeGameOnly());\n\tFPKInputRouter::Register();\n", "\tSetInputMode(FInputModeGameOnly());\n\tFPKInputRouter::Register();\n\tApplyUserSettings();\n")
save("PKPlayerController.cpp", c)
print("settings pc ok")
