#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PKHUD.generated.h"

// Canvas-drawn HUD: scoreboard, clock, messages, shot power, radar, player marker, controls help.
UCLASS()
class PITCHKINGS_API APKHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawScoreboard(class APKGameMode* GM);
	void DrawRadar(class APKGameMode* GM);
	void DrawCentered(const FString& Text, float Y, float Scale, const FLinearColor& Color);
};
