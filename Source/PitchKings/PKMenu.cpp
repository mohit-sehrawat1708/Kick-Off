#include "PKMenu.h"
#include "PKGameInstance.h"
#include "PKGameMode.h"
#include "PKPlayerController.h"
#include "PKTeamData.h"
#include "PKInput.h"
#include "PKSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(1.f, 0.76f, 0.1f);
	const FLinearColor Navy(0.012f, 0.025f, 0.07f, 0.9f);
	const FLinearColor Blue(0.1f, 0.38f, 0.92f);
	const FLinearColor Red(0.86f, 0.14f, 0.12f);
	const FLinearColor Dim(0.68f, 0.73f, 0.84f);
	const FLinearColor Orange(1.f, 0.55f, 0.1f);

	// Team-sheet pitch (attack upward).
	constexpr float PitchW = 520.f;
	constexpr float PitchH = 700.f;
	constexpr float PitchMargin = 40.f;

	FSlateFontInfo Font(int32 Size, bool bBold = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FText Txt(const FString& S)
	{
		return FText::FromString(S);
	}

	FVector2D SlotCenter(const FVector2D& Home)
	{
		// Team frame y > 0 is the left of the attacking direction, which is screen-left when attacking up.
		const float Spread = FMath::Abs(Home.Y) < 0.01f ? 0.f : FMath::Sign(Home.Y) * FMath::Min(0.95f, FMath::Abs(Home.Y) * 1.25f + 0.12f);
		// a lone central midfielder sits just behind the wide pair: drop him a little so the labels don't collide
		const float Drop = Spread == 0.f && Home.X < -0.05f && Home.X > -0.6f ? 46.f : 0.f;
		return FVector2D(PitchW * 0.5f - Spread * (PitchW * 0.5f - 62.f), PitchMargin + (0.08f - Home.X) / 1.08f * (PitchH - 2.f * PitchMargin) + Drop);
	}

	FLinearColor PosColor(EPKPos Pos)
	{
		switch (Pos)
		{
		case EPKPos::GK:  return FLinearColor(1.f, 0.8f, 0.2f);
		case EPKPos::DEF: return FLinearColor(0.35f, 0.75f, 1.f);
		case EPKPos::MID: return FLinearColor(0.4f, 0.95f, 0.5f);
		default:          return FLinearColor(1.f, 0.45f, 0.4f);
		}
	}

	FLinearColor FitColor(float Fit)
	{
		return Fit >= 0.99f ? FLinearColor(0.35f, 1.f, 0.45f) : (Fit >= 0.9f ? Orange : FLinearColor(1.f, 0.3f, 0.25f));
	}

	const TCHAR* StyleNames[] = { TEXT("Balanced"), TEXT("Possession"), TEXT("Counter Attack"), TEXT("Long Ball"), TEXT("High Press") };
}

void SPKMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	OpenTime = FPlatformTime::Seconds();

	LogoBrush.SetResourceObject(InArgs._Logo);
	LogoBrush.ImageSize = FVector2D(1240.f, 560.f);
	CrestBrush.SetResourceObject(InArgs._Crest);
	CrestBrush.ImageSize = FVector2D(512.f, 512.f);
	WhiteBrush = FSlateBrush();
	PanelBrush = FSlateRoundedBoxBrush(FLinearColor::White, 14.f);
	CircleBrush = FSlateRoundedBoxBrush(FLinearColor::White, 22.f);
	OutlineBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.f, FLinearColor(1.f, 1.f, 1.f, 0.7f), 2.f);
	CircleOutlineBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 200.f, FLinearColor(1.f, 1.f, 1.f, 0.7f), 2.f);

	const FSlateRoundedBoxBrush Rounded(FLinearColor::White, 10.f);
	MenuButtonStyle = FButtonStyle()
		.SetNormal(Rounded).SetHovered(Rounded).SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.85f, 0.85f), 10.f))
		.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
	const FSlateRoundedBoxBrush Flat(FLinearColor::White, 8.f);
	FlatButtonStyle = FButtonStyle()
		.SetNormal(Flat).SetHovered(Flat).SetPressed(Flat)
		.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity_Lambda([this]()
			{
				switch (Page)
				{
				case EPKMenuPage::Landing:   return FSlateColor(FLinearColor(0.f, 0.f, 0.02f, 0.35f));
				case EPKMenuPage::Home:      return FSlateColor(FLinearColor(0.f, 0.f, 0.02f, 0.3f));
				case EPKMenuPage::Pause:     return FSlateColor(FLinearColor(0.f, 0.f, 0.02f, 0.55f));
				default:                     return FSlateColor(FLinearColor(0.f, 0.01f, 0.03f, 0.78f));
				}
			})
		]
		+ SOverlay::Slot()
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()[ BuildLanding() ]
			+ SWidgetSwitcher::Slot()[ BuildHome() ]
			+ SWidgetSwitcher::Slot()[ SAssignNew(TeamSetupBox, SBox) ]
			+ SWidgetSwitcher::Slot()[ SAssignNew(ControlsBox, SBox) ]
			+ SWidgetSwitcher::Slot()[ BuildPause() ]
			+ SWidgetSwitcher::Slot()[ BuildVersusShell() ]
			+ SWidgetSwitcher::Slot()[ SAssignNew(SettingsBox, SBox) ]
			+ SWidgetSwitcher::Slot()[ SAssignNew(SubsBox, SBox) ]
		]
	];
}

