#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

// Rebindable player actions (keyboard). Gamepad buttons are fixed and shown alongside.
enum class EPKInput : uint8
{
	MoveUp,
	MoveDown,
	MoveLeft,
	MoveRight,
	Sprint,
	Hold,      // jockey / shield / close control
	Pass,
	Through,
	Lob,
	Shoot,
	Tackle,
	Slide,
	Switch,
	Barge,
	KnockOn,
	DragBack,
	Feint,
	FlickUp,
	Pause,
	// Added later: kept after the original actions so older saved bindings still load.
	Precision, // with the ball: controlled sprint, precision pass, finesse shot; without: teammate press
	Runs,      // with the ball: send a run, one-two, chip
	Skill,     // hold + direction: skill move (gamepad: right stick flick)
	Count
};

struct FPKInputInfo
{
	const TCHAR* Name;
	FKey Key1;   // keyboard defaults
	FKey Key2;
	FKey Pad1;   // gamepad (fixed)
	FKey Pad2;
};

// Everything the settings page changes, apart from graphics (which live in UGameUserSettings).
struct FPKUserSettings
{
	FKey Keys[(int32)EPKInput::Count][2];
	bool bInvertX = false;
	bool bInvertY = false;
	float DeadZone = 0.2f;     // gamepad stick
	float Rumble = 1.f;        // 0 = off
	int32 MatchMinutes = 6;
	int32 Difficulty = 1;      // 0 easy, 1 normal, 2 hard (computer team ratings)
	int32 CameraZoom = 1;      // 0 near, 1 normal, 2 far
	bool bShowNumbers = false; // every player's number (off: only the controlled player and the ball carrier)
	bool bShowHelp = false;    // controls panel (H)
	bool bShowRadar = true;
	bool bShowFPS = false;

	FPKUserSettings() { ResetKeys(); }
	void ResetKeys();
	void ResetAll() { *this = FPKUserSettings(); }
	// Bind Key to an action slot; a key used elsewhere moves here (the other slot gets this slot's old key).
	void Bind(EPKInput Action, int32 Slot, const FKey& Key);
	FString KeyText(EPKInput Action) const; // "J / Space"
};

namespace PKInputs
{
	const FPKInputInfo& Info(EPKInput Action);
	FString KeyName(const FKey& Key);    // short display name ("Left Shift", "Up", "J")
	FString PadName(const FKey& Key);    // "A", "RT", "D-pad down" ...
	FString PadText(EPKInput Action);    // "A" / "RB / R3"
}
