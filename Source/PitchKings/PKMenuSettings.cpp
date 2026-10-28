// Settings page of the front end (SPKMenu): CONTROLS (key bindings, stick), GRAPHICS (UGameUserSettings),
// GAMEPLAY (match length, difficulty, camera, HUD).
#include "PKMenu.h"
#include "PKGameInstance.h"
#include "PKPlayerController.h"
#include "PKSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor SGold(1.f, 0.76f, 0.1f);
	const FLinearColor SDim(0.68f, 0.73f, 0.84f);

	FSlateFontInfo SFont(int32 Size, bool bBold = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FText STxt(const FString& S)
	{
		return FText::FromString(S);
	}

	UGameUserSettings* GUS()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}

	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	const TCHAR* WindowNames[] = { TEXT("Fullscreen"), TEXT("Borderless window"), TEXT("Windowed") };
	const int32 FrameLimits[] = { 30, 60, 90, 120, 144, 240, 0 };
	const int32 MatchLengths[] = { 2, 4, 6, 8, 10, 15 };
	const float RumbleLevels[] = { 0.f, 0.35f, 0.7f, 1.f };
	const TCHAR* RumbleNames[] = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High") };

	TArray<FIntPoint> Resolutions()
	{
		TArray<FIntPoint> List;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(List);
		for (const FIntPoint R : { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080) })
		{
			List.AddUnique(R);
		}
		if (UGameUserSettings* S = GUS())
		{
			List.AddUnique(S->GetScreenResolution());
		}
		List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X); });
		return List;
	}

	template <typename T, int32 N>
	int32 IndexOf(const T (&Values)[N], const T& Value)
	{
		for (int32 i = 0; i < N; ++i)
		{
			if (Values[i] == Value)
			{
				return i;
			}
		}
		return 0;
	}
}

