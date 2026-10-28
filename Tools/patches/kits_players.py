# Kits and player looks: M_PK_Kit on the body, M_PK_KitShell on a cloth shell that follows the body's pose,
# club kit designs, per-player appearance (skin, hair, beard, boots, height, build).
import os, sys
SRC = os.path.join(os.path.dirname(__file__), "..", "..", "Source", "PitchKings")

def patch(name, pairs):
    p = os.path.join(SRC, name)
    s = open(p, encoding="utf-8").read()
    for a, b in pairs:
        if b in s:
            continue
        if a not in s:
            sys.exit(f"{name}: not found: {a[:70]!r}")
        s = s.replace(a, b, 1)
    open(p, "w", encoding="utf-8").write(s)

patch("PKFootballer.h", [
("""// Shot techniques (FC-style): picked by the modifiers held when shoot is released.""",
"""// A club's kit (the keeper wears his own colours with long sleeves and gloves). Pattern: 0 plain, 1 stripes,
// 2 hoops, 3 sash, 4 halves, 5 pinstripes.
struct FPKKit
{
	FLinearColor Primary = FLinearColor(0.02f, 0.12f, 0.6f);
	FLinearColor Secondary = FLinearColor(0.01f, 0.03f, 0.15f);
	FLinearColor Trim = FLinearColor(0.85f, 0.85f, 0.85f);
	FLinearColor Shorts = FLinearColor(0.85f, 0.85f, 0.85f);
	FLinearColor Socks = FLinearColor(0.02f, 0.12f, 0.6f);
	FLinearColor Number = FLinearColor(0.9f, 0.9f, 0.9f);
	int32 Pattern = 0;
	FLinearColor KeeperPrimary = FLinearColor(0.1f, 0.55f, 0.08f);
	FLinearColor KeeperSecondary = FLinearColor(0.02f, 0.2f, 0.02f);
	FLinearColor KeeperShorts = FLinearColor(0.02f, 0.02f, 0.02f);
	FLinearColor KeeperGloves = FLinearColor(0.85f, 0.85f, 0.85f);
};

// How a player looks: skin, hair (0 shaved, 1 buzz, 2 short, 3 medium, 4 big), beard (0..1), boots, build.
struct FPKLook
{
	FLinearColor Skin = FLinearColor(0.5f, 0.32f, 0.2f);
	FLinearColor Hair = FLinearColor(0.02f, 0.015f, 0.01f);
	FLinearColor Boots = FLinearColor(0.02f, 0.02f, 0.02f);
	int32 HairStyle = 2;
	float Beard = 0.f;
	bool bBootFlash = true;
	float Height = 1.f; // body scale
	float Build = 1.f;
};

// Shot techniques (FC-style): picked by the modifiers held when shoot is released."""),
("""	TObjectPtr<UMaterialInterface> KitMaterial;""", """	TObjectPtr<UMaterialInterface> KitMaterial;
	// Cloth shell: a second copy of the body following its pose, drawn with M_PK_KitShell (sleeve and shorts hems,
	// flared shorts, sock tops, hair volume).
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> KitShell;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitBodyMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitShellMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> KitBodyMID;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> KitShellMID;"""),
("""	void InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome);""",
"""	void InitFootballer(int32 InTeam, EPKRole InRole, int32 InIndex, const FVector2D& InHome);
	// Dress the player: club kit, shirt number, appearance and body build.
	void ApplyKit(const FPKKit& Kit, const FPKLook& Look);
	FPKLook Look;"""),
])

