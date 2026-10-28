#include "PKHUD.h"
#include "PKBall.h"
#include "PKFootballer.h"
#include "PKGameMode.h"
#include "PKPlayerController.h"
#include "PKTypes.h"
#include "PKInput.h"
#include "PKSettings.h"
#include "PKGameInstance.h"
#include "Misc/App.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"

void APKHUD::DrawHUD()
{
	Super::DrawHUD();

	APKGameMode* GM = GetWorld()->GetAuthGameMode<APKGameMode>();
	if (!GM || !Canvas)
	{
		return;
	}

	if (GM->Phase == EPKPhase::Menu || GM->IsCriticHudHidden())
	{
		return; // the front end is drawn by the Slate menu; critic shots can hide the HUD
	}

	if (GM->IsAnimTest())
	{
		for (APKFootballer* P : GM->GetFootballers())
		{
			const FString Label = GM->GetAnimTestLabel(P);
			const FVector Screen = Project(P->GetActorLocation() + FVector(0.f, 0.f, 135.f));
			if (!Label.IsEmpty() && Screen.Z > 0.f)
			{
				float W = 0.f, H = 0.f;
				GetTextSize(Label, W, H, GEngine->GetMediumFont(), 1.f);
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Screen.X - W * 0.5f - 4.f, Screen.Y - 2.f, W + 8.f, H + 4.f);
				DrawText(Label, FLinearColor::White, Screen.X - W * 0.5f, Screen.Y, GEngine->GetMediumFont(), 1.f);
				// Foot markers so the striking foot and its height are readable in captures.
				const FVector R = Project(P->GetMesh()->GetSocketLocation(TEXT("ball_r")));
				const FVector L = Project(P->GetMesh()->GetSocketLocation(TEXT("ball_l")));
				DrawRect(FLinearColor(1.f, 0.1f, 0.1f), R.X - 4.f, R.Y - 4.f, 8.f, 8.f);
				DrawRect(FLinearColor(1.f, 1.f, 0.1f), L.X - 4.f, L.Y - 4.f, 8.f, 8.f);
			}
		}
		return;
	}

	DrawScoreboard(GM);
	const UPKGameInstance* SettingsGI = GetGameInstance<UPKGameInstance>();
	if (!SettingsGI || SettingsGI->Settings.bShowRadar)
	{
		DrawRadar(GM);
	}
	if (SettingsGI && SettingsGI->Settings.bShowFPS)
	{
		static float SmoothedFps = 60.f;
		const float Dt = FMath::Max((float)FApp::GetDeltaTime(), 0.0001f);
		SmoothedFps = FMath::Lerp(SmoothedFps, 1.f / Dt, 0.05f);
		const FString Fps = FString::Printf(TEXT("%.0f FPS"), SmoothedFps);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Canvas->ClipX * 0.5f - 46.f, 18.f, 92.f, 24.f);
		DrawText(Fps, SmoothedFps >= 50.f ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor(1.f, 0.6f, 0.2f), Canvas->ClipX * 0.5f - 34.f, 21.f, GEngine->GetSmallFont(), 1.1f);
	}
	if (GM->CardTimer > 0.f)
	{
		// Referee's card: a coloured card and who was booked.
		const float CX = Canvas->ClipX * 0.5f;
		const float CY = Canvas->ClipY * 0.36f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - 230.f, CY - 6.f, 460.f, 64.f);
		DrawRect(GM->CardColor, CX - 216.f, CY + 2.f, 34.f, 48.f);
		DrawText(GM->CardText, FLinearColor::White, CX - 170.f, CY + 14.f, GEngine->GetMediumFont(), 1.2f);
	}
	// Sent-off players: red marks next to the score.
	{
		int32 Count[2] = { 0, 0 };
		for (const APKFootballer* Off : GM->SentOff)
		{
			const int32 T = Off->Team;
			DrawRect(FLinearColor(0.9f, 0.05f, 0.05f), (T == 0 ? 26.f : 330.f) + Count[T] * 12.f, 120.f, 9.f, 13.f);
			++Count[T];
		}
	}
	const APKPlayerController* OwnerPC = Cast<APKPlayerController>(GetOwningPlayerController());
	if (GM->IsVersus() && OwnerPC)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 20.f, 92.f, 330.f, 22.f);
		DrawText(FString::Printf(TEXT("P1 %s"), *FPKInputRouter::DeviceName(OwnerPC->GetTeamDevice(0))), FLinearColor(1.f, 0.85f, 0.2f), 30.f, 95.f, GEngine->GetSmallFont(), 1.f);
		DrawText(FString::Printf(TEXT("P2 %s"), *FPKInputRouter::DeviceName(OwnerPC->GetTeamDevice(1))), FLinearColor(0.3f, 0.9f, 1.f), 190.f, 95.f, GEngine->GetSmallFont(), 1.f);
	}
	const bool bPaused = GetWorld()->IsPaused();

	// Shirt numbers above every player (N toggles), so set plays and positions can be read at a glance.
	// The controlled player's tag is gold with "P1"; a booked player gets a small yellow card.
	const APKPlayerController* NumbersPC = Cast<APKPlayerController>(GetOwningPlayerController());
	const bool bNumbers = !NumbersPC || NumbersPC->bShowNumbers;
	const APKFootballer* Carrier = GM->GetBall() ? GM->GetBall()->GetOwnerPlayer() : nullptr;
	// Screen areas taken by the scorebug, the player card and the radar: tags are not drawn over them.
	const float RadarW = 300.f, RadarH = RadarW * PK::HalfWidth / PK::HalfLength;
	const FBox2D Panels[] = {
		FBox2D(FVector2D(0.f, 0.f), FVector2D(440.f, 124.f)),
		FBox2D(FVector2D(Canvas->ClipX - 400.f, 0.f), FVector2D(Canvas->ClipX, 160.f)),
		FBox2D(FVector2D(Canvas->ClipX - RadarW - 40.f, Canvas->ClipY - RadarH - 40.f), FVector2D(Canvas->ClipX, Canvas->ClipY)),
	};
	const FVector CamLoc = NumbersPC && NumbersPC->PlayerCameraManager ? NumbersPC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (APKFootballer* P : GM->GetFootballers())
	{
		const bool bIsHuman = GM->IsHumanTeam(P->Team) && P == GM->GetHuman(P->Team) && !GM->IsSpectating();
		const bool bP2 = bIsHuman && P->Team == 1;
		const TCHAR* Tag = bP2 ? TEXT("P2") : TEXT("P1");
		const FLinearColor TagColor = bP2 ? FLinearColor(0.3f, 0.9f, 1.f) : FLinearColor(1.f, 0.85f, 0.f);
		const bool bShowThis = bNumbers || bIsHuman || P == Carrier;
		if (!bShowThis || bPaused)
		{
			continue;
		}
		const FVector Screen = Project(P->GetActorLocation() + FVector(0.f, 0.f, 105.f));
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.Y < 0.f || Screen.X > Canvas->ClipX || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		const float Dist = FVector::Dist(CamLoc, P->GetActorLocation());
		const float Scale = FMath::Clamp(3500.f / FMath::Max(Dist, 1.f), 0.9f, 1.4f);
		UFont* Font = GEngine->GetMediumFont();
		const FString Num = FString::FromInt(P->ShirtNumber);
		float W = 0.f, H = 0.f;
		GetTextSize(Num.IsEmpty() ? FString(TEXT("P1")) : Num, W, H, Font, Scale);
		const float PadX = 4.f * Scale;
		const float BoxW = W + PadX * 2.f;
		const float BoxH = H + 2.f;
		const float X = Screen.X - BoxW * 0.5f;
		const float Y = Screen.Y - BoxH;
		const FBox2D TagBox(FVector2D(X - 20.f, Y - 24.f), FVector2D(X + BoxW + 20.f, Y + BoxH + 10.f));
		bool bCovered = false;
		for (const FBox2D& Panel : Panels)
		{
			bCovered |= Panel.Intersect(TagBox);
		}
		if (bCovered)
		{
			continue;
		}
		const FLinearColor Kit = PK::TeamColor(P->Team, P->IsKeeper());
		const FLinearColor Back = bIsHuman ? FLinearColor(TagColor.R, TagColor.G * 0.95f, TagColor.B, 0.95f) : FLinearColor(Kit.R * 0.55f, Kit.G * 0.55f, Kit.B * 0.55f, 0.8f);
		const FLinearColor Ink = bIsHuman ? FLinearColor(0.05f, 0.05f, 0.05f) : FLinearColor::White;
		if (!Num.IsEmpty())
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 1.f, Y - 1.f, BoxW + 2.f, BoxH + 2.f);
			DrawRect(Back, X, Y, BoxW, BoxH);
			DrawText(Num, Ink, X + PadX, Y + 1.f, Font, Scale);
			if (P->YellowCards > 0)
			{
				DrawRect(FLinearColor(1.f, 0.85f, 0.f), X + BoxW + 3.f, Y + 1.f, 6.f * Scale, 9.f * Scale);
			}
		}
		if (bIsHuman)
		{
			// Stamina bar under the tag.
			const float SW = 34.f * Scale;
			const float SX = Screen.X - SW * 0.5f;
			const float SY = (Num.IsEmpty() ? Screen.Y : Y + BoxH) + 3.f;
			const float St = FMath::Clamp(P->Stamina, 0.f, 1.f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), SX - 1.f, SY - 1.f, SW + 2.f, 5.f);
			DrawRect(FLinearColor(0.25f, 0.25f, 0.25f, 0.8f), SX + SW * P->MaxStamina, SY, SW * (1.f - P->MaxStamina), 3.f);
			DrawRect(St > 0.5f ? FLinearColor(0.2f, 0.9f, 0.3f) : (St > 0.25f ? FLinearColor(1.f, 0.8f, 0.1f) : FLinearColor(1.f, 0.2f, 0.15f)), SX, SY, SW * St, 3.f);
			if (GM->IsVersus())
			{
				// Only local 1v1 needs the P1 / P2 label; otherwise the gold number box already marks the player.
				float PW = 0.f, PH = 0.f;
				GetTextSize(Tag, PW, PH, GEngine->GetSmallFont(), 1.f);
				DrawText(Tag, TagColor, Screen.X - PW * 0.5f, (Num.IsEmpty() ? Screen.Y - PH : Y - PH - 1.f), GEngine->GetSmallFont(), 1.f);
			}
		}
	}

	// Dead-ball prompt: tells the player which key takes the kick-off / free kick / throw-in.
	if (const APKPlayerController* PromptPC = Cast<APKPlayerController>(GetOwningPlayerController()))
	{
		for (int32 Team = 0; Team < 2; ++Team)
		{
			if (!GM->HumanOnDeadBall(Team) || GM->Phase != EPKPhase::Playing || GM->IsSpectating() || bPaused)
			{
				continue;
			}
			const int32 Device = GM->IsVersus() ? PromptPC->GetTeamDevice(Team) : FPKInputRouter::Get().LastActiveDevice();
			const bool bPad = PKDevice::IsGamepad(Device);
			const UPKGameInstance* PromptGI = GetGameInstance<UPKGameInstance>();
			const FPKUserSettings Keys = PromptGI ? PromptGI->Settings : FPKUserSettings();
			const FString PassText = bPad ? PKInputs::PadText(EPKInput::Pass) : Keys.KeyText(EPKInput::Pass);
			const FString LobText = bPad ? PKInputs::PadText(EPKInput::Lob) : Keys.KeyText(EPKInput::Lob);
			const FString ShootText = TEXT("hold ") + (bPad ? PKInputs::PadText(EPKInput::Shoot) : Keys.KeyText(EPKInput::Shoot));
			const TCHAR* Pass = *PassText;
			const TCHAR* Lob = *LobText;
			const TCHAR* Shoot = *ShootText;
			FString Prompt;
			switch (GM->GetSetPiece())
			{
			case EPKSetPiece::Kickoff:  Prompt = FString::Printf(TEXT("KICK OFF  -  push a direction to play it yourself, or %s to pass"), Pass); break;
			case EPKSetPiece::Penalty:  Prompt = FString::Printf(TEXT("PENALTY  -  aim with the direction keys, %s and release to shoot"), Shoot); break;
			case EPKSetPiece::ThrowIn:  Prompt = FString::Printf(TEXT("THROW-IN  -  aim, then %s to throw  (%s long)"), Pass, Lob); break;
			case EPKSetPiece::Corner:   Prompt = FString::Printf(TEXT("CORNER  -  aim, then %s to cross  (%s short pass)"), Lob, Pass); break;
			default:                    Prompt = FString::Printf(TEXT("FREE KICK  -  %s pass,  %s lob,  %s shoot"), Pass, Lob, Shoot); break;
			}
			if (GM->IsVersus())
			{
				Prompt = (Team == 0 ? TEXT("P1  ") : TEXT("P2  ")) + Prompt;
			}
			const float Left = FMath::Max(0.f, 6.f - GM->GetRestartTime());
			Prompt += FString::Printf(TEXT("     (auto in %.0f s)"), FMath::CeilToFloat(Left));
			float W = 0.f, H = 0.f;
			GetTextSize(Prompt, W, H, GEngine->GetMediumFont(), 1.1f);
			const float X = (Canvas->ClipX - W) * 0.5f;
			const float Y = Canvas->ClipY * 0.68f + Team * 40.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - 14.f, Y - 6.f, W + 28.f, H + 12.f);
			DrawRect(Team == 0 ? FLinearColor(1.f, 0.8f, 0.f) : FLinearColor(0.3f, 0.9f, 1.f), X - 14.f, Y - 6.f, 4.f, H + 12.f);
			DrawText(Prompt, FLinearColor::White, X, Y, GEngine->GetMediumFont(), 1.1f);
		}
	}

	if (GM->IsReplaying())
	{
		const float Blink = FMath::Fmod((float)GetWorld()->GetRealTimeSeconds(), 1.f) < 0.6f ? 1.f : 0.25f;
		const float RY = 132.f; // below the scorebug and the AI-vs-AI chip
		DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f), 24.f, RY, 150.f, 34.f);
		DrawRect(FLinearColor(0.9f, 0.1f, 0.1f, Blink), 34.f, RY + 10.f, 14.f, 14.f);
		DrawText(TEXT("REPLAY"), FLinearColor::White, 56.f, RY + 4.f, GEngine->GetMediumFont(), 1.2f);
		DrawText(TEXT("pass / shoot: skip"), FLinearColor(0.8f, 0.8f, 0.8f), 26.f, RY + 38.f, GEngine->GetSmallFont(), 1.f);
	}
	if (GM->MessageTimer > 0.f && !GM->Message.IsEmpty() && !GM->IsReplaying() && !bPaused)
	{
		FVector Celebration;
		const bool bCelebrating = GM->GetCelebrationFocus(Celebration); // keep the banner off the scorer
		// lower third, like a broadcast caption (the old mid-screen slot sat on the centre circle at kick-off)
		DrawCentered(GM->Message, Canvas->ClipY * (bCelebrating ? 0.13f : 0.8f), GM->Message.StartsWith(TEXT("GOAL")) ? 3.f : 2.2f, FLinearColor::White);
		if (GM->Phase == EPKPhase::FullTime)
		{
			DrawCentered(APKGameMode::UsesMenus() ? TEXT("R / Enter / Start: play again     Esc / B: main menu") : TEXT("Press R / Enter / Start to play again"), Canvas->ClipY * 0.28f + 60.f, 1.2f, FLinearColor(0.9f, 0.9f, 0.9f));
		}
	}

	// Shot power bars (one per human player) and the controls panel.
	if (const APKPlayerController* PC = Cast<APKPlayerController>(GetOwningPlayerController()))
	{
		if (GM->ShotFeedbackTimer > 0.f && !GM->ShotFeedback.IsEmpty())
		{
			FLinearColor C = GM->ShotFeedbackColor;
			C.A = FMath::Clamp(GM->ShotFeedbackTimer / 0.4f, 0.f, 1.f);
			DrawCentered(GM->ShotFeedback, Canvas->ClipY - 280.f, 1.4f, C);
		}
		const bool bVersus = GM->IsVersus();
		for (int32 Team = 0; Team < (bVersus ? 2 : 1); ++Team)
		{
			const float PassCharge = PC->GetPassCharge(Team);
			const bool bPassBar = PassCharge > 0.f;
			const float Charge = bPassBar ? PassCharge : PC->GetShotCharge(Team);
			if (Charge <= 0.f)
			{
				continue;
			}
			const float W = 260.f;
			const float X = bVersus ? (Team == 0 ? Canvas->ClipX * 0.3f - W * 0.5f : Canvas->ClipX * 0.7f - W * 0.5f) : (Canvas->ClipX - W) * 0.5f;
			const float Y = Canvas->ClipY - 230.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 3.f, Y - 3.f, W + 6.f, 22.f);
			DrawRect(bPassBar ? FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.6f, 1.f), FLinearColor(1.f, 0.85f, 0.f), Charge) : FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.9f, 0.1f), FLinearColor(1.f, 0.1f, 0.05f), Charge), X, Y, W * Charge, 16.f);
			const FString BarName = bPassBar ? TEXT("PRECISION PASS") : TEXT("SHOT POWER");
			DrawText(bVersus ? (Team == 0 ? TEXT("P1 ") : TEXT("P2 ")) + BarName : BarName, FLinearColor::White, X, Y - 22.f, GEngine->GetSmallFont(), 1.f);
		}

		if (!PC->bShowHelp && !bPaused && GM->Phase != EPKPhase::FullTime && !GM->IsSpectating())
		{
			DrawText(TEXT("H  controls"), FLinearColor(1.f, 1.f, 1.f, 0.55f), 26.f, Canvas->ClipY - 34.f, GEngine->GetSmallFont(), 1.f);
		}
		if (PC->bShowHelp && !bPaused)
		{
			// Prompts follow the device: keyboard keys, or gamepad buttons once a gamepad is used.
			const UPKGameInstance* HelpGI = GetGameInstance<UPKGameInstance>();
			const FPKUserSettings K = HelpGI ? HelpGI->Settings : FPKUserSettings();
			const FString KeyLine[] = {
				FString::Printf(TEXT("MOVE  %s %s %s %s    SPRINT  %s    CONTROLLED SPRINT  %s    HOLD  %s"), *K.KeyText(EPKInput::MoveUp), *K.KeyText(EPKInput::MoveLeft), *K.KeyText(EPKInput::MoveDown), *K.KeyText(EPKInput::MoveRight), *K.KeyText(EPKInput::Sprint), *K.KeyText(EPKInput::Precision), *K.KeyText(EPKInput::Hold)),
				FString::Printf(TEXT("PASS  %s   THROUGH  %s   LOB  %s     +%s precision   +%s swerve   +%s one-two"), *K.KeyText(EPKInput::Pass), *K.KeyText(EPKInput::Through), *K.KeyText(EPKInput::Lob), *K.KeyText(EPKInput::Precision), *K.KeyText(EPKInput::Hold), *K.KeyText(EPKInput::Runs)),
				FString::Printf(TEXT("SHOOT  hold %s (again = timed)     +%s finesse   +%s chip   +both power   +%s low"), *K.KeyText(EPKInput::Shoot), *K.KeyText(EPKInput::Precision), *K.KeyText(EPKInput::Runs), *K.KeyText(EPKInput::Hold)),
				FString::Printf(TEXT("TACKLE  %s   SLIDE  %s   SWITCH  %s   BARGE  %s   TEAM PRESS  %s   KEEPER RUSH  %s"), *K.KeyText(EPKInput::Tackle), *K.KeyText(EPKInput::Slide), *K.KeyText(EPKInput::Switch), *K.KeyText(EPKInput::Barge), *K.KeyText(EPKInput::Precision), *K.KeyText(EPKInput::Through)),
				FString::Printf(TEXT("SKILLS  %s+direction   EFFORT TOUCH  %s   SEND RUN  tap %s   drag-back %s  feint %s  flick-up %s"), *K.KeyText(EPKInput::Skill), *K.KeyText(EPKInput::KnockOn), *K.KeyText(EPKInput::Runs), *K.KeyText(EPKInput::DragBack), *K.KeyText(EPKInput::Feint), *K.KeyText(EPKInput::FlickUp)),
				FString::Printf(TEXT("STYLE  1-5   GAME PLAN  9 / 0   PAUSE  %s  (subs, settings)   H help   N numbers"), *K.KeyText(EPKInput::Pause)),
			};
			const TCHAR* KeyboardLines[] = { *KeyLine[0], *KeyLine[1], *KeyLine[2], *KeyLine[3], *KeyLine[4], *KeyLine[5] };
			static const TCHAR* GamepadLines[] = {
				TEXT("MOVE  left stick    SPRINT  RT    CONTROLLED SPRINT  RB    HOLD  LT (jockey / shield)"),
				TEXT("PASS  A   THROUGH  Y   LOB  X     +RB precision   +LT swerve   +LB one-two"),
				TEXT("SHOOT  hold B (again = timed)     +RB finesse   +LB chip   +RB+LB power   +LT low"),
				TEXT("TACKLE  B   SLIDE  X   SWITCH  LB   BARGE  A   TEAM PRESS  RB   KEEPER RUSH  Y"),
				TEXT("SKILLS  right stick flick   EFFORT TOUCH  RB+right stick / R3   SEND RUN  tap LB"),
				TEXT("STYLE  D-pad left/right   GAME PLAN  LT + D-pad   PAUSE  Start (subs, settings)"),
			};
			const FPKInputRouter& Router = FPKInputRouter::Get();
			auto DrawPanel = [&](float X, const FString& Header, const FLinearColor& HeaderColor, int32 Device, float Width)
			{
				const bool bPad = PKDevice::IsGamepad(Device == PKDevice::Any ? Router.LastActiveDevice() : Device);
				const TCHAR* const* Lines = bPad ? GamepadLines : KeyboardLines;
				float Y = Canvas->ClipY - 170.f;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X, Y - 30.f, Width, 160.f);
				DrawText(Header, HeaderColor, X + 8.f, Y - 24.f, GEngine->GetSmallFont(), 1.f);
				for (int32 i = 0; i < 6; ++i)
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
				DrawPanel(12.f, bPad ? TEXT("CONTROLS  -  GAMEPAD") : TEXT("CONTROLS  -  KEYBOARD   (prompts switch when you use a gamepad)"), FLinearColor(1.f, 0.85f, 0.2f), PKDevice::Any, 700.f);
			}
		}
	}
}

