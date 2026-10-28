#include "PKAnimInstance.h"
#include "PKFootballer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* DirSuffix[8] = { TEXT("Fwd"), TEXT("Fwd_Right"), TEXT("Right"), TEXT("Bwd_Right"), TEXT("Bwd"), TEXT("Bwd_Left"), TEXT("Left"), TEXT("Fwd_Left") };

	// Mannequin component space: +Y forward, +Z up, +X = character's left.
	// Positive rotation about +X swings a hanging limb forward and tips an upright bone backward.
	const FVector AxisSide(1.f, 0.f, 0.f);
	const FVector AxisForward(0.f, 1.f, 0.f);
	const FVector AxisUp(0.f, 0.f, 1.f);

	// Action clip setup. Start/Stop trim run-ups and long tails; the contact frame is measured
	// from the clip (fastest moment of ContactBone between Start and Stop).
	struct FActionDef
	{
		EPKAction Action;
		const TCHAR* Asset;
		float Start;
		float Stop;
		const TCHAR* ContactBone;
		bool bForwardContact; // contact = peak forward velocity (strikes, throws, headers) instead of peak speed
		bool bTravel;
		bool bLock;
		bool bGrounded;
		float BlendIn;
		float BlendOut;
	};

	// Clip choices and trims come from the clip-sheet review (-PKClipSheet / -PKClipFrames).
	const FActionDef ActionDefs[] = {
		{ EPKAction::Pass,        TEXT("PK_KickSoccerball_2"),        0.00f, 0.50f, TEXT("ball_r"), true,  false, false, false, 0.05f, 0.15f },
		{ EPKAction::PassLeft,    TEXT("PK_KickSoccerball"),          0.05f, 0.56f, TEXT("ball_l"), true,  false, false, false, 0.05f, 0.15f },
		{ EPKAction::Shot,        TEXT("PK_SoccerPenaltyKick"),       0.45f, 1.25f, TEXT("ball_r"), true,  false, true,  false, 0.10f, 0.25f },
		{ EPKAction::Volley,      TEXT("PK_ScissorKick"),             0.58f, 2.70f, TEXT("ball_r"), false, true,  true,  true,  0.06f, 0.25f },
		{ EPKAction::RunningShot, TEXT("PK_StrikeFowardJog"),         0.15f, 0.95f, TEXT("ball_r"), true,  false, false, false, 0.10f, 0.20f },
		{ EPKAction::Header,      TEXT("PK_HeaderSoccerball"),        0.84f, 1.55f, TEXT("head"),   true,  false, true,  false, 0.08f, 0.20f },
		{ EPKAction::Tackle,      TEXT("PK_SoccerTackle"),            0.30f, 0.92f, TEXT("ball_r"), true,  false, true,  false, 0.08f, 0.28f },
		{ EPKAction::SlideTackle, TEXT("PK_SoccerTackle_2"),          0.00f, 1.77f, TEXT("ball_r"), false, true,  true,  true,  0.06f, 0.20f },
		{ EPKAction::SlideTackleB,TEXT("PK_SoccerTackle"),            0.35f, 2.60f, TEXT("ball_r"), false, true,  true,  true,  0.06f, 0.20f },
		{ EPKAction::SlideTackleC,TEXT("PK_SoccerTackle_3"),          0.00f, 2.25f, TEXT("ball_r"), false, true,  true,  true,  0.06f, 0.20f },
		{ EPKAction::Trip,        TEXT("PK_SoccerTrip"),              0.00f, 1.60f, TEXT("head"),   false, true,  true,  true,  0.06f, 0.08f },
		{ EPKAction::Fallen,      TEXT("PK_FallenIdle"),              0.00f, 0.90f, TEXT("head"),   false, false, true,  true,  0.10f, 0.10f },
		{ EPKAction::StandUp,     TEXT("PK_StandingUp"),              0.00f, 1.67f, TEXT("head"),   false, false, true,  true,  0.10f, 0.25f },
		{ EPKAction::GKDiveLeft,  TEXT("PK_GoalkeeperDivingSave"),    0.85f, 2.60f, TEXT("hand_l"), false, true,  true,  true,  0.06f, 0.25f },
		{ EPKAction::GKDiveRight, TEXT("PK_GoalkeeperDivingSave_2"),  0.80f, 2.50f, TEXT("hand_r"), false, true,  true,  true,  0.06f, 0.25f },
		{ EPKAction::GKCatch,     TEXT("PK_GoalkeeperCatch_2"),       0.30f, 1.27f, TEXT("hand_r"), false, false, true,  false, 0.08f, 0.20f },
		{ EPKAction::GKCatchHigh, TEXT("PK_GoalkeeperCatch_3"),       1.55f, 2.20f, TEXT("hand_r"), false, false, true,  false, 0.08f, 0.25f },
		{ EPKAction::GKCatchLow,  TEXT("PK_GoalkeeperCatch"),         0.60f, 1.80f, TEXT("hand_r"), false, false, true,  false, 0.08f, 0.25f },
		{ EPKAction::GKBlock,     TEXT("PK_GoalkeeperBodyBlock"),     0.50f, 2.40f, TEXT("head"),   false, false, true,  true,  0.08f, 0.25f },
		{ EPKAction::GKPunt,      TEXT("PK_GoalkeeperDropKick"),      1.70f, 3.00f, TEXT("ball_r"), true,  false, true,  false, 0.10f, 0.25f },
		{ EPKAction::GKRoll,      TEXT("PK_GoalkeeperPass"),          0.60f, 1.60f, TEXT("hand_r"), true,  false, true,  false, 0.10f, 0.25f },
		{ EPKAction::GKThrow,     TEXT("PK_GoalkeeperOverhandThrow"), 0.80f, 2.20f, TEXT("hand_r"), true,  false, true,  false, 0.10f, 0.25f },
		{ EPKAction::GKScoop,     TEXT("PK_GoalkeeperScoop"),         0.40f, 2.00f, TEXT("hand_r"), false, false, true,  false, 0.10f, 0.25f },
		{ EPKAction::GKMiss,      TEXT("PK_GoalkeeperMiss"),          0.00f, 2.90f, TEXT("head"),   false, false, true,  false, 0.10f, 0.30f },
		{ EPKAction::ThrowIn,     TEXT("PK_ThrowIn"),                 1.20f, 2.40f, TEXT("hand_r"), true,  false, true,  false, 0.10f, 0.25f },
		{ EPKAction::ThighControl,TEXT("PK_KneeingSoccerball"),       0.25f, 0.85f, TEXT("calf_l"), false, false, false, false, 0.08f, 0.20f },
		// Holding poses: frozen frames from the catch / throw-in clips.
		{ EPKAction::GKHold,      TEXT("PK_GoalkeeperCatch_2"),       0.62f, 0.66f, TEXT("hand_r"), false, false, true,  false, 0.20f, 0.00f },
		{ EPKAction::ThrowHold,   TEXT("PK_ThrowIn"),                 1.18f, 1.22f, TEXT("hand_r"), false, false, true,  false, 0.20f, 0.00f },
	};

	FPKActionClip GActionClips[(int32)EPKAction::Count];
	bool GActionTableBuilt = false;

	FCompactPoseBoneIndex FindBone(const FCompactPose& Pose, FName Name)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		if (MeshIndex == INDEX_NONE)
		{
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	FTransform ComponentSpace(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FTransform Result = Pose[Bone];
		FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
		while (Parent.IsValid())
		{
			Result = Result * Pose[Parent];
			Parent = Pose.GetParentBoneIndex(Parent);
		}
		return Result;
	}

	// Rotates a bone about a component-space axis through its own pivot.
	void RotateBone(FCompactPose& Pose, FName Name, const FVector& Axis, float Degrees)
	{
		if (FMath::IsNearlyZero(Degrees))
		{
			return;
		}
		const FCompactPoseBoneIndex Bone = FindBone(Pose, Name);
		if (!Bone.IsValid())
		{
			return;
		}
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
		const FQuat ParentRot = Parent.IsValid() ? ComponentSpace(Pose, Parent).GetRotation() : FQuat::Identity;
		const FQuat Delta(Axis, FMath::DegreesToRadians(Degrees));
		FQuat NewLocal = ParentRot.Inverse() * Delta * ParentRot * Pose[Bone].GetRotation();
		NewLocal.Normalize();
		Pose[Bone].SetRotation(NewLocal);
	}

	// Turns a bone so the segment to its child points along Target (component space), blended by Alpha. Used for
	// poses that must not depend on what the clip underneath is doing (celebrating arms).
	void AimBone(FCompactPose& Pose, FName Name, FName ChildName, const FVector& Target, float Alpha)
	{
		const FCompactPoseBoneIndex Bone = FindBone(Pose, Name);
		const FCompactPoseBoneIndex Child = FindBone(Pose, ChildName);
		if (!Bone.IsValid() || !Child.IsValid() || Alpha <= 0.f)
		{
			return;
		}
		const FVector Cur = (ComponentSpace(Pose, Child).GetLocation() - ComponentSpace(Pose, Bone).GetLocation()).GetSafeNormal();
		if (Cur.IsNearlyZero())
		{
			return;
		}
		const FVector Want = FMath::Lerp(Cur, Target.GetSafeNormal(), FMath::Clamp(Alpha, 0.f, 1.f)).GetSafeNormal();
		FVector Axis;
		float Angle = 0.f;
		FQuat::FindBetweenNormals(Cur, Want).ToAxisAndAngle(Axis, Angle);
		if (Angle > 1.e-3f)
		{
			RotateBone(Pose, Name, Axis, FMath::RadiansToDegrees(Angle));
		}
	}

	void OffsetBone(FCompactPose& Pose, FName Name, const FVector& Offset)
	{
		const FCompactPoseBoneIndex Bone = FindBone(Pose, Name);
		if (!Bone.IsValid() || Offset.IsNearlyZero())
		{
			return;
		}
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
		const FQuat ParentRot = Parent.IsValid() ? ComponentSpace(Pose, Parent).GetRotation() : FQuat::Identity;
		Pose[Bone].AddToTranslation(ParentRot.Inverse().RotateVector(Offset));
	}

	float SmoothStep01(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	}

	// Component-space position of a bone in a sequence at a time (game thread).
	FVector BoneComponentPosition(const UAnimSequence* Seq, const FReferenceSkeleton& Ref, int32 Bone, float Time)
	{
		FTransform CS = FTransform::Identity;
		const FAnimExtractContext Context(static_cast<double>(Time));
		for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B))
		{
			FTransform Local;
			Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(B), Context, false);
			CS = CS * Local;
		}
		return CS.GetLocation();
	}
}

