// -PKCritic=<shots file>: screenshots for the art-direction critic, taken during a live AI-vs-AI match from camera
// positions the critic chooses. One shot per line, fields separated by '|':
//
//   name | seconds into play | camera | look-at | fov | options
//
//   camera   broadcast                  the normal match camera (look-at / fov ignored)
//            x,y,z                      a fixed point (cm; pitch centre 0,0,0, Z up, pitch 105 x 68 m along X)
//            follow:<target>:dx,dy,dz   offset from a moving target
//            orbit:<target>:fwd,right,up  offset in the target player's own frame (portraits)
//   look-at  ball | carrier | player:<team 0/1>:<shirt number>[:head|:feet] | x,y,z
//   fov      degrees (smaller = more zoom)
//   options  nohud  (hide the HUD for this shot)
//
// Lines starting with '#' are comments. Shots go to Saved/Screenshots/Critic/<name>.png; the game quits after the
// last one. A missing / unknown target falls back to the ball.

#include "PKGameMode.h"
#include "Misc/App.h"
#include "PitchKings.h"
#include "PKBall.h"
#include "PKFootballer.h"
#include "PKTypes.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HighResScreenshot.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	bool ParseVec(const FString& S, FVector& Out)
	{
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(","));
		if (P.Num() != 3)
		{
			return false;
		}
		Out = FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
		return true;
	}
}

void APKGameMode::LoadCriticShots(const FString& File)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *File))
	{
		UE_LOG(LogPitchKings, Error, TEXT("[Critic] cannot read shot list %s"), *File);
		return;
	}
	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> F;
		Line.ParseIntoArray(F, TEXT("|"), false);
		for (FString& X : F)
		{
			X.TrimStartAndEndInline();
		}
		if (F.Num() < 3)
		{
			UE_LOG(LogPitchKings, Warning, TEXT("[Critic] skipped line: %s"), *Line);
			continue;
		}
		FPKCriticShot Shot;
		Shot.Name = F[0];
		Shot.Time = FCString::Atof(*F[1]);
		Shot.Camera = F[2];
		Shot.Look = F.Num() > 3 ? F[3] : TEXT("ball");
		Shot.Fov = F.Num() > 4 && !F[4].IsEmpty() && F[4] != TEXT("-") ? FCString::Atof(*F[4]) : 60.f;
		Shot.bNoHud = F.Num() > 5 && F[5].Contains(TEXT("nohud"));
		CriticShots.Add(Shot);
	}
	CriticShots.Sort([](const FPKCriticShot& A, const FPKCriticShot& B) { return A.Time < B.Time; });
	UE_LOG(LogPitchKings, Log, TEXT("[Critic] %d shots loaded"), CriticShots.Num());
}

FVector APKGameMode::CriticTarget(const FString& Spec) const
{
	FVector V;
	if (ParseVec(Spec, V))
	{
		return V;
	}
	if (Spec.StartsWith(TEXT("player:")))
	{
		TArray<FString> P;
		Spec.ParseIntoArray(P, TEXT(":"));
		if (P.Num() == 3 || P.Num() == 4)
		{
			const int32 Team = FCString::Atoi(*P[1]);
			const int32 Number = FCString::Atoi(*P[2]);
			for (const APKFootballer* F : Footballers)
			{
				if (F->Team == Team && F->ShirtNumber == Number)
				{
					if (P.Num() == 4 && P[3] == TEXT("head"))
					{
						return F->GetMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 6.f);
					}
					if (P.Num() == 4 && P[3] == TEXT("feet"))
					{
						return (F->GetMesh()->GetSocketLocation(TEXT("foot_l")) + F->GetMesh()->GetSocketLocation(TEXT("foot_r"))) * 0.5f;
					}
					return F->GetActorLocation() + FVector(0.f, 0.f, 20.f);
				}
			}
		}
	}
	if (Spec == TEXT("carrier"))
	{
		if (const APKFootballer* C = Ball->GetOwnerPlayer())
		{
			return C->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		}
	}
	return Ball->GetActorLocation();
}

void APKGameMode::UpdateCriticShots(float Dt)
{
	if (CriticShots.Num() == 0)
	{
		return;
	}
	// keep the developer console shut: a stray key press opened its prompt bar along the bottom of the shots
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleState != NAME_None)
	{
		GEngine->GameViewport->ViewportConsole->FakeGotoState(NAME_None);
	}
	// The clock runs while the ball is in play or stopped (not in menus); a short warm-up lets shaders settle.
	bool bCompiling = false;
