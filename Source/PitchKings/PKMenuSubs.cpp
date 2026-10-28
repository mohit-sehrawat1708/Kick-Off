// Substitutions page of the front end (SPKMenu), reached from the pause menu: pick the player to come off
// (stamina shown), then who comes on from the bench. Up to APKGameMode::MaxSubs changes; keepers only for keepers.
#include "PKMenu.h"
#include "PKGameMode.h"
#include "PKPlayerController.h"
#include "PKTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor UGold(1.f, 0.76f, 0.1f);
	const FLinearColor UDim(0.68f, 0.73f, 0.84f);

	FSlateFontInfo SubsFont(int32 Size, bool bBold = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FText UTxt(const FString& S)
	{
		return FText::FromString(S);
	}
}

void SPKMenu::RebuildSubs()
{
	APKGameMode* GM = Owner.IsValid() && Owner->GetWorld() ? Owner->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
	if (!GM || !SubsBox.IsValid())
	{
		return;
	}
	TArray<APKGameMode::FPKSubEntry> OnPitch, Bench;
	GM->GetSubsInfo(SubsTeam, OnPitch, Bench);
	const int32 Left = GM->SubsLeft(SubsTeam);

	TSharedRef<SVerticalBox> PitchList = SNew(SVerticalBox);
	TSharedRef<SVerticalBox> BenchList = SNew(SVerticalBox);
	TSharedPtr<SButton> FocusButton;
	for (const APKGameMode::FPKSubEntry& E : OnPitch)
	{
		const int32 Slot = E.Slot;
		const FString Label = FString::Printf(TEXT("%s   #%d %s   %d%%"), *E.Pos, E.Number, *E.Name, FMath::RoundToInt(E.Stamina * 100.f));
		TSharedRef<SButton> B = MakeButton(Label, FOnClicked::CreateLambda([this, Slot]()
		{
			SubsSlot = SubsSlot == Slot ? INDEX_NONE : Slot;
			RebuildSubs();
			return FReply::Handled();
		}), 430.f, Slot == SubsSlot);
		B->SetEnabled(E.bAvailable && Left > 0);
		if ((SubsSlot == INDEX_NONE && !FocusButton.IsValid() && E.bAvailable) || Slot == SubsSlot)
		{
			FocusButton = B;
		}
		PitchList->AddSlot().AutoHeight().Padding(0.f, 2.f)[ B ];
	}
	bool bSlotKeeper = false;
	for (const APKGameMode::FPKSubEntry& E : OnPitch)
	{
		bSlotKeeper |= E.Slot == SubsSlot && E.bKeeper;
	}
	TSharedPtr<SButton> FirstBench;
	for (const APKGameMode::FPKSubEntry& E : Bench)
	{
		const bool bFits = E.bKeeper == bSlotKeeper; // keepers only for keepers
		const int32 Squad = E.Squad;
		const FString Label = FString::Printf(TEXT("%s   #%d %s   OVR %d%s"), *E.Pos, E.Number, *E.Name, E.Rating, E.bAvailable ? TEXT("") : TEXT("   (used)"));
		TSharedRef<SButton> B = MakeButton(Label, FOnClicked::CreateLambda([this, Squad]()
		{
			APKGameMode* G = Owner.IsValid() && Owner->GetWorld() ? Owner->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
			if (G && SubsSlot != INDEX_NONE)
			{
				if (G->MakeSub(SubsTeam, SubsSlot, Squad))
				{
					SubsSlot = INDEX_NONE;
				}
			}
			RebuildSubs();
			return FReply::Handled();
		}), 430.f);
		B->SetEnabled(E.bAvailable && bFits && SubsSlot != INDEX_NONE && Left > 0);
		if (E.bAvailable && bFits && SubsSlot != INDEX_NONE && !FirstBench.IsValid())
		{
			FirstBench = B;
		}
		BenchList->AddSlot().AutoHeight().Padding(0.f, 2.f)[ B ];
	}
	if (FirstBench.IsValid())
	{
		FocusButton = FirstBench; // picked who comes off: now choose who comes on
	}

	TSharedRef<SButton> BackButton = MakeButton(TEXT("BACK"), FOnClicked::CreateLambda([this]()
	{
		Back();
		return FReply::Handled();
	}), 220.f);
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[ BackButton ];
	if (GM->IsVersus())
	{
		Buttons->AddSlot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
		[
			MakeButton(TEXT("OTHER TEAM"), FOnClicked::CreateLambda([this]()
			{
				SubsTeam = 1 - SubsTeam;
				SubsSlot = INDEX_NONE;
				RebuildSubs();
				return FReply::Handled();
			}), 260.f)
		];
	}
	if (!FocusButton.IsValid())
	{
		FocusButton = BackButton;
	}

	const FString Title = FString::Printf(TEXT("SUBSTITUTIONS  -  %s"), PK::TeamName(SubsTeam));
	const FString Hint = Left <= 0 ? FString(TEXT("No substitutions left."))
		: (SubsSlot == INDEX_NONE ? FString::Printf(TEXT("Pick the player to come off (%d of %d left). Stamina is shown in %%."), Left, APKGameMode::MaxSubs)
			: FString(TEXT("Now pick who comes on.  Keepers can only replace keepers.")));

	SubsBox->SetContent(
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(STextBlock).Text(UTxt(Title)).Font(SubsFont(24)).ColorAndOpacity(UGold) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 12.f)[ SNew(STextBlock).Text(UTxt(Hint)).Font(SubsFont(12, false)).ColorAndOpacity(UDim) ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SNew(STextBlock).Text(UTxt(TEXT("ON THE PITCH"))).Font(SubsFont(13)).ColorAndOpacity(UDim) ]
						+ SVerticalBox::Slot().AutoHeight()[ PitchList ]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(24.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SNew(STextBlock).Text(UTxt(TEXT("BENCH"))).Font(SubsFont(13)).ColorAndOpacity(UDim) ]
						+ SVerticalBox::Slot().AutoHeight()[ BenchList ]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)[ Buttons ],
				FMargin(30.f, 24.f))
		]);
	Focus(FocusButton);
}

bool SPKMenu::TestSubsBench(int32 Nth)
{
	APKGameMode* GM = Owner.IsValid() && Owner->GetWorld() ? Owner->GetWorld()->GetAuthGameMode<APKGameMode>() : nullptr;
	if (!GM || SubsSlot == INDEX_NONE)
	{
		return false;
	}
	TArray<APKGameMode::FPKSubEntry> OnPitch, Bench;
	GM->GetSubsInfo(SubsTeam, OnPitch, Bench);
	bool bSlotKeeper = false;
	for (const APKGameMode::FPKSubEntry& E : OnPitch)
	{
		bSlotKeeper |= E.Slot == SubsSlot && E.bKeeper;
	}
	for (const APKGameMode::FPKSubEntry& E : Bench)
	{
		if (E.bAvailable && E.bKeeper == bSlotKeeper && Nth-- == 0)
		{
			const bool bDone = GM->MakeSub(SubsTeam, SubsSlot, E.Squad);
			SubsSlot = bDone ? INDEX_NONE : SubsSlot;
			RebuildSubs();
			return bDone;
		}
	}
	return false;
}
