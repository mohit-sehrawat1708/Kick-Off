#include "PKSettings.h"

namespace PKInputs
{
	const FPKInputInfo& Info(EPKInput Action)
	{
		// Built on first use (after InputCore's key objects exist).
		static const FPKInputInfo Table[(int32)EPKInput::Count] = {
			{ TEXT("Move up"), EKeys::W, EKeys::Up, FKey(), FKey() },
			{ TEXT("Move down"), EKeys::S, EKeys::Down, FKey(), FKey() },
			{ TEXT("Move left"), EKeys::A, EKeys::Left, FKey(), FKey() },
			{ TEXT("Move right"), EKeys::D, EKeys::Right, FKey(), FKey() },
			{ TEXT("Sprint"), EKeys::LeftShift, FKey(), EKeys::Gamepad_RightTrigger, FKey() },
			{ TEXT("Hold: jockey / shield / close control"), EKeys::LeftControl, FKey(), EKeys::Gamepad_LeftTrigger, FKey() },
			{ TEXT("Attack: pass"), EKeys::J, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom, FKey() },
			{ TEXT("Attack: through ball"), EKeys::L, FKey(), EKeys::Gamepad_FaceButton_Top, FKey() },
			{ TEXT("Attack: lob / cross"), EKeys::I, FKey(), EKeys::Gamepad_FaceButton_Left, FKey() },
			{ TEXT("Attack: shoot (hold, release)"), EKeys::K, FKey(), EKeys::Gamepad_FaceButton_Right, FKey() },
			{ TEXT("Defence: standing tackle"), EKeys::E, FKey(), EKeys::Gamepad_FaceButton_Right, FKey() },
			{ TEXT("Defence: slide tackle"), EKeys::C, FKey(), EKeys::Gamepad_FaceButton_Left, FKey() },
			{ TEXT("Switch player"), EKeys::Q, FKey(), EKeys::Gamepad_LeftShoulder, FKey() },
			{ TEXT("Defence: shoulder barge"), EKeys::X, FKey(), EKeys::Gamepad_FaceButton_Bottom, FKey() },
			{ TEXT("Effort touch (tap short, hold long)"), EKeys::F, FKey(), EKeys::Gamepad_RightThumbstick, FKey() },
			{ TEXT("Drag-back"), EKeys::G, FKey(), EKeys::Gamepad_DPad_Down, FKey() },
			{ TEXT("Body feint"), EKeys::T, FKey(), EKeys::Gamepad_DPad_Up, FKey() },
			{ TEXT("Flick-up"), EKeys::V, FKey(), EKeys::Gamepad_LeftThumbstick, FKey() },
			{ TEXT("Pause menu"), EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right, FKey() },
			{ TEXT("Controlled sprint / precision / finesse; no ball: teammate press"), EKeys::R, FKey(), EKeys::Gamepad_RightShoulder, FKey() },
			{ TEXT("Runs: send a run / one-two / chip"), EKeys::Z, FKey(), EKeys::Gamepad_LeftShoulder, FKey() },
			{ TEXT("Skill move (hold + direction)"), EKeys::B, FKey(), EKeys::Gamepad_Right2D, FKey() },
		};
		return Table[FMath::Clamp((int32)Action, 0, (int32)EPKInput::Count - 1)];
	}

	FString KeyName(const FKey& Key)
	{
		if (!Key.IsValid())
		{
			return TEXT("-");
		}
		if (Key == EKeys::SpaceBar) return TEXT("Space");
		if (Key == EKeys::LeftShift) return TEXT("Left Shift");
		if (Key == EKeys::RightShift) return TEXT("Right Shift");
		if (Key == EKeys::LeftControl) return TEXT("Left Ctrl");
		if (Key == EKeys::RightControl) return TEXT("Right Ctrl");
		if (Key == EKeys::Escape) return TEXT("Esc");
		return Key.GetDisplayName(false).ToString();
	}

	FString PadName(const FKey& Key)
	{
		if (!Key.IsValid()) return FString();
		if (Key == EKeys::Gamepad_FaceButton_Bottom) return TEXT("A");
		if (Key == EKeys::Gamepad_FaceButton_Right) return TEXT("B");
		if (Key == EKeys::Gamepad_FaceButton_Left) return TEXT("X");
		if (Key == EKeys::Gamepad_FaceButton_Top) return TEXT("Y");
		if (Key == EKeys::Gamepad_LeftShoulder) return TEXT("LB");
		if (Key == EKeys::Gamepad_RightShoulder) return TEXT("RB");
		if (Key == EKeys::Gamepad_LeftTrigger) return TEXT("LT");
		if (Key == EKeys::Gamepad_RightTrigger) return TEXT("RT");
		if (Key == EKeys::Gamepad_LeftThumbstick) return TEXT("L3");
		if (Key == EKeys::Gamepad_RightThumbstick) return TEXT("R3");
		if (Key == EKeys::Gamepad_DPad_Up) return TEXT("D-pad up");
		if (Key == EKeys::Gamepad_DPad_Down) return TEXT("D-pad down");
		if (Key == EKeys::Gamepad_Special_Right) return TEXT("Start");
		if (Key == EKeys::Gamepad_Right2D) return TEXT("right stick flick");
		return Key.GetDisplayName(false).ToString();
	}

	FString PadText(EPKInput Action)
	{
		const FPKInputInfo& I = Info(Action);
		if (Action <= EPKInput::MoveRight)
		{
			return TEXT("left stick");
		}
		FString Text = PadName(I.Pad1);
		if (I.Pad2.IsValid())
		{
			Text += TEXT(" / ") + PadName(I.Pad2);
		}
		return Text;
	}
}

void FPKUserSettings::ResetKeys()
{
	for (int32 i = 0; i < (int32)EPKInput::Count; ++i)
	{
		const FPKInputInfo& I = PKInputs::Info((EPKInput)i);
		Keys[i][0] = I.Key1;
		Keys[i][1] = I.Key2;
	}
}

void FPKUserSettings::Bind(EPKInput Action, int32 Slot, const FKey& Key)
{
	const int32 A = (int32)Action;
	Slot = FMath::Clamp(Slot, 0, 1);
	const FKey Old = Keys[A][Slot];
	if (Key.IsValid())
	{
		for (int32 i = 0; i < (int32)EPKInput::Count; ++i)
		{
			for (int32 s = 0; s < 2; ++s)
			{
				if ((i != A || s != Slot) && Keys[i][s] == Key)
				{
					Keys[i][s] = Old; // swap so nothing ends up unbound by surprise
				}
			}
		}
	}
	Keys[A][Slot] = Key;
}

FString FPKUserSettings::KeyText(EPKInput Action) const
{
	const int32 A = (int32)Action;
	FString Text = Keys[A][0].IsValid() ? PKInputs::KeyName(Keys[A][0]) : FString();
	if (Keys[A][1].IsValid())
	{
		Text += (Text.IsEmpty() ? TEXT("") : TEXT(" / ")) + PKInputs::KeyName(Keys[A][1]);
	}
	return Text.IsEmpty() ? FString(TEXT("unbound")) : Text;
}