FVector2D FPKActionClip::TravelAt(float Time) const
{
	if (Travel.Num() == 0)
	{
		return FVector2D::ZeroVector;
	}
	const float F = FMath::Max(0.f, Time) * TravelRate;
	const int32 I = FMath::Min(FMath::FloorToInt(F), Travel.Num() - 1);
	const int32 J = FMath::Min(I + 1, Travel.Num() - 1);
	return FMath::Lerp(Travel[I], Travel[J], F - FMath::FloorToFloat(F));
}

// ---------------------------------------------------------------------------------------------
// UPKAnimInstance (game thread)
// ---------------------------------------------------------------------------------------------

UPKAnimInstance::UPKAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> KeeperIdleFinder(TEXT("/Game/Characters/Mannequins/Anims/Soccer/PK_GoalkeeperIdle.PK_GoalkeeperIdle"));
	IdleClip = IdleFinder.Object;
	KeeperIdleClip = KeeperIdleFinder.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ShuffleLeftFinder(TEXT("/Game/Characters/Mannequins/Anims/Soccer/PK_GoalkeeperSidestep.PK_GoalkeeperSidestep"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ShuffleRightFinder(TEXT("/Game/Characters/Mannequins/Anims/Soccer/PK_GoalkeeperSidestep_2.PK_GoalkeeperSidestep_2"));
	KeeperShuffleLeft = ShuffleLeftFinder.Object;
	KeeperShuffleRight = ShuffleRightFinder.Object;

	for (int32 i = 0; i < 8; ++i)
	{
		const FString WalkName = FString::Printf(TEXT("MF_Unarmed_Walk_%s"), DirSuffix[i]);
		const FString JogName = FString::Printf(TEXT("MF_Unarmed_Jog_%s"), DirSuffix[i]);
		ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/%s.%s"), *WalkName, *WalkName));
		ConstructorHelpers::FObjectFinder<UAnimSequence> JogFinder(*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/%s.%s"), *JogName, *JogName));
		WalkClips.Add(WalkFinder.Object);
		JogClips.Add(JogFinder.Object);
	}

	ActionClips.SetNum((int32)EPKAction::Count);
	for (const FActionDef& Def : ActionDefs)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Soccer/%s.%s"), Def.Asset, Def.Asset));
		ActionClips[(int32)Def.Action] = Finder.Object;
	}
}

void UPKAnimInstance::BuildActionTable(const UPKAnimInstance* Source)
{
	if (GActionTableBuilt)
	{
		return;
	}
	GActionTableBuilt = true;

	for (const FActionDef& Def : ActionDefs)
	{
		FPKActionClip& Clip = GActionClips[(int32)Def.Action];
		const UAnimSequence* Seq = Source->ActionClips.IsValidIndex((int32)Def.Action) ? Source->ActionClips[(int32)Def.Action].Get() : nullptr;
		if (!Seq || !Seq->GetSkeleton())
		{
			continue;
		}

		const float Length = Seq->GetPlayLength();
		Clip.Seq = Seq;
		Clip.Start = FMath::Clamp(Def.Start, 0.f, Length);
		Clip.Stop = FMath::Clamp(Def.Stop, Clip.Start + 0.05f, Length);
		Clip.BlendIn = Def.BlendIn;
		Clip.BlendOut = Def.BlendOut;
		Clip.bApplyTravel = Def.bTravel;
		const bool bForward = Def.bForwardContact;
		const bool bFootStrike = FCString::Strncmp(Def.ContactBone, TEXT("ball_"), 5) == 0;
		Clip.bLockMovement = Def.bLock;
		Clip.bGrounded = Def.bGrounded;
		Clip.bHoldAtEnd = Def.Action == EPKAction::GKHold || Def.Action == EPKAction::ThrowHold;

		const FReferenceSkeleton& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		const int32 ContactBone = Ref.FindBoneIndex(Def.ContactBone);

		// Pelvis travel (in-place conversion + capsule root motion).
		if (Pelvis != INDEX_NONE)
		{
			const FVector P0 = BoneComponentPosition(Seq, Ref, Pelvis, 0.f);
			const int32 Samples = FMath::CeilToInt(Length * FPKActionClip::TravelRate) + 1;
			Clip.Travel.SetNum(Samples);
			for (int32 i = 0; i < Samples; ++i)
			{
				const FVector P = BoneComponentPosition(Seq, Ref, Pelvis, FMath::Min(Length, i / FPKActionClip::TravelRate));
				Clip.Travel[i] = FVector2D(P.X - P0.X, P.Y - P0.Y);
			}
		}

		// Contact = fastest moment of the contact bone in the action window.
		Clip.Contact = FMath::Lerp(Clip.Start, Clip.Stop, 0.4f);
		if (ContactBone != INDEX_NONE)
		{
			float BestSpeed = 0.f;
			const float Step = 1.f / 60.f;
			const float From = Def.Action == EPKAction::Volley ? 0.6f : Clip.Start;
			const float To = Def.Action == EPKAction::Volley ? 0.95f : Clip.Stop;
			FVector Prev = BoneComponentPosition(Seq, Ref, ContactBone, From);
			float PrevRel = Pelvis != INDEX_NONE ? Prev.Y - BoneComponentPosition(Seq, Ref, Pelvis, From).Y : 0.f;
			for (float T = From + Step; T <= To; T += Step)
			{
				const FVector P = BoneComponentPosition(Seq, Ref, ContactBone, T);
				// Mesh space +Y is forward: strikes peak on forward velocity, catches on overall speed.
				const float Speed = bForward ? (P.Y - Prev.Y) / Step : (P - Prev).Size() / Step;
				if (Speed > BestSpeed)
				{
					BestSpeed = Speed;
					Clip.Contact = T - Step * 0.5f;
				}
				// Foot strikes: contact is when the swinging foot reaches the ball spot in front of the body
				// (peak swing speed comes a few frames earlier, during the backswing-to-strike).
				if (bForward && bFootStrike && Pelvis != INDEX_NONE)
				{
					const float Rel = P.Y - BoneComponentPosition(Seq, Ref, Pelvis, T).Y;
					if (Rel >= 30.f && PrevRel < 30.f && P.Y > Prev.Y)
					{
						Clip.Contact = T;
						break;
					}
					PrevRel = Rel;
				}
				Prev = P;
			}
		}
		Clip.ContactBone = Def.ContactBone;
		if (ContactBone != INDEX_NONE)
		{
			const FVector2D TravelAtContact = Clip.TravelAt(Clip.Contact);
			Clip.ContactPos = BoneComponentPosition(Seq, Ref, ContactBone, Clip.Contact) - FVector(TravelAtContact.X, TravelAtContact.Y, 0.f);
			// How fast the striking foot / head / hand is moving at contact (over +-2 frames, body travel removed).
			const float H = 2.f / 60.f;
			const float T0 = FMath::Max(0.f, Clip.Contact - H);
			const float T1 = FMath::Min(Length, Clip.Contact + H);
			const FVector2D Tr0 = Clip.TravelAt(T0);
			const FVector2D Tr1 = Clip.TravelAt(T1);
			const FVector A = BoneComponentPosition(Seq, Ref, ContactBone, T0) - FVector(Tr0.X, Tr0.Y, 0.f);
			const FVector B = BoneComponentPosition(Seq, Ref, ContactBone, T1) - FVector(Tr1.X, Tr1.Y, 0.f);
			Clip.ContactSpeed = (B - A).Size() / FMath::Max(T1 - T0, 1.e-3f);
		}
		UE_LOG(LogTemp, Log, TEXT("PK action %s: start %.2f contact %.2f stop %.2f travel %.0f cm contact pos %s contact speed %.0f"), Def.Asset, Clip.Start, Clip.Contact, Clip.Stop, Clip.Travel.Num() ? Clip.Travel.Last().Size() : 0.f, *Clip.ContactPos.ToCompactString(), Clip.ContactSpeed);
	}
}

const FPKActionClip& UPKAnimInstance::GetActionClip(EPKAction Action)
{
	return GActionClips[FMath::Clamp((int32)Action, 0, (int32)EPKAction::Count - 1)];
}

FVector2D UPKAnimInstance::MeasureTravel(const UAnimSequence* Seq, float Time)
{
	if (!Seq || !Seq->GetSkeleton())
	{
		return FVector2D::ZeroVector;
	}
	const FReferenceSkeleton& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
	const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
	if (Pelvis == INDEX_NONE)
	{
		return FVector2D::ZeroVector;
	}
	const FVector P0 = BoneComponentPosition(Seq, Ref, Pelvis, 0.f);
	const FVector P = BoneComponentPosition(Seq, Ref, Pelvis, Time);
	return FVector2D(P.X - P0.X, P.Y - P0.Y);
}

void UPKAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BuildActionTable(this);

	// Measure each clip's stride speed from its root motion so play rate can match ground speed.
	auto MeasureSpeed = [](const UAnimSequence* Seq, float Fallback)
	{
		if (!Seq || Seq->GetPlayLength() <= 0.f)
		{
			return Fallback;
		}
		const FTransform Root = Seq->ExtractRootMotionFromRange(0.0, Seq->GetPlayLength(), FAnimExtractContext());
		const float Speed = FVector2D(Root.GetTranslation().X, Root.GetTranslation().Y).Size() / Seq->GetPlayLength();
		return Speed > 10.f ? Speed : Fallback;
	};

	WalkSpeeds.SetNum(8);
	JogSpeeds.SetNum(8);
	for (int32 i = 0; i < 8; ++i)
	{
		WalkSpeeds[i] = MeasureSpeed(WalkClips.IsValidIndex(i) ? WalkClips[i].Get() : nullptr, 165.f);
		JogSpeeds[i] = MeasureSpeed(JogClips.IsValidIndex(i) ? JogClips[i].Get() : nullptr, 400.f);
	}

	// Keeper shuffle stride speed from the clips' body travel.
	auto ShuffleSpeed = [](const UAnimSequence* Seq)
	{
		return Seq && Seq->GetPlayLength() > 0.f ? FMath::Max(80.f, MeasureTravel(Seq, Seq->GetPlayLength()).Size() / Seq->GetPlayLength()) : 200.f;
	};
	KeeperShuffleSpeed[0] = ShuffleSpeed(KeeperShuffleRight);
	KeeperShuffleSpeed[1] = ShuffleSpeed(KeeperShuffleLeft);

	if (APawn* Pawn = TryGetPawnOwner())
	{
		LastYaw = Pawn->GetActorRotation().Yaw;
	}
}