patch("PKFootballer.cpp", [
("""	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/PitchKings/Materials/M_PK_Surface.M_PK_Surface"));""",
"""	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/PitchKings/Materials/M_PK_Surface.M_PK_Surface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> KitBody(TEXT("/Game/PitchKings/Materials/M_PK_Kit.M_PK_Kit"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> KitShellMat(TEXT("/Game/PitchKings/Materials/M_PK_KitShell.M_PK_KitShell"));
	KitBodyMaterial = KitBody.Object;
	KitShellMaterial = KitShellMat.Object;"""),
("""	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
""", """	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	KitShell = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("KitShell"));
	KitShell->SetupAttachment(Body);
	KitShell->SetSkeletalMesh(MannyMesh.Object);
	KitShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	KitShell->SetVisibility(KitShellMaterial != nullptr);
"""),
("""	LookTarget = GetActorLocation() + GetActorForwardVector() * 500.f;""", """	LookTarget = GetActorLocation() + GetActorForwardVector() * 500.f;
	if (KitShell)
	{
		KitShell->SetLeaderPoseComponent(GetMesh()); // follows the body's pose, no animation of its own
	}"""),
("""	// Kit: shirt on the torso slot, shorts/socks on the legs slot.
	USkeletalMeshComponent* Body = GetMesh();
	const int32 TorsoSlot = Body->GetMaterialIndex(TEXT("M_Torso"));
	const int32 LegsSlot = Body->GetMaterialIndex(TEXT("M_HeadLegs"));
	if (KitMaterial)
	{""", """	// Kit: a plain team kit until the team sheet dresses the player (ApplyKit).
	USkeletalMeshComponent* Body = GetMesh();
	const int32 TorsoSlot = Body->GetMaterialIndex(TEXT("M_Torso"));
	const int32 LegsSlot = Body->GetMaterialIndex(TEXT("M_HeadLegs"));
	if (KitBodyMaterial)
	{
		FPKKit Plain;
		Plain.Primary = PK::TeamColor(Team, false);
		Plain.Secondary = Plain.Primary * 0.5f;
		Plain.Socks = Plain.Primary;
		Plain.Shorts = PK::ShortsColor(Team, false);
		ApplyKit(Plain, Look);
	}
	else if (KitMaterial)
	{"""),
("""void APKFootballer::ApplyProfile(""", """void APKFootballer::ApplyKit(const FPKKit& Kit, const FPKLook& InLook)
{
	Look = InLook;
	USkeletalMeshComponent* Body = GetMesh();
	if (!KitBodyMaterial || !Body)
	{
		return;
	}
	if (!KitBodyMID)
	{
		KitBodyMID = UMaterialInstanceDynamic::Create(KitBodyMaterial, this);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, KitBodyMID);
		}
	}
	if (!KitShellMID && KitShellMaterial && KitShell)
	{
		KitShellMID = UMaterialInstanceDynamic::Create(KitShellMaterial, this);
		for (int32 Slot = 0; Slot < KitShell->GetNumMaterials(); ++Slot)
		{
			KitShell->SetMaterial(Slot, KitShellMID);
		}
	}
	const bool bKeeper = IsKeeper();
	const float Lum = (bKeeper ? Kit.KeeperPrimary : Kit.Primary).GetLuminance();
	for (UMaterialInstanceDynamic* MID : { KitBodyMID.Get(), KitShellMID.Get() })
	{
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(TEXT("Primary"), bKeeper ? Kit.KeeperPrimary : Kit.Primary);
		MID->SetVectorParameterValue(TEXT("Secondary"), bKeeper ? Kit.KeeperSecondary : Kit.Secondary);
		MID->SetVectorParameterValue(TEXT("Trim"), Kit.Trim);
		MID->SetVectorParameterValue(TEXT("Shorts"), bKeeper ? Kit.KeeperShorts : Kit.Shorts);
		MID->SetVectorParameterValue(TEXT("Socks"), bKeeper ? Kit.KeeperPrimary : Kit.Socks);
		MID->SetVectorParameterValue(TEXT("NumberColor"), bKeeper ? (Lum > 0.25f ? FLinearColor(0.02f, 0.02f, 0.02f) : FLinearColor(0.9f, 0.9f, 0.9f)) : Kit.Number);
		MID->SetVectorParameterValue(TEXT("Boots"), Look.Boots);
		MID->SetVectorParameterValue(TEXT("Skin"), Look.Skin);
		MID->SetVectorParameterValue(TEXT("Hair"), Look.Hair);
		MID->SetVectorParameterValue(TEXT("Gloves"), Kit.KeeperGloves);
		// KitA = (pattern, number, sleeve length, shorts length), KitB = (hair style, beard, keeper, boot flash)
		MID->SetVectorParameterValue(TEXT("KitA"), FLinearColor(bKeeper ? 0.f : (float)Kit.Pattern, (float)ShirtNumber, bKeeper ? 1.f : 0.52f, 0.46f));
		MID->SetVectorParameterValue(TEXT("KitB"), FLinearColor((float)Look.HairStyle, Look.Beard, bKeeper ? 1.f : 0.f, Look.bBootFlash ? 1.f : 0.f));
	}
	// Body build: taller / broader players (feet stay on the ground: the mesh origin is at the feet).
	Body->SetRelativeScale3D(FVector(Look.Build, Look.Build * 0.97f + 0.03f, Look.Height));
}

void APKFootballer::ApplyProfile("""),
])

