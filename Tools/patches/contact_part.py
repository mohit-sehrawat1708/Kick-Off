import os
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PitchKings")


def patch(name, pairs):
    path = os.path.join(base, name)
    s = open(path, encoding="utf-8-sig").read().replace("\r\n", "\n")
    for a, b in pairs:
        if a not in s:
            raise SystemExit("NOT FOUND in %s: %s" % (name, a[:100]))
        s = s.replace(a, b, 1)
    open(path, "w", encoding="utf-8").write(s)


# ---- anim: remember where the contact bone is at the contact frame
patch("PKAnimInstance.h", [
    ("\tfloat Contact = 0.f;      // clip time of the strike / catch / release (fastest contact bone)\n",
     "\tfloat Contact = 0.f;      // clip time of the strike / catch / release (fastest contact bone)\n"
     "\tFName ContactBone;\n"
     "\tFVector ContactPos = FVector::ZeroVector; // contact bone in mesh space at the contact frame, travel removed (in place)\n"),
])
patch("PKAnimInstance.cpp", [
    ("\t\tUE_LOG(LogTemp, Log, TEXT(\"PK action %s: start %.2f contact %.2f stop %.2f travel %.0f cm\"), Def.Asset, Clip.Start, Clip.Contact, Clip.Stop, Clip.Travel.Num() ? Clip.Travel.Last().Size() : 0.f);",
     "\t\tClip.ContactBone = Def.ContactBone;\n"
     "\t\tif (ContactBone != INDEX_NONE)\n\t\t{\n"
     "\t\t\tconst FVector2D TravelAtContact = Clip.TravelAt(Clip.Contact);\n"
     "\t\t\tClip.ContactPos = BoneComponentPosition(Seq, Ref, ContactBone, Clip.Contact) - FVector(TravelAtContact.X, TravelAtContact.Y, 0.f);\n\t\t}\n"
     "\t\tUE_LOG(LogTemp, Log, TEXT(\"PK action %s: start %.2f contact %.2f stop %.2f travel %.0f cm contact pos %s\"), Def.Asset, Clip.Start, Clip.Contact, Clip.Stop, Clip.Travel.Num() ? Clip.Travel.Last().Size() : 0.f, *Clip.ContactPos.ToCompactString());"),
])

# ---- footballer: contact point queries, pending touches, face the ball
patch("PKFootballer.h", [
    ("\tFVector GetKickFootPoint() const; // where the striking foot meets the ball\n",
     "\tFVector GetKickFootPoint() const; // where the striking foot meets the ball\n"
     "\tFVector GetKickContactPoint() const; // 3D: where the striking foot is at the contact frame\n"
     "\tFVector GetActionContactPoint(EPKAction Action, float Yaw) const; // contact bone at the contact frame, world\n"
     "\tFVector GetContactBoneLocation(EPKAction Action) const; // live contact bone position\n"
     "\tEPKAction GetKickClip() const { return KickClip; }\n"
     "\tfloat GetKickYaw() const { return KickYaw; }\n"
     "\tfloat GetActionRate() const { return ActionRate; }\n"
     "\tfloat TimeToContact() const; // seconds until the current action's contact frame (<= 0 once past)\n"),
    ("\tfloat RunTimer = 0.f;\n",
     "\tfloat RunTimer = 0.f;\n\n"
     "\t// A ball touch that waits for the action's contact frame (headers, thigh control, flicks).\n"
     "\tEPKAction TouchAction = EPKAction::None;\n"
     "\tFVector TouchVelocity = FVector::ZeroVector;\n"
     "\tbool bTouchIsKick = false;\n"
     "\tbool bWatchBall = true; // AI: turn to face the ball when standing or walking\n"),
])

