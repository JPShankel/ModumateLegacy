// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "Interfaces/IHttpRequest.h"

#include "HelpMenu.generated.h"

/**
 *
 */

UENUM()
enum class ETutorialTaxonomyColumn : uint8
{
	None = 0,
	GUID,
	Include,
	CategoryPath,
	Title,
	Body,
	VideoURL
};

USTRUCT()
struct FTutorialTaxonomyNode
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	bool bInclude = false;

	UPROPERTY()
	FBIMTagPath TagPath;

	UPROPERTY()
	FString Title;

	UPROPERTY()
	FString Body;

	UPROPERTY()
	FString VideoURL;
};


UCLASS()
class MODUMATE_API UHelpMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpMenu(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonClose;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHelpBlockTutorialArticle* HelpBlockTutorialArticleBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHelpBlockTutorialSearch* HelpBlockTutorialsSearchBP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TutorialLibraryDataLink;

	UPROPERTY()
	TMap<FGuid, FTutorialTaxonomyNode> AllTutorialNodesByGUID;

	UPROPERTY()
	TSet<FBIMTagPath> AllTutorialTagPaths;

	void BuildTutorialLibraryMenu();
	void ReadTutorialCSV(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	EBIMResult GetTutorialNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const;
	EBIMResult GetTutorialsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutGUIDs, bool bExactMatch = false) const;

	void ResetMenu();
	void ToArticleMenu(const FGuid& InGUID);

	UFUNCTION()
	void OnButtonCloseRelease();
};
