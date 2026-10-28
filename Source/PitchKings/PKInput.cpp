#include "PKInput.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/IInputInterface.h"

namespace
{
	TSharedPtr<FPKInputRouter> GRouter;
}

FPKInputRouter& FPKInputRouter::Get()
{
	if (!GRouter.IsValid())
	{
		GRouter = MakeShared<FPKInputRouter>();
	}
	return *GRouter;
}

void FPKInputRouter::Register()
{
	if (FSlateApplication::IsInitialized())
	{
		Get();
		FSlateApplication::Get().UnregisterInputPreProcessor(GRouter); // no double registration across PIE sessions
		FSlateApplication::Get().RegisterInputPreProcessor(GRouter, 0);
	}
}

void FPKInputRouter::Unregister()
{
	if (GRouter.IsValid() && FSlateApplication::IsInitialized())
	{
		if (IInputInterface* Input = FSlateApplication::Get().GetInputInterface())
		{
			for (auto& Pair : GRouter->Devices)
			{
				if (PKDevice::IsGamepad(Pair.Key) && Pair.Value.bRumbling)
				{
					Input->SetForceFeedbackChannelValues(Pair.Key - 1, FForceFeedbackValues());
				}
			}
		}
		FSlateApplication::Get().UnregisterInputPreProcessor(GRouter);
	}
}

int32 FPKInputRouter::DeviceOf(const FKeyEvent& Event)
{
	// Keyboard (and mouse) is one device; each gamepad is its own, by input device id (= XInput slot).
	return Event.GetKey().IsGamepadKey() ? PKDevice::FromGamepadIndex(Event.GetInputDeviceId().GetId()) : PKDevice::Keyboard;
}

void FPKInputRouter::KeyDown(int32 Device, const FKey& Key, bool bRepeat)
{
	FState& S = Devices.FindOrAdd(Device);
	if (!bRepeat || !S.Down.Contains(Key))
	{
		S.PressSeq.Add(Key, ++Seq);
	}
	S.Down.Add(Key);
	LastActive = Device;
	if (PKDevice::IsGamepad(Device))
	{
		LastGamepad = Device;
	}
}

void FPKInputRouter::KeyUp(int32 Device, const FKey& Key)
{
	FState& S = Devices.FindOrAdd(Device);
	S.Down.Remove(Key);
	S.ReleaseSeq.Add(Key, ++Seq);
}

bool FPKInputRouter::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	KeyDown(DeviceOf(InKeyEvent), InKeyEvent.GetKey(), InKeyEvent.IsRepeat());
	UE_LOG(LogTemp, Verbose, TEXT("[Router] down %s"), *InKeyEvent.GetKey().ToString());
	return false; // never consume: menus and the viewport still get it
}

bool FPKInputRouter::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	KeyUp(DeviceOf(InKeyEvent), InKeyEvent.GetKey());
	return false;
}

bool FPKInputRouter::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	const int32 Device = DeviceOf(InAnalogInputEvent);
	FState& S = Devices.FindOrAdd(Device);
	S.Axes.Add(InAnalogInputEvent.GetKey(), InAnalogInputEvent.GetAnalogValue());
	if (FMath::Abs(InAnalogInputEvent.GetAnalogValue()) > 0.35f)
	{
		LastActive = Device;
		if (PKDevice::IsGamepad(Device))
		{
			LastGamepad = Device;
		}
	}
	return false;
}

void FPKInputRouter::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// Window lost focus (alt-tab): forget held keys so nothing stays stuck down.
	const bool bActive = SlateApp.IsActive();
	if (bWasActive && !bActive)
	{
		for (auto& Pair : Devices)
		{
			Pair.Value.Down.Reset();
			Pair.Value.Axes.Reset();
		}
	}
	bWasActive = bActive;
	IInputInterface* Input = SlateApp.GetInputInterface();
	for (auto& Pair : Devices)
	{
		FState& S = Pair.Value;
		if (!PKDevice::IsGamepad(Pair.Key) || (!S.bRumbling && S.RumbleTime <= 0.f))
		{
			continue;
		}
		S.RumbleTime -= DeltaTime;
		const float V = S.RumbleTime > 0.f ? S.RumbleStrength : 0.f;
		if (Input)
		{
			FForceFeedbackValues Values;
			Values.LeftLarge = V;
			Values.RightLarge = V;
			Values.LeftSmall = V * 0.6f;
			Values.RightSmall = V * 0.6f;
			Input->SetForceFeedbackChannelValues(Pair.Key - 1, Values);
		}
		S.bRumbling = V > 0.f;
	}
}