patch("PKFootballer.cpp", [
    ("FVector APKFootballer::GetKickFootPoint() const\n{\n\tconst FVector Right = PK::Flat(GetActorRightVector()).GetSafeNormal();\n\treturn GetFlatLocation() + GetFacing() * 38.f + Right * (bKickLeftFoot ? -14.f : 14.f);\n}",
     "FVector APKFootballer::GetKickFootPoint() const\n{\n"
     "\tif (KickClip != EPKAction::None && UPKAnimInstance::GetActionClip(KickClip).IsValid() && !UPKAnimInstance::GetActionClip(KickClip).ContactPos.IsNearlyZero())\n\t{\n"
     "\t\treturn PK::Flat(GetKickContactPoint());\n\t}\n"
     "\tconst FVector Right = PK::Flat(GetActorRightVector()).GetSafeNormal();\n"
     "\treturn GetFlatLocation() + GetFacing() * 38.f + Right * (bKickLeftFoot ? -14.f : 14.f);\n}\n\n"
     "FVector APKFootballer::GetActionContactPoint(EPKAction Action, float Yaw) const\n{\n"
     "\tconst FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);\n"
     "\tconst USkeletalMeshComponent* Body = GetMesh();\n"
     "\tif (!Clip.IsValid() || !Body)\n\t{\n\t\treturn GetFlatLocation() + GetFacing() * 40.f + FVector(0.f, 0.f, PK::BallRadius);\n\t}\n"
     "\tconst FVector Local = Body->GetRelativeTransform().TransformPosition(Clip.ContactPos);\n"
     "\treturn GetActorLocation() + FRotator(0.f, Yaw, 0.f).RotateVector(Local);\n}\n\n"
     "FVector APKFootballer::GetKickContactPoint() const\n{\n"
     "\t// The foot meets the back of the ball: the ball centre sits a little ahead of the foot bone.\n"
     "\tconst FVector Foot = GetActionContactPoint(KickClip, KickYaw);\n"
     "\tconst FVector Dir = FRotator(0.f, KickYaw, 0.f).Vector();\n"
     "\treturn FVector(Foot.X, Foot.Y, 0.f) + Dir * 6.f + FVector(0.f, 0.f, FMath::Max(PK::BallRadius, Foot.Z));\n}\n\n"
     "FVector APKFootballer::GetContactBoneLocation(EPKAction Action) const\n{\n"
     "\tconst FPKActionClip& Clip = UPKAnimInstance::GetActionClip(Action);\n"
     "\tconst USkeletalMeshComponent* Body = GetMesh();\n"
     "\tif (Body && !Clip.ContactBone.IsNone() && Body->DoesSocketExist(Clip.ContactBone))\n\t{\n"
     "\t\treturn Body->GetSocketLocation(Clip.ContactBone);\n\t}\n"
     "\treturn GetActorLocation();\n}\n\n"
     "float APKFootballer::TimeToContact() const\n{\n"
     "\tconst FPKActionClip& Clip = UPKAnimInstance::GetActionClip(CurrentAction);\n"
     "\tif (CurrentAction == EPKAction::None || !Clip.IsValid())\n\t{\n\t\treturn 0.f;\n\t}\n"
     "\treturn (Clip.Contact - ActionTime) / FMath::Max(ActionRate, 0.1f);\n}"),

    # face the ball when standing / walking without it
    ("\tconst bool bFacingStance = (Stance == EPKStance::Jockey || Stance == EPKStance::Shield) && !FaceTarget.IsNearlyZero() && !IsInAction();\n"
     "\tMove->bOrientRotationToMovement = !bFacingStance && !IsActionLockingMovement() && !Kick.bPending;",
     "\tconst bool bFacingStance = (Stance == EPKStance::Jockey || Stance == EPKStance::Shield) && !FaceTarget.IsNearlyZero() && !IsInAction();\n"
     "\t// Off the ball at walking pace: keep the body open to the ball and side-step / back-pedal (8-way locomotion).\n"
     "\tconst float WatchSpeed = bHuman ? 40.f : 280.f;\n"
     "\tconst bool bFaceBall = bWatchBall && !bFacingStance && !bHasBall && !IsInAction() && !Kick.bPending && Stance == EPKStance::Normal\n"
     "\t\t&& Speed < WatchSpeed && MoveInput.Size() < 0.75f && !LookTarget.IsNearlyZero();\n"
     "\tMove->bOrientRotationToMovement = !bFacingStance && !bFaceBall && !IsActionLockingMovement() && !Kick.bPending;\n"
     "\tif (bFaceBall)\n\t{\n"
     "\t\tconst FVector ToBall = PK::Flat(LookTarget - GetActorLocation());\n"
     "\t\tif (ToBall.SizeSquared() > 40.f * 40.f)\n\t\t{\n"
     "\t\t\tSetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, ToBall.Rotation().Yaw, 0.f), DeltaSeconds, 5.f));\n\t\t}\n\t}"),
])

