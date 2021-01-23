// Copyright 2020 Modumate, Inc. All Rights Reserved.
#include "UI/DimensionActor.h"

#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UI/WidgetClassAssetData.h"

ADimensionActor::ADimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DimensionText(nullptr)
	, Document(nullptr)
	, ID(MOD_ID_NONE)
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADimensionActor::BeginPlay()
{
	Super::BeginPlay();
	CreateWidget();

	UWorld* world = GetWorld();
	auto gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	Document = gameState ? gameState->Document : nullptr;
}

void ADimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWidget();
	Super::EndPlay(EndPlayReason);
}

void ADimensionActor::CreateWidget()
{
	TWeakObjectPtr<AEditModelPlayerController> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	AEditModelPlayerHUD *playerHUD = playerController.IsValid() ? Cast<AEditModelPlayerHUD>(playerController->GetHUD()) : nullptr;
	DimensionText = playerController->HUDDrawWidget->UserWidgetPool.GetOrCreateInstance<UDimensionWidget>(playerHUD->WidgetClasses->DimensionClass);

	DimensionText->SetVisibility(ESlateVisibility::Visible);
	DimensionText->AddToViewport();
	DimensionText->SetPositionInViewport(FVector2D(0.0f, 0.0f));
}

void ADimensionActor::ReleaseWidget()
{
	DimensionText->RemoveFromViewport();

	auto playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (playerController != nullptr)
	{
		playerController->HUDDrawWidget->UserWidgetPool.Release(DimensionText);
	}
}

ALineActor* ADimensionActor::GetLineActor()
{
	return nullptr;
}
