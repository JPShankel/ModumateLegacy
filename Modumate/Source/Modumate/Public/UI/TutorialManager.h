// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

#include "TutorialManager.generated.h"

enum class EObjectType : uint8;
enum class EToolMode : uint8;
enum class EEditViewModes : uint8;
enum class EInputCommand : uint8;

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
	ToggleFullscreen,
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

UCLASS()
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
	bool AdvanceWalkthrough();

	UFUNCTION()
	bool RewindWalkthrough();

	UFUNCTION()
	bool EndWalkthrough();

	UFUNCTION()
	bool RecordWalkthroughCustomAction(EModumateWalkthroughCustomActions CustomAction);

	const TArray<FModumateWalkthroughStepData>& GetCurWalkthroughSteps() const;
	const FModumateWalkthroughStepData& GetCurWalkthroughStepData() const;

	UPROPERTY()
	FModumateWalkthroughStepCompleted OnWalkthroughStepCompleted;

protected:
	UPROPERTY()
	class AEditModelPlayerController* Controller = nullptr;

	UPROPERTY()
	class UTutorialWalkthroughMenu* WalkthroughMenu = nullptr;

	void SetWalkthroughStepIndex(int32 NewStepIndex);
	bool CacheObjects();
	void CheckCurrentStepRequirements();

	UFUNCTION()
	void OnToolModeChanged();

	UFUNCTION()
	void OnExecutedInputCommand(EInputCommand InputCommand);

	UFUNCTION()
	void OnPlayerInputAction(FName ActionName, EInputEvent InputEvent);

	UFUNCTION()
	void OnPlayerInputAxis(FName AxisName, float Value);

	UFUNCTION()
	void OnToggleFullscreen(bool bIsFullscreen);

	EModumateWalkthroughCategories CurWalkthroughCategory = EModumateWalkthroughCategories::None;
	int32 CurWalkthroughStepIdx = INDEX_NONE;
	FModumateWalkthroughStepReqs CurWalkthroughStepReqsRemaining;

	TMap<EModumateWalkthroughCategories, TArray<FModumateWalkthroughStepData>> WalkthroughStepsByCategory;
};
