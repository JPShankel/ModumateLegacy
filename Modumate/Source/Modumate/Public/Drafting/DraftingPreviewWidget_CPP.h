// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Drafting/ModumateDraftingView.h"
#include "ModumateCore/ModumateDecisionTree.h"
#include "Database/ModumateObjectEnums.h"
#include "Database/ModumateCrafting.h"
#include "Runtime/UMG/Public/Components/Border.h"
#include "DraftingPreviewWidget_CPP.generated.h"

/**
 *
 */

namespace Modumate
{
	class FModumateDocument;

	struct MODUMATE_API FDraftingDrawingScaleOption : public FCraftingOptionBase
	{
		float Scale;
	};
}

USTRUCT(BlueprintType)

struct MODUMATE_API FDraftingItem
{
	GENERATED_BODY();

	UPROPERTY(BlueprintReadWrite)
	EDecisionType Type = EDecisionType::None;

	UPROPERTY(BlueprintReadWrite)
	FText Label;

	UPROPERTY(BlueprintReadWrite)
	FString Value;

	UPROPERTY(BlueprintReadWrite)
	FName GUID;

	UPROPERTY(BlueprintReadWrite)
	FName ParentGUID;

	UPROPERTY(BlueprintReadWrite)
	EDecisionSelectStyle SelectStyle = EDecisionSelectStyle::Icons;

	bool HasPreset = false;

	FName SelectedSubnodeGUID;
	FString Key;

	TMap<FString, FString> FormValues;

	Modumate::FDraftingDrawingScaleOption ScaleOption;

	void AssimilateSubfieldValues(const FDraftingItem &sf)
	{
		ScaleOption = sf.ScaleOption;
	};
	void UpdateMetaValue()
	{
		FormValues.Add(Label.ToString(), Value);
	}

};

typedef Modumate::TDecisionTree<FDraftingItem> FDraftingDecisionTree;
typedef Modumate::TDecisionTreeNode<FDraftingItem> FDraftingDecisionTreeNode;


USTRUCT(BlueprintType)
struct FDraftingPreviewLine
{
	GENERATED_BODY();

	FDraftingPreviewLine() {}
	FDraftingPreviewLine(const FVector2D &p1, const FVector2D &p2) : Point1(p1), Point2(p2) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	FVector2D Point1 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	FVector2D Point2 = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct FDraftingPreviewString
{
	GENERATED_BODY();
	FDraftingPreviewString() {};
	FDraftingPreviewString(const FVector2D &p, const TCHAR *text, float fontSize) :
		Position(p), Text(text), FontSize(fontSize) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	FString Text = FString();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	float FontSize = 0.0f;
};

class AEditModelPlayerController_CPP;

UCLASS()
class MODUMATE_API UDraftingPreviewWidget_CPP : public UUserWidget
{
private:
	Modumate::FModumateDraftingView* View = nullptr;
	float RunTime;
public:
	GENERATED_BODY()

	AEditModelPlayerController_CPP *Controller;
	FDraftingDecisionTree DecisionTree;

	void InitDecisionTree();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Drafting)
	UDataTable* DraftingDecisionTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Drafting)
	UDataTable* DraftingDecisionDrawingScaleOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	TArray<FDraftingPreviewLine> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	TArray<FDraftingPreviewString> Strings;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	FVector2D DrawOrigin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	FVector2D DrawSize;

	Modumate::FModumateDocument *Document;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	UBorder *DrawableArea_CPP;

	UFUNCTION(BlueprintCallable, Category = "Export")
	void OnSave();

	UFUNCTION(BlueprintCallable, Category = "Export")
	void OnCancel();

	UFUNCTION(BlueprintCallable, Category = "Initialization")
	void SetupDocument();

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void NextPage();

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void PreviousPage();

	void ShowPage(int32 pageNum);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	int32 CurrentPage;

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	TArray<FDraftingItem> GetCurrentDraftingDecisions();

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	TArray<FDraftingItem>  SetDraftingDecision(const FName &nodeGUID,const FString &val);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void ResetDraftingDecisions();

	virtual void BeginDestroy() override;

	virtual void NativeTick(const FGeometry & MyGeometry, float InDeltaTime) override;

	bool Ready;

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void RefreshUI();
};
