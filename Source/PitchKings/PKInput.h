#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"

// Device numbers used by the game: keyboard & mouse, or one gamepad each.
namespace PKDevice
{
	constexpr int32 Any = -1;     // single player: every device controls player 1
	constexpr int32 Keyboard = 0;
	inline bool IsGamepad(int32 Device) { return Device >= 1; }
	inline int32 FromGamepadIndex(int32 Index) { return 1 + Index; }
}

// Sees every key / stick event before the game viewport (Slate input pre-processor) and keeps
// per-device state, so two players can share one screen with separate devices: keyboard vs
// gamepad, or gamepad vs gamepad. Also drives controller rumble.
class FPKInputRouter : public IInputProcessor
{
public:
	static FPKInputRouter& Get();
	static void Register();
	static void Unregister();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("PitchKingsInputRouter"); }

	// Queries. Device = PKDevice::Any merges every device. "Since" is the caller's last seen sequence number,
	// so several readers (match input, menus) each see every press exactly once.
	bool IsDown(int32 Device, const FKey& Key) const;
	bool WasPressed(int32 Device, const FKey& Key, uint64 Since) const;
	bool WasReleased(int32 Device, const FKey& Key, uint64 Since) const;
	float Axis(int32 Device, const FKey& Key) const;
	uint64 Sequence() const { return Seq; }

	TArray<int32> KnownGamepads() const;  // gamepads that have sent input this session
	int32 LastActiveDevice() const { return LastActive; }
	int32 LastActiveGamepad() const { return LastGamepad; }
	static FString DeviceName(int32 Device);

	// Rumble a gamepad (Any = the last gamepad used). Strength 0..1.
	void Rumble(int32 Device, float Strength, float Seconds);

	// Self-test: feed synthetic input as if it came from a device.
	void InjectKey(int32 Device, const FKey& Key, bool bDown);
	void InjectAxis(int32 Device, const FKey& Key, float Value);

private:
	struct FState
	{
		TSet<FKey> Down;
		TMap<FKey, uint64> PressSeq;
		TMap<FKey, uint64> ReleaseSeq;
		TMap<FKey, float> Axes;
		float RumbleTime = 0.f;
		float RumbleStrength = 0.f;
		bool bRumbling = false;
	};

	static int32 DeviceOf(const FKeyEvent& Event);
	void KeyDown(int32 Device, const FKey& Key, bool bRepeat);
	void KeyUp(int32 Device, const FKey& Key);

	TMap<int32, FState> Devices;
	uint64 Seq = 0;
	int32 LastActive = PKDevice::Keyboard;
	int32 LastGamepad = INDEX_NONE;
	bool bWasActive = true;
};