FAnimInstanceProxy* UPKAnimInstance::CreateAnimInstanceProxy()
{
	return new FPKAnimProxy(this);
}

void UPKAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APKFootballer* Footballer = Cast<APKFootballer>(TryGetPawnOwner());
	if (!Footballer || DeltaSeconds <= 0.f)
	{
		return;
	}

	const FRotator Facing = Footballer->GetActorRotation();
	const FVector Velocity = Footballer->GetVelocity();
	const FVector Local = Facing.UnrotateVector(FVector(Velocity.X, Velocity.Y, 0.f));

	Inputs.Speed = FVector2D(Local.X, Local.Y).Size();
	if (Inputs.Speed > 20.f)
	{
		const float RawDir = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		SmoothedDirection = FMath::FixedTurn(SmoothedDirection, RawDir, 720.f * DeltaSeconds);
	}
	Inputs.Direction = SmoothedDirection;

	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Facing.Yaw) / DeltaSeconds;
	LastYaw = Facing.Yaw;
	SmoothedYawRate = FMath::FInterpTo(SmoothedYawRate, YawRate, DeltaSeconds, 8.f);
	Inputs.Lean = FMath::Clamp(SmoothedYawRate * Inputs.Speed / 600.f * 0.035f, -14.f, 14.f);

	Inputs.SprintAlpha = FMath::FInterpTo(Inputs.SprintAlpha, Footballer->IsSprinting() ? 1.f : 0.f, DeltaSeconds, 5.f);
	Inputs.CrouchAlpha = FMath::FInterpTo(Inputs.CrouchAlpha, Footballer->IsInStance() ? 1.f : 0.f, DeltaSeconds, 8.f);
	Inputs.KickAlpha = Footballer->GetKickAlpha();
	Inputs.KickPower = Footballer->GetKickPower();
	Inputs.bKeeperIdle = Footballer->IsKeeper() && KeeperIdleClip != nullptr;

	// Action layer.
	Inputs.ActionSeq = nullptr;
	Inputs.ActionWeight = 0.f;
	const EPKAction Action = Footballer->GetCurrentAction();
	const FPKActionClip& Clip = GetActionClip(Action);
	if (Footballer->DebugClip)
	{
		Inputs.ActionSeq = Footballer->DebugClip;
		Inputs.ActionTime = Footballer->DebugTime;
		Inputs.ActionTravel = MeasureTravel(Footballer->DebugClip, Footballer->DebugTime);
		Inputs.ActionWeight = 1.f;
	}
	else if (Action != EPKAction::None && Clip.IsValid())
	{
		const float T = Footballer->GetActionTime();
		Inputs.ActionSeq = Clip.Seq;
		Inputs.ActionTime = T;
		Inputs.ActionTravel = Clip.TravelAt(T);
		const float In = Clip.BlendIn > 0.f ? FMath::Clamp((T - Footballer->GetActionEntry()) / Clip.BlendIn, 0.f, 1.f) : 1.f;
		const float Out = (Clip.BlendOut > 0.f && !Clip.bHoldAtEnd) ? FMath::Clamp((Clip.Stop - T) / Clip.BlendOut, 0.f, 1.f) : 1.f;
		Inputs.ActionWeight = SmoothStep01(FMath::Min(In, Out));
	}

	// Track the ball with the head.
	const FVector ToLook = Facing.UnrotateVector(Footballer->GetLookTarget() - Footballer->GetActorLocation() - FVector(0.f, 0.f, 70.f));
	const float TargetYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToLook.Y, ToLook.X)), -70.f, 70.f);
	const float TargetPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToLook.Z, FVector2D(ToLook.X, ToLook.Y).Size())), -35.f, 20.f);
	Inputs.HeadYaw = FMath::FInterpTo(Inputs.HeadYaw, TargetYaw, DeltaSeconds, 6.f);
	Inputs.HeadPitch = FMath::FInterpTo(Inputs.HeadPitch, TargetPitch, DeltaSeconds, 6.f);

	// Dribble cut: quick in (the foot is already swinging through), slower out.
	const float TouchAge = (float)(Footballer->GetWorld()->GetTimeSeconds() - Footballer->FootTouchTime);
	Inputs.TouchAlpha = 0.f;
	if (Footballer->FootTouchSide != 0.f && TouchAge >= 0.f && TouchAge < 0.32f)
	{
		Inputs.TouchAlpha = TouchAge < 0.07f ? SmoothStep01(TouchAge / 0.07f) : 1.f - SmoothStep01((TouchAge - 0.07f) / 0.25f);
	}
	Inputs.TouchFoot = Footballer->FootTouchFoot;
	Inputs.CelebrateAlpha = FMath::FInterpTo(Inputs.CelebrateAlpha, Footballer->bCelebrating ? 1.f : 0.f, DeltaSeconds, 4.f);
	Inputs.CelebrateTime = Footballer->bCelebrating ? Inputs.CelebrateTime + DeltaSeconds : 0.f;
	{
		static const float SkillSeconds[] = { 0.f, 0.45f, 0.45f, 0.6f, 0.5f, 0.3f };
		const uint8 Pose = Footballer->SkillPose < 6 ? Footballer->SkillPose : 0;
		const float Age = (float)(Footballer->GetWorld()->GetTimeSeconds() - Footballer->SkillTime);
		Inputs.SkillPose = Pose;
		Inputs.SkillT = (Pose != 0 && Age >= 0.f && Age < SkillSeconds[Pose]) ? Age / SkillSeconds[Pose] : -1.f;
		Inputs.SkillSide = Footballer->SkillSide;
	}
	Inputs.TouchSide = Footballer->FootTouchSide;
	Inputs.bTouchOutside = Footballer->bFootTouchOutside;
}

