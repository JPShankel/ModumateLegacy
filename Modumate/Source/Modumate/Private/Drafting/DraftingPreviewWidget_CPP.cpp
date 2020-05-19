// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Drafting/DraftingPreviewWidget_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "ModumateCore/ModumateDecisionTreeImpl.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"


namespace Modumate
{
	template class TDecisionTree<FDraftingItem>;

	class MODUMATE_API FModumatePreviewDraw : public IModumateDraftingDraw
	{
	private:
		FVector2D TranslatePoint(const FVector2D &p)
		{
			FVector2D origin = Widget->DrawOrigin;
			origin.Y += Widget->DrawSize.Y;
			FVector2D scale = Scale;
			scale.Y = -scale.Y;

			return origin + scale * p;
		}

	public:

		UDraftingPreviewWidget_CPP *Widget;
		FVector2D Scale;

		virtual EDrawError DrawLine(
			const Units::FXCoord &x1,
			const Units::FYCoord &y1,
			const Units::FXCoord &x2,
			const Units::FYCoord &y2,
			const Units::FThickness &thickness,
			const FMColor &lineColor,
			const LinePattern &linePattern,
			const Units::FPhase &patternPhase) override
		{
			FDraftingPreviewLine line;

			line.Point1 = TranslatePoint(FVector2D(x1.AsFloorplanInches(), y1.AsFloorplanInches()));
			line.Point2 = TranslatePoint(FVector2D(x2.AsFloorplanInches(), y2.AsFloorplanInches()));
			Widget->Lines.Add(line);
			return EDrawError::ErrorNone;
		}

		virtual EDrawError AddText(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			const Units::FXCoord &xpos,
			const Units::FYCoord &ypos,
			const Units::FAngle &rotateByRadians,
			const FMColor &color,
			DraftingAlignment textJustify,
			const Units::FWidth &containingRectWidth,
			FontType type
		) override
		{
			FDraftingPreviewString newStr;
			newStr.FontSize = fontSize.AsFontHeight() * Scale.Y / 100;
			newStr.Position = TranslatePoint(FVector2D(xpos.AsFloorplanInches(), ypos.AsFloorplanInches()));
			newStr.Position.Y -= newStr.FontSize;
			newStr.Text = text;
			Widget->Strings.Add(newStr);
			return EDrawError::ErrorNone;

		};

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			Units::FUnitValue &textLength,
			FontType type) override
		{
			textLength = Units::FUnitValue::FloorplanInches(FString(text).Len() * fontSize.AsFontHeight() / 100);
			return EDrawError::ErrorNone;
		}

		virtual EDrawError DrawArc(
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FAngle &a1,
			const Units::FAngle &a2,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices) override
		{
			return EDrawError::ErrorNone;
		}

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &w,
			const Units::FHeight &h) override
		{
			return EDrawError::ErrorNone;
		}

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color)
		{
			return EDrawError::ErrorNone;
		}

		virtual EDrawError DrawCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color)
		{
			return EDrawError::ErrorNone;
		}

		virtual EDrawError FillCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const FMColor &color)
		{
			return EDrawError::ErrorNone;
		}

	};
}

void UDraftingPreviewWidget_CPP::BeginDestroy()
{
	Super::BeginDestroy();

	if (View != nullptr)
	{
		delete View;
		View = nullptr;
	}
}

void UDraftingPreviewWidget_CPP::NativeTick(const FGeometry & MyGeometry, float InDeltaTime)
{
	UUserWidget::NativeTick(MyGeometry, InDeltaTime);
	if (!Ready)
	{
		if (DrawSize.X != 0.0f)
		{
			Ready = true;
			RunTime = 0.0f;
			SetupDocument();
		}
	}

	RunTime += InDeltaTime;
	const float looptime = 30.0f;
	while (RunTime > looptime)
	{
		RunTime -= looptime;
	}

	//RunTime = 20.9412193f;

	float angle = 2*PI*RunTime / looptime;

	return;

	/*
	if (View->DraftingPages.Num() > 0)
	{
		View->UpdateElevationView(*View->DraftingPages[0], FVector(-cosf(angle), sinf(angle), -0.3f));
		ShowPage(0);
	}
	//*/
}

void UDraftingPreviewWidget_CPP::OnSave()
{
	Controller->OnSavePDF();
}

void UDraftingPreviewWidget_CPP::OnCancel()
{
	Controller->OnCancelPDF();
}


