#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PKCookList.generated.h"

// Assets the code loads by path from outside /Game/PitchKings (City Sample Crowds faces, clothes, scalp maps).
// /Game/PitchKings is always cooked, so a list there pulls exactly these into a packaged build.
// Filled by Tools/editor_scripts/make_cook_list.py.
UCLASS(BlueprintType)
class PITCHKINGS_API UPKCookList : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Cook")
	TArray<TObjectPtr<UObject>> Assets;
};