// ---------------------------------------------------------------------------------------------
// FPKAnimProxy (may run on a worker thread)
// ---------------------------------------------------------------------------------------------

void FPKAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	const UPKAnimInstance* Instance = CastChecked<UPKAnimInstance>(InAnimInstance);
	auto MakeClip = [](const UAnimSequence* Seq, float Speed)
	{
		FClip Clip;
		Clip.Seq = Seq;
		Clip.Length = Seq ? FMath::Max(Seq->GetPlayLength(), 0.01f) : 1.f;
		Clip.Speed = Speed;
		return Clip;
	};

	Idle = MakeClip(Instance->IdleClip, 0.f);
	KeeperIdle = MakeClip(Instance->KeeperIdleClip, 0.f);
	KeeperShuffle[0] = MakeClip(Instance->KeeperShuffleRight, 200.f);
	KeeperShuffle[1] = MakeClip(Instance->KeeperShuffleLeft, 200.f);
	for (FClip& Shuffle : KeeperShuffle)
	{
		Shuffle.bInPlace = true;
		if (Shuffle.Seq && Shuffle.Seq->GetSkeleton())
		{
			const FReferenceSkeleton& Ref = Shuffle.Seq->GetSkeleton()->GetReferenceSkeleton();
			const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
			if (Pelvis != INDEX_NONE)
			{
				const FVector P0 = BoneComponentPosition(Shuffle.Seq, Ref, Pelvis, 0.f);
				Shuffle.PelvisXY = FVector2D(P0.X, P0.Y);
			}
		}
	}
	for (int32 i = 0; i < 8; ++i)
	{
		Walk[i] = MakeClip(Instance->WalkClips.IsValidIndex(i) ? Instance->WalkClips[i].Get() : nullptr, 165.f);
		Jog[i] = MakeClip(Instance->JogClips.IsValidIndex(i) ? Instance->JogClips[i].Get() : nullptr, 400.f);
	}
}

void FPKAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UPKAnimInstance* Instance = CastChecked<UPKAnimInstance>(InAnimInstance);
	Inputs = Instance->Inputs;
	KeeperShuffle[0].Speed = Instance->KeeperShuffleSpeed[0];
	KeeperShuffle[1].Speed = Instance->KeeperShuffleSpeed[1];
	for (int32 i = 0; i < 8; ++i)
	{
		if (Instance->WalkSpeeds.IsValidIndex(i))
		{
			Walk[i].Speed = Instance->WalkSpeeds[i];
			Jog[i].Speed = Instance->JogSpeeds[i];
		}
	}
}

void FPKAnimProxy::Update(float DeltaSeconds)
{
	const float Speed = Inputs.Speed;
	IdleTime += DeltaSeconds;

	WeightIdle = FMath::Clamp(1.f - Speed / 90.f, 0.f, 1.f);

	const float Dir = FMath::Fmod(Inputs.Direction + 360.f, 360.f);
	const float Slot = Dir / 45.f;
	DirA = FMath::FloorToInt(Slot) % 8;
	DirB = (DirA + 1) % 8;
	DirBlend = Slot - FMath::FloorToFloat(Slot);

	const float WalkRef = FMath::Lerp(Walk[DirA].Speed, Walk[DirB].Speed, DirBlend);
	const float JogRef = FMath::Lerp(Jog[DirA].Speed, Jog[DirB].Speed, DirBlend);
	GaitAlpha = FMath::Clamp((Speed - WalkRef) / FMath::Max(JogRef - WalkRef, 1.f), 0.f, 1.f);

	const float RefSpeed = FMath::Lerp(WalkRef, JogRef, GaitAlpha);
	const float PlayRate = FMath::Clamp(Speed / FMath::Max(RefSpeed, 1.f), 0.55f, 1.8f);
	const float CycleLength = FMath::Lerp(
		FMath::Lerp(Walk[DirA].Length, Walk[DirB].Length, DirBlend),
		FMath::Lerp(Jog[DirA].Length, Jog[DirB].Length, DirBlend), GaitAlpha);

	Phase = FMath::Frac(Phase + DeltaSeconds * PlayRate / FMath::Max(CycleLength, 0.01f));
}