# ---- game mode
patch("PKGameMode.h", [
    ("\tint32 Chips = 0;\n",
     "\tint32 Chips = 0;\n\tint32 KickCount = 0;\n\tint32 Whiffs = 0;\n\tfloat ContactErrSum = 0.f;\n\tfloat ContactErrMax = 0.f;\n"
     "\tfloat BoneErrSum = 0.f;\n\tfloat FacingErrSum = 0.f;\n\tfloat FacingErrMax = 0.f;\n\tint32 DeferredTouches = 0;\n\tint32 MissedTouches = 0;\n"),
    ("\tvoid FlickUp(APKFootballer* P, const FVector& Dir);",
     "\tvoid FlickUp(APKFootballer* P, const FVector& Dir);\n"
     "\tvoid UpdatePendingTouches(float Dt);\n"
     "\tvoid QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate = 1.f);"),
])

patch("PKGameMode.cpp", [
    # tick order: pending touches with the kick alignment
    ("\t\tAlignBallForKicks(DeltaSeconds);\n",
     "\t\tAlignBallForKicks(DeltaSeconds);\n\t\tUpdatePendingTouches(DeltaSeconds);\n"),

    # autotest stats
    ("possession %.0fs-%.0fs | air %.0fs max %.1fm chips %d crosses %d flicks %d\"),",
     "possession %.0fs-%.0fs | air %.0fs max %.1fm chips %d crosses %d flicks %d | kicks %d whiffs %d contact err avg %.0f max %.0f bone %.0f cm facing avg %.0f max %.0f deg | touches %d missed %d\"),"),
    ("PossessionTime[1], AirTime, MaxBallZ / 100.f, Chips, Crosses, Flicks);",
     "PossessionTime[1], AirTime, MaxBallZ / 100.f, Chips, Crosses, Flicks,\n"
     "\t\tKickCount, Whiffs, KickCount ? ContactErrSum / KickCount : 0.f, ContactErrMax, KickCount ? BoneErrSum / KickCount : 0.f, KickCount ? FacingErrSum / KickCount : 0.f, FacingErrMax,\n"
     "\t\tDeferredTouches, MissedTouches);"),

    # release: the foot must actually be at the ball
    ("\t\tif (!bOwns && !(Ball->IsFree() && CanReachBall(P, bVolley ? 170.f : KickReach, bVolley ? 220.f : 90.f)))\n\t\t{\n\t\t\tcontinue; // the ball was taken or ran away during the wind-up: air kick\n\t\t}\n\t\tExecuteKick(P, Kick);",
     "\t\tif (!bOwns && !(Ball->IsFree() && CanReachBall(P, bVolley ? 170.f : KickReach, bVolley ? 220.f : 90.f)))\n\t\t{\n\t\t\tcontinue; // the ball was taken or ran away during the wind-up: air kick\n\t\t}\n"
     "\t\tconst bool bFromHands = Ball->GetHolder() == P;\n"
     "\t\tif (!bFromHands && !Ball->IsPlaced() && P->GetKickClip() != EPKAction::None)\n\t\t{\n"
     "\t\t\t// The ball only leaves when the striking foot is on it.\n"
     "\t\t\tconst FVector Contact = P->GetKickContactPoint();\n"
     "\t\t\tconst float Err = FVector::Dist(Ball->GetActorLocation(), Contact);\n"
     "\t\t\tif (Err > (bVolley ? 110.f : 70.f))\n\t\t\t{\n"
     "\t\t\t\t++Whiffs;\n"
     "\t\t\t\tUE_LOG(LogPitchKings, Log, TEXT(\"%s #%d air kick: ball %.0f cm from the foot\"), PK::TeamName(P->Team), P->Index, Err);\n"
     "\t\t\t\tcontinue;\n\t\t\t}\n"
     "\t\t\tBall->SetActorLocation(Contact);\n"
     "\t\t\tconst float BoneErr = FVector::Dist(Contact, P->GetContactBoneLocation(P->GetKickClip()));\n"
     "\t\t\tconst FVector KickDir = PK::Flat(Kick.Target - Contact).GetSafeNormal();\n"
     "\t\t\tconst float FacingErr = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(KickDir, P->GetFacing()), -1.f, 1.f)));\n"
     "\t\t\t++KickCount;\n\t\t\tContactErrSum += Err;\n\t\t\tContactErrMax = FMath::Max(ContactErrMax, Err);\n"
     "\t\t\tBoneErrSum += BoneErr;\n\t\t\tFacingErrSum += FacingErr;\n\t\t\tFacingErrMax = FMath::Max(FacingErrMax, FacingErr);\n\t\t}\n"
     "\t\tExecuteKick(P, Kick);"),

    # headers wait for the head
    ("\t\t\tP->PlayAction(EPKAction::Header);\n\t\t\tEventShot(TEXT(\"Header\"), P);\n"
     "\t\t\tconst FVector Toward = (OppGoal(P->Team) - PK::Flat(BallLoc)).GetSafeNormal2D().RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);\n"
     "\t\t\tBall->Kick(P, Toward * FMath::FRandRange(700.f, 1100.f) + FVector(0.f, 0.f, 250.f));\n\t\t\tcontinue;",
     "\t\t\tconst FVector Toward = (OppGoal(P->Team) - PK::Flat(BallLoc)).GetSafeNormal2D().RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);\n"
     "\t\t\tP->SetActorRotation(FRotator(0.f, (-PK::Flat(Ball->Velocity)).Rotation().Yaw * 0.5f + Toward.Rotation().Yaw * 0.5f, 0.f));\n"
     "\t\t\tQueueTouch(P, EPKAction::Header, Toward * FMath::FRandRange(700.f, 1100.f) + FVector(0.f, 0.f, 250.f), true);\n"
     "\t\t\tEventShot(TEXT(\"Header\"), P);\n\t\t\tcontinue;"),

    # thigh control waits for the knee
    ("\t\tP->PlayAction(EPKAction::ThighControl);\n\t\tBall->Touch(P, FirstTouch * 0.5f + FVector(0.f, 0.f, -100.f));",
     "\t\tQueueTouch(P, EPKAction::ThighControl, FirstTouch * 0.5f + FVector(0.f, 0.f, -100.f), false, 1.5f);"),

    # flick-up: a real foot touch
    ("\tP->PlayAction(EPKAction::ThighControl);\n\tBall->Touch(P, Dir * (PK::Flat(P->GetVelocity()).Size() * 0.6f + 320.f) + FVector(0.f, 0.f, 520.f));\n\tP->TouchTimer = 0.9f;",
     "\tP->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));\n"
     "\tQueueTouch(P, EPKAction::Pass, Dir * (PK::Flat(P->GetVelocity()).Size() * 0.6f + 320.f) + FVector(0.f, 0.f, 520.f), false, 1.15f);\n"
     "\tP->TouchTimer = 1.2f;"),

    # dribble: no touches while a deferred touch is pending; touches stay close to where the body faces
    ("\tif (P->IsKicking() || BallLoc.Z > 60.f)\n\t{\n\t\treturn;\n\t}\n\n\tconst FVector Vel = PK::Flat(P->GetVelocity());",
     "\tif (P->IsKicking() || BallLoc.Z > 60.f || P->TouchAction != EPKAction::None)\n\t{\n\t\treturn;\n\t}\n\n\tconst FVector Vel = PK::Flat(P->GetVelocity());"),
    ("\tconst FVector ToBall = Flat - P->GetFlatLocation();\n\tconst float Ahead = FVector::DotProduct(ToBall, Desired);",
     "\t// Touch the ball where the body can take it: at most ~40 degrees off the facing, smaller touch when turning.\n"
     "\tif (P->Stance != EPKStance::Shield)\n\t{\n"
     "\t\tconst FVector Facing = P->GetFacing();\n"
     "\t\tconst float TurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Desired), -1.f, 1.f)));\n"
     "\t\tif (TurnDeg > 40.f)\n\t\t{\n"
     "\t\t\tconst float SideSign = FVector::CrossProduct(Facing, Desired).Z >= 0.f ? 1.f : -1.f;\n"
     "\t\t\tDesired = Facing.RotateAngleAxis(40.f * SideSign, FVector::UpVector);\n"
     "\t\t\tLead *= 0.5f;\n\t\t\tBase *= 0.6f;\n\t\t}\n\t}\n\n"
     "\tconst FVector ToBall = Flat - P->GetFlatLocation();\n\tconst float Ahead = FVector::DotProduct(ToBall, Desired);"),
])

