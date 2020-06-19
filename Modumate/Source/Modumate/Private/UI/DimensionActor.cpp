#include "UI/DimensionActor.h"

#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UI/WidgetClassAssetData.h"

ADimensionActor::ADimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADimensionActor::CreateWidget()
{
	TWeakObjectPtr<AEditModelPlayerController_CPP> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	AEditModelPlayerHUD *playerHUD = playerController.IsValid() ? Cast<AEditModelPlayerHUD>(playerController->GetHUD()) : nullptr;
	DimensionText = playerController->HUDDrawWidget->UserWidgetPool.GetOrCreateInstance<UDimensionWidget>(playerHUD->WidgetClasses->DimensionClass);

	DimensionText->AddToViewport();
	DimensionText->SetPositionInViewport(FVector2D(0.0f, 0.0f));
}

void ADimensionActor::ReleaseWidget()
{
	DimensionText->RemoveFromViewport();

	auto playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (playerController != nullptr)
	{
		playerController->HUDDrawWidget->UserWidgetPool.Release(DimensionText);
	}
}

ALineActor* ADimensionActor::GetLineActor()
{
	return nullptr;
}