void FPKAnimProxy::Accumulate(const FClip& Clip, float Time, float Weight, FPoseContext& Output, bool& bFirst)
{
	if (!Clip.Seq || Weight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FPoseContext Sample(this);
	FAnimationPoseData PoseData(Sample);
	Clip.Seq->GetAnimationPose(PoseData, FAnimExtractContext(double(Time), false));

	if (Clip.bInPlace)
	{
		const FCompactPoseBoneIndex Root(0);
		const FCompactPoseBoneIndex Pelvis = FindBone(Sample.Pose, TEXT("pelvis"));
		if (Pelvis.IsValid() && Sample.Pose.GetParentBoneIndex(Pelvis) == Root)
		{
			FTransform PelvisCS = Sample.Pose[Pelvis] * Sample.Pose[Root];
			PelvisCS.SetTranslation(FVector(Clip.PelvisXY.X, Clip.PelvisXY.Y, PelvisCS.GetTranslation().Z));
			Sample.Pose[Root] = FTransform::Identity;
			Sample.Pose[Pelvis] = PelvisCS;
		}
	}

	const ScalarRegister W(Weight);
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		if (bFirst)
		{
			Output.Pose[Bone] = Sample.Pose[Bone] * W;
		}
		else
		{
			Output.Pose[Bone].AccumulateWithShortestRotation(Sample.Pose[Bone], W);
		}
	}
	bFirst = false;
}

bool FPKAnimProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();

	const FClip& IdleClip = Inputs.bKeeperIdle && KeeperIdle.Seq ? KeeperIdle : Idle;

	bool bFirst = true;
	const float Loco = 1.f - WeightIdle;
	Accumulate(IdleClip, FMath::Fmod(IdleTime, IdleClip.Length), WeightIdle, Output, bFirst);
	auto WalkClip = [this](int32 Dir) -> const FClip&
	{
		if (Inputs.bKeeperIdle && (Dir == 2 || Dir == 6) && KeeperShuffle[Dir == 2 ? 0 : 1].Seq)
		{
			return KeeperShuffle[Dir == 2 ? 0 : 1];
		}
		return Walk[Dir];
	};
	const FClip& WalkA = WalkClip(DirA);
	const FClip& WalkB = WalkClip(DirB);
	Accumulate(WalkA, Phase * WalkA.Length, Loco * (1.f - GaitAlpha) * (1.f - DirBlend), Output, bFirst);
	Accumulate(WalkB, Phase * WalkB.Length, Loco * (1.f - GaitAlpha) * DirBlend, Output, bFirst);
	Accumulate(Jog[DirA], Phase * Jog[DirA].Length, Loco * GaitAlpha * (1.f - DirBlend), Output, bFirst);
	Accumulate(Jog[DirB], Phase * Jog[DirB].Length, Loco * GaitAlpha * DirBlend, Output, bFirst);

	if (bFirst)
	{
		return true; // no clips loaded: reference pose
	}
	Output.Pose.NormalizeRotations();

	ApplyProcedural(Output);
	ApplyAction(Output);
	ApplyHeadLook(Output);
	ApplyHands(Output);
	return true;
}

void FPKAnimProxy::ApplyAction(FPoseContext& Output)
{
	if (!Inputs.ActionSeq || Inputs.ActionWeight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FPoseContext Action(this);
	FAnimationPoseData PoseData(Action);
	Inputs.ActionSeq->GetAnimationPose(PoseData, FAnimExtractContext(double(Inputs.ActionTime), false));

	// Make the clip in-place: keep our locomotion root, and pull the pelvis back by the clip's travel
	// (the footballer moves the capsule by the same travel when the action applies it).
	const FCompactPoseBoneIndex Root(0);
	const FCompactPoseBoneIndex Pelvis = FindBone(Action.Pose, TEXT("pelvis"));
	if (Pelvis.IsValid() && Action.Pose.GetParentBoneIndex(Pelvis) == Root)
	{
		FTransform PelvisCS = Action.Pose[Pelvis] * Action.Pose[Root];
		PelvisCS.AddToTranslation(FVector(-Inputs.ActionTravel.X, -Inputs.ActionTravel.Y, 0.f));
		Action.Pose[Root] = Output.Pose[Root];
		Action.Pose[Pelvis] = PelvisCS.GetRelativeTransform(Output.Pose[Root]);
	}

	const float W = Inputs.ActionWeight;
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		FTransform Blended;
		Blended.Blend(Output.Pose[Bone], Action.Pose[Bone], W);
		Output.Pose[Bone] = Blended;
	}
	Output.Pose.NormalizeRotations();
}