void FPKInputRouter::Rumble(int32 Device, float Strength, float Seconds)
{
	if (Device == PKDevice::Any)
	{
		Device = LastGamepad;
	}
	if (!PKDevice::IsGamepad(Device))
	{
		return;
	}
	FState& S = Devices.FindOrAdd(Device);
	if (Seconds <= 0.f)
	{
		S.RumbleTime = 0.f;
		S.bRumbling = true; // next tick sends zero
		return;
	}
	S.RumbleStrength = FMath::Max(S.RumbleTime > 0.f ? S.RumbleStrength : 0.f, FMath::Clamp(Strength, 0.f, 1.f));
	S.RumbleTime = FMath::Max(S.RumbleTime, Seconds);
	S.bRumbling = true;
}

bool FPKInputRouter::IsDown(int32 Device, const FKey& Key) const
{
	for (const auto& Pair : Devices)
	{
		if ((Device == PKDevice::Any || Pair.Key == Device) && Pair.Value.Down.Contains(Key))
		{
			return true;
		}
	}
	return false;
}

bool FPKInputRouter::WasPressed(int32 Device, const FKey& Key, uint64 Since) const
{
	for (const auto& Pair : Devices)
	{
		if (Device == PKDevice::Any || Pair.Key == Device)
		{
			if (const uint64* At = Pair.Value.PressSeq.Find(Key))
			{
				if (*At > Since)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FPKInputRouter::WasReleased(int32 Device, const FKey& Key, uint64 Since) const
{
	for (const auto& Pair : Devices)
	{
		if (Device == PKDevice::Any || Pair.Key == Device)
		{
			if (const uint64* At = Pair.Value.ReleaseSeq.Find(Key))
			{
				if (*At > Since)
				{
					return true;
				}
			}
		}
	}
	return false;
}

float FPKInputRouter::Axis(int32 Device, const FKey& Key) const
{
	float Best = 0.f;
	for (const auto& Pair : Devices)
	{
		if (Device == PKDevice::Any || Pair.Key == Device)
		{
			if (const float* V = Pair.Value.Axes.Find(Key))
			{
				if (FMath::Abs(*V) > FMath::Abs(Best))
				{
					Best = *V;
				}
			}
		}
	}
	return Best;
}

TArray<int32> FPKInputRouter::KnownGamepads() const
{
	TArray<int32> Pads;
	for (const auto& Pair : Devices)
	{
		if (PKDevice::IsGamepad(Pair.Key))
		{
			Pads.Add(Pair.Key);
		}
	}
	Pads.Sort();
	return Pads;
}

FString FPKInputRouter::DeviceName(int32 Device)
{
	if (Device == PKDevice::Any)
	{
		return TEXT("ANY");
	}
	return Device == PKDevice::Keyboard ? FString(TEXT("KEYBOARD")) : FString::Printf(TEXT("GAMEPAD %d"), Device);
}

void FPKInputRouter::InjectKey(int32 Device, const FKey& Key, bool bDown)
{
	if (bDown)
	{
		KeyDown(Device, Key, false);
	}
	else
	{
		KeyUp(Device, Key);
	}
}

void FPKInputRouter::InjectAxis(int32 Device, const FKey& Key, float Value)
{
	Devices.FindOrAdd(Device).Axes.Add(Key, Value);
	if (FMath::Abs(Value) > 0.35f)
	{
		LastActive = Device;
		if (PKDevice::IsGamepad(Device))
		{
			LastGamepad = Device;
		}
	}
}