void APKHUD::DrawScoreboard(APKGameMode* GM)
{
	const FString Clock = GM->GetBroadcastClock();

	// Broadcast scorebug: clock | home chip + short name | score | away short name + chip.
	FString Names[2] = { TEXT("BLU"), TEXT("RED") };
	FLinearColor Chips[2] = { PK::TeamColor(0, false), PK::TeamColor(1, false) };
	if (const UPKGameInstance* GI = GetGameInstance<UPKGameInstance>())
	{
		if (GI->Clubs.Num() >= 2)
		{
			const FPKClub* Clubs[2] = { &GI->UserClubData(), &GI->OpponentClubData() };
			for (int32 T = 0; T < 2; ++T)
			{
				Names[T] = Clubs[T]->Short.IsEmpty() ? Clubs[T]->Name.Left(3).ToUpper() : Clubs[T]->Short.ToUpper();
				Chips[T] = Clubs[T]->Kit.Primary;
			}
		}
	}
	UFont* Font = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float X0 = 24.f, Y0 = 24.f, BugH = 40.f;
	const float ClockW = 84.f, NameW = 76.f, ScoreW = 78.f, ChipW = 7.f;
	float BugX = X0;
	auto TextIn = [&](const FString& Text, float BoxX, float BoxW, const FLinearColor& Color, float Scale)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, Font, Scale);
		DrawText(Text, Color, BoxX + (BoxW - TW) * 0.5f, Y0 + (BugH - TH) * 0.5f, Font, Scale);
	};
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), X0 + 2.f, Y0 + 3.f, ClockW + 2.f * (NameW + ChipW) + ScoreW, BugH); // drop shadow
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f), BugX, Y0, ClockW, BugH);
	TextIn(Clock, BugX, ClockW, FLinearColor::White, 1.15f);
	BugX += ClockW;
	DrawRect(Chips[0], BugX, Y0, ChipW, BugH);
	BugX += ChipW;
	DrawRect(FLinearColor(0.07f, 0.08f, 0.1f, 0.92f), BugX, Y0, NameW, BugH);
	TextIn(Names[0], BugX, NameW, FLinearColor::White, 1.15f);
	BugX += NameW;
	DrawRect(FLinearColor(0.94f, 0.94f, 0.92f, 0.97f), BugX, Y0, ScoreW, BugH);
	TextIn(FString::Printf(TEXT("%d - %d"), GM->Score[0], GM->Score[1]), BugX, ScoreW, FLinearColor(0.03f, 0.03f, 0.05f), 1.2f);
	BugX += ScoreW;
	DrawRect(FLinearColor(0.07f, 0.08f, 0.1f, 0.92f), BugX, Y0, NameW, BugH);
	TextIn(Names[1], BugX, NameW, FLinearColor::White, 1.15f);
	BugX += NameW;
	DrawRect(Chips[1], BugX, Y0, ChipW, BugH);

	// Game plans, small, under the scorebug.
	auto PlanText = [GM](int32 T)
	{
		const int32 M = GM->GetMentality(T);
		return M == 0 ? APKGameMode::StyleName(GM->GetTeamStyle(T)) : FString::Printf(TEXT("%s (%s)"), *APKGameMode::StyleName(GM->GetTeamStyle(T)), APKGameMode::MentalityName(M));
	};
	const FString Plans = FString::Printf(TEXT("%s  vs  %s"), *PlanText(0), *PlanText(1));
	float PW = 0.f, PH = 0.f;
	GetTextSize(Plans, PW, PH, Small, 1.f);
	if (!GM->IsSpectating())
	{
		DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.8f), X0 + ClockW, Y0 + BugH, FMath::Max(PW * 1.1f + 20.f, 2.f * (NameW + ChipW) + ScoreW), 24.f);
		DrawText(Plans, FLinearColor(0.95f, 0.95f, 0.95f), X0 + ClockW + 10.f, Y0 + BugH + 3.f, Small, 1.1f);
	}

	// Controlled player card: ratings and traits.
	if (const APKFootballer* H = GM->GetHuman())
	{
		const FPKAttributes& A = H->Attr;
		const FString Line1 = FString::Printf(TEXT("#%d %s  OVR %d"), H->ShirtNumber, *H->RoleShort(), A.Overall());
		const FString Line2 = FString::Printf(TEXT("PAC %d  CTL %d  PAS %d  SHO %d  DEF %d  PHY %d"),
			FMath::RoundToInt(A.Pace * 100.f), FMath::RoundToInt(A.Control * 100.f), FMath::RoundToInt(A.Passing * 100.f),
			FMath::RoundToInt(A.Shooting * 100.f), FMath::RoundToInt(A.Defending * 100.f), FMath::RoundToInt(A.Physical * 100.f));
		const FString Traits = H->TraitNames(3);
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
	// Local 1v1: player 2's card under it, cyan.
	if (const APKFootballer* H2 = GM->IsVersus() ? GM->GetHuman(1) : nullptr)
	{
		const FPKAttributes& A = H2->Attr;
		const FString Line1 = FString::Printf(TEXT("P2  #%d %s  OVR %d"), H2->ShirtNumber, *H2->RoleShort(), A.Overall());
		const FString Line2 = FString::Printf(TEXT("PAC %d  CTL %d  PAS %d  SHO %d  DEF %d  PHY %d"),
			FMath::RoundToInt(A.Pace * 100.f), FMath::RoundToInt(A.Control * 100.f), FMath::RoundToInt(A.Passing * 100.f),
			FMath::RoundToInt(A.Shooting * 100.f), FMath::RoundToInt(A.Defending * 100.f), FMath::RoundToInt(A.Physical * 100.f));
		const float X = Canvas->ClipX - 380.f;
		const float Y = 100.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, 360.f, 50.f);
		DrawRect(FLinearColor(0.3f, 0.9f, 1.f), X, Y, 4.f, 50.f);
		DrawText(Line1, FLinearColor(0.3f, 0.9f, 1.f), X + 14.f, Y + 6.f, GEngine->GetMediumFont(), 1.f);
		DrawText(Line2, FLinearColor::White, X + 14.f, Y + 28.f, Small, 1.f);
	}
}

