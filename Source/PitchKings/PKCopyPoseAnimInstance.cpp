#include "PKCopyPoseAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void FPKCopyPoseProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const USkeletalMeshComponent* Target = InAnimInstance->GetSkelMeshComponent();
	const USkeletalMeshComponent* Source = Target ? Cast<USkeletalMeshComponent>(Target->GetAttachParent()) : nullptr;
	const USkeletalMesh* SourceMesh = Source ? Source->GetSkeletalMeshAsset() : nullptr;
	const USkeletalMesh* TargetMesh = Target ? Target->GetSkeletalMeshAsset() : nullptr;
	if (!SourceMesh || !TargetMesh)
	{
		SourceSpace.Reset();
		return;
	}
	if (MappedSource.Get() != SourceMesh || MappedTarget.Get() != TargetMesh)
	{
		const FReferenceSkeleton& Ref = TargetMesh->GetRefSkeleton();
		SourceIndexOfMeshBone.SetNum(Ref.GetNum());
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			SourceIndexOfMeshBone[i] = Source->GetBoneIndex(Ref.GetBoneName(i));
		}
		MappedSource = SourceMesh;
		MappedTarget = TargetMesh;
	}
	SourceSpace = Source->GetComponentSpaceTransforms();
}

bool FPKCopyPoseProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();
	if (SourceSpace.Num() == 0)
	{
		return true;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	TArray<FTransform> Space;
	Space.SetNum(Output.Pose.GetNumBones());
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
		const int32 SourceIndex = SourceIndexOfMeshBone.IsValidIndex(MeshIndex) ? SourceIndexOfMeshBone[MeshIndex] : INDEX_NONE;
		FTransform Component;
		if (SourceIndex != INDEX_NONE && SourceSpace.IsValidIndex(SourceIndex))
		{
			Component = SourceSpace[SourceIndex];
		}
		else
		{
			Component = Parent.IsValid() ? Output.Pose[Bone] * Space[Parent.GetInt()] : Output.Pose[Bone];
		}
		Space[Bone.GetInt()] = Component;
		Output.Pose[Bone] = Parent.IsValid() ? Component.GetRelativeTransform(Space[Parent.GetInt()]) : Component;
	}
	return true;
}
