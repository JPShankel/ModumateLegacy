// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Components/RichTextBlockDecorator.h"

#include "RichInlineURLDecorator.generated.h"


class FRichInlineURLDecorator : public FRichTextDecorator
{
public:
	FRichInlineURLDecorator(URichTextBlock* InOwner, class URichTextBlockURLDecorator* decorator);
	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

protected:
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const override;
	FHyperlinkStyle LinkStyle;

	static const FString URLTagID;
	static const FString URLTagContentKey;
};


UCLASS()
class MODUMATE_API URichTextBlockURLDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FHyperlinkStyle Style;
};
