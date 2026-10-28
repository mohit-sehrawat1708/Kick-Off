#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PKPlayerController.generated.h"

class ACameraActor;
class SPKMenu;
class UTexture2D;
enum class EPKMenuPage : uint8;
enum class EPKInput : uint8;

// Reads keyboard / gamepad directly each frame and forwards it to the game mode,
// and drives the broadcast camera that follows the ball.
UCLASS()
class PITCHKINGS_API APKPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APKPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// Front-end actions (called by the Slate menu).
	void MenuKickOff();
	void MenuSpectate();
	void MenuQuit();
	void MenuResume();
	void MenuRestart();
	void MenuMainMenu();
	bool IsMenuOpen() const;
	virtual void PlayerTick(float DeltaTime) override;

	float GetShotCharge(int32 Team = 0) const { return bTeamCharging[Team & 1] ? TeamCharge[Team & 1] : 0.f; }
	// Device each human team uses (PKDevice::Any = every device, single player).
	int32 GetTeamDevice(int32 Team) const { return TeamDevice[Team & 1]; }
	void RumbleTeam(int32 Team, float Strength, float Seconds);
	void MenuVersus(int32 DeviceP1, int32 DeviceP2);
	// Re-read the saved settings (HUD toggles, camera zoom) after the settings page changes them.
	void ApplyUserSettings();
	void OpenSettingsFromPause();
	bool bShowHelp = false;
	uint32 LastCameraCutStamp = 0;
	bool bShowNumbers = false; // shirt numbers above every player (N); off: controlled player and ball carrier only

private:
	bool AnyDown(std::initializer_list<FKey> Keys) const;
	bool AnyPressed(std::initializer_list<FKey> Keys) const;
	bool AnyReleased(std::initializer_list<FKey> Keys) const;
	FVector ReadMove() const;
	bool DevDown(int32 Device, std::initializer_list<FKey> Keys) const;
	bool DevPressed(int32 Device, std::initializer_list<FKey> Keys) const;
	bool DevReleased(int32 Device, std::initializer_list<FKey> Keys) const;
	FVector ReadMove(int32 Device) const;
	TArray<FKey> ActionKeys(int32 Device, EPKInput Action) const;
	bool ActionDown(int32 Device, EPKInput Action) const;
	bool ActionPressed(int32 Device, EPKInput Action) const;
	bool ActionReleased(int32 Device, EPKInput Action) const;
	const struct FPKUserSettings* UserSettings() const;
	void HandlePlayer(class APKGameMode* GM, int32 Team, float DeltaTime);
	void StartVersusSelfTest();
	void StartSettingsSelfTest();
	void StartSubsSelfTest();
	void StartReplaySelfTest();
	void UpdateCamera(float DeltaTime);
	void UpdateMenuCamera(float DeltaTime);
	void ShowMenu(EPKMenuPage Page);
	void HideMenu();

	TSharedPtr<SPKMenu> Menu;
	void StartMenuSelfTest();
	float MenuOrbit = 0.f;

	UPROPERTY()
	TObjectPtr<UTexture2D> LogoTexture;
	UPROPERTY()
	TObjectPtr<UTexture2D> CrestTexture;

	UPROPERTY()
	TObjectPtr<ACameraActor> BroadcastCamera;

	FVector CameraFocus = FVector::ZeroVector;
	bool bWasReplaying = false; // the replay start cuts the camera to the rewound ball
	bool bTeamCharging[2] = { false, false };
	// Effort touch being held (keyboard F / R3), or a right stick flick with RB.
	bool bEffortHeld[2] = { false, false };
	float EffortTime[2] = { 0.f, 0.f };
	FVector EffortDir[2];
	bool bRightStickFlick[2] = { false, false };
	// Precision pass power being charged (Precision held + pass / through / lob).
	bool bPassCharging[2] = { false, false };
	float RunsHeld[2] = { -1.f, -1.f }; // seconds Runs has been held with the ball (-1 = not held)
	bool bRunsUsed[2] = { false, false }; // a pass / shot used it as a modifier
	float PassCharge[2] = { 0.f, 0.f };
	int32 PassChargeIdx[2] = { 0, 0 }; // 0 pass, 1 through, 2 lob
public:
	float GetPassCharge(int32 Team = 0) const { return bPassCharging[Team & 1] ? PassCharge[Team & 1] : 0.f; }
private:
	FVector ReadRightStick(int32 Device) const; // camera-relative, zero inside the dead zone
	float TeamCharge[2] = { 0.f, 0.f };
	int32 TeamDevice[2] = { -1, -1 };
	uint64 InputSeq = 0;
	float CameraZoomScale = 1.f; // last router event this controller has read
};
