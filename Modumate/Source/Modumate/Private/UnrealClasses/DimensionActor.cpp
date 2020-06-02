#include "UnrealClasses/DimensionActor.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"

void ADimensionActor::CreateWidget()
{
	TWeakObjectPtr<AEditModelPlayerController_CPP> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	AEditModelPlayerHUD *playerHUD = playerController.IsValid() ? Cast<AEditModelPlayerHUD>(playerController->GetHUD()) : nullptr;
	DimensionText = playerController->HUDDrawWidget->UserWidgetPool.GetOrCreateInstance<UDimensionWidget>(playerHUD->DimensionClass);

	DimensionText->AddToViewport();
	DimensionText->SetPositionInViewport(FVector2D(0.0f, 0.0f));
}

void ADimensionActor::ReleaseWidget()
{
	DimensionText->RemoveFromViewport();

	TWeakObjectPtr<AEditModelPlayerController_CPP> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (playerController != nullptr)
	{
		playerController->HUDDrawWidget->UserWidgetPool.Release(DimensionText);
	}
}

void ADimensionActor::BeginPlay()
{
	Super::BeginPlay();

	CreateWidget();
}

void ADimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWidget();

	Super::EndPlay(EndPlayReason);
}

void ADimensionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
