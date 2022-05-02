// Coyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "DrawingDesignerDocumentDelta.generated.h"

UENUM()
enum class EDeltaVerb
{
	unknown = 0,
	add,
	modify,
	remove
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerJsDeltaHeader
{
	GENERATED_BODY()
	FDrawingDesignerJsDeltaHeader() = default;

	UPROPERTY()
	EDeltaVerb verb = EDeltaVerb::unknown;

	UPROPERTY()
	int32 id = 0;

	UPROPERTY()
	int32 parent = 0;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerJsDelta
{
	GENERATED_BODY()
	FDrawingDesignerJsDelta() = default;

	UPROPERTY()
	FDrawingDesignerJsDeltaHeader header;

	UPROPERTY()
	FDrawingDesignerNode details;
};


USTRUCT()
struct MODUMATE_API FDrawingDesignerJsDeltaPackage
{
	GENERATED_BODY()
	FDrawingDesignerJsDeltaPackage() = default;

	UPROPERTY()
	TArray<FDrawingDesignerJsDelta> deltas;

	UPROPERTY()
	bool bAddToUndo = true;

	bool ReadJson(const FString& InJson);
};


USTRUCT()
struct MODUMATE_API FDrawingDesignerDocumentDelta : public FDocumentDelta
{
	GENERATED_BODY()
	FDrawingDesignerDocumentDelta() = default;
	FDrawingDesignerDocumentDelta(const FDrawingDesignerDocument& doc, FDrawingDesignerJsDeltaPackage package);

	UPROPERTY()
	FDrawingDesignerDocument to;

	UPROPERTY()
	FDrawingDesignerDocument from;

	virtual bool ApplyTo(UModumateDocument* Doc, UWorld* World) const override;
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const override;
	virtual FStructDataWrapper SerializeStruct() override;

protected:

	bool ParseDeltaVerb(FDrawingDesignerJsDelta& delta);
};
