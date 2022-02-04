// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Objects/ModumateObjectEnums.h"

#include "ToolTrayBlockProperties.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockProperties : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockProperties(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* PropertiesListBox;

	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<class UInstPropWidgetBase>> InstanceProperyBlueprints;

	UFUNCTION(BlueprintCallable)
	void ChangeBlockProperties(class UEditModelToolBase* CurrentTool);

	UFUNCTION()
	class UInstPropWidgetBase* RequestPropertyField(UObject* PropertySource, const FString& PropertyName, UClass* FieldClass);

	template<typename TFieldType>
	FORCEINLINE TFieldType* RequestPropertyField(UObject* PropertySource, const FString& PropertyName)
	{
		return Cast<TFieldType>(RequestPropertyField(PropertySource, PropertyName, TFieldType::StaticClass()));
	}

protected:
	UPROPERTY()
	TMap<FString, class UInstPropWidgetBase*> RequestedPropertyFields;

	UPROPERTY()
	TMap<UClass*, UClass*> InstPropClassesFromNativeClasses;
};