UPKGameInstance* SPKMenu::GI() const
{
	return Owner.IsValid() ? Owner->GetGameInstance<UPKGameInstance>() : nullptr;
}

void SPKMenu::Focus(const TSharedPtr<SWidget>& Widget)
{
	if (Widget.IsValid())
	{
		FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
		// A page that was just switched in has no layout yet: try again over the next frames.
		PendingFocus = Widget;
		PendingFocusFrames = 3;
	}
}

void SPKMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (Page == EPKMenuPage::Versus)
	{
		TickVersus();
	}
	if (PendingFocusFrames > 0)
	{
		--PendingFocusFrames;
		const TSharedPtr<SWidget> Widget = PendingFocus.Pin();
		if (Widget.IsValid() && !Widget->HasAnyUserFocus().IsSet())
		{
			FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
		}
	}
}

void SPKMenu::ShowPage(EPKMenuPage InPage)
{
	if (InPage == EPKMenuPage::Settings && Page != EPKMenuPage::Settings)
	{
		SettingsReturn = Page == EPKMenuPage::Pause ? EPKMenuPage::Pause : EPKMenuPage::Home;
		SettingsTab = 0;
		CaptureAction = INDEX_NONE;
		FocusSelector = TEXT("TAB0");
	}
	Page = InPage;
	Switcher->SetActiveWidgetIndex((int32)Page);
	switch (Page)
	{
	case EPKMenuPage::Landing:
		Focus(SharedThis(this));
		break;
	case EPKMenuPage::Home:
		RefreshHome();
		Focus(HomeFirstButton);
		break;
	case EPKMenuPage::TeamSetup:
		SelectedSlot = INDEX_NONE;
		FocusSlot = 0;
		RebuildTeamSetup();
		break;
	case EPKMenuPage::Controls:
		ControlsBox->SetContent(BuildControls()); // shows the current key bindings
		Focus(ControlsBackButton);
		break;
	case EPKMenuPage::Settings:
		RebuildSettings();
		break;
	case EPKMenuPage::Subs:
		SubsSlot = INDEX_NONE;
		SubsFocus = 0;
		RebuildSubs();
		break;
	case EPKMenuPage::Pause:
		Focus(PauseFirstButton);
		break;
	case EPKMenuPage::Versus:
		// Fresh assignment: devices start on sensible sides, earlier presses are ignored.
		VersusSide.Reset();
		VersusDevices.Reset();
		MenuSeq = FPKInputRouter::Get().Sequence();
		TickVersus();
		RebuildVersus();
		Focus(SharedThis(this));
		break;
	}
}

void SPKMenu::Back()
{
	switch (Page)
	{
	case EPKMenuPage::TeamSetup:
		if (SelectedSlot != INDEX_NONE)
		{
			FocusSlot = SelectedSlot;
			SelectedSlot = INDEX_NONE;
			RebuildTeamSetup();
			return;
		}
		if (UPKGameInstance* G = GI())
		{
			G->Save();
		}
		ShowPage(EPKMenuPage::Home);
		break;
	case EPKMenuPage::Controls:
	case EPKMenuPage::Versus:
		ShowPage(EPKMenuPage::Home);
		break;
	case EPKMenuPage::Settings:
		if (UPKGameInstance* G = GI())
		{
			G->SaveSettings();
		}
		if (Owner.IsValid())
		{
			Owner->ApplyUserSettings();
		}
		ShowPage(SettingsReturn);
		break;
	case EPKMenuPage::Home:
		ShowPage(EPKMenuPage::Landing);
		break;
	case EPKMenuPage::Pause:
		if (Owner.IsValid())
		{
			Owner->MenuResume();
		}
		break;
	case EPKMenuPage::Subs:
		if (SubsSlot != INDEX_NONE)
		{
			SubsSlot = INDEX_NONE;
			RebuildSubs();
			return;
		}
		ShowPage(EPKMenuPage::Pause);
		break;
	default:
		break;
	}
}

