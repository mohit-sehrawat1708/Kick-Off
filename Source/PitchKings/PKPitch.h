#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PKPitch.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialParameterCollection;

// The stadium, assembled in the construction script (visible in the editor and at runtime) from the generated meshes in
// /Game/PitchKings/Stadium (Tools/art/gen_stadium.py): the pitch (markings are painted by M_PK_Pitch), the seating
// bowl and roof, goals, LED boards, dugouts and corner flags, and the crowd as one instanced component of cards.
// Club colours and crowd excitement reach the materials through MPC_PK_Stadium.
UCLASS()
class PITCHKINGS_API APKPitch : public AActor
{
	GENERATED_BODY()

public:
	APKPitch();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Rebuild without the crowd / roof for fast iteration.
	UPROPERTY(EditAnywhere, Category = "Pitch Kings")
	bool bBuildStands = true;

	UPROPERTY(EditAnywhere, Category = "Pitch Kings")
	bool bBuildFloodlights = true;

	// Seat and fan colours: home club (most of the ground) and the away end.
	void SetClubColours(const FLinearColor& HomeA, const FLinearColor& HomeB, const FLinearColor& AwayA, const FLinearColor& AwayB);

	// Fans of Team (0 home, 1 away) get on their feet: Amount 0..1, fades over a few seconds.
	void Cheer(int32 Team, float Amount);

	// Live score and clock on the big screens (M_PK_Score reads them from MPC_PK_Stadium).
	void SetScoreboard(int32 Home, int32 Away, float ClockSeconds);

	int32 GetCrowdCount() const;

private:
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide, bool bShadow, const FRotator& Rotation = FRotator::ZeroRotator);
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, bool bShadow);
	void AddBlocker(const FVector& Center, const FVector& Size);
	void BuildAll();
	void BuildGoals();
	void BuildBoards();
	void BuildCrowd();
	void BuildSurroundings();
	void PushCrowdState();

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BowlMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RoofMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> GoalMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoardMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CardMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FlagMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DugoutMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> TreeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ScreenMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CarMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BannerMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BuildingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PitchMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GroundMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> StadiumParams;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Crowd;

	// 3D fans in the front rows (City Sample Crowds variants with baked animation, /Game/PitchKings/Crowd3D):
	// one instanced component per variant. Custom data: 0-3 animation playback, 4 team, 5 seed, 6 skin tone.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Fans3D;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> FanMeshes;

	void UpdateFanAnimations();
	bool bFansCheering[2] = { false, false };

	FVector2D Excite = FVector2D::ZeroVector;
	FVector2D ExciteTarget = FVector2D::ZeroVector;
	float ExciteHold[2] = { 0.f, 0.f };
	FIntVector LastScoreboard = FIntVector(-1, -1, -1);
};