void FPKAnimProxy::ApplyProcedural(FPoseContext& Output) const
{
	FCompactPose& Pose = Output.Pose;

	// Body lean: forward with speed/sprint, sideways into turns.
	const float ForwardLean = 3.f * FMath::Clamp(Inputs.Speed / 500.f, 0.f, 1.f) + 9.f * Inputs.SprintAlpha + 10.f * Inputs.CrouchAlpha;
	RotateBone(Pose, TEXT("spine_01"), AxisSide, -ForwardLean);
	RotateBone(Pose, TEXT("pelvis"), AxisForward, -Inputs.Lean);

	// Defensive / shielding stance: sink the hips and bend both knees.
	if (Inputs.CrouchAlpha > 0.01f)
	{
		const float C = Inputs.CrouchAlpha;
		OffsetBone(Pose, TEXT("pelvis"), FVector(0.f, 0.f, -12.f * C));
		for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
		{
			RotateBone(Pose, *FString::Printf(TEXT("thigh_%s"), Side), AxisSide, 24.f * C);
			RotateBone(Pose, *FString::Printf(TEXT("calf_%s"), Side), AxisSide, -40.f * C);
			RotateBone(Pose, *FString::Printf(TEXT("foot_%s"), Side), AxisSide, 16.f * C);
		}
	}

	// Dribble cut with the outside (or inside) of the foot: the touching leg swings towards the side the ball goes,
	// the foot turns so its outside (inside) faces the ball and, for the outside, rolls onto its outer edge.
	if (Inputs.TouchAlpha > 0.01f)
	{
		const float A = Inputs.TouchAlpha * (1.f - Inputs.ActionWeight);
		const TCHAR* Side = Inputs.TouchFoot == 0 ? TEXT("l") : TEXT("r");
		const float S = Inputs.TouchSide;
		RotateBone(Pose, *FString::Printf(TEXT("thigh_%s"), Side), AxisForward, S * (Inputs.bTouchOutside ? 14.f : 11.f) * A);
		RotateBone(Pose, *FString::Printf(TEXT("thigh_%s"), Side), AxisSide, 10.f * A);
		RotateBone(Pose, *FString::Printf(TEXT("foot_%s"), Side), AxisUp, -S * (Inputs.bTouchOutside ? 32.f : 38.f) * A);
		if (Inputs.bTouchOutside)
		{
			RotateBone(Pose, *FString::Printf(TEXT("foot_%s"), Side), AxisForward, -S * 12.f * A);
		}
	}

	// Goal celebration: the "aeroplane" - arms spread out flat and swept back a little, hands open, banking from side
	// to side as he runs. Every segment is aimed outright (upper arm, forearm, hand, each finger), so the run cycle's
	// arm swing and clenched fists can't bend them.
	if (Inputs.CelebrateAlpha > 0.01f)
	{
		const float C = Inputs.CelebrateAlpha * (1.f - Inputs.ActionWeight);
		const float Bank = FMath::Sin(Inputs.CelebrateTime * 2.1f) * 9.f;
		RotateBone(Pose, TEXT("spine_02"), AxisForward, Bank * C);
		RotateBone(Pose, TEXT("spine_03"), AxisSide, 5.f * C); // chest up
		for (const float Side : { 1.f, -1.f })
		{
			const TCHAR* S = Side > 0.f ? TEXT("l") : TEXT("r"); // +X is the player's left
			// The banking tilts the wings: the low side a little lower.
			const float Tilt = FMath::DegreesToRadians(Bank) * Side * -0.6f;
			const FVector Upper(0.95f * Side, -0.16f, 0.2f + Tilt);
			const FVector Fore(0.95f * Side, -0.08f, 0.24f + Tilt);
			AimBone(Pose, *FString::Printf(TEXT("upperarm_%s"), S), *FString::Printf(TEXT("lowerarm_%s"), S), Upper, C);
			AimBone(Pose, *FString::Printf(TEXT("lowerarm_%s"), S), *FString::Printf(TEXT("hand_%s"), S), Fore, C);
			AimBone(Pose, *FString::Printf(TEXT("hand_%s"), S), *FString::Printf(TEXT("middle_01_%s"), S), Fore, C);
			// Open hand: fingers straight along the forearm (the run clip has them in a fist).
			for (const TCHAR* Finger : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
			{
				for (int32 J = 1; J <= 2; ++J)
				{
					AimBone(Pose, *FString::Printf(TEXT("%s_%02d_%s"), Finger, J, S), *FString::Printf(TEXT("%s_%02d_%s"), Finger, J + 1, S), Fore, C);
				}
			}
		}
	}

	// Skill moves (no clips in the pack): procedural leg work layered on the run.
	if (Inputs.SkillT >= 0.f && Inputs.SkillPose != 0)
	{
		const float T = Inputs.SkillT;
		const float Env = FMath::Sin(PI * T) * (1.f - Inputs.ActionWeight);
		const float S = Inputs.SkillSide;
		const TCHAR* Leg = S > 0.f ? TEXT("r") : TEXT("l");
		const FString Thigh = FString::Printf(TEXT("thigh_%s"), Leg);
		const FString Calf = FString::Printf(TEXT("calf_%s"), Leg);
		const FString Foot = FString::Printf(TEXT("foot_%s"), Leg);
		switch (Inputs.SkillPose)
		{
		case 1: // step-over: the leg circles over the ball from inside to outside, body dips that way
			RotateBone(Pose, *Thigh, AxisSide, 35.f * Env);
			RotateBone(Pose, *Thigh, AxisForward, S * FMath::Lerp(-18.f, 28.f, SmoothStep01(T)) * Env);
			RotateBone(Pose, *Calf, AxisSide, -40.f * Env);
			RotateBone(Pose, TEXT("pelvis"), AxisForward, -S * 9.f * Env);
			break;
		case 2: // ball roll: sole on the ball, sweeping across
			RotateBone(Pose, *Thigh, AxisSide, 22.f * Env);
			RotateBone(Pose, *Thigh, AxisForward, S * FMath::Lerp(-22.f, 24.f, SmoothStep01(T)) * Env);
			RotateBone(Pose, *Calf, AxisSide, -35.f * Env);
			RotateBone(Pose, *Foot, AxisSide, 22.f * Env);
			break;
		case 3: // roulette: low and compact while turning
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				RotateBone(Pose, *FString::Printf(TEXT("thigh_%s"), Side), AxisSide, 14.f * Env);
				RotateBone(Pose, *FString::Printf(TEXT("calf_%s"), Side), AxisSide, -24.f * Env);
			}
			OffsetBone(Pose, TEXT("pelvis"), FVector(0.f, 0.f, -6.f * Env));
			break;
		case 4: // rainbow flick: heel kicks up behind
			RotateBone(Pose, *Thigh, AxisSide, -22.f * Env);
			RotateBone(Pose, *Calf, AxisSide, -105.f * Env);
			RotateBone(Pose, *Foot, AxisSide, -25.f * Env);
			break;
		case 5: // nutmeg: quick poke
			RotateBone(Pose, *Thigh, AxisSide, 20.f * Env);
			RotateBone(Pose, *Calf, AxisSide, -18.f * Env);
			break;
		default:
			break;
		}
	}

	// Procedural kick, only used when an action clip is missing.
	if (Inputs.KickAlpha >= 0.f && !Inputs.ActionSeq)
	{
		const float T = Inputs.KickAlpha;
		const float Amp = FMath::Lerp(0.65f, 1.f, Inputs.KickPower);
		float Thigh = 0.f;
		float Calf = 0.f;
		if (T < 0.38f)
		{
			const float S = SmoothStep01(T / 0.38f);
			Thigh = -32.f * S;
			Calf = -85.f * S;
		}
		else if (T < 0.58f)
		{
			const float S = SmoothStep01((T - 0.38f) / 0.2f);
			Thigh = FMath::Lerp(-32.f, 62.f, S);
			Calf = FMath::Lerp(-85.f, -4.f, S);
		}
		else
		{
			const float S = SmoothStep01((T - 0.58f) / 0.42f);
			Thigh = FMath::Lerp(62.f, 0.f, S);
			Calf = FMath::Lerp(-4.f, 0.f, S);
		}
		RotateBone(Pose, TEXT("thigh_r"), AxisSide, Thigh * Amp);
		RotateBone(Pose, TEXT("calf_r"), AxisSide, Calf * Amp);
	}
}