FReply SPKMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	UE_LOG(LogTemp, Verbose, TEXT("[Menu] key %s on page %d"), *Key.ToString(), (int32)Page);
	if (HandleKeyCapture(Key))
	{
		return FReply::Handled();
	}
	if (Page == EPKMenuPage::Landing)
	{
		if (FPlatformTime::Seconds() - OpenTime > 0.4)
		{
			ShowPage(EPKMenuPage::Home);
		}
		return FReply::Handled();
	}
	if (Page == EPKMenuPage::Versus)
	{
		return FReply::Handled(); // the page reads every device itself (TickVersus)
	}
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::BackSpace)
	{
		Back();
		return FReply::Handled();
	}
	if (Page == EPKMenuPage::Pause && Key == EKeys::Gamepad_Special_Right)
	{
		Back();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SPKMenu::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (Page == EPKMenuPage::Landing)
	{
		ShowPage(EPKMenuPage::Home);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------------------------

TSharedRef<SButton> SPKMenu::MakeButton(const FString& Label, FOnClicked OnClicked, float Width, bool bPrimary, bool bFocusable)
{
	TSharedRef<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
	auto IsHot = [Self]()
	{
		const TSharedPtr<SButton> B = Self->Pin();
		// focus only (hovering moves the focus), so a resting mouse cannot light up a second button
		return B.IsValid() && (B->SupportsKeyboardFocus() ? B->HasAnyUserFocus().IsSet() : B->IsHovered());
	};
	const bool bSmall = Width < 100.f;
	TSharedRef<SButton> Button = SNew(SButton)
		.ButtonStyle(&MenuButtonStyle)
		.OnClicked(OnClicked)
		.IsFocusable(bFocusable)
		.HAlign(bSmall ? HAlign_Center : HAlign_Fill)
		.ContentPadding(bSmall ? FMargin(0.f, 10.f) : FMargin(26.f, 13.f))
		.OnHovered_Lambda([Self]()
		{
			// the mouse takes the focus with it, so only one button is ever highlighted
			if (const TSharedPtr<SButton> B = Self->Pin(); B.IsValid() && B->SupportsKeyboardFocus())
			{
				FSlateApplication::Get().SetAllUserFocus(B, EFocusCause::Mouse);
			}
		})
		.ButtonColorAndOpacity_Lambda([IsHot, bPrimary]()
		{
			return FSlateColor(IsHot() ? Gold : (bPrimary ? FLinearColor(0.07f, 0.16f, 0.38f, 0.95f) : FLinearColor(0.04f, 0.07f, 0.16f, 0.92f)) /* only the focused button is gold */);
		})
		[
			SNew(SBox)
			.WidthOverride(bSmall ? Width : Width - 52.f)
			.HAlign(bSmall ? HAlign_Center : HAlign_Fill)
			[
				SNew(STextBlock)
				.Text(Txt(Label))
				.Justification(bSmall ? ETextJustify::Center : ETextJustify::Left)
				.Font(Font(bSmall ? 22 : 20))
				.ColorAndOpacity_Lambda([IsHot]() { return FSlateColor(IsHot() ? FLinearColor(0.03f, 0.03f, 0.06f) : FLinearColor::White); })
			]
		];
	*Self = Button;
	return Button;
}

TSharedRef<SWidget> SPKMenu::Panel(const TSharedRef<SWidget>& Content, const FMargin& Padding)
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(Navy)
		.Padding(Padding)
		[
			Content
		];
}

TSharedRef<SWidget> SPKMenu::BuildSelector(const FString& Label, const FString& Value, FOnClicked OnPrev, FOnClicked OnNext)
{
	TSharedRef<SButton> Prev = MakeButton(TEXT("<"), OnPrev, 56.f);
	TSharedRef<SButton> Next = MakeButton(TEXT(">"), OnNext, 56.f);
	NamedButtons.Add(Label + TEXT("<"), Prev);
	NamedButtons.Add(Label + TEXT(">"), Next);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).Text(Txt(Label)).Font(Font(11)).ColorAndOpacity(Dim)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Prev ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Txt(Value)).Font(Font(16)).ColorAndOpacity(FLinearColor::White)
			]
			+ SHorizontalBox::Slot().AutoWidth()[ Next ]
		];
}

