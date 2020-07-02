#include "UnrealClasses/CutPlaneCaptureActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Drafting/DraftingManager.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UObject/Object.h"

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

	if (bRenderOnTick)
	{
		bRenderOnTick = false;
	}
	// TODO: SceneCaptureComponent2D::TickComponent calls CaptureSceneDeferred if bCaptureEveryFrame is true.
	// we should be able to call CaptureSceneDeferred during StartRender and call ExportRenderTarget
	// once the scene is captured
	else if (CaptureComponent->bCaptureEveryFrame)
	{
		CaptureComponent->bCaptureEveryFrame = false;

		TPair<int32, int32> SceneCaptureObjID = TPair<int32, int32>(ObjID, ScopeBoxID);
		FString filePath, fileName;
		UDraftingManager::GetImagePath(filePath);
		UDraftingManager::GetImageName(SceneCaptureObjID, fileName);

		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), CurrentTextureTarget, *filePath, *fileName);
		UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), CurrentTextureTarget);

		Parent->PublishPage();
	}

}

ACutPlaneCaptureActor::ACutPlaneCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("test_capture_component"));

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
	CaptureComponent->bCaptureEveryFrame = false;

	RootComponent = CaptureComponent;
}

void ACutPlaneCaptureActor::ResetHiddenActorsToDefault()
{
	CaptureComponent->HiddenActors.Reset();

	AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
	CaptureComponent->HiddenActors.Add(gameMode->Axes);
}
