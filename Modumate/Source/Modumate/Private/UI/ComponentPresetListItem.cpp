// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentPresetListItem.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
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

bool UComponentPresetListItem::CaptureIconFromPresetKey(class AEditModelPlayerController_CPP *Controller, const FBIMKey& AsmKey, EToolMode mode)
{
	if (!(Controller && IconImage))
	{
		return false;
	}
	UMaterialInterface* iconMat = nullptr;
	bool result = Controller->DynamicIconGenerator->SetIconMeshForAssemblyByToolMode(AsmKey, mode, iconMat);
	if (result)
	{
		IconImage->SetBrushFromMaterial(iconMat);
	}
	return result;
}

bool UComponentPresetListItem::CaptureIconForBIMDesignerSwap(class AEditModelPlayerController_CPP *Controller, const FBIMKey& PresetKey, int32 NodeID)
{
	if (!(Controller && IconImage))
	{
		return false;
	}
	bool result = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(true, PresetKey, IconMaterial, IconTexture, NodeID);
	if (result)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	return result;
}