// ---------------------------------------------------------------------------------------------
// Landing
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> SPKMenu::BuildLanding()
{
	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(930.f).HeightOverride(420.f)
				[
					SNew(SImage).Image(&LogoBrush)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 50.f, 0.f, 0.f)
			[
				SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(FLinearColor(0.01f, 0.02f, 0.06f, 0.82f)).Padding(FMargin(34.f, 12.f))
				[
					SNew(STextBlock)
					.Text(Txt(TEXT("PRESS ANY KEY TO START")))
					.Font(Font(26))
					.ColorAndOpacity_Lambda([]()
					{
						const float A = 0.7f + 0.3f * FMath::Sin(FPlatformTime::Seconds() * 3.0);
						return FSlateColor(FLinearColor(1.f, 0.82f, 0.2f, A));
					})
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 28.f)
		[
			SNew(STextBlock)
			.Text(Txt(TEXT("Prototype build  -  Unreal Engine 5.8  -  all clubs and players are fictional")))
			.Font(Font(11, false))
			.ColorAndOpacity(FLinearColor(0.85f, 0.88f, 0.95f, 0.75f))
		];
}

// ---------------------------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> SPKMenu::BuildHome()
{
	TSharedRef<SButton> KickOff = MakeButton(TEXT("KICK OFF"), FOnClicked::CreateLambda([this]()
	{
		UE_LOG(LogTemp, Log, TEXT("[Menu] KICK OFF clicked (page %d, %.2fs after open)"), (int32)Page, FPlatformTime::Seconds() - OpenTime);
		if (UPKGameInstance* G = GI()) { G->Save(); }
		if (Owner.IsValid()) { Owner->MenuKickOff(); }
		return FReply::Handled();
	}), 420.f, true);
	HomeFirstButton = KickOff;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(80.f, 50.f, 0.f, 40.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(520.f).HeightOverride(235.f)[ SNew(SImage).Image(&LogoBrush) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 0.f)[ KickOff ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("LOCAL 1v1  (2 PLAYERS)"), FOnClicked::CreateLambda([this]()
				{
					ShowPage(EPKMenuPage::Versus);
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("SPECTATE  AI vs AI"), FOnClicked::CreateLambda([this]()
				{
					if (Owner.IsValid()) { Owner->MenuSpectate(); }
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("TEAM SETUP"), FOnClicked::CreateLambda([this]()
				{
					ShowPage(EPKMenuPage::TeamSetup);
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("SETTINGS"), FOnClicked::CreateLambda([this]()
				{
					ShowPage(EPKMenuPage::Settings);
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("CONTROLS"), FOnClicked::CreateLambda([this]()
				{
					ShowPage(EPKMenuPage::Controls);
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				MakeButton(TEXT("QUIT"), FOnClicked::CreateLambda([this]()
				{
					if (Owner.IsValid()) { Owner->MenuQuit(); }
					return FReply::Handled();
				}))
			]
			+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSpacer) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Txt(TEXT("Mouse, or arrows / D-pad to move  -  Enter / A select  -  Esc / B back")))
				.Font(Font(11, false))
				.ColorAndOpacity(FLinearColor(0.9f, 0.92f, 1.f, 0.8f))
				.ShadowOffset(FVector2D(1.f, 1.f))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 80.f, 0.f)
		[
			SAssignNew(HomeCardBox, SBox).WidthOverride(600.f)
		];
}

void SPKMenu::RefreshHome()
{
	if (HomeCardBox.IsValid())
	{
		HomeCardBox->SetContent(BuildMatchCard());
	}
}

TSharedRef<SWidget> SPKMenu::BuildMatchCard()
{
	const UPKGameInstance* G = GI();
	if (!G)
	{
		return SNullWidget::NullWidget;
	}
	const FPKClub& Club = G->UserClubData();
	const FPKClub& Opp = G->OpponentClubData();
	const FPKFormation& F = PKTeams::Formations()[G->Formation];
	const int32 Rating = PKTeams::TeamRating(Club, G->Formation, G->Lineup);
	const TArray<int32> OppLineup = G->OpponentLineup();
	const int32 OppRating = PKTeams::TeamRating(Opp, Opp.Formation, OppLineup);

	TSharedRef<SVerticalBox> XI = SNew(SVerticalBox);
	for (int32 s = 0; s < 11; ++s)
	{
		const FPKSquadPlayer& Pl = Club.Squad[G->Lineup[s]];
		XI->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(52.f)[ SNew(STextBlock).Text(Txt(F.Slots[s].Label)).Font(Font(11)).ColorAndOpacity(PosColor(F.Slots[s].Pos)) ] ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(36.f)[ SNew(STextBlock).Text(Txt(FString::FromInt(Pl.Number))).Font(Font(11)).ColorAndOpacity(Gold) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Txt(Pl.Name)).Font(Font(11, false)).ColorAndOpacity(FLinearColor::White) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Text(Txt(FString::FromInt(PKTeams::RatingInSlot(Pl, F.Slots[s].Pos)))).Font(Font(11)).ColorAndOpacity(Dim) ]
		];
	}

	return Panel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(TEXT("NEXT MATCH"))).Font(Font(12)).ColorAndOpacity(Gold) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(96.f).HeightOverride(96.f)[ SNew(SImage).Image(&CrestBrush) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(18.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(Club.Name.ToUpper())).Font(Font(22)).ColorAndOpacity(FLinearColor(0.55f, 0.75f, 1.f)) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(TEXT("vs"))).Font(Font(12, false)).ColorAndOpacity(Dim) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(Opp.Name.ToUpper())).Font(Font(22)).ColorAndOpacity(FLinearColor(1.f, 0.45f, 0.4f)) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock).Font(Font(12, false)).ColorAndOpacity(FLinearColor::White)
			.Text(Txt(FString::Printf(TEXT("You:  %s  -  %s  -  team rating %d"), F.Name, StyleNames[G->Style], Rating)))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 14.f)
		[
			SNew(STextBlock).Font(Font(12, false)).ColorAndOpacity(Dim)
			.Text(Txt(FString::Printf(TEXT("Opponent:  %s  -  %s  -  team rating %d"), PKTeams::Formations()[Opp.Formation].Name, StyleNames[Opp.Style], OppRating)))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SNew(STextBlock).Text(Txt(TEXT("STARTING XI"))).Font(Font(12)).ColorAndOpacity(Gold) ]
		+ SVerticalBox::Slot().AutoHeight()[ XI ],
		FMargin(28.f, 22.f));
}

// ---------------------------------------------------------------------------------------------
// Team setup (player selection)
// ---------------------------------------------------------------------------------------------

void SPKMenu::ClickSlot(int32 Slot)
{
	UPKGameInstance* G = GI();
	if (!G)
	{
		return;
	}
	FocusSlot = Slot;
	FocusRow = INDEX_NONE;
	FocusSelector.Empty();
	if (SelectedSlot == INDEX_NONE)
	{
		SelectedSlot = Slot;
		DetailPlayer = G->Lineup[Slot];
	}
	else if (SelectedSlot == Slot)
	{
		SelectedSlot = INDEX_NONE;
	}
	else
	{
		G->SwapSlots(SelectedSlot, Slot); // swap the two starters' positions
		DetailPlayer = G->Lineup[Slot];
		SelectedSlot = INDEX_NONE;
		G->Save();
	}
	RebuildTeamSetup();
}

void SPKMenu::ClickSquadPlayer(int32 SquadIndex)
{
	UPKGameInstance* G = GI();
	if (!G)
	{
		return;
	}
	FocusRow = SquadIndex;
	FocusSlot = INDEX_NONE;
	FocusSelector.Empty();
	DetailPlayer = SquadIndex;
	if (SelectedSlot != INDEX_NONE)
	{
		G->PutPlayerInSlot(SquadIndex, SelectedSlot);
		SelectedSlot = INDEX_NONE;
		G->Save();
	}
	RebuildTeamSetup();
}

TSharedRef<SWidget> SPKMenu::BuildPitch()
{
	const UPKGameInstance* G = GI();
	const FPKClub& Club = G->UserClubData();
	const FPKFormation& F = PKTeams::Formations()[G->Formation];

	TSharedRef<SVerticalBox> Stripes = SNew(SVerticalBox);
	for (int32 i = 0; i < 10; ++i)
	{
		Stripes->AddSlot().FillHeight(1.f)
		[
			SNew(SImage).Image(&WhiteBrush).ColorAndOpacity(i % 2 ? FLinearColor(0.09f, 0.36f, 0.13f) : FLinearColor(0.11f, 0.42f, 0.15f))
		];
	}

	const float In = 12.f;
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	auto Line = [&Canvas](const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* Brush)
	{
		Canvas->AddSlot().Position(Pos).Size(Size)[ SNew(SImage).Image(Brush) ];
	};
	Line(FVector2D(In, In), FVector2D(PitchW - 2 * In, PitchH - 2 * In), &OutlineBrush);
	Line(FVector2D(In, PitchH * 0.5f - 1.f), FVector2D(PitchW - 2 * In, 2.f), &WhiteBrush);
	Line(FVector2D(PitchW * 0.5f - 65.f, PitchH * 0.5f - 65.f), FVector2D(130.f, 130.f), &CircleOutlineBrush);
	Line(FVector2D(PitchW * 0.5f - 150.f, In), FVector2D(300.f, 105.f), &OutlineBrush);
	Line(FVector2D(PitchW * 0.5f - 65.f, In), FVector2D(130.f, 38.f), &OutlineBrush);
	Line(FVector2D(PitchW * 0.5f - 150.f, PitchH - In - 105.f), FVector2D(300.f, 105.f), &OutlineBrush);
	Line(FVector2D(PitchW * 0.5f - 65.f, PitchH - In - 38.f), FVector2D(130.f, 38.f), &OutlineBrush);

	SlotButtons.SetNum(11);
	for (int32 s = 0; s < 11; ++s)
	{
		const FPKSquadPlayer& Pl = Club.Squad[G->Lineup[s]];
		const FPKFormationSlot& Slot = F.Slots[s];
		const float Fit = PKTeams::PositionFit(Pl.Pos, Slot.Pos);
		const bool bSelected = s == SelectedSlot;
		const FVector2D C = SlotCenter(Slot.Home);
		const FLinearColor Ring = bSelected ? Gold : (Fit >= 0.99f ? Blue : (Fit >= 0.9f ? Orange : Red));

		TSharedRef<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
		TSharedRef<SButton> Button = SNew(SButton)
			.ButtonStyle(&FlatButtonStyle)
			.ContentPadding(FMargin(2.f))
			.OnClicked(FOnClicked::CreateLambda([this, s]() { ClickSlot(s); return FReply::Handled(); }))
			.ButtonColorAndOpacity_Lambda([Self]()
			{
				const TSharedPtr<SButton> B = Self->Pin();
				const bool bHot = B.IsValid() && (B->IsHovered() || B->HasAnyUserFocus().IsSet());
				return FSlateColor(FLinearColor(1.f, 0.85f, 0.3f, bHot ? 0.35f : 0.f));
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(44.f).HeightOverride(44.f)
					[
						SNew(SBorder).BorderImage(&CircleBrush).BorderBackgroundColor(Ring).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Txt(FString::FromInt(Pl.Number))).Font(Font(16)).ColorAndOpacity(bSelected ? FLinearColor(0.05f, 0.05f, 0.05f) : FLinearColor::White)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f, 0.f, 0.f)
				[
					// Name plate: a dark backing so labels stay readable over the pitch lines.
					SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)).Padding(FMargin(6.f, 1.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(Txt(Pl.Name)).Font(Font(10)).ColorAndOpacity(FLinearColor::White)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(Txt(FString::Printf(TEXT("%s  %d"), Slot.Label, PKTeams::RatingInSlot(Pl, Slot.Pos)))).Font(Font(10))
							.ColorAndOpacity(FitColor(Fit))
						]
					]
				]
			];
		*Self = Button;
		SlotButtons[s] = Button;
		Canvas->AddSlot().Position(C - FVector2D(62.f, 26.f)).Size(FVector2D(124.f, 84.f))[ Button ];
	}

	return SNew(SBox).WidthOverride(PitchW).HeightOverride(PitchH)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ Stripes ]
			+ SOverlay::Slot()[ Canvas ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, -26.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Txt(TEXT("ATTACK"))).Font(Font(10)).ColorAndOpacity(Dim)
			]
		];
}