void APKHUD::DrawRadar(APKGameMode* GM)
{
	const float W = 220.f; // smaller and see-through: the old opaque block hid live play
	const float H = W * PK::HalfWidth / PK::HalfLength;
	const float X0 = Canvas->ClipX - W - 24.f;
	const float Y0 = Canvas->ClipY - H - 24.f;

	// World +Y is "up" on screen, so it maps to radar top.
	auto ToRadar = [&](const FVector& L)
	{
		return FVector2D(X0 + (0.5f - L.X / PK::HalfLength * 0.5f) * W, Y0 + (0.5f - L.Y / PK::HalfWidth * 0.5f) * H);
	};
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), X0 - 6.f, Y0 - 6.f, W + 12.f, H + 12.f);
	DrawRect(FLinearColor(0.03f, 0.16f, 0.06f, 0.55f), X0, Y0, W, H);
	const FLinearColor LineC(1.f, 1.f, 1.f, 0.35f);
	auto Line = [&](const FVector& A, const FVector& B)
	{
		const FVector2D RA = ToRadar(A), RB = ToRadar(B);
		DrawLine(RA.X, RA.Y, RB.X, RB.Y, LineC, 1.f);
	};
	const float L = PK::HalfLength, Wd = PK::HalfWidth;
	Line(FVector(-L, -Wd, 0.f), FVector(L, -Wd, 0.f));
	Line(FVector(-L, Wd, 0.f), FVector(L, Wd, 0.f));
	Line(FVector(-L, -Wd, 0.f), FVector(-L, Wd, 0.f));
	Line(FVector(L, -Wd, 0.f), FVector(L, Wd, 0.f));
	Line(FVector(0.f, -Wd, 0.f), FVector(0.f, Wd, 0.f));
	for (int32 i = 0; i < 24; ++i)
	{
		const float A0 = UE_TWO_PI * i / 24.f, A1 = UE_TWO_PI * (i + 1) / 24.f;
		Line(FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * PK::CentreRadius, FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * PK::CentreRadius);
	}
	for (const float S : { -1.f, 1.f })
	{
		const float BX = S * (L - PK::PenaltyDepth);
		Line(FVector(BX, -PK::PenaltyHalfWidth, 0.f), FVector(BX, PK::PenaltyHalfWidth, 0.f));
		Line(FVector(BX, -PK::PenaltyHalfWidth, 0.f), FVector(S * L, -PK::PenaltyHalfWidth, 0.f));
		Line(FVector(BX, PK::PenaltyHalfWidth, 0.f), FVector(S * L, PK::PenaltyHalfWidth, 0.f));
	}

	for (APKFootballer* P : GM->GetFootballers())
	{
		const FVector2D R = ToRadar(P->GetActorLocation());
		const bool bControlled = GM->IsHumanTeam(P->Team) && P == GM->GetHuman(P->Team);
		const FLinearColor C = bControlled ? (P->Team == 0 ? FLinearColor(1.f, 0.85f, 0.f) : FLinearColor(0.3f, 0.9f, 1.f)) : PK::TeamColor(P->Team, P->IsKeeper());
		const float D = bControlled ? 9.f : 7.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), R.X - D * 0.5f - 1.f, R.Y - D * 0.5f - 1.f, D + 2.f, D + 2.f);
		DrawRect(C, R.X - D * 0.5f, R.Y - D * 0.5f, D, D);
	}
	const FVector2D B = ToRadar(GM->GetBall()->GetActorLocation());
	DrawRect(FLinearColor::Black, B.X - 3.f, B.Y - 3.f, 6.f, 6.f);
	DrawRect(FLinearColor::White, B.X - 2.f, B.Y - 2.f, 4.f, 4.f);
}

void APKHUD::DrawCentered(const FString& Text, float Y, float Scale, const FLinearColor& Color)
{
	UFont* Font = GEngine->GetLargeFont();
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	// Broadcast banner: deep navy band with gold rules above and below.
	const float BX = (Canvas->ClipX - W) * 0.5f - 40.f, BW = W + 80.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.3f), BX + 4.f, Y - 6.f, BW, H + 16.f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.08f, 0.88f), BX, Y - 10.f, BW, H + 16.f);
	DrawRect(FLinearColor(1.f, 0.78f, 0.1f, 0.95f), BX, Y - 10.f, BW, 3.f);
	DrawRect(FLinearColor(1.f, 0.78f, 0.1f, 0.95f), BX, Y + H + 3.f, BW, 3.f);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y - 2.f, Font, Scale);
}
