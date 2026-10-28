"""HUD for local 1v1 and controller-aware prompts; P2 marker colour; router clears keys on focus loss."""
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


s = load("PKHUD.cpp")
s = rep(s, '#include "PKTypes.h"\n', '#include "PKTypes.h"\n#include "PKInput.h"\n', "hud")

# versus label under the scoreboard
s = rep(s, "\tif (GM->IsSpectating())\n\t{\n\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 20.f, 92.f, 330.f, 22.f);",
        "\tconst APKPlayerController* OwnerPC = Cast<APKPlayerController>(GetOwningPlayerController());\n"
        "\tif (GM->IsVersus() && OwnerPC)\n\t{\n"
        "\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 20.f, 92.f, 330.f, 22.f);\n"
        "\t\tDrawText(FString::Printf(TEXT(\"P1 %s\"), *FPKInputRouter::DeviceName(OwnerPC->GetTeamDevice(0))), FLinearColor(1.f, 0.85f, 0.2f), 30.f, 95.f, GEngine->GetSmallFont(), 1.f);\n"
        "\t\tDrawText(FString::Printf(TEXT(\"P2 %s\"), *FPKInputRouter::DeviceName(OwnerPC->GetTeamDevice(1))), FLinearColor(0.3f, 0.9f, 1.f), 190.f, 95.f, GEngine->GetSmallFont(), 1.f);\n"
        "\t}\n"
        "\tif (GM->IsSpectating())\n\t{\n\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 20.f, 92.f, 330.f, 22.f);", "hud")

# tags: P1 gold, P2 cyan
s = rep(s, "\t\tconst bool bIsHuman = P == GM->GetHuman();\n",
        "\t\tconst bool bIsHuman = GM->IsHumanTeam(P->Team) && P == GM->GetHuman(P->Team);\n"
        "\t\tconst bool bP2 = bIsHuman && P->Team == 1;\n"
        "\t\tconst TCHAR* Tag = bP2 ? TEXT(\"P2\") : TEXT(\"P1\");\n"
        "\t\tconst FLinearColor TagColor = bP2 ? FLinearColor(0.3f, 0.9f, 1.f) : FLinearColor(1.f, 0.85f, 0.f);\n", "hud")
s = rep(s, "\t\tconst FLinearColor Back = bIsHuman ? FLinearColor(1.f, 0.8f, 0.f, 0.95f) :",
        "\t\tconst FLinearColor Back = bIsHuman ? FLinearColor(TagColor.R, TagColor.G * 0.95f, TagColor.B, 0.95f) :", "hud")
s = rep(s, "\t\t\tGetTextSize(TEXT(\"P1\"), PW, PH, GEngine->GetSmallFont(), 1.f);\n\t\t\tDrawText(TEXT(\"P1\"), FLinearColor(1.f, 0.85f, 0.f),",
        "\t\t\tGetTextSize(Tag, PW, PH, GEngine->GetSmallFont(), 1.f);\n\t\t\tDrawText(Tag, TagColor,", "hud")

