"""Front end: LOCAL 1v1 button and the device / side assignment page."""
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


h = load("PKMenu.h")
h = rep(h, "\tPause\n};", "\tPause,\n\tVersus     // local 1v1: each player picks a side with their own device\n};", "h")
h = rep(h, "\tvoid RebuildTeamSetup();\n", "\tvoid RebuildTeamSetup();\n\tTSharedRef<SWidget> BuildVersusShell();\n\tvoid RebuildVersus();\n\tvoid TickVersus();\n\tvoid MoveDevice(int32 Device, int32 Step);\n\tbool VersusReady(int32& OutP1, int32& OutP2) const;\n\tvoid TryStartVersus();\n", "h")
h = rep(h, "\tTSharedPtr<SBox> TeamSetupBox;\n", "\tTSharedPtr<SBox> TeamSetupBox;\n\tTSharedPtr<SBox> VersusBox;\n\tTMap<int32, int32> VersusSide; // device -> -1 BLUE (P1), 0 not playing, +1 RED (P2)\n\tTArray<int32> VersusDevices;\n\tuint64 MenuSeq = 0;\n", "h")
h = rep(h, "\tvoid TestClickSquadPlayer(int32 Index) { ClickSquadPlayer(Index); }",
        "\tvoid TestClickSquadPlayer(int32 Index) { ClickSquadPlayer(Index); }\n\tvoid TestVersusTick() { TickVersus(); }", "h")
h = rep(h, "TSharedRef<SButton> MakeButton(const FString& Label, FOnClicked OnClicked, float Width = 420.f, bool bPrimary = false);",
        "TSharedRef<SButton> MakeButton(const FString& Label, FOnClicked OnClicked, float Width = 420.f, bool bPrimary = false, bool bFocusable = true);", "h")
save("PKMenu.h", h)

c = load("PKMenu.cpp")
c = rep(c, "TSharedRef<SButton> SPKMenu::MakeButton(const FString& Label, FOnClicked OnClicked, float Width, bool bPrimary)", "TSharedRef<SButton> SPKMenu::MakeButton(const FString& Label, FOnClicked OnClicked, float Width, bool bPrimary, bool bFocusable)", "cpp")
c = rep(c, "\t\t.ButtonStyle(&MenuButtonStyle)\n\t\t.OnClicked(OnClicked)\n", "\t\t.ButtonStyle(&MenuButtonStyle)\n\t\t.OnClicked(OnClicked)\n\t\t.IsFocusable(bFocusable)\n", "cpp")
c = rep(c, '#include "PKTeamData.h"\n', '#include "PKTeamData.h"\n#include "PKInput.h"\n', "cpp")
c = rep(c, "\t\t\t+ SWidgetSwitcher::Slot()[ BuildPause() ]\n", "\t\t\t+ SWidgetSwitcher::Slot()[ BuildPause() ]\n\t\t\t+ SWidgetSwitcher::Slot()[ BuildVersusShell() ]\n", "cpp")
c = rep(c, "\tcase EPKMenuPage::Pause:\n\t\tFocus(PauseFirstButton);\n\t\tbreak;\n\t}\n}",
        "\tcase EPKMenuPage::Pause:\n\t\tFocus(PauseFirstButton);\n\t\tbreak;\n"
        "\tcase EPKMenuPage::Versus:\n"
        "\t\t// Fresh assignment: devices start on sensible sides, earlier presses are ignored.\n"
        "\t\tVersusSide.Reset();\n\t\tVersusDevices.Reset();\n\t\tMenuSeq = FPKInputRouter::Get().Sequence();\n"
        "\t\tTickVersus();\n\t\tRebuildVersus();\n\t\tFocus(SharedThis(this));\n\t\tbreak;\n\t}\n}", "cpp")