patch("PKTeamData.h", [
("""	uint32 TraitsPlus = 0; // PlayStyle+ (elite) versions""", """	uint32 TraitsPlus = 0; // PlayStyle+ (elite) versions
	FPKLook Look;"""),
("""	TArray<FPKSquadPlayer> Squad; // 18: 2 GK, 6 DEF, 6 MID, 4 FWD""", """	TArray<FPKSquadPlayer> Squad; // 18: 2 GK, 6 DEF, 6 MID, 4 FWD
	FPKKit Kit;"""),
])

patch("PKTeamData.cpp", [
("""	TArray<FPKClub> MakeClubs()
	{""", """	FLinearColor Lin(float R, float G, float B)
	{
		return FLinearColor(FColor((uint8)(R * 255.f), (uint8)(G * 255.f), (uint8)(B * 255.f)));
	}

	// Appearance from the player's own seed: skin, hair, beard, boots, and a build that follows his strength.
	FPKLook MakeLook(FRandomStream& R, const FPKAttributes& A)
	{
		static const FLinearColor Skins[] = { Lin(0.96f, 0.80f, 0.69f), Lin(0.90f, 0.72f, 0.58f), Lin(0.80f, 0.60f, 0.45f), Lin(0.68f, 0.48f, 0.33f),
			Lin(0.55f, 0.37f, 0.25f), Lin(0.42f, 0.27f, 0.18f), Lin(0.30f, 0.19f, 0.13f) };
		static const FLinearColor Hairs[] = { Lin(0.05f, 0.04f, 0.035f), Lin(0.12f, 0.08f, 0.05f), Lin(0.25f, 0.16f, 0.09f), Lin(0.42f, 0.28f, 0.15f),
			Lin(0.78f, 0.62f, 0.38f), Lin(0.48f, 0.20f, 0.09f) };
		static const FLinearColor BootColors[] = { Lin(0.06f, 0.06f, 0.06f), Lin(0.92f, 0.92f, 0.92f), Lin(0.75f, 0.95f, 0.15f), Lin(1.f, 0.42f, 0.1f),
			Lin(0.15f, 0.45f, 0.95f), Lin(0.9f, 0.12f, 0.2f), Lin(0.95f, 0.85f, 0.2f) };
		FPKLook L;
		const int32 SkinI = R.RandRange(0, UE_ARRAY_COUNT(Skins) - 1);
		L.Skin = Skins[SkinI];
		// Dark hair is the most common; light hair goes with lighter skin.
		int32 HairI = R.RandRange(0, 3);
		if (SkinI <= 2 && R.FRand() < 0.3f)
		{
			HairI = R.RandRange(3, 5);
		}
		L.Hair = Hairs[HairI];
		const float S = R.FRand();
		L.HairStyle = S < 0.12f ? 0 : (S < 0.35f ? 1 : (S < 0.72f ? 2 : (S < 0.92f ? 3 : 4)));
		const float B = R.FRand();
		L.Beard = B < 0.55f ? 0.f : (B < 0.8f ? 0.45f : 0.9f);
		L.Boots = BootColors[R.RandRange(0, UE_ARRAY_COUNT(BootColors) - 1)];
		L.bBootFlash = R.FRand() < 0.7f;
		L.Height = FMath::Clamp(0.965f + 0.06f * R.FRand() + 0.02f * (A.Physical - 0.7f), 0.95f, 1.04f);
		L.Build = FMath::Clamp(0.95f + 0.1f * A.Physical + R.FRandRange(-0.02f, 0.02f), 0.95f, 1.06f);
		return L;
	}

	TArray<FPKClub> MakeClubs()
	{"""),
("""				Pl.Attr = MakeAttributes(R, Pl.Pos, Club.Quality + Depth[i]);""", """				Pl.Attr = MakeAttributes(R, Pl.Pos, Club.Quality + Depth[i]);
				{
					FRandomStream LookSeed(R.GetCurrentSeed() ^ (i * 7919));
					Pl.Look = MakeLook(LookSeed, Pl.Attr);
				}"""),
("""			Club.Formation = Defs[c].Formation;""", """			Club.Formation = Defs[c].Formation;
			Club.Kit = MakeKit(c);"""),
("""	FPKLook MakeLook(FRandomStream& R, const FPKAttributes& A)
	{""", """	// Kits: the person's club in blue, the opponents in reds, each with its own design and keeper colours.
	FPKKit MakeKit(int32 Club)
	{
		FPKKit K;
		switch (Club)
		{
		case 0: // Pitch Kings FC: royal blue with navy pinstripes, white shorts
			K.Primary = Lin(0.08f, 0.30f, 0.85f); K.Secondary = Lin(0.03f, 0.10f, 0.40f); K.Trim = Lin(0.95f, 0.95f, 0.95f);
			K.Shorts = Lin(0.95f, 0.95f, 0.97f); K.Socks = Lin(0.08f, 0.30f, 0.85f); K.Number = Lin(0.97f, 0.97f, 0.97f); K.Pattern = 5;
			K.KeeperPrimary = Lin(0.15f, 0.75f, 0.25f); K.KeeperSecondary = Lin(0.05f, 0.35f, 0.1f); K.KeeperShorts = Lin(0.08f, 0.08f, 0.1f);
			break;
		case 1: // Red Rovers: red and white hoops
			K.Primary = Lin(0.80f, 0.06f, 0.08f); K.Secondary = Lin(0.96f, 0.96f, 0.96f); K.Trim = Lin(0.96f, 0.96f, 0.96f);
			K.Shorts = Lin(0.96f, 0.96f, 0.96f); K.Socks = Lin(0.80f, 0.06f, 0.08f); K.Number = Lin(0.08f, 0.08f, 0.1f); K.Pattern = 2;
			K.KeeperPrimary = Lin(0.98f, 0.82f, 0.1f); K.KeeperSecondary = Lin(0.4f, 0.32f, 0.02f); K.KeeperShorts = Lin(0.08f, 0.08f, 0.08f);
			break;
		case 2: // Crimson Athletic: crimson and black stripes, gold trim
			K.Primary = Lin(0.62f, 0.03f, 0.10f); K.Secondary = Lin(0.07f, 0.07f, 0.08f); K.Trim = Lin(0.95f, 0.75f, 0.2f);
			K.Shorts = Lin(0.07f, 0.07f, 0.08f); K.Socks = Lin(0.07f, 0.07f, 0.08f); K.Number = Lin(0.95f, 0.78f, 0.25f); K.Pattern = 1;
			K.KeeperPrimary = Lin(0.2f, 0.75f, 0.85f); K.KeeperSecondary = Lin(0.05f, 0.3f, 0.4f); K.KeeperShorts = Lin(0.05f, 0.2f, 0.3f);
			break;
		case 3: // Union Rojo: red with a white sash, red shorts
			K.Primary = Lin(0.85f, 0.12f, 0.12f); K.Secondary = Lin(0.97f, 0.97f, 0.97f); K.Trim = Lin(0.97f, 0.97f, 0.97f);
			K.Shorts = Lin(0.85f, 0.12f, 0.12f); K.Socks = Lin(0.97f, 0.97f, 0.97f); K.Number = Lin(0.97f, 0.97f, 0.97f); K.Pattern = 3;
			K.KeeperPrimary = Lin(0.45f, 0.15f, 0.65f); K.KeeperSecondary = Lin(0.15f, 0.04f, 0.25f); K.KeeperShorts = Lin(0.08f, 0.08f, 0.08f);
			break;
		default: // Scarlet City: scarlet and maroon halves
			K.Primary = Lin(0.95f, 0.18f, 0.12f); K.Secondary = Lin(0.40f, 0.05f, 0.08f); K.Trim = Lin(0.98f, 0.85f, 0.75f);
			K.Shorts = Lin(0.40f, 0.05f, 0.08f); K.Socks = Lin(0.95f, 0.18f, 0.12f); K.Number = Lin(0.98f, 0.95f, 0.9f); K.Pattern = 4;
			K.KeeperPrimary = Lin(0.65f, 0.9f, 0.15f); K.KeeperSecondary = Lin(0.25f, 0.4f, 0.05f); K.KeeperShorts = Lin(0.1f, 0.1f, 0.1f);
			break;
		}
		return K;
	}

	FPKLook MakeLook(FRandomStream& R, const FPKAttributes& A)
	{"""),
])

patch("PKGameMode.cpp", [
("""			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name, Pl.TraitsPlus);""",
"""			P->ApplyProfile(PKTeams::RoleFor(Slot.Pos), Slot.Home, Attr, Pl.Traits, Pl.Number, Pl.Name, Pl.TraitsPlus);
			P->ApplyKit(Club.Kit, Pl.Look);"""),
("""	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name, In.TraitsPlus);""",
"""	P->ApplyProfile(PKTeams::RoleFor(S.Pos), S.Home, Attr, In.Traits, In.Number, In.Name, In.TraitsPlus);
	P->ApplyKit(Club.Kit, In.Look);"""),
])
print("kits ok")
