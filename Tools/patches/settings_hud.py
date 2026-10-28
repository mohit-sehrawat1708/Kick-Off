"""HUD + game mode for the settings: radar matches the camera view, FPS counter, help/prompt text from
the key bindings, match length and difficulty from the settings."""
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


s = load("PKHUD.cpp")
s = rep(s, '#include "PKInput.h"\n', '#include "PKInput.h"\n#include "PKSettings.h"\n#include "PKGameInstance.h"\n#include "Misc/App.h"\n')

# radar: the broadcast camera looks along +Y, so world +X is screen-left; draw it the way it is seen
s = rep(s, "\t\treturn FVector2D(X0 + (L.X / PK::HalfLength * 0.5f + 0.5f) * W, Y0 + (0.5f - L.Y / PK::HalfWidth * 0.5f) * H);",
        "\t\treturn FVector2D(X0 + (0.5f - L.X / PK::HalfLength * 0.5f) * W, Y0 + (0.5f - L.Y / PK::HalfWidth * 0.5f) * H);")

# radar toggle + FPS counter
s = rep(s, "\tDrawScoreboard(GM);\n\tDrawRadar(GM);",
        "\tDrawScoreboard(GM);\n"
        "\tconst UPKGameInstance* SettingsGI = GetGameInstance<UPKGameInstance>();\n"
        "\tif (!SettingsGI || SettingsGI->Settings.bShowRadar)\n\t{\n\t\tDrawRadar(GM);\n\t}\n"
        "\tif (SettingsGI && SettingsGI->Settings.bShowFPS)\n\t{\n"
        "\t\tstatic float SmoothedFps = 60.f;\n"
        "\t\tconst float Dt = FMath::Max((float)FApp::GetDeltaTime(), 0.0001f);\n"
        "\t\tSmoothedFps = FMath::Lerp(SmoothedFps, 1.f / Dt, 0.05f);\n"
        "\t\tconst FString Fps = FString::Printf(TEXT(\"%.0f FPS\"), SmoothedFps);\n"
        "\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Canvas->ClipX * 0.5f - 46.f, 18.f, 92.f, 24.f);\n"
        "\t\tDrawText(Fps, SmoothedFps >= 50.f ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor(1.f, 0.6f, 0.2f), Canvas->ClipX * 0.5f - 34.f, 21.f, GEngine->GetSmallFont(), 1.1f);\n\t}")

# dead-ball prompt: the player's own keys
s = rep(s, "\t\t\tconst TCHAR* Pass = bPad ? TEXT(\"A\") : TEXT(\"J / Space\");\n\t\t\tconst TCHAR* Lob = bPad ? TEXT(\"X\") : TEXT(\"I\");\n\t\t\tconst TCHAR* Shoot = bPad ? TEXT(\"hold B\") : TEXT(\"hold K\");\n",
        "\t\t\tconst UPKGameInstance* PromptGI = GetGameInstance<UPKGameInstance>();\n"
        "\t\t\tconst FPKUserSettings Keys = PromptGI ? PromptGI->Settings : FPKUserSettings();\n"
        "\t\t\tconst FString PassText = bPad ? PKInputs::PadText(EPKInput::Pass) : Keys.KeyText(EPKInput::Pass);\n"
        "\t\t\tconst FString LobText = bPad ? PKInputs::PadText(EPKInput::Lob) : Keys.KeyText(EPKInput::Lob);\n"
        "\t\t\tconst FString ShootText = TEXT(\"hold \") + (bPad ? PKInputs::PadText(EPKInput::Shoot) : Keys.KeyText(EPKInput::Shoot));\n"
        "\t\t\tconst TCHAR* Pass = *PassText;\n\t\t\tconst TCHAR* Lob = *LobText;\n\t\t\tconst TCHAR* Shoot = *ShootText;\n")