void FPKAnimProxy::ApplyHands(FPoseContext& Output) const
{
	// Relaxed athletic hands instead of the clips' flat splayed fingers, and a touch smaller (the mannequin's hands
	// read oversized next to the scanned heads). Curl is about each finger bone's own Z axis.
	static const float Curl = [] { float C = 1.f; FParse::Value(FCommandLine::Get(), TEXT("PKFingerCurl="), C); return C; }();
	static const TCHAR* Fingers[] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	static const float Joint[3] = { 22.f, 38.f, 24.f };
	for (const TCHAR* S : { TEXT("l"), TEXT("r") })
	{
		const float Side = S[0] == 'l' ? 1.f : -1.f;
		const FCompactPoseBoneIndex Hand = FindBone(Output.Pose, *FString::Printf(TEXT("hand_%s"), S));
		if (Hand.IsValid())
		{
			Output.Pose[Hand].SetScale3D(FVector(0.92f));
		}
		for (int32 F = 0; F < 4; ++F)
		{
			for (int32 J = 1; J <= 3; ++J)
			{
				const FCompactPoseBoneIndex Bone = FindBone(Output.Pose, *FString::Printf(TEXT("%s_%02d_%s"), Fingers[F], J, S));
				if (Bone.IsValid())
				{
					const float Deg = Joint[J - 1] * (1.f + 0.12f * F) * Curl * Side;
					Output.Pose[Bone].SetRotation((Output.Pose[Bone].GetRotation() * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Deg))).GetNormalized());
				}
			}
		}
	}
}

void FPKAnimProxy::ApplyHeadLook(FPoseContext& Output) const
{
	const float W = 1.f - Inputs.ActionWeight;
	if (W <= 0.01f)
	{
		return;
	}
	RotateBone(Output.Pose, TEXT("neck_01"), AxisUp, Inputs.HeadYaw * 0.45f * W);
	RotateBone(Output.Pose, TEXT("head"), AxisUp, Inputs.HeadYaw * 0.35f * W);
	// eyes on the ball, but mostly with the eyes: the full head tilt left every close-up staring at the grass
	RotateBone(Output.Pose, TEXT("head"), AxisSide, FMath::Clamp(Inputs.HeadPitch, -25.f, 25.f) * 0.3f * W);
	// chin up: the jog clips carry the head tipped forward, so every close-up looked at the grass
	static const float Lift = [] { float L = 12.f; FParse::Value(FCommandLine::Get(), TEXT("PKHeadLift="), L); return L; }();
	RotateBone(Output.Pose, TEXT("neck_01"), AxisSide, Lift * 0.5f * W);
	RotateBone(Output.Pose, TEXT("head"), AxisSide, Lift * 0.5f * W);
}
