"""Game mode: one human-controlled player per team (local 1v1), rumble hooks."""
import os
import re
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def load(name):
    return open(os.path.join(base, name), encoding="utf-8-sig").read().replace("\r\n", "\n")


def save(name, s):
    open(os.path.join(base, name), "w", encoding="utf-8").write(s)


def rep(s, a, b, name):
    if a not in s:
        raise SystemExit("NOT FOUND in %s: %s" % (name, a[:110]))
    return s.replace(a, b, 1)


# ---------------------------------------------------------------- header
h = load("PKGameMode.h")
for fn in ["HumanMove(", "HumanPass(", "HumanShoot(", "HumanSlideTackle(", "HumanKnockOn(", "HumanFeint(", "HumanFlickUp("]:
    h = rep(h, "\tvoid " + fn, "\tvoid " + fn + "int32 Team, ", "h")
for fn in ["HumanTackle()", "HumanSwitch()", "HumanDragBack()", "HumanShoulder()"]:
    h = rep(h, "\tvoid " + fn, "\tvoid " + fn.replace("()", "(int32 Team)"), "h")
h = rep(h, "\tbool HumanHasBall() const;", "\tbool HumanHasBall(int32 Team = 0) const;", "h")
h = rep(h, "\tAPKFootballer* GetHuman() const { return Human; }",
        "\tAPKFootballer* GetHuman() const { return Humans[0]; }\n"
        "\tAPKFootballer* GetHuman(int32 Team) const { return Humans[FMath::Clamp(Team, 0, 1)]; }\n"
        "\tbool IsHumanTeam(int32 Team) const { return bHumanTeam[FMath::Clamp(Team, 0, 1)]; }\n"
        "\tbool IsVersus() const { return bHumanTeam[0] && bHumanTeam[1] && !bSpectate; }\n"
        "\t// Controller feedback for the player controlling a team.\n"
        "\tvoid Rumble(int32 Team, float Strength, float Seconds);", "h")
h = rep(h, "\tTObjectPtr<APKFootballer> Human;", "\tTObjectPtr<APKFootballer> Humans[2];\n\tbool bHumanTeam[2] = { true, false }; // which teams a person controls", "h")
h = rep(h, "\tvoid StartMatch(bool bSpectateMatch);", "\tvoid StartMatch(bool bSpectateMatch, bool bVersus = false);", "h")
save("PKGameMode.h", h)

# ---------------------------------------------------------------- source
s = load("PKGameMode.cpp")

# Human* API: team parameter, local "Human" = that team's controlled player.
human_fns = ["HumanMove", "HumanPass", "HumanShoot", "HumanTackle", "HumanSlideTackle", "HumanSwitch",
             "HumanKnockOn", "HumanDragBack", "HumanFeint", "HumanFlickUp", "HumanShoulder"]
for fn in human_fns:
    m = re.search(r"void APKGameMode::%s\(([^)]*)\)\n\{\n" % fn, s)
    if not m:
        raise SystemExit("def not found " + fn)
    params = m.group(1)
    new_params = "int32 Team" + (", " + params if params.strip() else "")
    s = s[:m.start()] + "void APKGameMode::%s(%s)\n{\n\tAPKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];\n" % (fn, new_params) + s[m.end():]

s = rep(s, "bool APKGameMode::HumanHasBall() const\n{\n\treturn Human && Ball && Ball->GetOwnerPlayer() == Human;",
        "bool APKGameMode::HumanHasBall(int32 Team) const\n{\n\tconst APKFootballer* Human = Humans[FMath::Clamp(Team, 0, 1)];\n\treturn Human && Ball && Ball->GetOwnerPlayer() == Human;", "cpp")
s = s.replace("!HumanHasBall()", "!HumanHasBall(Team)")
s = rep(s, "\t\tSetHuman(ClosestOutfield(Human->Team, Ball->GetActorLocation(), Human));",
        "\t\tSetHuman(ClosestOutfield(Human->Team, Ball->GetActorLocation(), Human));", "cpp")