#if WITH_EDITOR
	// In the editor new materials compile on first use: hold the clock until they are done.
	bCompiling = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
#endif
	if (Phase != EPKPhase::Menu && !bCompiling)
	{
		CriticClock += Dt;
	}
	const float Warmup = WITH_EDITOR ? 20.f : 4.f;
	const float T = CriticClock - Warmup;
	bCriticActive = false;
	bCriticInShot = false;
	// frame times after the warm-up (real time), logged on exit: the cost of what the shots show
	static TArray<float> FrameMs;
	if (T > 0.f && !bCompiling)
	{
		FrameMs.Add(FApp::GetDeltaTime() * 1000.f);
	}
	if (CriticIndex >= CriticShots.Num())
	{
		if (T > CriticShots.Last().Time + 1.5f)
		{
			if (FrameMs.Num())
			{
				TArray<float> Sorted = FrameMs;
				Sorted.Sort();
				float Sum = 0.f;
				for (float Ms : Sorted)
				{
					Sum += Ms;
				}
				UE_LOG(LogTemp, Display, TEXT("[Critic] perf: %d frames, avg %.2f ms (%.1f fps), median %.2f ms, 95th %.2f ms"),
					Sorted.Num(), Sum / Sorted.Num(), 1000.f * Sorted.Num() / Sum, Sorted[Sorted.Num() / 2], Sorted[Sorted.Num() * 95 / 100]);
			}
			FGenericPlatformMisc::RequestExit(false);
		}
		return;
	}
	const FPKCriticShot& Shot = CriticShots[CriticIndex];
	if (T < Shot.Time)
	{
		return;
	}
	// Frame the shot, hold it a moment (camera cut, temporal AA settles), then capture.
	bCriticActive = true;
	bCriticInShot = true;
	CriticHideHud = Shot.bNoHud;
	CriticFov = FMath::Clamp(Shot.Fov, 5.f, 120.f);
	CriticLookAt = CriticTarget(Shot.Look);
	if (Shot.Camera.StartsWith(TEXT("follow:")))
	{
		const int32 Last = Shot.Camera.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		FVector Offset(-400.f, -400.f, 200.f);
		ParseVec(Shot.Camera.Mid(Last + 1), Offset);
		CriticCamLoc = CriticTarget(Shot.Camera.Mid(7, Last - 7)) + Offset;
	}
	else if (Shot.Camera.StartsWith(TEXT("orbit:")))
	{
		// offset in the player's own frame (forward, right, up), for portraits
		const int32 Last = Shot.Camera.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		FVector Offset(150.f, 0.f, 0.f);
		ParseVec(Shot.Camera.Mid(Last + 1), Offset);
		const FString Target = Shot.Camera.Mid(6, Last - 6);
		FRotator Facing = FRotator::ZeroRotator;
		TArray<FString> P;
		Target.ParseIntoArray(P, TEXT(":"));
		for (const APKFootballer* F : Footballers)
		{
			if (P.Num() >= 3 && F->Team == FCString::Atoi(*P[1]) && F->ShirtNumber == FCString::Atoi(*P[2]))
			{
				Facing = FRotator(0.f, F->GetActorRotation().Yaw, 0.f);
			}
		}
		CriticCamLoc = CriticTarget(Target) + Facing.RotateVector(Offset);
	}
	else if (!ParseVec(Shot.Camera, CriticCamLoc))
	{
		bCriticActive = false; // "broadcast": the match camera
	}
	if (T >= Shot.Time + 0.35f && !bCriticShotTaken)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("Critic"), Shot.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		bCriticShotTaken = true;
		UE_LOG(LogPitchKings, Log, TEXT("[Critic] shot %s at %.1f s"), *Shot.Name, T);
	}
	if (T >= Shot.Time + 0.6f)
	{
		++CriticIndex;
		bCriticShotTaken = false;
	}
}

bool APKGameMode::GetCriticCamera(FVector& OutLocation, FVector& OutLookAt, float& OutFov) const
{
	if (!bCriticActive)
	{
		return false;
	}
	OutLocation = CriticCamLoc;
	OutLookAt = CriticLookAt;
	OutFov = CriticFov;
	return true;
}