TSharedRef<SWidget> SPKMenu::BuildSquadList()
{
	const UPKGameInstance* G = GI();
	const FPKClub& Club = G->UserClubData();
	const FPKFormation& F = PKTeams::Formations()[G->Formation];

	auto Header = [](const FString& S, float W)
	{
		return SNew(SBox).WidthOverride(W)[ SNew(STextBlock).Text(Txt(S)).Font(Font(10)).ColorAndOpacity(Dim) ];
	};
	TSharedRef<SScrollBox> List = SNew(SScrollBox);
	RowButtons.SetNum(Club.Squad.Num());
	for (int32 i = 0; i < Club.Squad.Num(); ++i)
	{
		const FPKSquadPlayer& Pl = Club.Squad[i];
		const int32 Slot = G->SlotOfPlayer(i);
		const bool bStarter = Slot != INDEX_NONE;
		const bool bMarked = (SelectedSlot != INDEX_NONE && G->Lineup[SelectedSlot] == i) || (SelectedSlot == INDEX_NONE && DetailPlayer == i);
		const FString Status = bStarter ? FString(F.Slots[Slot].Label) : FString(TEXT("SUB"));

		TSharedRef<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
		TSharedRef<SButton> Row = SNew(SButton)
			.ButtonStyle(&FlatButtonStyle)
			.ContentPadding(FMargin(10.f, 3.f)  /* all 18 rows fit without a cut-off row */)
			.OnClicked(FOnClicked::CreateLambda([this, i]() { ClickSquadPlayer(i); return FReply::Handled(); }))
			.ButtonColorAndOpacity_Lambda([Self, bMarked, bStarter]()
			{
				const TSharedPtr<SButton> B = Self->Pin();
				const bool bHot = B.IsValid() && (B->IsHovered() || B->HasAnyUserFocus().IsSet());
				if (bHot) return FSlateColor(FLinearColor(1.f, 0.76f, 0.1f, 0.4f));
				if (bMarked) return FSlateColor(FLinearColor(0.1f, 0.38f, 0.92f, 0.45f));
				return FSlateColor(FLinearColor(1.f, 1.f, 1.f, bStarter ? 0.07f : 0.f));
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(40.f)[ SNew(STextBlock).Text(Txt(FString::FromInt(Pl.Number))).Font(Font(13)).ColorAndOpacity(Gold) ] ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Txt(Pl.Name)).Font(Font(13, bStarter)).ColorAndOpacity(bStarter ? FLinearColor::White : Dim) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(56.f)[ SNew(STextBlock).Text(Txt(PKTeams::PosName(Pl.Pos))).Font(Font(12)).ColorAndOpacity(PosColor(Pl.Pos)) ] ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(48.f)[ SNew(STextBlock).Text(Txt(FString::FromInt(Pl.Attr.Overall()))).Font(Font(13)).ColorAndOpacity(FLinearColor::White) ] ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(58.f)[ SNew(STextBlock).Text(Txt(Status)).Font(Font(12)).ColorAndOpacity(bStarter ? Gold : FLinearColor(0.55f, 0.58f, 0.65f)) ] ]
			];
		*Self = Row;
		RowButtons[i] = Row;
		List->AddSlot().Padding(0.f, 1.f)[ Row ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f, 10.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Header(TEXT("NO"), 40.f) ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Txt(TEXT("SQUAD"))).Font(Font(10)).ColorAndOpacity(Dim) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Header(TEXT("POS"), 56.f) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Header(TEXT("OVR"), 48.f) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Header(TEXT("ROLE"), 58.f) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)[ List ];
}