# kick-off: each human team starts on its kicker / nearest man
s = rep(s, "\tif (Kicker && Kicker->Team == 0)\n\t{\n\t\tSetHuman(Kicker);\n\t}\n\telse\n\t{\n\t\tSetHuman(ClosestOutfield(0, FVector::ZeroVector, nullptr));\n\t}",
        "\tfor (int32 T = 0; T < 2; ++T)\n\t{\n\t\tif (bHumanTeam[T])\n\t\t{\n"
        "\t\t\tSetHuman(Kicker && Kicker->Team == T ? Kicker : ClosestOutfield(T, FVector::ZeroVector, nullptr));\n\t\t}\n\t}", "cpp")

# spectate auto-switch
s = rep(s, "\t\tif (bSpectate && Human && Ball->GetOwnerPlayer() != Human)", "\t\tAPKFootballer* Human = Humans[0];\n\t\tif (bSpectate && Human && Ball->GetOwnerPlayer() != Human)", "cpp")

# AI skips every human-controlled player
s = rep(s, "\t\tif (P == Human && !bSpectate)\n", "\t\tif (P->bHuman && !bSpectate)\n", "cpp")

# passes and receptions: the controlled player follows the ball for any human team
s = rep(s, "\tif (bStarted && P->Team == 0 && !Target->IsKeeper())", "\tif (bStarted && bHumanTeam[P->Team] && !Target->IsKeeper())", "cpp")
s = rep(s, "\tif (P->Team == 0 && !P->IsKeeper())\n\t{\n\t\tSetHuman(P);\n\t}\n\telse if (P->Team == 1 && Human && FVector::Dist(Human->GetFlatLocation(), P->GetFlatLocation()) > 1500.f)\n\t{\n\t\tSetHuman(ClosestOutfield(0, P->GetFlatLocation(), nullptr));\n\t}",
        "\tif (bHumanTeam[P->Team] && !P->IsKeeper())\n\t{\n\t\tSetHuman(P);\n\t}\n"
        "\tconst int32 Other = 1 - P->Team;\n"
        "\tif (bHumanTeam[Other] && Humans[Other] && FVector::Dist(Humans[Other]->GetFlatLocation(), P->GetFlatLocation()) > 1500.f)\n\t{\n"
        "\t\tSetHuman(ClosestOutfield(Other, P->GetFlatLocation(), nullptr)); // defending: jump to the man nearest the ball\n\t}", "cpp")
s = rep(s, "\tif (Team == 0 && !Taker->IsKeeper())\n\t{\n\t\tSetHuman(Taker);\n\t}", "\tif (bHumanTeam[Team] && !Taker->IsKeeper())\n\t{\n\t\tSetHuman(Taker);\n\t}", "cpp")
s = rep(s, "if (TryStartKick(P, Kick) && P->Team == 0 && Kick.Receiver.IsValid()", "if (TryStartKick(P, Kick) && bHumanTeam[P->Team] && Kick.Receiver.IsValid()", "cpp")
s = rep(s, "\tTaker->bHoldPosition = false;\n\tif (Team == 0)\n\t{\n\t\tSetHuman(Taker);\n\t}", "\tTaker->bHoldPosition = false;\n\tif (bHumanTeam[Team])\n\t{\n\t\tSetHuman(Taker);\n\t}", "cpp")

# SetHuman per team
s = rep(s, """void APKGameMode::SetHuman(APKFootballer* P)
{
	if (bAutoTest || !P || P == Human)
	{
		return;
	}
	if (Human)
	{
		Human->SetHumanControlled(false);
	}
	Human = P;
	Human->SetHumanControlled(true);
}""", """void APKGameMode::SetHuman(APKFootballer* P)
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
}""", "cpp")

# send-off
s = rep(s, "\tif (Human == P)\n\t{\n\t\tP->SetHumanControlled(false);\n\t\tHuman = nullptr;\n\t}",
        "\tif (Humans[P->Team] == P)\n\t{\n\t\tP->SetHumanControlled(false);\n\t\tHumans[P->Team] = nullptr;\n\t}", "cpp")
s = rep(s, "\tif (!Human)\n\t{\n\t\tSetHuman(ClosestOutfield(0, Ball->GetActorLocation(), nullptr));\n\t}\n\tUE_LOG(LogPitchKings, Log, TEXT(\"SENT OFF",
        "\tif (bHumanTeam[P->Team] && !Humans[P->Team])\n\t{\n\t\tSetHuman(ClosestOutfield(P->Team, Ball->GetActorLocation(), nullptr));\n\t}\n\tUE_LOG(LogPitchKings, Log, TEXT(\"SENT OFF", "cpp")

