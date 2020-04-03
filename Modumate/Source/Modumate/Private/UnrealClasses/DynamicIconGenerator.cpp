// Copyright 2018 Modumate, Inc. All Rights Reserved.


#include "DynamicIconGenerator.h"
#include "Runtime/Engine/Classes/GameFramework/SpringArmComponent.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ConstructorHelpers.h"
#include "Runtime/Engine/Classes/Components/RectLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetRenderingLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "ModumateFunctionLibrary.h"

//C:\Program Files\Epic Games\UE_4.22\Engine\Source\Runtime\Engine\Classes\Kismet\KismetRenderingLibrary.h

// Sets default values
ADynamicIconGenerator::ADynamicIconGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Setup spring arm and capture
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = 550.f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 6.f), FRotator(-0.13f, -30.f, -66.72f));

	SceneCaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComp"));
	SceneCaptureComp->SetupAttachment(SpringArm);
	SceneCaptureComp->FOVAngle = 5.f;
	SceneCaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCaptureComp->bCaptureEveryFrame = false;
	SceneCaptureComp->bCaptureOnMovement = false;
	SceneCaptureComp->bAlwaysPersistRenderingState = true;
	SceneCaptureComp->ShowFlags.Fog = false;
	SceneCaptureComp->ShowFlags.Atmosphere = false;
	SceneCaptureComp->ShowFlags.TemporalAA = false;
	SceneCaptureComp->ShowFlags.SkyLighting = false;
	SceneCaptureComp->ShowFlags.LightShafts = false;
	SceneCaptureComp->ShowFlags.VolumetricFog = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostProcessAsset(TEXT("Material'/Game/Materials/MaterialAffordance/PP_IconGenMeshPass.PP_IconGenMeshPass'"));
	SceneCaptureComp->PostProcessSettings.AddBlendable(PostProcessAsset.Object, 1.f);
	SceneCaptureComp->PostProcessSettings.AutoExposureMinBrightness = 1.f;
	SceneCaptureComp->PostProcessSettings.AutoExposureMaxBrightness = 1.f;
	SceneCaptureComp->PostProcessSettings.AmbientCubemapIntensity = 3.f;
	static ConstructorHelpers::FObjectFinder<UTextureCube> CubeMapTextureAsset(TEXT("TextureCube'/Game/Textures/HDR/HDRI_Epic_Courtyard_Daylight.HDRI_Epic_Courtyard_Daylight'"));
	SceneCaptureComp->PostProcessSettings.AmbientCubemap = CubeMapTextureAsset.Object;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionIntensity = 1.f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionRadius = 10.f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionStaticFraction = 0.5f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionRadiusInWS = false;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionFadeRadius = 20000.f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionPower = 20.f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionQuality = 100.f;
	SceneCaptureComp->PostProcessSettings.AmbientOcclusionMipScale = 0.5f;


	GroundPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundPlane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GroundPlaneAsset(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundPlaneMatAsset(TEXT("Material'/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial'"));
	GroundPlane->SetMaterial(0, GroundPlaneMatAsset.Object);
	GroundPlane->SetupAttachment(Root);
	GroundPlane->SetStaticMesh(GroundPlaneAsset.Object);
	GroundPlane->SetRelativeLocation(FVector(0.f, 100.f, 0.f));
	GroundPlane->SetRelativeScale3D(FVector(1.f, 2.f, 1.f));
	GroundPlane->Mobility = EComponentMobility::Movable;
	GroundPlane->LightingChannels.bChannel0 = false;
	GroundPlane->LightingChannels.bChannel2 = true;
	GroundPlane->bRenderCustomDepth = true;
	GroundPlane->CustomDepthStencilValue = 2;

	// Cps are reference for walls that need control points
	CpRef1 = CreateDefaultSubobject<USceneComponent>(TEXT("CpRef1"));
	CpRef1->SetupAttachment(Root);
	CpRef1->SetRelativeLocation(FVector(-20.f, 0.f, 0.f));
	CpRef2 = CreateDefaultSubobject<USceneComponent>(TEXT("CpRef2"));
	CpRef2->SetupAttachment(Root);
	CpRef2->SetRelativeLocation(FVector(20.f, 0.f, 0.f));

	// Create lights
	RectLight = CreateDefaultSubobject<URectLightComponent>(TEXT("RectLight"));
	RectLight->SetupAttachment(Root);
	RectLight->SetMobility(EComponentMobility::Movable);
	RectLight->IntensityUnits = ELightUnits::Candelas;
	RectLight->SetIntensity(4.f);
	RectLight->LightingChannels.bChannel0 = false;
	RectLight->LightingChannels.bChannel2 = true;
	RectLight->SetRelativeTransform(
		FTransform(
			//FRotator(0.f, 0.f, -26.59f),
			FRotator(0.f, -26.59f, 0.f),
			FVector(-119.33f, 68.6448f, 149.8512f), //translation
			FVector(34.f, 34.f, 34.f))
	);
	RectLight1 = CreateDefaultSubobject<URectLightComponent>(TEXT("RectLight1"));
	RectLight1->SetupAttachment(Root);
	RectLight->SetMobility(EComponentMobility::Movable);
	RectLight1->IntensityUnits = ELightUnits::Candelas;
	RectLight1->SetIntensity(2.f);
	RectLight1->LightingChannels.bChannel0 = false;
	RectLight1->LightingChannels.bChannel2 = true;
	RectLight1->CastShadows = false;
	RectLight1->SetRelativeTransform(
		FTransform(
			FRotator(-27.03f, -131.37f, -1.32f),
			FVector(44.125f, 128.83f, 233.419f), //translation
			FVector(34.f, 34.f, 34.f))
	);

	// Create Procedural meshes pool. This is limited because scene capture can only capture itself
	for (int32 i = 0; i < 10; ++i)
	{
		FName name = *FString::Printf(TEXT("ProceduralMesh %i"), i);
		UProceduralMeshComponent* curProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(name);
		curProcMesh->LightingChannels.bChannel0 = false;
		curProcMesh->LightingChannels.bChannel2 = true;
		curProcMesh->bUseAsOccluder = false;
		curProcMesh->bRenderCustomDepth = true;
		curProcMesh->CustomDepthStencilValue = 1;
		curProcMesh->BoundsScale = 10000.f;
		Procedurallayers.Add(curProcMesh);
	}
	// Create static meshes pool. Same limit as procedural mesh
	for (int32 i = 0; i < 20; ++i)
	{
		FName name = *FString::Printf(TEXT("StaticMesh %i"), i);
		StaticMeshes.Add(CreateDefaultSubobject<UStaticMeshComponent>(name));
	}

	//SceneCaptureComp->ShowOnlyActorComponents(this);
}

