// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: when c++17 is available, switch static to inline extern
#define MODUMATE_COMMAND(cmdName,cmdText) static const TCHAR * cmdName = TEXT(cmdText)
#define MODUMATE_PARAM(paramName,paramText) MODUMATE_COMMAND(paramName,paramText)

namespace ModumateCommands
{
	// System commands
	MODUMATE_COMMAND(kDebug, "debug");
	MODUMATE_COMMAND(kValidateData, "validate_data");
	MODUMATE_COMMAND(kYield, "yield");
	MODUMATE_COMMAND(kScreenPrint, "screen_print");
	MODUMATE_COMMAND(kReanalyzeGraph, "reanalyze_graph");
	MODUMATE_COMMAND(kCleanAllObjects, "clean_all_objects");
	MODUMATE_COMMAND(kReplayDeltas, "replay_deltas");

	// Edit environment commands
	MODUMATE_COMMAND(kSetFOV, "set_fov");
	MODUMATE_COMMAND(kGraphDirection, "graph_direction");

	// Object creators
	MODUMATE_COMMAND(kMakeScopeBox, "make_scopebox");
	MODUMATE_COMMAND(kImportDatasmith, "import_datasmith");

	// Groups
	MODUMATE_COMMAND(kViewGroupObject, "view_group_object");
	MODUMATE_COMMAND(kGroup, "group");

	// Selected Objects
	MODUMATE_COMMAND(kDeleteSelectedObjects, "delete_selected");
	MODUMATE_COMMAND(kDeleteObjects, "delete_objects");
	MODUMATE_COMMAND(kCutSelected, "cut_selected");
	MODUMATE_COMMAND(kCopySelected, "copy_selected");
	MODUMATE_COMMAND(kPaste, "paste");
	MODUMATE_COMMAND(kCloneObjects, "clone_objects");
	MODUMATE_COMMAND(kRestoreDeletedObjects, "restore_deleted_objects");

	// BIM
	MODUMATE_COMMAND(kBIMDebug, "bim_debug");

	// Design Options
	MODUMATE_COMMAND(kDesignOption, "design_option");
}

namespace ModumateParameters
{
	MODUMATE_PARAM(kAssembly, "assembly");
	MODUMATE_PARAM(kArrangement, "arrangement");
	MODUMATE_PARAM(kBaseElevation, "base_elevation");
	MODUMATE_PARAM(kBaseKeyName, "base_key_name");
	MODUMATE_PARAM(kBatchSize, "batch_size");
	MODUMATE_PARAM(kChildPresets, "child_presets");
	MODUMATE_PARAM(kChildNodePinSetIndices, "child_node_pinset_indices");
	MODUMATE_PARAM(kChildNodePinSetPositions, "child_node_pinset_positions");
	MODUMATE_PARAM(kChildNodePinSetPresetIDs, "child_node_pinset_presetids");
	MODUMATE_PARAM(kParentTagPaths, "parent_tag_paths");
	MODUMATE_PARAM(kMyTagPath, "my_tag_path");
	MODUMATE_PARAM(kCount, "count");
	MODUMATE_PARAM(kControlPoints, "points");
	MODUMATE_PARAM(kDelta, "delta");
	MODUMATE_PARAM(kDisplayName, "display_name");
	MODUMATE_PARAM(kDynamicLists, "dynamic_lists");
	MODUMATE_PARAM(kEdgesHaveFaces, "edges_have_faces");
	MODUMATE_PARAM(kEditViewMode, "edit_view_mode");
	MODUMATE_PARAM(kExcluded, "excluded");
	MODUMATE_PARAM(kExtents, "extents");
	MODUMATE_PARAM(kFieldOfView, "fov");
	MODUMATE_PARAM(kFilename, "filename");
	MODUMATE_PARAM(kFormName, "form_name");
	MODUMATE_PARAM(kFunction, "function");
	MODUMATE_PARAM(kHeight, "height");
	MODUMATE_PARAM(kHeights, "heights");
	MODUMATE_PARAM(kIncludeConnected, "include_connected");
	MODUMATE_PARAM(kIncluded, "included");
	MODUMATE_PARAM(kIndex, "index");
	MODUMATE_PARAM(kIndices, "indices");
	MODUMATE_PARAM(kInterval, "interval");
	MODUMATE_PARAM(kInverted, "inverted");
	MODUMATE_PARAM(kKeys, "keys");
	MODUMATE_PARAM(kLocation, "location");
	MODUMATE_PARAM(kMake, "make");
	MODUMATE_PARAM(kMiter, "miter");
	MODUMATE_PARAM(kMode, "mode");
	MODUMATE_PARAM(kObjectIDs, "ids");
	MODUMATE_PARAM(kObjectID, "id");
	MODUMATE_PARAM(kObjectType, "object_type");
	MODUMATE_PARAM(kOffset, "offset");
	MODUMATE_PARAM(kOrigin, "origin");
	MODUMATE_PARAM(kOutput, "output");
	MODUMATE_PARAM(kNodeType, "node_type");
	MODUMATE_PARAM(kNormal, "normal");
	MODUMATE_PARAM(kNumber, "number");
	MODUMATE_PARAM(kParent, "parent");
	MODUMATE_PARAM(kParentIDs, "parent_ids");
	MODUMATE_PARAM(kPoint1, "point1");
	MODUMATE_PARAM(kPoint2, "point2");
	MODUMATE_PARAM(kPresetKey, "preset_key");
	MODUMATE_PARAM(kProjection, "projection");
	MODUMATE_PARAM(kPropertyNames, "property_names");
	MODUMATE_PARAM(kPropertyValues, "property_values");
	MODUMATE_PARAM(kQuaternion, "quaternion");
	MODUMATE_PARAM(kReadOnly, "read_only");
	MODUMATE_PARAM(kReplacementKey, "replacement_key");
	MODUMATE_PARAM(kResult, "result");
	MODUMATE_PARAM(kRotator, "rotator");
	MODUMATE_PARAM(kSeconds, "seconds");
	MODUMATE_PARAM(kSelected, "selected");
	MODUMATE_PARAM(kSubcategory, "subcategory");
	MODUMATE_PARAM(kSkip, "skip");
	MODUMATE_PARAM(kSuccess, "success");
	MODUMATE_PARAM(kText, "text");
	MODUMATE_PARAM(kTags, "tags");
	MODUMATE_PARAM(kToolMode, "tool_mode");
	MODUMATE_PARAM(kTransversed, "transversed");
	MODUMATE_PARAM(kType, "type");
	MODUMATE_PARAM(kValues, "values");
	MODUMATE_PARAM(kWidth, "width");
	MODUMATE_PARAM(kWorldCoordinates, "world_coords");
}
