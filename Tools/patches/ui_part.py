import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def patch(name, pairs):
    path = os.path.join(base, name)
    s = open(path, encoding="utf-8-sig").read().replace("\r\n", "\n")
    for a, b in pairs:
        if a not in s:
            raise SystemExit("NOT FOUND in %s: %s" % (name, a[:100]))
        s = s.replace(a, b, 1)
    open(path, "w", encoding="utf-8").write(s)


# ---- volley hold + run timer with real delta
patch("PKGameMode.cpp", [
    ("\tAPKFootballer* P = Ball->GetController();\n\tif (!P || !P->IsKicking() || !Ball->IsFree() || Ball->GetActorLocation().Z > 40.f)\n\t{\n\t\treturn;\n\t}",
     "\tAPKFootballer* P = Ball->GetController();\n\tif (!P || !P->IsKicking() || !Ball->IsFree())\n\t{\n\t\treturn;\n\t}\n"
     "\tif (P->GetCurrentAction() == EPKAction::Volley)\n\t{\n"
     "\t\t// Hang the dropping ball where the overhead kick meets it.\n"
     "\t\tconst FVector Meet = P->GetFlatLocation() + P->GetFacing() * 40.f + FVector(0.f, 0.f, 120.f);\n"
     "\t\tBall->SetActorLocation(FMath::VInterpTo(Ball->GetActorLocation(), Meet, Dt, 10.f));\n"
     "\t\tBall->Velocity *= FMath::Max(0.f, 1.f - 10.f * Dt);\n\t\treturn;\n\t}\n"
     "\tif (Ball->GetActorLocation().Z > 40.f)\n\t{\n\t\treturn;\n\t}"),
    ("\tP->RunTimer = FMath::Max(0.f, P->RunTimer - 0.016f);\n\tP->AITargetTimer -= GetWorld()->GetDeltaSeconds();",
     "\tP->RunTimer = FMath::Max(0.f, P->RunTimer - GetWorld()->GetDeltaSeconds());\n\tP->AITargetTimer -= GetWorld()->GetDeltaSeconds();"),
])

# ---- controls
patch("PKPlayerController.cpp", [
    ("\t\telse if (AnyPressed({ EKeys::F, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightThumbstick }))\n\t\t{\n\t\t\tGM->HumanKnockOn(Move);\n\t\t}",
     "\t\telse if (AnyPressed({ EKeys::F, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightThumbstick }))\n\t\t{\n\t\t\tGM->HumanKnockOn(Move);\n\t\t}\n"
     "\t\telse if (AnyPressed({ EKeys::G, EKeys::Gamepad_DPad_Down }))\n\t\t{\n\t\t\tGM->HumanDragBack();\n\t\t}\n"
     "\t\telse if (AnyPressed({ EKeys::T, EKeys::Gamepad_DPad_Up }))\n\t\t{\n\t\t\tGM->HumanFeint(Move);\n\t\t}"),
    ("\tif (AnyPressed({ EKeys::H }))",
     "\t// Team play style: 1-5 or D-pad left / right.\n"
     "\tconst FKey StyleKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };\n"
     "\tfor (int32 i = 0; i < 5; ++i)\n\t{\n\t\tif (WasInputKeyJustPressed(StyleKeys[i]))\n\t\t{\n\t\t\tGM->SetTeamStyle(0, (EPKTeamStyle)i);\n\t\t}\n\t}\n"
     "\tif (AnyPressed({ EKeys::Gamepad_DPad_Right }))\n\t{\n\t\tGM->CycleTeamStyle(0, 1);\n\t}\n"
     "\tif (AnyPressed({ EKeys::Gamepad_DPad_Left }))\n\t{\n\t\tGM->CycleTeamStyle(0, -1);\n\t}\n\n"
     "\tif (AnyPressed({ EKeys::H }))"),
])

# ---- HUD: player card, styles, controls
patch("PKHUD.cpp", [
    ("\t\t\t\tTEXT(\"HOLD Ctrl / LT:  jockey (defending) - shield or close control (with ball)      H hide help\"),",
     "\t\t\t\tTEXT(\"HOLD Ctrl / LT:  jockey (defending) - shield or close control (with ball)\"),\n"
     "\t\t\t\tTEXT(\"SKILLS  drag-back G / D-down   feint T / D-up   first touch: stick = direction, Ctrl = cushion\"),\n"
     "\t\t\t\tTEXT(\"STYLE  1 Balanced  2 Possession  3 Counter  4 Long Ball  5 High Press  (D-pad L/R)    H hide help\"),"),
    ("\t\t\tfloat Y = Canvas->ClipY - 110.f;\n\t\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 12.f, Y - 8.f, 660.f, 92.f);",
     "\t\t\tfloat Y = Canvas->ClipY - 150.f;\n\t\t\tDrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 12.f, Y - 8.f, 700.f, 132.f);"),
])
path = os.path.join(base, "PKHUD.cpp")
s = open(path, encoding="utf-8-sig").read().replace("\r\n", "\n")
old = "\tDrawText(Clock, FLinearColor(1.f, 0.85f, 0.2f), 270.f, 30.f, Font, 1.3f);\n}"
if old not in s:
    raise SystemExit("NOT FOUND scoreboard")
s = s.replace(old, old[:-1] + r"""
	// Play styles under the score.
	UFont* Small = GEngine->GetSmallFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 20.f, 66.f, 330.f, 22.f);
	DrawText(FString::Printf(TEXT("%s  vs  %s"), *APKGameMode::StyleName(GM->GetTeamStyle(0)), *APKGameMode::StyleName(GM->GetTeamStyle(1))), FLinearColor(0.85f, 0.85f, 0.85f), 30.f, 69.f, Small, 1.f);

	// Controlled player card: ratings and traits.
	if (const APKFootballer* H = GM->GetHuman())
	{
		const FPKAttributes& A = H->Attr;
		const FString Line1 = FString::Printf(TEXT("#%d %s  OVR %d"), H->Index + 1, *H->RoleShort(), A.Overall());
		const FString Line2 = FString::Printf(TEXT("PAC %d  CTL %d  PAS %d  SHO %d  DEF %d  PHY %d"),
			FMath::RoundToInt(A.Pace * 100.f), FMath::RoundToInt(A.Control * 100.f), FMath::RoundToInt(A.Passing * 100.f),
			FMath::RoundToInt(A.Shooting * 100.f), FMath::RoundToInt(A.Defending * 100.f), FMath::RoundToInt(A.Physical * 100.f));
		const FString Traits = H->TraitNames();
		const float X = Canvas->ClipX - 380.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, 20.f, 360.f, Traits.IsEmpty() ? 50.f : 70.f);
		DrawRect(FLinearColor(1.f, 0.8f, 0.f), X, 20.f, 4.f, Traits.IsEmpty() ? 50.f : 70.f);
		DrawText(Line1, FLinearColor(1.f, 0.85f, 0.2f), X + 14.f, 26.f, GEngine->GetMediumFont(), 1.f);
		DrawText(Line2, FLinearColor::White, X + 14.f, 48.f, Small, 1.f);
		if (!Traits.IsEmpty())
		{
			DrawText(Traits, FLinearColor(0.6f, 0.85f, 1.f), X + 14.f, 68.f, Small, 1.f);
		}
	}
}""")
open(path, "w", encoding="utf-8").write(s)
print("ui ok")