c = rep(c, "\tcase EPKMenuPage::Controls:\n\t\tShowPage(EPKMenuPage::Home);\n\t\tbreak;\n\tcase EPKMenuPage::Home:",
        "\tcase EPKMenuPage::Controls:\n\tcase EPKMenuPage::Versus:\n\t\tShowPage(EPKMenuPage::Home);\n\t\tbreak;\n\tcase EPKMenuPage::Home:", "cpp")
c = rep(c, "\tif (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::BackSpace)",
        "\tif (Page == EPKMenuPage::Versus)\n\t{\n\t\treturn FReply::Handled(); // the page reads every device itself (TickVersus)\n\t}\n"
        "\tif (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::BackSpace)", "cpp")
c = rep(c, "\tSCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);\n",
        "\tSCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);\n\tif (Page == EPKMenuPage::Versus)\n\t{\n\t\tTickVersus();\n\t}\n", "cpp")

# home: LOCAL 1v1 button under KICK OFF
c = rep(c, "\t\t\t+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 0.f)[ KickOff ]\n",
        "\t\t\t+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 0.f)[ KickOff ]\n"
        "\t\t\t+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)\n\t\t\t[\n"
        "\t\t\t\tMakeButton(TEXT(\"LOCAL 1v1  (2 PLAYERS)\"), FOnClicked::CreateLambda([this]()\n\t\t\t\t{\n"
        "\t\t\t\t\tShowPage(EPKMenuPage::Versus);\n\t\t\t\t\treturn FReply::Handled();\n\t\t\t\t}))\n\t\t\t]\n", "cpp")

