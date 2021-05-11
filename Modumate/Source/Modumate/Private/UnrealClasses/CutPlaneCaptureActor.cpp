// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/CutPlaneCaptureActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"
#include "Drafting/DraftingManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/AxesActor.h"
#include "UnrealClasses/EditModelPlayerController.h"

// Called when the game starts or when spawned
void ACutPlaneCaptureActor::BeginPlay()
{
	Super::BeginPlay();

	ResetHiddenActorsToDefault();
}

void ACutPlaneCaptureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ACutPlaneCaptureActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// TODO: Investigate operation of USceneCaptureComponent2D::CaptureScene() & CaptureSceneDeferred(),
	// see if more reliable way to determine render completion.
	if (bCaptureStarted && --TickCount == 0)
	{
		CaptureComponent->bCaptureEveryFrame = false;
		bCaptureStarted = false;

		TPair<int32, int32> SceneCaptureObjID = TPair<int32, int32>(ObjID, ScopeBoxID);
		FString filePath, fileName;
		UDraftingManager::GetImagePath(filePath);
		UDraftingManager::GetImageName(SceneCaptureObjID, fileName);

		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), CurrentTextureTarget, *filePath, *fileName);
		UKismetRenderingLibrary::ReleaseRenderTarget2D(CurrentTextureTarget);
		CaptureComponent->TextureTarget = nullptr;
		CurrentTextureTarget = nullptr;

		Parent->CaptureComplete();
	}

}

ACutPlaneCaptureActor::ACutPlaneCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("test_capture_component"));

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = false;

	RootComponent = CaptureComponent;
}

void ACutPlaneCaptureActor::ResetHiddenActorsToDefault()
{
	CaptureComponent->HiddenActors.Reset();

	auto* controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->AxesActor)
	{
		CaptureComponent->HiddenActors.Add(controller->AxesActor);
	}
}
