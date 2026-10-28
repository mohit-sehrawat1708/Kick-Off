#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"

class APKPlayerController;
class UPKGameInstance;
class UTexture2D;
class SBox;
class SButton;
class SWidgetSwitcher;

enum class EPKMenuPage : uint8
{
	Landing,   // logo, "press any key"
	Home,      // main menu + next match card
	TeamSetup, // formation, style, opponent, starting XI
	Controls,
	Pause,
	Versus,    // local 1v1: each player picks a side with their own device
	Settings,  // controls (key bindings), graphics, gameplay
	Subs       // substitutions (from the pause menu)
};

// Code-built front end (Slate): landing page, home, team setup (player selection), controls, pause.
// Mouse, keyboard (arrows / Enter / Esc) and gamepad (D-pad / A / B) all work.
class SPKMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPKMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APKPlayerController>, Owner)
		SLATE_ARGUMENT(UTexture2D*, Logo)
		SLATE_ARGUMENT(UTexture2D*, Crest)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void ShowPage(EPKMenuPage InPage);
	EPKMenuPage GetPage() const { return Page; }
	// Self-test hooks (-PKMenuShots).
	void TestClickSlot(int32 Slot) { ClickSlot(Slot); }
	void TestClickSquadPlayer(int32 Index) { ClickSquadPlayer(Index); }
	void TestVersusTick() { TickVersus(); }
	void TestSettingsTab(int32 Tab) { SettingsTab = Tab; RebuildSettings(); }
	void TestStartCapture(int32 Action, int32 Slot) { CaptureAction = Action; CaptureSlot = Slot; RebuildSettings(); }
	bool TestKey(const FKey& Key) { return HandleKeyCapture(Key); }
	void TestSubsSlot(int32 Slot) { SubsSlot = Slot; RebuildSubs(); }
	bool TestSubsBench(int32 Nth); // brings on the Nth available bench player for the picked slot

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	UPKGameInstance* GI() const;
	void Back();
	void Focus(const TSharedPtr<SWidget>& Widget);

	TSharedRef<SWidget> BuildLanding();
	TSharedRef<SWidget> BuildHome();
	TSharedRef<SWidget> BuildControls();
	TSharedRef<SWidget> BuildPause();
	void RefreshHome();
	void RebuildTeamSetup();
	void RebuildSettings();
	void RebuildSubs();
	TSharedPtr<SBox> SubsBox;
	int32 SubsTeam = 0;
	int32 SubsSlot = INDEX_NONE; // on-pitch player picked to come off
	int32 SubsFocus = 0;
	bool HandleKeyCapture(const FKey& Key);
	TSharedRef<SWidget> SettingRow(const FString& Label, const FString& Value, TFunction<void(int32)> Step, const FString& Hint = FString());
	TSharedRef<SWidget> BuildVersusShell();
	void RebuildVersus();
	void TickVersus();
	void MoveDevice(int32 Device, int32 Step);
	bool VersusReady(int32& OutP1, int32& OutP2) const;
	void TryStartVersus();

	TSharedRef<SWidget> BuildMatchCard();
	TSharedRef<SWidget> BuildPitch();
	TSharedRef<SWidget> BuildSquadList();
	TSharedRef<SWidget> BuildPlayerDetail();
	TSharedRef<SWidget> BuildSelector(const FString& Label, const FString& Value, FOnClicked OnPrev, FOnClicked OnNext);

	// A big menu button that turns gold when hovered or focused.
	TSharedRef<SButton> MakeButton(const FString& Label, FOnClicked OnClicked, float Width = 420.f, bool bPrimary = false, bool bFocusable = true);
	TSharedRef<SWidget> Panel(const TSharedRef<SWidget>& Content, const FMargin& Padding = FMargin(24.f));

	void ClickSlot(int32 Slot);
	void ClickSquadPlayer(int32 SquadIndex);

	TWeakObjectPtr<APKPlayerController> Owner;
	EPKMenuPage Page = EPKMenuPage::Landing;

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SBox> HomeCardBox;
	TSharedPtr<SBox> TeamSetupBox;
	TSharedPtr<SBox> SettingsBox;
	TSharedPtr<SBox> ControlsBox;
	int32 SettingsTab = 0;
	int32 CaptureAction = INDEX_NONE; // key rebinding in progress
	int32 CaptureSlot = 0;
	EPKMenuPage SettingsReturn = EPKMenuPage::Home;
	TSharedPtr<SBox> VersusBox;
	TMap<int32, int32> VersusSide; // device -> -1 BLUE (P1), 0 not playing, +1 RED (P2)
	TArray<int32> VersusDevices;
	uint64 MenuSeq = 0;
	TSharedPtr<SButton> HomeFirstButton;
	TSharedPtr<SButton> ControlsBackButton;
	TSharedPtr<SButton> PauseFirstButton;
	TSharedPtr<SButton> SetupKickOffButton;
	TArray<TSharedPtr<SButton>> SlotButtons;
	TArray<TSharedPtr<SButton>> RowButtons;
	TMap<FString, TSharedPtr<SButton>> NamedButtons; // selector arrows etc., for focus after a rebuild

	// Team setup state.
	int32 SelectedSlot = INDEX_NONE;
	int32 DetailPlayer = INDEX_NONE;
	int32 FocusSlot = INDEX_NONE; // restore focus after a rebuild
	int32 FocusRow = INDEX_NONE;
	FString FocusSelector;

	FSlateBrush LogoBrush;
	FSlateBrush CrestBrush;
	FSlateBrush WhiteBrush;
	FSlateBrush PanelBrush;
	FSlateBrush CircleBrush;
	FSlateBrush OutlineBrush;
	FSlateBrush CircleOutlineBrush;
	FButtonStyle MenuButtonStyle;
	FButtonStyle FlatButtonStyle;
	double OpenTime = 0.0;
	TWeakPtr<SWidget> PendingFocus; // applied once the page is laid out
	int32 PendingFocusFrames = 0;
};
