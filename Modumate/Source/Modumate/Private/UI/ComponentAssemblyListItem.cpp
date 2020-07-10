// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentAssemblyListItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/Custom/ModumateButton.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"

using namespace Modumate;

UComponentAssemblyListItem::UComponentAssemblyListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UComponentAssemblyListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ModumateButtonMain && ButtonEdit))
	{
		return false;
	}

	ModumateButtonMain->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnModumateButtonMainReleased);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonEditReleased);

	return true;
}

void UComponentAssemblyListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UComponentAssemblyListItem::NativeDestruct()
{
	Super::NativeDestruct();
	if (IconRenderTarget)
	{
		IconRenderTarget->ReleaseResource();
	}
}

bool UComponentAssemblyListItem::BuildFromAssembly(AEditModelPlayerController_CPP *Controller, EToolMode mode, const FModumateObjectAssembly *Asm)
{
	MainText->ChangeText(FText::FromString(Asm->GetProperty(BIM::Parameters::Name)));
	AsmKey = Asm->DatabaseKey;
	EMPlayerController = Controller;
	ToolMode = mode;
	CaptureIconRenderTarget();

	TArray<FString> propertyTips;
	GetItemTips(propertyTips);
	VerticalBoxProperties->ClearChildren();
	for (auto &curTip : propertyTips)
	{
		UModumateTextBlockUserWidget *newModumateTextBlockUserWidget = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UModumateTextBlockUserWidget>(ModumateTextBlockUserWidgetClass);
		if (newModumateTextBlockUserWidget)
		{
			newModumateTextBlockUserWidget->ChangeText(FText::FromString(curTip), true);
			VerticalBoxProperties->AddChildToVerticalBox(newModumateTextBlockUserWidget);
		}
	}
	return true;
}

void UComponentAssemblyListItem::OnModumateButtonMainReleased()
{
	AEditModelPlayerController_CPP *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	if (controller && controller->EMPlayerState)
	{
		controller->EMPlayerState->SetAssemblyForToolMode(ToolMode, AsmKey);
	}
}

void UComponentAssemblyListItem::OnButtonEditReleased()
{
	if (ToolTrayBlockAssembliesList && 
		ToolTrayBlockAssembliesList->ToolTray && 
		ToolTrayBlockAssembliesList->ToolTray->EditModelUserWidget)
	{
		ToolTrayBlockAssembliesList->ToolTray->EditModelUserWidget->EventEditExistingAssembly(ToolMode, AsmKey);
	}
}

bool UComponentAssemblyListItem::CaptureIconRenderTarget()
{
	if (!IconImage)
	{
		return false;
	}
	if (!IconRenderTarget)
	{
		IconRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), 256, 256, ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor::Black, true);
	}
	bool bCaptureSucess = EMPlayerController->DynamicIconGenerator->SetIconMeshForAssemblyByToolMode(AsmKey, ToolMode, IconRenderTarget);
	if (bCaptureSucess)
	{
		static const FName textureParamName(TEXT("Texture"));
		IconImage->GetDynamicMaterial()->SetTextureParameterValue(textureParamName, IconRenderTarget);
	}
	return bCaptureSucess;
}

bool UComponentAssemblyListItem::GetItemTips(TArray<FString> &OutTips)
{
	if (!EMPlayerController)
	{
		return false;
	}
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	const FModumateObjectAssembly *assembly = doc->PresetManager.GetAssemblyByKey(EMPlayerController->GetToolMode(), AsmKey);
	if (!assembly)
	{
		return false;
	}

	// Get properties for tips
	BIM::FModumateAssemblyPropertySpec presetSpec;
	doc->PresetManager.PresetToSpec(assembly->RootPreset, presetSpec);

	FModumateFunctionParameterSet params;
	for (auto &curLayerProperties : presetSpec.LayerProperties)
	{
		// TODO: Only tips for wall tools for now. Will expand to other tool modes
		if (ToolMode == EToolMode::VE_WALL)
		{
			FString layerFunction, layerThickness;
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Function, layerFunction);
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Thickness, layerThickness);
			OutTips.Add(layerThickness + FString(TEXT(", ")) + layerFunction);
		}

	}
	return true;
}
