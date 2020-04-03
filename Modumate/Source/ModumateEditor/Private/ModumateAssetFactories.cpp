// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateAssetFactories.h"

#include "Database/ModumateSimpleMesh.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModumateEditorModule.h"
#include "ModumateUnits.h"
#include "Subsystems/ImportSubsystem.h"

#define LOCTEXT_NAMESPACE "ModumateEditor"

using namespace Modumate;

/*------------------------------------------------------------------------------
	USimpleMeshImportFactory.
------------------------------------------------------------------------------*/

USimpleMeshImportFactory::USimpleMeshImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USimpleMeshData::StaticClass();
	bEditorImport = true;
	bText = true;
	ImportPriority = DefaultImportPriority + 10;

	Formats.Add(TEXT("obj;OBJ Static meshes"));
}

bool USimpleMeshImportFactory::ImportFromOBJ(class USimpleMeshData* TargetObject, const FString& InFullPath, const FString& InFileContents, FFeedbackContext* InWarn)
{
	static const FString vertexType(TEXT("v"));
	static const FString triangleType(TEXT("f"));
	static const FString triSplitStr(TEXT("/"));

	if (!TargetObject->SetSourcePath(InFullPath))
	{
		InWarn->Logf(ELogVerbosity::Error, TEXT("Could not parse given source file path: \"%s\""), *InFullPath);
		return false;
	}

	TargetObject->Polygons.Reset();
	FSimplePolygon curPolygon;
	bool bFoundVertices = false, bReadingVertices = true, bVertexWarning = false;
	bool bFoundTriangles = false, bReadingTriangles = true, bTriangleWarning = false;
	TArray<FString> objLines, objTokens;
	TSet<int32> triangleIndexSet;

	// Parse the first continuous list of vertices in the .obj file, which start with the "v " prefix.
	InFileContents.ParseIntoArrayLines(objLines, false);
	for (int32 lineIdx = 0; lineIdx <= objLines.Num(); ++lineIdx)
	{
		FString objLine;
		FVector parsedValue;
		objTokens.Reset();

		if (lineIdx < objLines.Num())
		{
			objLine = objLines[lineIdx];
			if (!objLine.IsEmpty())
			{
				objLine.ParseIntoArray(objTokens, TEXT(" "));
			}
		}
		int32 numTokens = objTokens.Num();

		// Read a vertex
		if ((numTokens > 1) && objTokens[0].Equals(vertexType) && (!bFoundVertices || bReadingVertices))
		{
			bFoundVertices = true;
			bReadingVertices = true;

			if ((numTokens == 4) &&
				LexTryParseString(parsedValue.X, *objTokens[1]) &&
				LexTryParseString(parsedValue.Y, *objTokens[2]) &&
				LexTryParseString(parsedValue.Z, *objTokens[3]) &&
				(parsedValue.Z == 0.0f))
			{
				// Polygons are assumed to be given in inches, serialized as centimeters.
				Units::FCoordinates2D saved2DCoord(
					Units::FXCoord::WorldInches(parsedValue.X),
					Units::FYCoord::WorldInches(parsedValue.Y)
				);
				FVector2D saved2DValue = saved2DCoord.AsWorldCentimeters();

				bool bVertexUnique = true;
				for (const FVector2D &existingPoint : curPolygon.Points)
				{
					if (saved2DValue.Equals(existingPoint))
					{
						bVertexUnique = false;
						break;
					}
				}

				if (bVertexUnique)
				{
					curPolygon.Points.Add(saved2DValue);
				}
				else
				{
					bVertexWarning = true;
					InWarn->Logf(ELogVerbosity::Warning, TEXT("Redundant vertex on line #%d: %s"),
						lineIdx + 1, *saved2DValue.ToString());
				}
			}
			else
			{
				bVertexWarning = true;
				InWarn->Logf(ELogVerbosity::Warning, TEXT("Could not parse vertex on line #%d: \"%s\""),
					lineIdx + 1, *objLine);
			}
		}
		else if (bFoundVertices && bReadingVertices)
		{
			// We're done reading the first continuous set of vertices
			bReadingVertices = false;
		}

		// Read a triangle
		if ((numTokens > 1) && objTokens[0].Equals(triangleType) && (!bFoundTriangles || bReadingTriangles))
		{
			bFoundTriangles = true;
			bReadingTriangles = true;

			int32 triIdx1, triIdx2, triIdx3;
			FString triStr1A, triStr1B;
			FString triStr2A, triStr2B;
			FString triStr3A, triStr3B;
			if ((numTokens == 4) &&
				objTokens[1].Split(triSplitStr, &triStr1A, &triStr1B) && triStr1A.Equals(triStr1B) &&
				objTokens[2].Split(triSplitStr, &triStr2A, &triStr2B) && triStr1A.Equals(triStr1B) &&
				objTokens[3].Split(triSplitStr, &triStr3A, &triStr3B) && triStr1A.Equals(triStr1B) &&
				LexTryParseString(triIdx1, *triStr1A) && (triIdx1 >= 1) &&
				LexTryParseString(triIdx2, *triStr2A) && (triIdx2 >= 1) &&
				LexTryParseString(triIdx3, *triStr3A) && (triIdx3 >= 1))
			{
				int32 indices[] = { triIdx1, triIdx2, triIdx3 };
				for (int32 &triIdx : indices)
				{
					triIdx--;
					curPolygon.Triangles.Add(triIdx);
					triangleIndexSet.Add(triIdx);
				}
			}
			else
			{
				bTriangleWarning = true;
				InWarn->Logf(ELogVerbosity::Warning, TEXT("Could not parse triangle on line #%d: \"%s\""),
					lineIdx + 1, *objLine);
			}
		}
		else if (bFoundTriangles && bReadingTriangles)
		{
			// We're done reading the first continuous set of triangles
			bReadingTriangles = false;

			if (curPolygon.Points.Num() < 3)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Not enough vertices! Need at least 3."));
				return false;
			}

			if (curPolygon.Triangles.Num() < 1)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Not enough triangles! Need at least 1."));
				return false;
			}

			if (triangleIndexSet.Num() != curPolygon.Points.Num())
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Triangle indices do not correspond to vertices!"));
				return false;
			}

			// Make sure that the polygon is valid before adding it to the mesh.
			bool bValidPolygon = curPolygon.ValidateSimple(InWarn);

			// If it's invalid, it might be a quad that is trivially easy to fix; if so, try to fix it.
			if (!bValidPolygon && (curPolygon.Points.Num() == 4) && (curPolygon.Triangles.Num() == 6))
			{
				// If the edges of the quad crossed each other, then swap the last two vertices and it should be valid.
				Swap(curPolygon.Points[2], curPolygon.Points[3]);
				for (int32 &vertIndex : curPolygon.Triangles)
				{
					if (vertIndex == 2)
					{
						vertIndex = 3;
					}
					else if (vertIndex == 3)
					{
						vertIndex = 2;
					}
				}

				bValidPolygon = curPolygon.ValidateSimple(InWarn);
			}

			if (bValidPolygon)
			{
				curPolygon.UpdateExtents();
				TargetObject->Polygons.Add(curPolygon);
			}
			else
			{
				InWarn->Logf(ELogVerbosity::Warning, TEXT("Polygon #%d (with %d points, ending on line #%d) was invalid, and could not be fixed!"),
					TargetObject->Polygons.Num() + 1, curPolygon.Points.Num(), lineIdx + 1);
			}

			// Allow finding more polygons after the first one.
			curPolygon.Reset();
			triangleIndexSet.Reset();
			bFoundVertices = false;
			bFoundTriangles = false;
		}
	}

	return (TargetObject->Polygons.Num() > 0);
}

