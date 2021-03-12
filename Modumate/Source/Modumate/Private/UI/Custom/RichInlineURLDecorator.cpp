// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/RichInlineURLDecorator.h"

#include "Components/RichTextBlock.h"
#include "Widgets/Input/SRichTextHyperlink.h"


const FString FRichInlineURLDecorator::URLTagID(TEXT("a"));
const FString FRichInlineURLDecorator::URLTagContentKey(TEXT("href"));

FRichInlineURLDecorator::FRichInlineURLDecorator(URichTextBlock* InOwner, URichTextBlockURLDecorator* Decorator)
	: FRichTextDecorator(InOwner)
{
	LinkStyle = Decorator->Style;
}

bool FRichInlineURLDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	return (RunParseResult.Name == FRichInlineURLDecorator::URLTagID) &&
		RunParseResult.MetaData.Contains(FRichInlineURLDecorator::URLTagContentKey);
}

TSharedPtr<SWidget> FRichInlineURLDecorator::CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const
{
	FString url = RunInfo.MetaData.FindRef(FRichInlineURLDecorator::URLTagContentKey);
	auto viewModel = MakeShared<FSlateHyperlinkRun::FWidgetViewModel>();

	return SNew(SRichTextHyperlink, viewModel)
		.Text(RunInfo.Content)
		.ToolTipText(FText::FromString(url))
		.Style(&LinkStyle)
		.OnNavigate_Lambda([url]() { FPlatformProcess::LaunchURL(*url, nullptr, nullptr); });
}


TSharedPtr<ITextDecorator> URichTextBlockURLDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShared<FRichInlineURLDecorator>(InOwner, this);
}