void UDraftingPreviewWidget_CPP::ShowPage(int32 pageNum)
{
	if (pageNum < 0 || pageNum >= View->DraftingPages.Num())
	{
		return;
	}
	CurrentPage = pageNum;

	FVector2D windowPos(DrawOrigin);
	FVector2D windowSize(DrawSize);

	TSharedPtr<Modumate::FDraftingPage> page = View->DraftingPages[CurrentPage];

	Modumate::FModumatePreviewDraw drawInterface;
	drawInterface.Widget = this;

	drawInterface.Scale.X = DrawSize.X / page->Dimensions.X.AsFloorplanInches();
	drawInterface.Scale.Y = DrawSize.Y / page->Dimensions.Y.AsFloorplanInches();

	Lines.Empty();
	Strings.Empty();

	page->Draw(&drawInterface);
}

void UDraftingPreviewWidget_CPP::PreviousPage()
{
	ShowPage(CurrentPage - 1);
}

void UDraftingPreviewWidget_CPP::NextPage()
{
	ShowPage(CurrentPage + 1);
}

void UDraftingPreviewWidget_CPP::SetupDocument()
{
	delete View;

	View = new Modumate::FModumateDraftingView(GetWorld(), Document);

	CurrentPage = 0;

	if (View->DraftingPages.Num() == 0)
	{
		return;
	}

	ShowPage(0);

}

TArray<FDraftingItem> UDraftingPreviewWidget_CPP::GetCurrentDraftingDecisions()
{
	return DecisionTree.RootNode.GetOpenNodes();
}

TArray<FDraftingItem>  UDraftingPreviewWidget_CPP::SetDraftingDecision(const FName &nodeGUID, const FString &val)
{
	DecisionTree.RootNode.SetStringValueByGUID(nodeGUID, val);
	return GetCurrentDraftingDecisions();
}

void UDraftingPreviewWidget_CPP::ResetDraftingDecisions()
{
	DecisionTree.Reset();

	DecisionTree.RootNode.WalkDepthFirst([this](FDraftingDecisionTreeNode &dt)
	{
		if (dt.Data.Type == EDecisionType::Select)
		{
			if (ensureAlways(dt.Subfields.Num() > 0))
			{
				dt.SetSelectedSubfield(0);
			}
		}
		return true;
	});

}


void UDraftingPreviewWidget_CPP::InitDecisionTree()
{
	DecisionTree = FDraftingDecisionTree::FromDataTable(DraftingDecisionTree);

	Modumate::DataCollection<Modumate::TCraftingOptionSet<Modumate::FDraftingDrawingScaleOption>> scaleOptions;

	Modumate::ReadOptionSet<
		FDrawingScalesTableRow,
		Modumate::FDraftingDrawingScaleOption,
		Modumate::TCraftingOptionSet<Modumate::FDraftingDrawingScaleOption>>(
			DraftingDecisionDrawingScaleOptions,
			[](const FDrawingScalesTableRow &row, Modumate::FDraftingDrawingScaleOption &option)
			{
				option.DisplayName = FText::FromString(row.DisplayName);
				option.Scale = row.CorrespondingProjectScale;
				return true;
			},

			[&scaleOptions](const Modumate::TCraftingOptionSet<Modumate::FDraftingDrawingScaleOption> &optionSet)
			{
				scaleOptions.AddData(optionSet);
			}
	);

	TArray<FDraftingDecisionTreeNode*> tableSelects;

	DecisionTree.RootNode.WalkDepthFirst([&tableSelects](FDraftingDecisionTreeNode &dt)
	{
		if (dt.Data.Type == EDecisionType::TableSelect)
		{
			tableSelects.Add(&dt);
		}
		return true;
	});

	for (auto &ts : tableSelects)
	{
		const FDraftingDecisionTreeNode *parent = DecisionTree.RootNode.FindNodeByGUID(ts->Data.ParentGUID);
		if (ensureAlways(parent != nullptr))
		{
			parent = DecisionTree.RootNode.FindNodeByGUID(parent->Data.ParentGUID);
			const Modumate::TCraftingOptionSet<Modumate::FDraftingDrawingScaleOption> *optionSet = scaleOptions.GetData(*parent->Data.Key);
			if (ensureAlways(optionSet != nullptr))
			{
				TArray<FDraftingDecisionTreeNode> subfields;
				for (auto &op : optionSet->Options)
				{
					FDraftingDecisionTreeNode dt;
					dt.Data.Key = op.Key.ToString();
					dt.Data.Label = op.DisplayName;
					dt.Data.GUID = op.Key;
					dt.Data.ParentGUID = ts->Data.GUID;
					dt.Data.ScaleOption.Scale = op.Scale;
					dt.Data.Type = EDecisionType::Terminal;
					subfields.Add(dt);
				}
				ts->Data.Type = EDecisionType::Select;
				ts->Subfields = subfields;
			}
		}
	}

	DecisionTree.RootNode.WalkDepthFirst([this](FDraftingDecisionTreeNode &dt)
	{
		if (dt.Data.Type == EDecisionType::Select)
		{
			if (ensureAlways(dt.Subfields.Num() > 0))
			{
				dt.SetSelectedSubfield(0);
			}
		}
		return true;
	});
}