# shot power + help: per player, prompts for the device they use
start = s.index("\t// Shot power bar.\n")
end = s.index("void APKHUD::DrawScoreboard(APKGameMode* GM)")
s = s[:start] + r"""	// Shot power bars (one per human player) and the controls panel.
	if (const APKPlayerController* PC = Cast<APKPlayerController>(GetOwningPlayerController()))
	{
		const bool bVersus = GM->IsVersus();
		for (int32 Team = 0; Team < (bVersus ? 2 : 1); ++Team)
		{
			const float Charge = PC->GetShotCharge(Team);
			if (Charge <= 0.f)
			{
				continue;
			}
			const float W = 260.f;
			const float X = bVersus ? (Team == 0 ? Canvas->ClipX * 0.3f - W * 0.5f : Canvas->ClipX * 0.7f - W * 0.5f) : (Canvas->ClipX - W) * 0.5f;
			const float Y = Canvas->ClipY - 230.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 3.f, Y - 3.f, W + 6.f, 22.f);
			DrawRect(FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.9f, 0.1f), FLinearColor(1.f, 0.1f, 0.05f), Charge), X, Y, W * Charge, 16.f);
			DrawText(bVersus ? (Team == 0 ? TEXT("P1 SHOT POWER") : TEXT("P2 SHOT POWER")) : TEXT("SHOT POWER"), FLinearColor::White, X, Y - 22.f, GEngine->GetSmallFont(), 1.f);
		}

		if (PC->bShowHelp)
		{
			// Prompts follow the device: keyboard keys, or gamepad buttons once a gamepad is used.
			static const TCHAR* KeyboardLines[] = {
				TEXT("MOVE  WASD / arrows    SPRINT  Shift    HOLD  Ctrl (jockey / shield / close control)"),
				TEXT("PASS  J / Space    THROUGH  L    LOB / CROSS  I    SHOOT  hold K, release"),
				TEXT("TACKLE  E    SLIDE  C    SWITCH  Q    SHOULDER BARGE  X    KNOCK-ON  F"),
				TEXT("SKILLS  drag-back G    feint T    flick-up V"),
				TEXT("STYLE  1-5    PAUSE  Esc / P    H hide help    N numbers"),
			};
			static const TCHAR* GamepadLines[] = {
				TEXT("MOVE  left stick    SPRINT  RT    HOLD  LT (jockey / shield / close control)"),
				TEXT("PASS  A    THROUGH  Y    LOB / CROSS  X    SHOOT  hold B, release"),
				TEXT("TACKLE  B    SLIDE  X    SWITCH  LB    SHOULDER BARGE  RB    KNOCK-ON  RB / R3"),
				TEXT("SKILLS  drag-back D-down    feint D-up    flick-up L3"),
				TEXT("STYLE  D-pad left / right    PAUSE  Start"),
			};
			const FPKInputRouter& Router = FPKInputRouter::Get();
			auto DrawPanel = [&](float X, const FString& Header, const FLinearColor& HeaderColor, int32 Device, float Width)
			{
				const bool bPad = PKDevice::IsGamepad(Device == PKDevice::Any ? Router.LastActiveDevice() : Device);
				const TCHAR** Lines = bPad ? GamepadLines : KeyboardLines;
				float Y = Canvas->ClipY - 150.f;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X, Y - 30.f, Width, 140.f);
				DrawText(Header, HeaderColor, X + 8.f, Y - 24.f, GEngine->GetSmallFont(), 1.f);
				for (int32 i = 0; i < 5; ++i)
				{
					DrawText(Lines[i], FLinearColor(0.95f, 0.95f, 0.95f), X + 8.f, Y, GEngine->GetSmallFont(), 1.f);
					Y += 20.f;
				}
			};
			if (bVersus)
			{
				DrawPanel(12.f, FString::Printf(TEXT("P1  BLUE  -  %s"), *FPKInputRouter::DeviceName(PC->GetTeamDevice(0))), FLinearColor(1.f, 0.85f, 0.2f), PC->GetTeamDevice(0), 560.f);
				DrawPanel(584.f, FString::Printf(TEXT("P2  RED  -  %s"), *FPKInputRouter::DeviceName(PC->GetTeamDevice(1))), FLinearColor(0.3f, 0.9f, 1.f), PC->GetTeamDevice(1), 560.f);
			}
			else
			{
				const bool bPad = PKDevice::IsGamepad(Router.LastActiveDevice());
				DrawPanel(12.f, bPad ? TEXT("CONTROLS  -  GAMEPAD") : TEXT("CONTROLS  -  KEYBOARD   (prompts switch when you use a gamepad)"), FLinearColor(1.f, 0.85f, 0.2f), PKDevice::Any, 640.f);
			}
		}
	}
}

""" + s[end:]

