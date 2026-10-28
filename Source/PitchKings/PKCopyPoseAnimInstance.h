#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PKCopyPoseAnimInstance.generated.h"

// Follows the pose of the skeletal mesh component this one is attached to (the animated mannequin body): bones with
// the same name copy its component-space transforms, bones it does not have (MetaHuman twist / corrective bones)
// keep their reference pose relative to their parent. Used for the MetaHuman shirt and shorts on the players
// (leader pose would leave those extra bones behind at the reference pose).
struct FPKCopyPoseProxy : public FAnimInstanceProxy
{
	FPKCopyPoseProxy() = default;
	explicit FPKCopyPoseProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	TArray<FTransform> SourceSpace;      // component space of the source, by source bone index
	TArray<int32> SourceIndexOfMeshBone; // target mesh bone index -> source bone index (INDEX_NONE: not in the source)
	TWeakObjectPtr<const USkeletalMesh> MappedSource;
	TWeakObjectPtr<const USkeletalMesh> MappedTarget;
};

UCLASS(Transient, NotBlueprintable)
class PITCHKINGS_API UPKCopyPoseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FPKCopyPoseProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FPKCopyPoseProxy*>(InProxy); }
};