TSharedRef<SWidget> SPKMenu::SettingRow(const FString& Label, const FString& Value, TFunction<void(int32)> Step, const FString& Hint)
{
	// Label on the left, < value > on the right; the arrows change the value by one step.
	auto Make = [this, Step, Label](int32 Dir)
	{
		return FOnClicked::CreateLambda([this, Step, Label, Dir]()
		{
			Step(Dir);
			FocusSelector = Label + (Dir < 0 ? TEXT("<") : TEXT(">"));
			FocusSlot = INDEX_NONE;
			FocusRow = INDEX_NONE;
			RebuildSettings();
			return FReply::Handled();
		});
	};
	TSharedRef<SButton> Prev = MakeButton(TEXT("<"), Make(-1), 56.f);
	TSharedRef<SButton> Next = MakeButton(TEXT(">"), Make(1), 56.f);
	NamedButtons.Add(Label + TEXT("<"), Prev);
	NamedButtons.Add(Label + TEXT(">"), Next);
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(STxt(Label)).Font(SFont(15)).ColorAndOpacity(FLinearColor::White) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(STxt(Hint)).Font(SFont(10, false)).ColorAndOpacity(SDim).Visibility(Hint.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Prev ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(270.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(STxt(Value)).Font(SFont(15)).ColorAndOpacity(SGold)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Next ];
}

void SPKMenu::RebuildSettings()
{
	UPKGameInstance* G = GI();
	if (!SettingsBox.IsValid() || !G)
	{
		return;
	}
	NamedButtons.Reset();
	FPKUserSettings& S = G->Settings;
	auto Changed = [this]()
	{
		if (UPKGameInstance* G2 = GI())
		{
			G2->SaveSettings();
		}
		if (Owner.IsValid())
		{
			Owner->ApplyUserSettings();
		}
	};

	// Tabs.
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	const TCHAR* TabNames[] = { TEXT("CONTROLS"), TEXT("GRAPHICS"), TEXT("GAMEPLAY") };
	for (int32 t = 0; t < 3; ++t)
	{
		TSharedRef<SButton> Tab = MakeButton(TabNames[t], FOnClicked::CreateLambda([this, t]()
		{
			SettingsTab = t;
			CaptureAction = INDEX_NONE;
			FocusSelector = FString::Printf(TEXT("TAB%d"), t);
			RebuildSettings();
			return FReply::Handled();
		}), 250.f, t == SettingsTab);
		NamedButtons.Add(FString::Printf(TEXT("TAB%d"), t), Tab);
		Tabs->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[ Tab ];
	}

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto AddRow = [&Rows](const TSharedRef<SWidget>& W)
	{
		Rows->AddSlot().AutoHeight().Padding(0.f, 5.f)[ W ];
	};

	if (SettingsTab == 0)
	{
		// ---------------------------------------------------------------- CONTROLS
		AddRow(SettingRow(TEXT("Invert left / right"), S.bInvertX ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bInvertX = !GI()->Settings.bInvertX; Changed(); }, TEXT("flip the horizontal direction of keys and stick")));
		AddRow(SettingRow(TEXT("Invert up / down"), S.bInvertY ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bInvertY = !GI()->Settings.bInvertY; Changed(); }, TEXT("flip the vertical direction")));
		AddRow(SettingRow(TEXT("Stick dead zone"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(S.DeadZone * 100.f)),
			[this, Changed](int32 Dir) { float& D = GI()->Settings.DeadZone; D = FMath::Clamp(D + Dir * 0.05f, 0.05f, 0.4f); Changed(); }, TEXT("raise it if your player drifts with the stick untouched")));
		AddRow(SettingRow(TEXT("Controller vibration"), RumbleNames[IndexOf(RumbleLevels, S.Rumble)],
			[this, Changed](int32 Dir) { const int32 I = FMath::Clamp(IndexOf(RumbleLevels, GI()->Settings.Rumble) + Dir, 0, 3); GI()->Settings.Rumble = RumbleLevels[I]; Changed(); }));

		Rows->AddSlot().AutoHeight().Padding(0.f, 16.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(STxt(CaptureAction != INDEX_NONE ? TEXT("PRESS THE NEW KEY  -  Esc cancels, Backspace clears") : TEXT("KEYBOARD  (click a key to change it)"))).Font(SFont(12)).ColorAndOpacity(CaptureAction != INDEX_NONE ? SGold : SDim) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(200.f)[ SNew(STextBlock).Text(STxt(TEXT("KEY 1"))).Font(SFont(11)).ColorAndOpacity(SDim) ] ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(200.f)[ SNew(STextBlock).Text(STxt(TEXT("KEY 2"))).Font(SFont(11)).ColorAndOpacity(SDim) ] ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(170.f)[ SNew(STextBlock).Text(STxt(TEXT("GAMEPAD"))).Font(SFont(11)).ColorAndOpacity(SDim) ] ]
		];
		for (int32 a = 0; a < (int32)EPKInput::Count; ++a)
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ SNew(STextBlock).Text(STxt(PKInputs::Info((EPKInput)a).Name)).Font(SFont(13)).ColorAndOpacity(FLinearColor::White) ];
			for (int32 Slot = 0; Slot < 2; ++Slot)
			{
				const bool bCapturing = CaptureAction == a && CaptureSlot == Slot;
				const FString Label = bCapturing ? FString(TEXT("press a key...")) : PKInputs::KeyName(S.Keys[a][Slot]);
				TSharedRef<SButton> KeyButton = MakeButton(Label, FOnClicked::CreateLambda([this, a, Slot]()
				{
					CaptureAction = a;
					CaptureSlot = Slot;
					FocusSelector = FString::Printf(TEXT("KEY%d_%d"), a, Slot);
					RebuildSettings();
					return FReply::Handled();
				}), 190.f, bCapturing);
				NamedButtons.Add(FString::Printf(TEXT("KEY%d_%d"), a, Slot), KeyButton);
				Row->AddSlot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f).VAlign(VAlign_Center)[ KeyButton ];
			}
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(170.f)[ SNew(STextBlock).Text(STxt(PKInputs::PadText((EPKInput)a))).Font(SFont(12, false)).ColorAndOpacity(FLinearColor(0.55f, 0.75f, 1.f)) ] ];
			Rows->AddSlot().AutoHeight().Padding(0.f, 2.f)[ Row ];
		}
		TSharedRef<SButton> ResetKeys = MakeButton(TEXT("RESET KEYS TO DEFAULT"), FOnClicked::CreateLambda([this, Changed]()
		{
			GI()->Settings.ResetKeys();
			GI()->Settings.bInvertX = GI()->Settings.bInvertY = false;
			CaptureAction = INDEX_NONE;
			Changed();
			FocusSelector = TEXT("RESETKEYS");
			RebuildSettings();
			return FReply::Handled();
		}), 360.f);
		NamedButtons.Add(TEXT("RESETKEYS"), ResetKeys);
		Rows->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f, 0.f, 0.f)[ ResetKeys ];
	}
	else if (SettingsTab == 1)
	{
		// ---------------------------------------------------------------- GRAPHICS
		UGameUserSettings* U = GUS();
		if (U)
		{
			auto Apply = [](UGameUserSettings* X) { X->ApplySettings(false); };
			const int32 Mode = (int32)U->GetFullscreenMode();
			AddRow(SettingRow(TEXT("Display mode"), WindowNames[FMath::Clamp(Mode, 0, 2)], [Apply](int32 Dir)
			{
				UGameUserSettings* X = GUS();
				X->SetFullscreenMode((EWindowMode::Type)((((int32)X->GetFullscreenMode() + Dir) % 3 + 3) % 3));
				Apply(X);
			}));
			const TArray<FIntPoint> Res = Resolutions();
			const FIntPoint Cur = U->GetScreenResolution();
			AddRow(SettingRow(TEXT("Resolution"), FString::Printf(TEXT("%d x %d"), Cur.X, Cur.Y), [Res, Apply](int32 Dir)
			{
				UGameUserSettings* X = GUS();
				const int32 I = FMath::Clamp(Res.IndexOfByKey(X->GetScreenResolution()) + Dir, 0, Res.Num() - 1);
				X->SetScreenResolution(Res[I]);
				Apply(X);
			}));
			const int32 Overall = U->GetOverallScalabilityLevel();
			AddRow(SettingRow(TEXT("Graphics quality"), Overall >= 0 ? QualityNames[FMath::Clamp(Overall, 0, 4)] : TEXT("Custom"), [Apply](int32 Dir)
			{
				UGameUserSettings* X = GUS();
				const int32 L = X->GetOverallScalabilityLevel() < 0 ? 2 : X->GetOverallScalabilityLevel();
				X->SetOverallScalabilityLevel(FMath::Clamp(L + Dir, 0, 4));
				Apply(X);
			}, TEXT("sets all the options below at once")));
			AddRow(SettingRow(TEXT("Shadows"), QualityNames[FMath::Clamp(U->GetShadowQuality(), 0, 4)], [Apply](int32 Dir) { UGameUserSettings* X = GUS(); X->SetShadowQuality(FMath::Clamp(X->GetShadowQuality() + Dir, 0, 4)); Apply(X); }));
			AddRow(SettingRow(TEXT("Textures"), QualityNames[FMath::Clamp(U->GetTextureQuality(), 0, 4)], [Apply](int32 Dir) { UGameUserSettings* X = GUS(); X->SetTextureQuality(FMath::Clamp(X->GetTextureQuality() + Dir, 0, 4)); Apply(X); }));
			AddRow(SettingRow(TEXT("Effects"), QualityNames[FMath::Clamp(U->GetVisualEffectQuality(), 0, 4)], [Apply](int32 Dir) { UGameUserSettings* X = GUS(); X->SetVisualEffectQuality(FMath::Clamp(X->GetVisualEffectQuality() + Dir, 0, 4)); Apply(X); }));
			AddRow(SettingRow(TEXT("Anti-aliasing"), QualityNames[FMath::Clamp(U->GetAntiAliasingQuality(), 0, 4)], [Apply](int32 Dir) { UGameUserSettings* X = GUS(); X->SetAntiAliasingQuality(FMath::Clamp(X->GetAntiAliasingQuality() + Dir, 0, 4)); Apply(X); }));
			AddRow(SettingRow(TEXT("View distance"), QualityNames[FMath::Clamp(U->GetViewDistanceQuality(), 0, 4)], [Apply](int32 Dir) { UGameUserSettings* X = GUS(); X->SetViewDistanceQuality(FMath::Clamp(X->GetViewDistanceQuality() + Dir, 0, 4)); Apply(X); }));
			float ScaleNorm = 1.f, ScaleValue = 100.f, ScaleMin = 50.f, ScaleMax = 100.f;
			U->GetResolutionScaleInformationEx(ScaleNorm, ScaleValue, ScaleMin, ScaleMax);
			if (ScaleValue <= 1.f)
			{
				ScaleValue = 100.f; // not applied yet: full resolution
			}
			AddRow(SettingRow(TEXT("Render scale"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(ScaleValue)), [Apply](int32 Dir)
			{
				UGameUserSettings* X = GUS();
				float N = 1.f, V = 100.f, Min = 50.f, Max = 100.f;
				X->GetResolutionScaleInformationEx(N, V, Min, Max);
				V = V <= 1.f ? 100.f : V;
				Max = Max <= 1.f ? 100.f : Max;
				X->SetResolutionScaleValueEx(FMath::Clamp(FMath::RoundToFloat(V / 10.f) * 10.f + Dir * 10.f, 50.f, FMath::Max(Max, 50.f)));
				Apply(X);
			}, TEXT("below 100 % renders fewer pixels: faster on weak graphics cards")));
			AddRow(SettingRow(TEXT("VSync"), U->IsVSyncEnabled() ? TEXT("On") : TEXT("Off"), [Apply](int32) { UGameUserSettings* X = GUS(); X->SetVSyncEnabled(!X->IsVSyncEnabled()); Apply(X); }, TEXT("no screen tearing, may add a little input delay")));
			const int32 Limit = FMath::RoundToInt(U->GetFrameRateLimit());
			AddRow(SettingRow(TEXT("Frame rate limit"), Limit > 0 ? FString::Printf(TEXT("%d FPS"), Limit) : FString(TEXT("Unlimited")), [Apply](int32 Dir)
			{
				UGameUserSettings* X = GUS();
				const int32 I = FMath::Clamp(IndexOf(FrameLimits, FMath::RoundToInt(X->GetFrameRateLimit())) + Dir, 0, (int32)UE_ARRAY_COUNT(FrameLimits) - 1);
				X->SetFrameRateLimit((float)FrameLimits[I]);
				Apply(X);
			}));
			AddRow(SettingRow(TEXT("Show FPS counter"), S.bShowFPS ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bShowFPS = !GI()->Settings.bShowFPS; Changed(); }));
			TSharedRef<SButton> Auto = MakeButton(TEXT("AUTO-DETECT BEST SETTINGS"), FOnClicked::CreateLambda([this, Apply]()
			{
				UGameUserSettings* X = GUS();
				X->RunHardwareBenchmark();
				X->ApplyHardwareBenchmarkResults();
				FocusSelector = TEXT("AUTOGFX");
				RebuildSettings();
				return FReply::Handled();
			}), 420.f);
			NamedButtons.Add(TEXT("AUTOGFX"), Auto);
			Rows->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f, 0.f, 0.f)[ Auto ];
		}
	}
	else
	{
		// ---------------------------------------------------------------- GAMEPLAY
		AddRow(SettingRow(TEXT("Match length"), FString::Printf(TEXT("%d minutes"), S.MatchMinutes),
			[this, Changed](int32 Dir) { int32& M = GI()->Settings.MatchMinutes; M = MatchLengths[FMath::Clamp(IndexOf(MatchLengths, M) + Dir, 0, (int32)UE_ARRAY_COUNT(MatchLengths) - 1)]; Changed(); }, TEXT("applies from the next kick-off")));
		const TCHAR* Difficulties[] = { TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
		AddRow(SettingRow(TEXT("Difficulty"), Difficulties[S.Difficulty],
			[this, Changed](int32 Dir) { int32& D = GI()->Settings.Difficulty; D = FMath::Clamp(D + Dir, 0, 2); Changed(); }, TEXT("ratings of the computer team (single player)")));
		const TCHAR* Cameras[] = { TEXT("Near"), TEXT("Normal"), TEXT("Far") };
		AddRow(SettingRow(TEXT("Camera distance"), Cameras[S.CameraZoom],
			[this, Changed](int32 Dir) { int32& Z = GI()->Settings.CameraZoom; Z = FMath::Clamp(Z + Dir, 0, 2); Changed(); }));
		AddRow(SettingRow(TEXT("Shirt numbers"), S.bShowNumbers ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bShowNumbers = !GI()->Settings.bShowNumbers; Changed(); }));
		AddRow(SettingRow(TEXT("Controls help panel"), S.bShowHelp ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bShowHelp = !GI()->Settings.bShowHelp; Changed(); }));
		AddRow(SettingRow(TEXT("Mini-map"), S.bShowRadar ? TEXT("On") : TEXT("Off"), [this, Changed](int32) { GI()->Settings.bShowRadar = !GI()->Settings.bShowRadar; Changed(); }));
		TSharedRef<SButton> ResetAll = MakeButton(TEXT("RESET ALL SETTINGS"), FOnClicked::CreateLambda([this, Changed]()
		{
			GI()->Settings.ResetAll();
			Changed();
			FocusSelector = TEXT("RESETALL");
			RebuildSettings();
			return FReply::Handled();
		}), 360.f);
		NamedButtons.Add(TEXT("RESETALL"), ResetAll);
		Rows->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f, 0.f, 0.f)[ ResetAll ];
	}

	TSharedRef<SButton> BackButton = MakeButton(TEXT("BACK"), FOnClicked::CreateLambda([this]() { Back(); return FReply::Handled(); }), 220.f);
	NamedButtons.Add(TEXT("BACK"), BackButton);

	SettingsBox->SetContent(
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1240.f).HeightOverride(940.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(STxt(TEXT("SETTINGS"))).Font(SFont(30)).ColorAndOpacity(SGold) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Text(STxt(TEXT("Saved automatically"))).Font(SFont(11, false)).ColorAndOpacity(SDim) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)[ Tabs ]
					+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, 14.f, 0.f)[ Rows ] ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f, 0.f, 0.f)[ BackButton ],
					FMargin(36.f, 26.f))
			]
		]);

	TSharedPtr<SWidget> Target = NamedButtons.FindRef(FocusSelector);
	if (!Target.IsValid())
	{
		Target = NamedButtons.FindRef(FString::Printf(TEXT("TAB%d"), SettingsTab));
	}
	Focus(Target);
}

bool SPKMenu::HandleKeyCapture(const FKey& Key)
{
	// Rebinding: the next key pressed goes to the chosen action (Esc cancels, Backspace / Delete clears).
	if (Page != EPKMenuPage::Settings || CaptureAction == INDEX_NONE)
	{
		return false;
	}
	UPKGameInstance* G = GI();
	if (Key.IsGamepadKey() || Key.IsMouseButton() || !G)
	{
		return true; // keyboard only; swallow the rest while waiting
	}
	if (Key == EKeys::BackSpace || Key == EKeys::Delete)
	{
		G->Settings.Bind((EPKInput)CaptureAction, CaptureSlot, FKey());
	}
	else if (Key != EKeys::Escape)
	{
		G->Settings.Bind((EPKInput)CaptureAction, CaptureSlot, Key);
	}
	FocusSelector = FString::Printf(TEXT("KEY%d_%d"), CaptureAction, CaptureSlot);
	CaptureAction = INDEX_NONE;
	G->SaveSettings();
	RebuildSettings();
	return true;
}