# second player card (P1 card stays exactly as it was)
s = rep(s, "\t\tif (!Traits.IsEmpty())\n\t\t{\n\t\t\tDrawText(Traits, FLinearColor(0.6f, 0.85f, 1.f), X + 14.f, 68.f, Small, 1.f);\n\t\t}\n\t}\n}",
        "\t\tif (!Traits.IsEmpty())\n\t\t{\n\t\t\tDrawText(Traits, FLinearColor(0.6f, 0.85f, 1.f), X + 14.f, 68.f, Small, 1.f);\n\t\t}\n\t}\n"
        "\t// Local 1v1: player 2's card under it, cyan.\n"
        "\tif (const APKFootballer* H2 = GM->IsVersus() ? GM->GetHuman(1) : nullptr)\n\t{\n"
        "\t\tconst FPKAttributes& A = H2->Attr;\n"
        "\t\tconst FString Line1 = FString::Printf(TEXT(\"P2  #%d %s  OVR %d\"), H2->ShirtNumber, *H2->RoleShort(), A.Overall());\n"
        "\t\tconst FString Line2 = FString::Printf(TEXT(\"PAC %d  CTL %d  PAS %d  SHO %d  DEF %d  PHY %d\"),\n"
        "\t\t\tFMath::RoundToInt(A.Pace * 100.f), FMath::RoundToInt(A.Control * 100.f), FMath::RoundToInt(A.Passing * 100.f),\n"
        "\t\t\tFMath::RoundToInt(A.Shooting * 100.f), FMath::RoundToInt(A.Defending * 100.f), FMath::RoundToInt(A.Physical * 100.f));\n"
        "\t\tconst float X = Canvas->ClipX - 380.f;\n\t\tconst float Y = 100.f;\n"
        "\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, 360.f, 50.f);\n"
        "\t\tDrawRect(FLinearColor(0.3f, 0.9f, 1.f), X, Y, 4.f, 50.f);\n"
        "\t\tDrawText(Line1, FLinearColor(0.3f, 0.9f, 1.f), X + 14.f, Y + 6.f, GEngine->GetMediumFont(), 1.f);\n"
        "\t\tDrawText(Line2, FLinearColor::White, X + 14.f, Y + 28.f, Small, 1.f);\n\t}\n}", "hud")
s = rep(s, "\t\tconst FLinearColor C = P == GM->GetHuman() ? FLinearColor(1.f, 0.85f, 0.f) : PK::TeamColor(P->Team, P->IsKeeper());",
        "\t\tconst bool bControlled = GM->IsHumanTeam(P->Team) && P == GM->GetHuman(P->Team);\n"
        "\t\tconst FLinearColor C = bControlled ? (P->Team == 0 ? FLinearColor(1.f, 0.85f, 0.f) : FLinearColor(0.3f, 0.9f, 1.f)) : PK::TeamColor(P->Team, P->IsKeeper());", "hud")
save("PKHUD.cpp", s)

# P2 selection ring is cyan
f = load("PKFootballer.cpp")
f = rep(f, "\tPK::Paint(Marker, SurfaceMaterial, FLinearColor(1.f, 0.8f, 0.f), 0.5f, 6.f);",
        "\tPK::Paint(Marker, SurfaceMaterial, Team == 0 ? FLinearColor(1.f, 0.8f, 0.f) : FLinearColor(0.2f, 0.85f, 1.f), 0.5f, 6.f);", "fb")
save("PKFootballer.cpp", f)

# router: forget held keys when the window loses focus (alt-tab would leave keys stuck)
r = load("PKInput.cpp")
r = rep(r, "void FPKInputRouter::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)\n{\n",
        "void FPKInputRouter::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)\n{\n"
        "\tif (!SlateApp.IsActive())\n\t{\n\t\tfor (auto& Pair : Devices)\n\t\t{\n\t\t\tPair.Value.Down.Reset();\n\t\t\tPair.Value.Axes.Reset();\n\t\t}\n\t}\n", "router")
save("PKInput.cpp", r)
print("versus hud ok")
