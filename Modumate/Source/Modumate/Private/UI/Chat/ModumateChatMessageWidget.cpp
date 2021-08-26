// Copyright © 2021 Modumate, Inc. All Rights Reserved.


#include "UI/Chat/ModumateChatMessageWidget.h"

UModumateChatMessageWidget::UModumateChatMessageWidget(const FObjectInitializer& ObjectInitializer) :
	UUserWidget(ObjectInitializer)
{

}
bool UModumateChatMessageWidget::Initialize()
{
	return Super::Initialize();
}

void UModumateChatMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