# help panel: keyboard lines from the bindings
s = rep(s, """			static const TCHAR* KeyboardLines[] = {
				TEXT("MOVE  WASD / arrows    SPRINT  Shift    HOLD  Ctrl (jockey / shield / close control)"),
				TEXT("PASS  J / Space    THROUGH  L    LOB / CROSS  I    SHOOT  hold K, release"),
				TEXT("TACKLE  E    SLIDE  C    SWITCH  Q    SHOULDER BARGE  X    KNOCK-ON  F"),
				TEXT("SKILLS  drag-back G    feint T    flick-up V"),
				TEXT("STYLE  1-5    PAUSE  Esc / P    H hide help    N numbers"),
			};""", """			const UPKGameInstance* HelpGI = GetGameInstance<UPKGameInstance>();
			const FPKUserSettings K = HelpGI ? HelpGI->Settings : FPKUserSettings();
			const FString KeyLine[] = {
				FString::Printf(TEXT("MOVE  %s, %s, %s, %s    SPRINT  %s    HOLD  %s"), *K.KeyText(EPKInput::MoveUp), *K.KeyText(EPKInput::MoveLeft), *K.KeyText(EPKInput::MoveDown), *K.KeyText(EPKInput::MoveRight), *K.KeyText(EPKInput::Sprint), *K.KeyText(EPKInput::Hold)),
				FString::Printf(TEXT("PASS  %s    THROUGH  %s    LOB / CROSS  %s    SHOOT  hold %s, release"), *K.KeyText(EPKInput::Pass), *K.KeyText(EPKInput::Through), *K.KeyText(EPKInput::Lob), *K.KeyText(EPKInput::Shoot)),
				FString::Printf(TEXT("TACKLE  %s    SLIDE  %s    SWITCH  %s    SHOULDER BARGE  %s    KNOCK-ON  %s"), *K.KeyText(EPKInput::Tackle), *K.KeyText(EPKInput::Slide), *K.KeyText(EPKInput::Switch), *K.KeyText(EPKInput::Barge), *K.KeyText(EPKInput::KnockOn)),
				FString::Printf(TEXT("SKILLS  drag-back %s    feint %s    flick-up %s"), *K.KeyText(EPKInput::DragBack), *K.KeyText(EPKInput::Feint), *K.KeyText(EPKInput::FlickUp)),
				FString::Printf(TEXT("STYLE  1-5    PAUSE  %s    H hide help    N numbers    (change keys in SETTINGS)"), *K.KeyText(EPKInput::Pause)),
			};
			const TCHAR* KeyboardLines[] = { *KeyLine[0], *KeyLine[1], *KeyLine[2], *KeyLine[3], *KeyLine[4] };""")
s = rep(s, "\t\t\t\tconst TCHAR** Lines = bPad ? GamepadLines : KeyboardLines;", "\t\t\t\tconst TCHAR* const* Lines = bPad ? GamepadLines : KeyboardLines;")
save("PKHUD.cpp", s)

g = load("PKGameMode.cpp")
g = rep(g, "\tTimeLeft = bAutoTest ? 150.f : PK::MatchSeconds;",
        "\tconst UPKGameInstance* SettingsGI = GetGameInstance<UPKGameInstance>();\n"
        "\tTimeLeft = bAutoTest ? 150.f : (SettingsGI ? SettingsGI->Settings.MatchMinutes * 60.f : PK::MatchSeconds);")
g = rep(g, "\t\t\tP->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, PKTeams::AttributesInSlot(Pl, Slot.Pos), Pl.Traits, Pl.Number, Pl.Name);",
        "\t\t\tFPKAttributes Attr = PKTeams::AttributesInSlot(Pl, Slot.Pos);\n"
        "\t\t\t// Difficulty scales the computer team against a single player (not in 1v1 or spectate).\n"
        "\t\t\tif (Team == 1 && !bHumanTeam[1] && !bSpectate && !bAutoTest)\n\t\t\t{\n"
        "\t\t\t\tconst float Scale = GI->Settings.Difficulty == 0 ? 0.85f : (GI->Settings.Difficulty == 2 ? 1.12f : 1.f);\n"
        "\t\t\t\tfor (float* V : { &Attr.Pace, &Attr.Control, &Attr.Passing, &Attr.Shooting, &Attr.Defending, &Attr.Physical })\n\t\t\t\t{\n"
        "\t\t\t\t\t*V = FMath::Clamp(*V * Scale, 0.3f, 0.97f);\n\t\t\t\t}\n\t\t\t}\n"
        "\t\t\tP->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name);")
save("PKGameMode.cpp", g)
print("settings hud ok")