TSharedRef<SWidget> SPKMenu::BuildPlayerDetail()
{
	const UPKGameInstance* G = GI();
	const FPKClub& Club = G->UserClubData();
	const int32 Index = SelectedSlot != INDEX_NONE ? G->Lineup[SelectedSlot] : DetailPlayer;
	if (!Club.Squad.IsValidIndex(Index))
	{
		return SNew(STextBlock)
			.Text(Txt(TEXT("Pick a position on the pitch, then a squad player to put him there.\nTwo positions on the pitch swap the starters. Colours show how well a player fits:\ngreen natural, orange nearby position, red out of position.")))
			.Font(Font(11, false)).ColorAndOpacity(Dim).AutoWrapText(true);
	}
	const FPKSquadPlayer& Pl = Club.Squad[Index];
	const FPKAttributes& A = Pl.Attr;
	const float Values[] = { A.Pace, A.Control, A.Passing, A.Shooting, A.Defending, A.Physical };
	const TCHAR* Names[] = { TEXT("PACE"), TEXT("CONTROL"), TEXT("PASSING"), TEXT("SHOOTING"), TEXT("DEFENDING"), TEXT("PHYSICAL") };
	TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
	for (int32 k = 0; k < 6; ++k)
	{
		const float V = Values[k];
		Stats->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(96.f)[ SNew(STextBlock).Text(Txt(Names[k])).Font(Font(10)).ColorAndOpacity(Dim) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(10.f)
				[
					SNew(SProgressBar).Percent(V).FillColorAndOpacity(V > 0.8f ? FLinearColor(0.3f, 0.95f, 0.4f) : (V > 0.65f ? Gold : Orange))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 0.f, 0.f)[ SNew(SBox).WidthOverride(30.f)[ SNew(STextBlock).Text(Txt(FString::FromInt(FMath::RoundToInt(V * 100.f)))).Font(Font(11)).ColorAndOpacity(FLinearColor::White) ] ]
		];
	}
	const FString Traits = PKTeams::TraitNames(Pl.Traits, Pl.TraitsPlus) + (Pl.Pos == EPKPos::GK ? FString() : FString::Printf(TEXT("%sAcceleration: %s"), Pl.Traits ? TEXT("   |   ") : TEXT(""), APKFootballer::AccelName(APKFootballer::AccelTypeFor(Pl.Attr))));
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(Pl.Name)).Font(Font(18)).ColorAndOpacity(FLinearColor::White) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(FString::Printf(TEXT("#%d  -  %s"), Pl.Number, PKTeams::PosName(Pl.Pos)))).Font(Font(11)).ColorAndOpacity(PosColor(Pl.Pos)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Txt(FString::FromInt(A.Overall()))).Font(Font(28)).ColorAndOpacity(Gold)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 6.f)[ Stats ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Txt(Traits.IsEmpty() ? FString(TEXT("No traits")) : Traits)).Font(Font(11)).ColorAndOpacity(FLinearColor(0.6f, 0.85f, 1.f))
		];
}