c += r"""
// ---------------------------------------------------------------------------------------------
// Local 1v1: device / side assignment (like a console couch-play screen)
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> SPKMenu::BuildVersusShell()
{
	return SAssignNew(VersusBox, SBox);
}

bool SPKMenu::VersusReady(int32& OutP1, int32& OutP2) const
{
	int32 Left = 0, Right = 0;
	for (const auto& Pair : VersusSide)
	{
		if (Pair.Value < 0) { ++Left; OutP1 = Pair.Key; }
		if (Pair.Value > 0) { ++Right; OutP2 = Pair.Key; }
	}
	return Left == 1 && Right == 1;
}

void SPKMenu::TryStartVersus()
{
	int32 P1 = PKDevice::Keyboard, P2 = PKDevice::Keyboard;
	if (VersusReady(P1, P2) && Owner.IsValid())
	{
		if (UPKGameInstance* G = GI())
		{
			G->Save();
		}
		Owner->MenuVersus(P1, P2);
	}
}

void SPKMenu::MoveDevice(int32 Device, int32 Step)
{
	int32& Side = VersusSide.FindOrAdd(Device);
	const int32 NewSide = FMath::Clamp(Side + Step, -1, 1);
	if (NewSide == Side)
	{
		return;
	}
	// One device per side: a side that is taken stays taken.
	for (const auto& Pair : VersusSide)
	{
		if (Pair.Key != Device && NewSide != 0 && Pair.Value == NewSide)
		{
			FPKInputRouter::Get().Rumble(Device, 0.25f, 0.1f);
			return;
		}
	}
	Side = NewSide;
	if (Side != 0)
	{
		FPKInputRouter::Get().Rumble(Device, 0.35f, 0.12f);
	}
	RebuildVersus();
}

void SPKMenu::TickVersus()
{
	FPKInputRouter& R = FPKInputRouter::Get();
	TArray<int32> Devices = { PKDevice::Keyboard };
	Devices.Append(R.KnownGamepads());
	if (Devices != VersusDevices)
	{
		// New devices: keyboard on BLUE unless two gamepads are plugged in; gamepads fill the free sides.
		for (const int32 D : Devices)
		{
			if (VersusSide.Contains(D))
			{
				continue;
			}
			bool bLeftTaken = false, bRightTaken = false;
			for (const auto& Pair : VersusSide)
			{
				bLeftTaken |= Pair.Value < 0;
				bRightTaken |= Pair.Value > 0;
			}
			int32 Side = 0;
			if (D == PKDevice::Keyboard)
			{
				Side = -1;
			}
			else if (!bRightTaken)
			{
				Side = 1;
			}
			else if (!bLeftTaken)
			{
				Side = -1;
			}
			VersusSide.Add(D, Side);
		}
		// Two gamepads: let them play, keyboard steps aside.
		const TArray<int32> Pads = R.KnownGamepads();
		if (Pads.Num() >= 2 && VersusSide.FindRef(PKDevice::Keyboard) < 0 && VersusSide.FindRef(Pads[0]) == 0)
		{
			VersusSide[PKDevice::Keyboard] = 0;
			VersusSide[Pads[0]] = -1;
		}
		VersusDevices = Devices;
		RebuildVersus();
	}

	for (const int32 D : VersusDevices)
	{
		auto Pressed = [&](std::initializer_list<FKey> Keys)
		{
			for (const FKey& K : Keys)
			{
				if (R.WasPressed(D, K, MenuSeq))
				{
					return true;
				}
			}
			return false;
		};
		if (Pressed({ EKeys::A, EKeys::Left, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left }))
		{
			MoveDevice(D, -1);
		}
		if (Pressed({ EKeys::D, EKeys::Right, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right }))
		{
			MoveDevice(D, 1);
		}
		if (Pressed({ EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right }))
		{
			MenuSeq = R.Sequence();
			TryStartVersus();
			return;
		}
		if (Pressed({ EKeys::Escape, EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right }))
		{
			MenuSeq = R.Sequence();
			if (VersusSide.FindRef(D) != 0)
			{
				VersusSide[D] = 0;
				RebuildVersus();
			}
			else
			{
				ShowPage(EPKMenuPage::Home);
			}
			return;
		}
	}
	MenuSeq = R.Sequence();
}

void SPKMenu::RebuildVersus()
{
	const UPKGameInstance* G = GI();
	if (!VersusBox.IsValid() || !G)
	{
		return;
	}
	const FPKClub& Club = G->UserClubData();
	const FPKClub& Opp = G->OpponentClubData();
	const FLinearColor BlueSide(0.12f, 0.42f, 1.f);
	const FLinearColor RedSide(0.9f, 0.18f, 0.14f);

	auto TeamPanel = [&](const FString& PlayerLabel, const FPKClub& C, int32 Formation, int32 StyleIndex, const FLinearColor& Accent, int32 Side)
	{
		int32 Dev = INDEX_NONE;
		for (const auto& Pair : VersusSide)
		{
			if (Pair.Value == Side)
			{
				Dev = Pair.Key;
			}
		}
		return SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(FLinearColor(Accent.R * 0.25f, Accent.G * 0.25f, Accent.B * 0.25f, 0.92f))
			.Padding(FMargin(24.f, 18.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(PlayerLabel)).Font(Font(24)).ColorAndOpacity(Accent) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[ SNew(STextBlock).Text(Txt(C.Name.ToUpper())).Font(Font(18)).ColorAndOpacity(FLinearColor::White) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(FString::Printf(TEXT("%s  -  %s"), PKTeams::Formations()[Formation].Name, StyleNames[StyleIndex]))).Font(Font(12, false)).ColorAndOpacity(Dim) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Txt(Dev == INDEX_NONE ? FString(TEXT("waiting for a player...")) : FString::Printf(TEXT("READY  -  %s"), *FPKInputRouter::DeviceName(Dev))))
					.Font(Font(14)).ColorAndOpacity(Dev == INDEX_NONE ? FLinearColor(0.7f, 0.7f, 0.75f) : FLinearColor(0.4f, 1.f, 0.5f))
				]
			];
	};

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const int32 D : VersusDevices)
	{
		const int32 Side = VersusSide.FindRef(D);
		const FLinearColor ChipColor = Side < 0 ? BlueSide : (Side > 0 ? RedSide : FLinearColor(0.25f, 0.28f, 0.36f));
		const FString Label = FString::Printf(TEXT("%s%s"), *FPKInputRouter::DeviceName(D), Side < 0 ? TEXT("  -  P1") : (Side > 0 ? TEXT("  -  P2") : TEXT("")));
		TSharedRef<SWidget> Chip = SNew(SBorder)
			.BorderImage(&PanelBrush).BorderBackgroundColor(ChipColor).Padding(FMargin(18.f, 10.f)).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Txt(Label)).Font(Font(16)).ColorAndOpacity(FLinearColor::White)
			];
		// Not focusable: keyboard / gamepad stay with the page, these arrows are for the mouse.
		TSharedRef<SButton> LeftArrow = MakeButton(TEXT("<"), FOnClicked::CreateLambda([this, D]() { MoveDevice(D, -1); return FReply::Handled(); }), 56.f, false, false);
		TSharedRef<SButton> RightArrow = MakeButton(TEXT(">"), FOnClicked::CreateLambda([this, D]() { MoveDevice(D, 1); return FReply::Handled(); }), 56.f, false, false);
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ LeftArrow ];
		for (int32 Column = -1; Column <= 1; ++Column)
		{
			Row->AddSlot().FillWidth(1.f).Padding(12.f, 0.f).VAlign(VAlign_Center)
			[
				Column == Side ? Chip : SNullWidget::NullWidget
			];
		}
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[ RightArrow ];
		Rows->AddSlot().AutoHeight().Padding(0.f, 6.f)[ Row ];
	}

	int32 P1 = 0, P2 = 0;
	const bool bReady = VersusReady(P1, P2);
	TSharedRef<SButton> Start = MakeButton(TEXT("START MATCH"), FOnClicked::CreateLambda([this]() { TryStartVersus(); return FReply::Handled(); }), 300.f, true, false);
	TSharedRef<SButton> BackButton = MakeButton(TEXT("BACK"), FOnClicked::CreateLambda([this]() { ShowPage(EPKMenuPage::Home); return FReply::Handled(); }), 220.f, false, false);
	Start->SetEnabled(bReady);

	VersusBox->SetContent(
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1320.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(TEXT("LOCAL 1v1"))).Font(Font(34)).ColorAndOpacity(Gold) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 20.f)
				[
					SNew(STextBlock).Text(Txt(TEXT("Two players, one screen. Each player uses their own keyboard or gamepad and controls the team on their side.")))
					.Font(Font(13, false)).ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[ TeamPanel(TEXT("PLAYER 1  -  BLUE"), Club, G->Formation, G->Style, BlueSide, -1) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(30.f, 0.f)
					[
						SNew(STextBlock).Text(Txt(TEXT("VS"))).Font(Font(40)).ColorAndOpacity(Gold)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)[ TeamPanel(TEXT("PLAYER 2  -  RED"), Opp, Opp.Formation, Opp.Style, RedSide, 1) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[ SNew(STextBlock).Text(Txt(TEXT("<  BLUE"))).Font(Font(12)).ColorAndOpacity(BlueSide) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[ SNew(STextBlock).Text(Txt(TEXT("NOT PLAYING"))).Font(Font(12)).ColorAndOpacity(Dim) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[ SNew(STextBlock).Text(Txt(TEXT("RED  >"))).Font(Font(12)).ColorAndOpacity(RedSide) ]
				]
				+ SVerticalBox::Slot().AutoHeight()[ Panel(Rows, FMargin(16.f, 12.f)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Txt(bReady ? FString(TEXT("Ready!  Press Enter / A / Start to kick off.")) : FString(TEXT("Each player: push LEFT or RIGHT on your own keyboard (A / D, arrows) or gamepad (D-pad / stick) to join a side."))))
					.Font(Font(14)).ColorAndOpacity(bReady ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor::White)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Txt(TEXT("Gamepad not listed? Press any button on it.  P2 plays the opponent picked in Team Setup.  Esc / B: leave a side, then back.")))
					.Font(Font(11, false)).ColorAndOpacity(Dim)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ BackButton ]
					+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Start ]
				]
			]
		]);
}
"""
save("PKMenu.cpp", c)
print("versus menu ok")
