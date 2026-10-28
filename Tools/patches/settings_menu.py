"""Menu: SETTINGS page wiring (home + pause buttons, key capture, controls page from the bindings)."""
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


h = load("PKMenu.h")
h = rep(h, "\tVersus     // local 1v1: each player picks a side with their own device\n};",
        "\tVersus,    // local 1v1: each player picks a side with their own device\n\tSettings   // controls (key bindings), graphics, gameplay\n};")
h = rep(h, "\tvoid RebuildTeamSetup();\n",
        "\tvoid RebuildTeamSetup();\n\tvoid RebuildSettings();\n\tbool HandleKeyCapture(const FKey& Key);\n"
        "\tTSharedRef<SWidget> SettingRow(const FString& Label, const FString& Value, TFunction<void(int32)> Step, const FString& Hint = FString());\n")
h = rep(h, "\tTSharedPtr<SBox> TeamSetupBox;\n",
        "\tTSharedPtr<SBox> TeamSetupBox;\n\tTSharedPtr<SBox> SettingsBox;\n\tTSharedPtr<SBox> ControlsBox;\n"
        "\tint32 SettingsTab = 0;\n\tint32 CaptureAction = INDEX_NONE; // key rebinding in progress\n\tint32 CaptureSlot = 0;\n"
        "\tEPKMenuPage SettingsReturn = EPKMenuPage::Home;\n")
save("PKMenu.h", h)

c = load("PKMenu.cpp")
c = rep(c, '#include "PKInput.h"\n', '#include "PKInput.h"\n#include "PKSettings.h"\n')
c = rep(c, "\t\t\t+ SWidgetSwitcher::Slot()[ BuildControls() ]\n", "\t\t\t+ SWidgetSwitcher::Slot()[ SAssignNew(ControlsBox, SBox) ]\n")
c = rep(c, "\t\t\t+ SWidgetSwitcher::Slot()[ BuildVersusShell() ]\n", "\t\t\t+ SWidgetSwitcher::Slot()[ BuildVersusShell() ]\n\t\t\t+ SWidgetSwitcher::Slot()[ SAssignNew(SettingsBox, SBox) ]\n")
c = rep(c, "void SPKMenu::ShowPage(EPKMenuPage InPage)\n{\n\tPage = InPage;",
        "void SPKMenu::ShowPage(EPKMenuPage InPage)\n{\n"
        "\tif (InPage == EPKMenuPage::Settings && Page != EPKMenuPage::Settings)\n\t{\n"
        "\t\tSettingsReturn = Page == EPKMenuPage::Pause ? EPKMenuPage::Pause : EPKMenuPage::Home;\n"
        "\t\tSettingsTab = 0;\n\t\tCaptureAction = INDEX_NONE;\n\t\tFocusSelector = TEXT(\"TAB0\");\n\t}\n"
        "\tPage = InPage;")
c = rep(c, "\tcase EPKMenuPage::Controls:\n\t\tFocus(ControlsBackButton);\n\t\tbreak;",
        "\tcase EPKMenuPage::Controls:\n\t\tControlsBox->SetContent(BuildControls()); // shows the current key bindings\n\t\tFocus(ControlsBackButton);\n\t\tbreak;\n"
        "\tcase EPKMenuPage::Settings:\n\t\tRebuildSettings();\n\t\tbreak;")
c = rep(c, "\tcase EPKMenuPage::Controls:\n\tcase EPKMenuPage::Versus:\n\t\tShowPage(EPKMenuPage::Home);\n\t\tbreak;",
        "\tcase EPKMenuPage::Controls:\n\tcase EPKMenuPage::Versus:\n\t\tShowPage(EPKMenuPage::Home);\n\t\tbreak;\n"
        "\tcase EPKMenuPage::Settings:\n"
        "\t\tif (UPKGameInstance* G = GI())\n\t\t{\n\t\t\tG->SaveSettings();\n\t\t}\n"
        "\t\tif (Owner.IsValid())\n\t\t{\n\t\t\tOwner->ApplyUserSettings();\n\t\t}\n"
        "\t\tShowPage(SettingsReturn);\n\t\tbreak;")
c = rep(c, "\tconst FKey Key = KeyEvent.GetKey();\n",
        "\tconst FKey Key = KeyEvent.GetKey();\n\tif (HandleKeyCapture(Key))\n\t{\n\t\treturn FReply::Handled();\n\t}\n")
# home: SETTINGS button after TEAM SETUP
c = rep(c, """				MakeButton(TEXT("CONTROLS"), FOnClicked::CreateLambda([this]()
				{
					ShowPage(EPKMenuPage::Controls);
					return FReply::Handled();
				}))
			]""", """				MakeButton(TEXT("SETTINGS"), FOnClicked::CreateLambda([this]()
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
			]""")
# pause: SETTINGS button before MAIN MENU
c = rep(c, """					MakeButton(TEXT("MAIN MENU"), FOnClicked::CreateLambda([this]()""", """					MakeButton(TEXT("SETTINGS"), FOnClicked::CreateLambda([this]()
					{
						ShowPage(EPKMenuPage::Settings);
						return FReply::Handled();
					}), 380.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeButton(TEXT("MAIN MENU"), FOnClicked::CreateLambda([this]()""")
# controls page rows from the bindings
start = c.index("\tconst TCHAR* Rows[][3] = {")
end = c.index("\tTSharedRef<SVerticalBox> Table = SNew(SVerticalBox);")
c = c[:start] + """	FPKUserSettings Bindings;
	if (const UPKGameInstance* G = GI())
	{
		Bindings = G->Settings;
	}
	TArray<TArray<FString>> Rows;
	for (int32 a = 0; a < (int32)EPKInput::Count; ++a)
	{
		Rows.Add({ PKInputs::Info((EPKInput)a).Name, Bindings.KeyText((EPKInput)a), PKInputs::PadText((EPKInput)a) });
	}
	Rows.Add({ TEXT("Team play style"), TEXT("1 - 5"), TEXT("D-pad left / right") });
	Rows.Add({ TEXT("Help  -  shirt numbers"), TEXT("H  -  N"), TEXT("") });
""" + c[end:]
c = rep(c, "\tfor (const auto& Row : Rows)\n", "\tfor (const TArray<FString>& Row : Rows)\n")
save("PKMenu.cpp", c)
print("settings menu ok")