void SPKMenu::RebuildTeamSetup()
{
	UPKGameInstance* G = GI();
	if (!TeamSetupBox.IsValid() || !G)
	{
		return;
	}
	NamedButtons.Reset();
	const FPKClub& Club = G->UserClubData();
	const FPKClub& Opp = G->OpponentClubData();
	const int32 Rating = PKTeams::TeamRating(Club, G->Formation, G->Lineup);
	const int32 OppRating = PKTeams::TeamRating(Opp, Opp.Formation, G->OpponentLineup());

	auto Change = [this](const FString& Key, TFunction<void(UPKGameInstance*)> Fn)
	{
		return FOnClicked::CreateLambda([this, Key, Fn]()
		{
			if (UPKGameInstance* G2 = GI())
			{
				Fn(G2);
				G2->Save();
			}
			FocusSelector = Key;
			FocusSlot = INDEX_NONE;
			FocusRow = INDEX_NONE;
			RebuildTeamSetup();
			return FReply::Handled();
		});
	};
	const int32 NumClubs = G->Clubs.Num();
	auto StepOpponent = [NumClubs](UPKGameInstance* G2, int32 Step)
	{
		do
		{
			G2->Opponent = ((G2->Opponent + Step) % NumClubs + NumClubs) % NumClubs;
		} while (G2->Opponent == G2->UserClub);
	};

	TSharedRef<SButton> BackButton = MakeButton(TEXT("BACK"), FOnClicked::CreateLambda([this]() { Back(); return FReply::Handled(); }), 220.f);
	SetupKickOffButton = MakeButton(TEXT("KICK OFF"), FOnClicked::CreateLambda([this]()
	{
		if (UPKGameInstance* G2 = GI()) { G2->Save(); }
		if (Owner.IsValid()) { Owner->MenuKickOff(); }
		return FReply::Handled();
	}), 260.f, true);

	TSharedRef<SWidget> Left = Panel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildSelector(TEXT("FORMATION"), PKTeams::Formations()[G->Formation].Name,
				Change(TEXT("FORMATION<"), [](UPKGameInstance* G2) { G2->SetFormation(G2->Formation - 1); }),
				Change(TEXT("FORMATION>"), [](UPKGameInstance* G2) { G2->SetFormation(G2->Formation + 1); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			BuildSelector(TEXT("PLAY STYLE"), StyleNames[G->Style],
				Change(TEXT("PLAY STYLE<"), [](UPKGameInstance* G2) { G2->Style = (G2->Style + 4) % 5; }),
				Change(TEXT("PLAY STYLE>"), [](UPKGameInstance* G2) { G2->Style = (G2->Style + 1) % 5; }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			BuildSelector(TEXT("OPPONENT"), Opp.Name,
				Change(TEXT("OPPONENT<"), [StepOpponent](UPKGameInstance* G2) { StepOpponent(G2, -1); }),
				Change(TEXT("OPPONENT>"), [StepOpponent](UPKGameInstance* G2) { StepOpponent(G2, 1); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(Txt(FString::Printf(TEXT("%s  -  %s  -  rating %d"), PKTeams::Formations()[Opp.Formation].Name, StyleNames[Opp.Style], OppRating)))
			.Font(Font(10, false)).ColorAndOpacity(FLinearColor(1.f, 0.55f, 0.5f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
		[
			MakeButton(TEXT("AUTO PICK BEST XI"), Change(TEXT("AUTO"), [](UPKGameInstance* G2) { G2->AutoPickUser(); }), 300.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
		[
			MakeButton(TEXT("RESET TEAM"), Change(TEXT("RESET"), [](UPKGameInstance* G2) { const int32 O = G2->Opponent; G2->ResetToDefaults(); G2->Opponent = O; }), 300.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(Txt(TEXT("TEAM RATING"))).Font(Font(11)).ColorAndOpacity(Dim)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Txt(FString::FromInt(Rating))).Font(Font(40)).ColorAndOpacity(Gold)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Txt(Rating >= OppRating ? FString(TEXT("Favourites for this one")) : FString::Printf(TEXT("%d below %s"), OppRating - Rating, *Opp.Short)))
			.Font(Font(11, false)).ColorAndOpacity(Rating >= OppRating ? FLinearColor(0.4f, 1.f, 0.5f) : Orange)
		],
		FMargin(22.f, 20.f));

	TeamSetupBox->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(60.f, 30.f, 60.f, 18.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)[ SNew(SImage).Image(&CrestBrush) ] ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(TEXT("TEAM SETUP"))).Font(Font(30)).ColorAndOpacity(Gold) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(Txt(Club.Name)).Font(Font(13, false)).ColorAndOpacity(FLinearColor::White) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Txt(SelectedSlot != INDEX_NONE
					? FString::Printf(TEXT("Moving %s - pick a squad player or another position  (Esc cancels)"), *Club.Squad[G->Lineup[SelectedSlot]].Name)
					: FString(TEXT("Select a position, then a player  -  Esc / B back"))))
				.Font(Font(12)).ColorAndOpacity(SelectedSlot != INDEX_NONE ? Gold : Dim)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(60.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(360.f)[ Left ] ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(36.f, 26.f, 36.f, 0.f)[ BuildPitch() ]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f)[ Panel(BuildSquadList(), FMargin(10.f, 14.f)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[ Panel(BuildPlayerDetail(), FMargin(22.f, 16.f)) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(60.f, 16.f, 60.f, 28.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BackButton ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SetupKickOffButton.ToSharedRef() ]
		]);

	// Keep keyboard / gamepad focus where it was.
	TSharedPtr<SWidget> Target = SlotButtons.IsValidIndex(0) ? SlotButtons[0] : nullptr;
	if (SlotButtons.IsValidIndex(FocusSlot))
	{
		Target = SlotButtons[FocusSlot];
	}
	else if (RowButtons.IsValidIndex(FocusRow))
	{
		Target = RowButtons[FocusRow];
	}
	else if (const TSharedPtr<SButton>* Named = NamedButtons.Find(FocusSelector))
	{
		Target = *Named;
	}
	Focus(Target);
}

// ---------------------------------------------------------------------------------------------
// Controls and pause
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> SPKMenu::BuildControls()
{
	FPKUserSettings Bindings;
	if (const UPKGameInstance* G = GI())
	{
		Bindings = G->Settings;
	}
	TArray<TArray<FString>> Rows;
	for (int32 a = 0; a < (int32)EPKInput::Count; ++a)
	{
		Rows.Add({ PKInputs::Info((EPKInput)a).Name, Bindings.KeyText((EPKInput)a), PKInputs::PadText((EPKInput)a) });
	}
	Rows.Add({ TEXT("Shot types: finesse / chip / power / low (hold with shoot)"), TEXT("R / Z / R+Z / Ctrl"), TEXT("RB / LB / RB+LB / LT") });
	Rows.Add({ TEXT("Keeper rush (no ball, hold)"), Bindings.KeyText(EPKInput::Through), TEXT("Y") });
	Rows.Add({ TEXT("Team play style"), TEXT("1 - 5"), TEXT("D-pad left / right") });
	Rows.Add({ TEXT("Game plan: more defensive / attacking"), TEXT("9 / 0"), TEXT("LT + D-pad left / right") });
	Rows.Add({ TEXT("Help  -  shirt numbers"), TEXT("H  -  N"), TEXT("") });
	TSharedRef<SVerticalBox> Table = SNew(SVerticalBox);
	Table->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Txt(TEXT("ACTION"))).Font(Font(11)).ColorAndOpacity(Dim) ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(260.f)[ SNew(STextBlock).Text(Txt(TEXT("KEYBOARD"))).Font(Font(11)).ColorAndOpacity(Dim) ] ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(260.f)[ SNew(STextBlock).Text(Txt(TEXT("GAMEPAD"))).Font(Font(11)).ColorAndOpacity(Dim) ] ]
	];
	for (const TArray<FString>& Row : Rows)
	{
		Table->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(Txt(Row[0])).Font(Font(13)).ColorAndOpacity(FLinearColor::White) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(260.f)[ SNew(STextBlock).Text(Txt(Row[1])).Font(Font(13, false)).ColorAndOpacity(Gold) ] ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(260.f)[ SNew(STextBlock).Text(Txt(Row[2])).Font(Font(13, false)).ColorAndOpacity(FLinearColor(0.55f, 0.75f, 1.f)) ] ]
		];
	}
	TSharedRef<SButton> BackButton = MakeButton(TEXT("BACK"), FOnClicked::CreateLambda([this]() { Back(); return FReply::Handled(); }), 220.f);
	ControlsBackButton = BackButton;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1200.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)[ SNew(STextBlock).Text(Txt(TEXT("CONTROLS"))).Font(Font(30)).ColorAndOpacity(Gold) ]
					+ SVerticalBox::Slot().AutoHeight()[ Table ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 24.f, 0.f, 0.f)[ BackButton ],
					FMargin(40.f, 30.f))
			]
		];
}

