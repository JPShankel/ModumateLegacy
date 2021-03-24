// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentPresetListItem.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Components/Image.h"


UComponentPresetListItem::UComponentPresetListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UComponentPresetListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UComponentPresetListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

bool UComponentPresetListItem::CaptureIconFromPresetKey(class AEditModelPlayerController* Controller, const FGuid& InGUID)
{
	if (!(Controller && IconImage))
	{
		return false;
	}
	UMaterialInterface* iconMat = nullptr;

	bool result = Controller->DynamicIconGenerator->SetIconMeshForAssembly(FBIMPresetCollectionProxy(Controller->GetDocument()->GetPresetCollection()),InGUID, iconMat);
	if (result)
	{
		IconImage->SetBrushFromMaterial(iconMat);
	}
	return result;
}

bool UComponentPresetListItem::CaptureIconForBIMDesignerSwap(class AEditModelPlayerController* Controller, const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID)
{
	if (!(Controller && IconImage))
	{
		return false;
	}

	bool result = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(FBIMPresetCollectionProxy(Controller->GetDocument()->GetPresetCollection()), true, InGUID, IconMaterial, NodeID);
	if (result)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(result ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	return result;
}