# alignment rewrite + pending touches
path = os.path.join(base, "PKGameMode.cpp")
s = open(path, encoding="utf-8").read()
start = s.index("void APKGameMode::AlignBallForKicks(float Dt)\n")
end = s.index("void APKGameMode::UpdatePredictedTaker(float Dt)")
s = s[:start] + r"""void APKGameMode::AlignBallForKicks(float Dt)
{
	// During a wind-up the ball is brought onto the striking foot so that it is exactly where the
	// foot will be on the contact frame: it leaves from the foot, in the direction the body faces.
	if (!Ball->IsFree() || Ball->IsPlaced())
	{
		return;
	}
	APKFootballer* P = Ball->GetController();
	if (!P || !P->IsKicking())
	{
		P = nullptr;
		float Best = 170.f;
		for (APKFootballer* F : Footballers)
		{
			if (F->IsKicking() && F->GetKickClip() != EPKAction::None)
			{
				const float D = FVector::Dist(PK::Flat(Ball->GetActorLocation()), F->GetFlatLocation());
				if (D < Best)
				{
					Best = D;
					P = F;
				}
			}
		}
	}
	if (!P || P->GetKickClip() == EPKAction::None || P->GetCurrentAction() != P->GetKickClip())
	{
		return;
	}
	const FVector BallLoc = Ball->GetActorLocation();
	const FVector Contact = P->GetKickContactPoint();
	const bool bVolley = P->GetCurrentAction() == EPKAction::Volley;
	if (FVector::Dist(BallLoc, Contact) > (bVolley ? 260.f : 170.f) || (!bVolley && BallLoc.Z > 60.f))
	{
		return;
	}
	const float Remaining = P->TimeToContact();
	const float Alpha = Remaining > Dt ? FMath::Clamp(Dt / Remaining, 0.f, 1.f) : 1.f;
	Ball->SetActorLocation(FMath::Lerp(BallLoc, Contact, Alpha));
	Ball->Velocity = FVector::ZeroVector;
}

void APKGameMode::QueueTouch(APKFootballer* P, EPKAction Action, const FVector& Velocity, bool bKick, float Rate)
{
	if (!P->PlayAction(Action, EPKAction::None, Rate))
	{
		// No clip: touch now.
		bKick ? Ball->Kick(P, Velocity) : Ball->Touch(P, Velocity);
		return;
	}
	P->TouchAction = Action;
	P->TouchVelocity = Velocity;
	P->bTouchIsKick = bKick;
	++DeferredTouches;
}

void APKGameMode::UpdatePendingTouches(float Dt)
{
	// Headers, thigh control and flicks: the ball moves only when the head / knee / foot reaches it.
	for (APKFootballer* P : Footballers)
	{
		if (P->TouchAction == EPKAction::None)
		{
			continue;
		}
		if (P->GetCurrentAction() != P->TouchAction || P->IsStunned() || !Ball->IsFree())
		{
			P->TouchAction = EPKAction::None;
			continue;
		}
		const FVector Contact = P->GetActionContactPoint(P->TouchAction, P->GetActorRotation().Yaw)
			+ (P->TouchAction == EPKAction::Pass ? P->GetFacing() * 6.f : (P->TouchAction == EPKAction::Header ? P->GetFacing() * 10.f : P->GetFacing() * 12.f));
		const FVector Target(Contact.X, Contact.Y, FMath::Max(PK::BallRadius, Contact.Z));
		const FVector BallLoc = Ball->GetActorLocation();
		const float Remaining = P->TimeToContact();
		if (Remaining > 0.f)
		{
			if (FVector::Dist(BallLoc, Target) < 220.f)
			{
				Ball->SetActorLocation(FMath::Lerp(BallLoc, Target, Remaining > Dt ? FMath::Clamp(Dt / Remaining, 0.f, 1.f) : 1.f));
				Ball->Velocity = FVector::ZeroVector;
			}
			continue;
		}
		const EPKAction Action = P->TouchAction;
		P->TouchAction = EPKAction::None;
		if (FVector::Dist(BallLoc, Target) > 90.f)
		{
			++MissedTouches;
			continue;
		}
		Ball->SetActorLocation(Target);
		if (P->bTouchIsKick)
		{
			Ball->Kick(P, P->TouchVelocity);
		}
		else
		{
			Ball->Touch(P, P->TouchVelocity);
		}
		UE_LOG(LogPitchKings, Verbose, TEXT("%s #%d touch %d at contact"), PK::TeamName(P->Team), P->Index, (int32)Action);
	}
}

""" + s[end:]
open(path, "w", encoding="utf-8").write(s)
print("contact ok")
