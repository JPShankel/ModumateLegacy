// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealClasses/EditModelInputHandler.h"

#include "TooltipManager.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FTooltipData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TooltipTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TooltipText;

	TArray<TArray<FInputChord>> AllBindings;
};

UCLASS(Blueprintable)
class MODUMATE_API UTooltipManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init();
	void Shutdown();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tooltips)
	UDataTable* TooltipNonInputDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tooltips)
	TSubclassOf<class UTooltipWidget> PrimaryTooltipWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tooltips)
	TSubclassOf<class UTooltipWidget> SecondaryTooltipWidgetClass;

	class UTooltipWidget* GetOrCreateTooltipWidget(TSubclassOf<class UTooltipWidget> TooltipClass);

	static class UWidget* GenerateTooltipNonInputWidget(const FName& TooltipID, const class UWidget* FromWidget);
	static class UWidget* GenerateTooltipWithInputWidget(EInputCommand InputCommand, const class UWidget* FromWidget);

protected:
	UPROPERTY()
	TMap<FName, FTooltipData> TooltipsMap;

	UPROPERTY()
	class UTooltipWidget* PrimaryTooltipWidget;

	UPROPERTY()
	class UTooltipWidget* SecondaryTooltipWidget;
};