UObject* USimpleMeshImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, const TCHAR* InType, const TCHAR*& InBuffer, const TCHAR* InBufferEnd, FFeedbackContext* InWarn)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, InType);

	USimpleMeshData* meshData = NewObject<USimpleMeshData>(InParent, InClass, InName, InFlags);
	if (meshData)
	{
		const int32 bufferSize = InBufferEnd - InBuffer;
		FString sourceString(InBuffer, bufferSize);

		if (!USimpleMeshImportFactory::ImportFromOBJ(meshData, CurrentFilename, sourceString, InWarn))
		{
			meshData = nullptr;
		}
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, meshData);

	return meshData;
}

bool USimpleMeshImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString fileExt = FPaths::GetExtension(Filename);

	if (fileExt == TEXT("obj"))
	{
		return true;
	}
	return false;
}

uint32 USimpleMeshImportFactory::GetMenuCategories() const
{
	FModumateEditorModule& modEditorModule = FModuleManager::GetModuleChecked<FModumateEditorModule>("ModumateEditor");

	return modEditorModule.ModumateAssetCategory;
}

FText USimpleMeshImportFactory::GetDisplayName() const
{
	return LOCTEXT("SimpleMeshDataEntry", "Simple Mesh");
}


bool USimpleMeshImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	USimpleMeshData* meshToReimport = Cast<USimpleMeshData>(Obj);
	if (meshToReimport)
	{
		OutFilenames.Add(meshToReimport->GetSourceFullPath());
		return true;
	}
	return false;
}

void USimpleMeshImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USimpleMeshData* meshToReimport = Cast<USimpleMeshData>(Obj);
	if (meshToReimport && ensure(NewReimportPaths.Num() == 1))
	{
		meshToReimport->SetSourcePath(NewReimportPaths[0]);
	}
}

EReimportResult::Type USimpleMeshImportFactory::Reimport(UObject* InObject)
{
	USimpleMeshData* meshToReimport = Cast<USimpleMeshData>(InObject);

	if (!meshToReimport)
	{
		return EReimportResult::Failed;
	}

	FString reimportFullPath = meshToReimport->GetSourceFullPath();
	if (reimportFullPath.IsEmpty() || !FPaths::FileExists(reimportFullPath))
	{
		return EReimportResult::Failed;
	}

	bool OutCanceled = false;
	if (ImportObject(InObject->GetClass(), InObject->GetOuter(), *InObject->GetName(), RF_Public | RF_Standalone, reimportFullPath, nullptr, OutCanceled))
	{
		return EReimportResult::Succeeded;
	}

	return OutCanceled ? EReimportResult::Cancelled : EReimportResult::Failed;
}

int32 USimpleMeshImportFactory::GetPriority() const
{
	return ImportPriority;
}


FText FAssetTypeActions_SimpleMeshData::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_SimpleMeshData", "Simple Mesh");
}

FColor FAssetTypeActions_SimpleMeshData::GetTypeColor() const
{
	return FColor(200, 50, 230);
}

UClass* FAssetTypeActions_SimpleMeshData::GetSupportedClass() const
{
	return USimpleMeshData::StaticClass();
}

uint32 FAssetTypeActions_SimpleMeshData::GetCategories()
{
	FModumateEditorModule& modEditorModule = FModuleManager::GetModuleChecked<FModumateEditorModule>("ModumateEditor");

	return modEditorModule.ModumateAssetCategory;
}

void FAssetTypeActions_SimpleMeshData::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (UObject *asset : TypeAssets)
	{
		const auto *simpleMesh = Cast<USimpleMeshData>(asset);
		if (simpleMesh)
		{
			FString sourceFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*simpleMesh->GetSourceFullPath());
			if (!sourceFilePath.IsEmpty())
			{
				OutSourceFilePaths.Add(MoveTemp(sourceFilePath));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
