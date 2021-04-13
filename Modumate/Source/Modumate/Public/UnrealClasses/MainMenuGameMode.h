// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/ModumateGameModeBase.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"

#include "MainMenuGameMode.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API AMainMenuGameMode : public AModumateGameModeBase
{
	GENERATED_BODY()

public:
	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	//~ End AGameModeBase Interface

	void LoadRecentProjectData();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateBrush DefaultProjectThumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLinearColor DefaultThumbnailTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.75f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLinearColor DefaultThumbnailHoverTint = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 NumRecentProjects;

	UFUNCTION(BlueprintPure, Category = "Files")
	bool GetRecentProjectData(int32 index, FString &outProjectPath, FText &outProjectName, FDateTime &outProjectTime, FSlateBrush &outDefaultThumbnail, FSlateBrush &outHoveredThumbnail) const;

	UFUNCTION()
	void OpenEditModelLevel();

	UFUNCTION(BlueprintCallable, Category = "Files")
	bool OpenProject(const FString& ProjectPath);

	UFUNCTION(BlueprintCallable, Category = "Files")
	bool OpenProjectFromPicker();

	UFUNCTION(BlueprintCallable, Category = "Files")
	bool GetLoadFilename(FString &loadFileName);

	UFUNCTION(BlueprintCallable, Category = "Date Time")
	FDateTime GetCurrentDateTime();

protected:
	TArray<FString> RecentProjectPaths;
	TArray<FText> RecentProjectNames;
	TArray<FDateTime> RecentProjectTimes;
	TArray<TSharedPtr<struct FSlateDynamicImageBrush>> ThumbnailBrushes;
};
