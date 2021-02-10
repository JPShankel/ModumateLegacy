// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Http.h"

#include "TutorialManager.generated.h"

enum class EEditViewModes : uint8;
enum class EInputCommand : uint8;
enum class EModumateAnalyticsCategory : uint8;
enum class EMOIDeltaType : uint8;
enum class EObjectType : uint8;
enum class EToolMode : uint8;

UENUM()
enum class EModumateWalkthroughCategories : uint8
{
	None = 0,
	Beginner,
	Intermediate
};

UENUM()
enum class EModumateWalkthroughCustomActions : uint8
{
	None = 0,
	SurfaceGraphBreak1,
	SurfaceGraphBreak2,
};

USTRUCT()
struct MODUMATE_API FModumateWalkthroughStepReqs
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<EToolMode> ToolModesToActivate;

	UPROPERTY()
	TArray<EObjectType> ObjectTypesToCreate;

	UPROPERTY()
	TArray<FName> InputBindingsToPerform;

	UPROPERTY()
	TArray<EInputCommand> InputCommandsToPerform;

	UPROPERTY()
	TArray<FName> AnalyticEventsToRecord;

	UPROPERTY()
	TArray<EModumateWalkthroughCustomActions> CustomActionsToPerform;

	bool IsEmpty() const;
};

USTRUCT()
struct MODUMATE_API FModumateWalkthroughStepData
{
	GENERATED_BODY()

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	FString VideoURL;

	UPROPERTY()
	float AutoProceedCountdown = 0.0f;

	UPROPERTY()
	FModumateWalkthroughStepReqs Requirements;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModumateWalkthroughStepCompleted, EModumateWalkthroughCategories, WalkthroughCategory, int32, StepIdx);
DECLARE_DELEGATE_TwoParams(FInputActionDelegate, FName, EInputEvent);
DECLARE_DELEGATE_TwoParams(FInputAxisDelegate, FName, float);

UCLASS(Blueprintable)
class MODUMATE_API UModumateTutorialManager : public UObject
{
	GENERATED_BODY()

public:
	void Init();
	void Shutdown();

	UFUNCTION()
	bool LoadWalkthroughData(const FString& WalkthroughDataJSON);

	UFUNCTION()
	bool BeginWalkthrough(EModumateWalkthroughCategories WalkthroughCategory);

	UFUNCTION()
	bool AdvanceWalkthrough(bool bSkipped);

	UFUNCTION()
	bool RewindWalkthrough();

	UFUNCTION()
	bool EndWalkthrough(bool bSkipped);

	UFUNCTION()
	bool OpenVideoTutorial(const FString& ProjectFilePath, const FString& VideoURL);

	UFUNCTION()
	bool RecordWalkthroughCustomAction(EModumateWalkthroughCustomActions CustomAction);

	const TArray<FModumateWalkthroughStepData>& GetCurWalkthroughSteps() const;
	const FModumateWalkthroughStepData& GetCurWalkthroughStepData() const;

	void OpenWalkthroughProject(EModumateWalkthroughCategories WalkthroughCategory);
	bool GetTutorialFilePath(const FString& TutorialFileName, FString& OutFullTutorialFilePath);
	bool CheckAbsoluteBeginner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DynamicTutorialDataURL;

	UPROPERTY()
	FModumateWalkthroughStepCompleted OnWalkthroughStepCompleted;

	UPROPERTY()
	bool bCompletedFirstTime = false;

	EModumateWalkthroughCategories FromMainMenuWalkthroughCategory = EModumateWalkthroughCategories::None;

protected:
	UPROPERTY()
	class AEditModelPlayerController* Controller = nullptr;

	UPROPERTY()
	class UTutorialWalkthroughMenu* WalkthroughMenu = nullptr;

	void OnLoadDataReply(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void SetWalkthroughStepIndex(int32 NewStepIndex, bool bSkipped);
	bool CacheObjects();
	void CheckCurrentStepRequirements();
	void ResetWalkthroughState();

	UFUNCTION()
	void OnToolModeChanged();

	UFUNCTION()
	void OnAppliedMOIDeltas(EObjectType ObjectType, int32 Count, EMOIDeltaType DeltaType);

	UFUNCTION()
	void OnPlayerInputAction(FName ActionName, EInputEvent InputEvent);

	UFUNCTION()
	void OnPlayerInputAxis(FName AxisName, float Value);

	UFUNCTION()
	void OnExecutedInputCommand(EInputCommand InputCommand);

	UFUNCTION()
	void OnRecordedAnalyticsEvent(EModumateAnalyticsCategory EventCategory, const FString& EventName);

	UFUNCTION()
	void OnControllerDestroyed(AActor* PlayerController);

	bool bDataLoaded = false;
	EModumateWalkthroughCategories CurWalkthroughCategory = EModumateWalkthroughCategories::None;
	int32 CurWalkthroughStepIdx = INDEX_NONE;
	FModumateWalkthroughStepReqs CurWalkthroughStepReqsRemaining;
	FDateTime CurWalkthroughStartTime = FDateTime::MinValue();
	FDateTime CurWalkthroughStepStartTime = FDateTime::MinValue();

	TMap<EModumateWalkthroughCategories, TArray<FModumateWalkthroughStepData>> WalkthroughStepsByCategory;
};
