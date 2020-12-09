// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Debugger/BIMDebugTestItem.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Debugger/BIMDebugger.h"


UBIMDebugTestItem::UBIMDebugTestItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMDebugTestItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (ModumateButtonMain)
	{
		ModumateButtonMain->OnReleased.AddDynamic(this, &UBIMDebugTestItem::OnButtonMainReleased);
	}

	return true;
}

void UBIMDebugTestItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMDebugTestItem::OnButtonMainReleased()
{
	bool result = ParentDebugger->PerformBIMDebugTest(BIMDebugTestType);
	if (result)
	{
		MainText->SetText(FText::FromString(FString::Printf(TEXT("Passed - ")) + TestItemName));
		ModumateButtonMain->SetBackgroundColor(FLinearColor::Green);
	}
	else
	{
		MainText->SetText(FText::FromString(FString::Printf(TEXT("Failed - ")) + TestItemName));
		ModumateButtonMain->SetBackgroundColor(FLinearColor::Red);
	}
}

void UBIMDebugTestItem::BuildDebugTestItem(class UBIMDebugger* OuterBIMDebugger, EBIMDebugTestType InBIMDebugTestType)
{
	ParentDebugger = OuterBIMDebugger;
	BIMDebugTestType = InBIMDebugTestType;
	TestItemName = *EnumValueString(EBIMDebugTestType, InBIMDebugTestType);
	MainText->SetText(FText::FromString(FString::Printf(TEXT("Run - ")) + TestItemName));
	ModumateButtonMain->SetBackgroundColor(FLinearColor::White);
}

