// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardHeader.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Components/Image.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

UPresetCardHeader::UPresetCardHeader(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardHeader::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardHeader::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardHeader::BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID)
{
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(InGUID);
	if (preset)
	{
		CaptureIcon(InGUID, NodeID, preset->NodeScope == EBIMValueScope::Assembly);
		MainText->ChangeText(preset->DisplayName);
	}
}

bool UPresetCardHeader::CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly)
{
	if (!(EMPlayerController && IconImageSmall))
	{
		return false;
	}

	bool result = false;
	if (bAsAssembly)
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForAssembly(InGUID, IconMaterial);
	}
	else
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForBIMDesigner(true, InGUID, IconMaterial, NodeID);
	}

	if (result)
	{
		IconImageSmall->SetBrushFromMaterial(IconMaterial);
	}
	IconImageSmall->SetVisibility(result ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	return result;
}
