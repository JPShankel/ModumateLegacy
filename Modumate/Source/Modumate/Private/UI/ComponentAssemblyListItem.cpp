// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentAssemblyListItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/Custom/ModumateButton.h"

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

	if (!ModumateButtonMain)
	{
		return false;
	}

	ModumateButtonMain->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnModumateButtonMainReleased);

	return true;
}

void UComponentAssemblyListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

bool UComponentAssemblyListItem::BuildFromAssembly(const FModumateObjectAssembly *Asm)
{
	MainText->ChangeText(FText::FromString(Asm->AsShoppingItem().DisplayName));
	AsmKey = Asm->DatabaseKey;
	return true;
}

void UComponentAssemblyListItem::OnModumateButtonMainReleased()
{
	AEditModelPlayerController_CPP *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	if (controller && controller->EMPlayerState)
	{
		const FModumateObjectAssembly *assembly = doc->PresetManager.GetAssemblyByKey(controller->GetToolMode(), AsmKey);
		if (assembly)
		{
			controller->EMPlayerState->SetAssemblyForToolMode(controller->GetToolMode(), assembly->AsShoppingItem());
		}
	}
}