TSharedRef<SWidget> SPKMenu::BuildPause()
{
	TSharedRef<SButton> Resume = MakeButton(TEXT("RESUME"), FOnClicked::CreateLambda([this]()
	{
		if (Owner.IsValid()) { Owner->MenuResume(); }
		return FReply::Handled();
	}), 380.f, true);
	PauseFirstButton = Resume;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(SBox).WidthOverride(330.f).HeightOverride(149.f)[ SNew(SImage).Image(&LogoBrush) ] ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 18.f)[ SNew(STextBlock).Text(Txt(TEXT("PAUSED"))).Font(Font(26)).ColorAndOpacity(Gold) ]
				+ SVerticalBox::Slot().AutoHeight()[ Resume ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("SUBSTITUTIONS"), FOnClicked::CreateLambda([this]()
					{
						ShowPage(EPKMenuPage::Subs);
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("RESTART MATCH"), FOnClicked::CreateLambda([this]()
					{
						if (Owner.IsValid()) { Owner->MenuRestart(); }
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("SETTINGS"), FOnClicked::CreateLambda([this]()
					{
						ShowPage(EPKMenuPage::Settings);
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("MAIN MENU"), FOnClicked::CreateLambda([this]()
					{
						if (Owner.IsValid()) { Owner->MenuMainMenu(); }
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("QUIT GAME"), FOnClicked::CreateLambda([this]()
					{
						if (Owner.IsValid()) { Owner->MenuQuit(); }
						return FReply::Handled();
					}), 380.f)
				],
				FMargin(40.f, 30.f))
		];
}

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