// Called when the game starts or when spawned
void ADynamicIconGenerator::BeginPlay()
{
	Super::BeginPlay();
	for (int32 i = 0; i < 162; ++i)
	{
		RenderTargetPool.Add(UKismetRenderingLibrary::CreateRenderTarget2D(this, 128, 128, RTF_RGBA8));
	}
}

// Called every frame
void ADynamicIconGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ADynamicIconGenerator::GetWallSliceParam(FVector& Location, FVector& Normal, int32 CurrentLayer, int32 NumberOfLayers, FVector Cp1, FVector Cp2, float Height)
{
	FVector start = Cp2;
	FVector end = FVector(Cp1.X, Cp1.Y, Cp1.Z + Height);
	float lerpAlpha = float(CurrentLayer + 2) / float(NumberOfLayers + 1);
	Location = UKismetMathLibrary::VLerp(start, end, lerpAlpha);
	Normal = UKismetMathLibrary::GetDirectionUnitVector(end, start);
}

TArray<FColor> ADynamicIconGenerator::GetFColorFromRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	//UKismetRenderingLibrary::ReadRenderTargetPixel()
	return TArray<FColor>{};
}

TArray<FColor> ADynamicIconGenerator::DownsampleTextureData(TArray<FColor>& Data, int32 Width, int32 Kernel)
{
	int32 Len = Data.Num();
	int32 W = Width;
	TArray<FColor> Downsampled;
	Downsampled.Init(FColor(), Len / (Kernel * Kernel));

	int32 Row = 0;
	for (int32 i = 0; i < Downsampled.Num(); i++)
	{
		TArray<FColor> KernelColors;
		KernelColors.Init(FColor(0, 0, 0, 255), Kernel * Kernel);

		int32 KernelIndex = i * Kernel;
		if (KernelIndex % W == 0 && i != 0)
		{
			Row++;
		}

		KernelIndex += Row * W * (Kernel - 1);

		for (int32 k = 0; k < Kernel; ++k)
		{
			for (int32 kr = 0; kr < Kernel; ++kr)
			{
				int32 c = KernelIndex + k + (W * kr);
				if (c > 0 && c < Len)
				{
					KernelColors[k * Kernel + kr] = Data[c];
				}
			}
		}

		int32 AvR = 0;
		int32 AvG = 0;
		int32 AvB = 0;
		int32 AvA = 0;
		for (int32 j = 0; j < KernelColors.Num(); ++j)
		{
			AvR += KernelColors[j].R;
			AvG += KernelColors[j].G;
			AvB += KernelColors[j].B;
			AvA += KernelColors[j].A;
		}

		FColor KernelAvarage;
		KernelAvarage.R = (uint8)(AvR / KernelColors.Num());
		KernelAvarage.G = (uint8)(AvG / KernelColors.Num());
		KernelAvarage.B = (uint8)(AvB / KernelColors.Num());
		KernelAvarage.A = (uint8)(AvA / KernelColors.Num());
		Downsampled[i] = KernelAvarage;
	}

	return Downsampled;
}

