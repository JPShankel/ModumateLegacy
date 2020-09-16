// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentPresetListItem.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Kismet/KismetRenderingLibrary.h"
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

void UComponentPresetListItem::NativeDestruct()
{
	Super::NativeDestruct();
	if (IconRenderTarget)
	{
		IconRenderTarget->ReleaseResource();
	}
}

bool UComponentPresetListItem::CaptureIconFromPresetKey(class AEditModelPlayerController_CPP *Controller, const FBIMKey& AsmKey, EToolMode mode)
{
	if (!(Controller && IconImage))
	{
		return false;
	}
	// TODO: Use icon caching instead of creating new render target each time
	if (!IconRenderTarget)
	{
		IconRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), 256, 256, ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor::Black, true);
	}
	bool bCaptureSucess = Controller->DynamicIconGenerator->SetIconMeshForAssemblyByToolMode(false, AsmKey, mode, IconRenderTarget);
	if (bCaptureSucess)
	{
		static const FName textureParamName(TEXT("Texture"));
		IconImage->GetDynamicMaterial()->SetTextureParameterValue(textureParamName, IconRenderTarget);
	}
	return bCaptureSucess;
}
