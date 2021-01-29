// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateCore/ModumateTypes.h"
#include "UObject/Object.h"

#include "ModumateAnalyticsStatics.generated.h"

class IAnalyticsProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModumateAnalyticsEvent, const FString&, EventCategory, const FString&, EventName);

UCLASS(BlueprintType)
class MODUMATE_API UModumateAnalyticsStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const FString AttrNameCategory;
	static const FString AttrNameCustomValue;
	static const FString AttrNameInTutorial;

	static const FString EventCategoryObjects;
	static const FString EventCategoryTools;
	static const FString EventCategoryHandles;
	static const FString EventCategoryPresets;
	static const FString EventCategoryView;
	static const FString EventCategorySession;

	static FOnModumateAnalyticsEvent OnRecordedAnalyticsEvent;

	static TSharedPtr<IAnalyticsProvider> InitAnalytics();

	static void ShutdownAnalytics(TSharedPtr<IAnalyticsProvider> &AnalyticsInstance);

	static IAnalyticsProvider *GetAnalyticsFromWorld(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static void SetInTutorial(bool bNewIntutorial);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordEventSimple(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordEventCustomFloat(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName, float CustomValue);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordToolUsage(UObject* WorldContextObject, EToolMode ToolMode, float UsedDuration);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordSimpleToolEvent(UObject* WorldContextObject, EToolMode ToolMode, const FString& SubEventName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordObjectCreation(UObject* WorldContextObject, EObjectType ObjectType, int32 Count);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordObjectDeletion(UObject* WorldContextObject, EObjectType ObjectType, int32 Count);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordSessionDuration(UObject* WorldContextObject, const FTimespan& SessionDuration);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordPresetCreation(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordPresetUpdate(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Analytics")
	static bool RecordPresetDeletion(UObject* WorldContextObject);

protected:
	static bool bInTutorial;
};
