// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/NCPButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "BIMKernel/Core/BIMTagPath.h"


UNCPNavigator::UNCPNavigator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNCPNavigator::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UNCPNavigator::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UNCPNavigator::BuildNCPNavigator()
{
	for (auto& curWidget : DynamicMainList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicMainList->ClearChildren();

	// TODO: Blueprintable string array for root NCP?

	// Build first "Assembly" button in NCP
	FBIMTagPath rootPath;
	rootPath.FromString("Assembly");
	UNCPButton* newButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
	newButton->BuildButton(rootPath, 0, true);
	DynamicMainList->AddChildToVerticalBox(newButton);


	// Build first "Part" button in NCP
	FBIMTagPath partRootPath;
	partRootPath.FromString("Part");
	UNCPButton* newPartButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
	newPartButton->BuildButton(partRootPath, 0, true);
	DynamicMainList->AddChildToVerticalBox(newPartButton);
}