# menu / match start: who is human
s = rep(s, "\t// Front end: both teams lined up for kick-off behind the menu, nothing moves.\n\tbSpectate = false;",
        "\t// Front end: both teams lined up for kick-off behind the menu, nothing moves.\n\tbSpectate = false;\n"
        "\tbHumanTeam[0] = true;\n\tbHumanTeam[1] = false;\n"
        "\tif (Humans[1])\n\t{\n\t\tHumans[1]->SetHumanControlled(false);\n\t\tHumans[1] = nullptr;\n\t}", "cpp")
s = rep(s, "void APKGameMode::StartMatch(bool bSpectateMatch)\n{\n\tbSpectate = bSpectateMatch;",
        "void APKGameMode::StartMatch(bool bSpectateMatch, bool bVersus)\n{\n\tbSpectate = bSpectateMatch;\n"
        "\tbHumanTeam[0] = true;\n\tbHumanTeam[1] = bVersus && !bSpectateMatch;\n"
        "\tif (!bHumanTeam[1] && Humans[1])\n\t{\n\t\tHumans[1]->SetHumanControlled(false);\n\t\tHumans[1] = nullptr;\n\t}", "cpp")
s = rep(s, "TEXT(\"Match started (%s)\"), bSpectate ? TEXT(\"AI vs AI\") : TEXT(\"you are BLUE\"));",
        "TEXT(\"Match started (%s)\"), bSpectate ? TEXT(\"AI vs AI\") : (bHumanTeam[1] ? TEXT(\"local 1v1: P1 BLUE, P2 RED\") : TEXT(\"you are BLUE\")));", "cpp")

# rumble hooks
s = rep(s, "\tconst EPKSetPiece KickedFrom = Ball->IsPlaced() || Ball->GetHolder() ? SetPiece : EPKSetPiece::None;",
        "\tif (P->bHuman)\n\t{\n\t\tRumble(P->Team, Kick.Type == EPKKickType::Shot ? 0.35f + 0.5f * Kick.Power : 0.18f, Kick.Type == EPKKickType::Shot ? 0.2f : 0.1f);\n\t}\n"
        "\tconst EPKSetPiece KickedFrom = Ball->IsPlaced() || Ball->GetHolder() ? SetPiece : EPKSetPiece::None;", "cpp")
s = rep(s, "\t\t\t\t\t++Steals[P->Team];\n\t\t\t\t\tGainControl(P);",
        "\t\t\t\t\t++Steals[P->Team];\n\t\t\t\t\tif (Controller->bHuman)\n\t\t\t\t\t{\n\t\t\t\t\t\tRumble(Controller->Team, 0.55f, 0.25f); // robbed\n\t\t\t\t\t}\n"
        "\t\t\t\t\tif (P->bHuman)\n\t\t\t\t\t{\n\t\t\t\t\t\tRumble(P->Team, 0.3f, 0.12f);\n\t\t\t\t\t}\n\t\t\t\t\tGainControl(P);", "cpp")
s = rep(s, "\t\tUE_LOG(LogPitchKings, Log, TEXT(\"GOAL %s  (BLUE %d - %d RED, %.0fs left)\")",
        "\t\tRumble(Scorer, 0.8f, 0.6f);\n\t\tRumble(1 - Scorer, 0.35f, 0.3f);\n"
        "\t\tUE_LOG(LogPitchKings, Log, TEXT(\"GOAL %s  (BLUE %d - %d RED, %.0fs left)\")", "cpp")
s = rep(s, "\tconst FVector Spot = Victim->GetFlatLocation();\n\t++Fouls[Fouler->Team];",
        "\tconst FVector Spot = Victim->GetFlatLocation();\n\t++Fouls[Fouler->Team];\n\tif (Victim->bHuman)\n\t{\n\t\tRumble(Team, 0.6f, 0.3f);\n\t}", "cpp")
s = rep(s, "\t\tCarrier->Stun(0.45f);\n", "\t\tCarrier->Stun(0.45f);\n\t\tif (Carrier->bHuman)\n\t\t{\n\t\t\tRumble(Carrier->Team, 0.45f, 0.18f);\n\t\t}\n", "cpp")
save("PKGameMode.cpp", s)
print("versus gm ok")
