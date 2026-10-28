#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PKHeadSet.generated.h"

class USkeletalMesh;
class UStaticMesh;

// One player head built from a MetaHuman Creator preset (Tools/editor_scripts/metahuman/build_player_heads.py): the
// face mesh with its own baked skin / eye materials, and the groom cards baked into head-bone space.
USTRUCT(BlueprintType)
struct FPKHead
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Head")
	FName Name;
	UPROPERTY(EditAnywhere, Category = "Head")
	TObjectPtr<USkeletalMesh> Face;
	UPROPERTY(EditAnywhere, Category = "Head")
	TObjectPtr<UStaticMesh> Hair;
	UPROPERTY(EditAnywhere, Category = "Head")
	TObjectPtr<UStaticMesh> Beard;
	UPROPERTY(EditAnywhere, Category = "Head")
	TObjectPtr<UStaticMesh> Mustache;
	UPROPERTY(EditAnywhere, Category = "Head")
	TObjectPtr<UStaticMesh> Eyebrows;
	// average face skin colour (linear), so the mannequin arms and legs can match the head
	UPROPERTY(EditAnywhere, Category = "Head")
	FLinearColor Skin = FLinearColor(0.4f, 0.27f, 0.2f);
	// hair / beard albedo from the groom's melanin (the card material's HairColor)
	UPROPERTY(EditAnywhere, Category = "Head")
	FLinearColor HairColor = FLinearColor(0.03f, 0.02f, 0.015f);
};

UCLASS(BlueprintType)
class PITCHKINGS_API UPKHeadSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Heads")
	TArray<FPKHead> Heads;
};
