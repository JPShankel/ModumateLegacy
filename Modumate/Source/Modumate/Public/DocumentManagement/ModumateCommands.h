// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: when c++17 is available, switch static to inline extern
#define MODUMATE_COMMAND(cmdName,cmdText) static const TCHAR * cmdName = TEXT(cmdText)
#define MODUMATE_PARAM(paramName,paramText) MODUMATE_COMMAND(paramName,paramText)

namespace Modumate
{
	namespace Commands
	{
		// System commands
		MODUMATE_COMMAND(kRunScript, "run_script");
		MODUMATE_COMMAND(kMakeNew, "make_new");
		MODUMATE_COMMAND(kDraft, "draft");
		MODUMATE_COMMAND(kDebug, "debug");
		MODUMATE_COMMAND(kValidateData, "validate_data");
		MODUMATE_COMMAND(kYield, "yield");
		MODUMATE_COMMAND(kDumpScript, "dump_script");
		MODUMATE_COMMAND(kScreenPrint, "screen_print");
		MODUMATE_COMMAND(kReanalyzeGraph, "reanalyze_graph");
		MODUMATE_COMMAND(kCleanAllObjects, "clean_all_objects");
		MODUMATE_COMMAND(kLogin, "login");

		// Edit environment commands
		MODUMATE_COMMAND(kSetToolMode, "set_tool_mode");
		MODUMATE_COMMAND(kSetEditViewMode, "set_edit_view_mode");
		MODUMATE_COMMAND(kSetFOV, "set_fov");

		// Object creators
		MODUMATE_COMMAND(kAddDoor, "add_door");
		MODUMATE_COMMAND(kAddWindow, "add_window");
		MODUMATE_COMMAND(kAddFinish, "add_finish");
		MODUMATE_COMMAND(kMakeRail, "make_rail");
		MODUMATE_COMMAND(kMakeFloor, "make_floor");
		MODUMATE_COMMAND(kMakeCountertop, "make_countertop");
		MODUMATE_COMMAND(kMakeTrim, "make_trim");
		MODUMATE_COMMAND(kMakeMetaVertex, "make_vertex");
		MODUMATE_COMMAND(kMakeMetaEdge, "make_edge");
		MODUMATE_COMMAND(kMakeMetaPlane, "make_plane");
		MODUMATE_COMMAND(kMakeCutPlane, "make_cutplane");
		MODUMATE_COMMAND(kMakeScopeBox, "make_scopebox");

		// Groups
		MODUMATE_COMMAND(kViewGroupObject, "view_group_object");
		MODUMATE_COMMAND(kGroup, "group");

		// Object mutators
		MODUMATE_COMMAND(kUpdateMOIHoleParams, "update_moi_hole_params");
		MODUMATE_COMMAND(kSetGeometry, "set_geometry");
		MODUMATE_COMMAND(kMoveMetaVertices, "move_vertices");
		MODUMATE_COMMAND(kJoinMetaObjects, "join_objects");
		MODUMATE_COMMAND(kSplit, "split");
		MODUMATE_COMMAND(kInvertObjects, "invert_objects");
		MODUMATE_COMMAND(kTransverseObjects, "transverse_objects");
		MODUMATE_COMMAND(kSetAssemblyForObjects, "set_assembly_for_objects");
		MODUMATE_COMMAND(kApplyObjectDelta, "apply_object_delta");

		// Selected Objects
		MODUMATE_COMMAND(kDeleteSelectedObjects, "delete_selected");
		MODUMATE_COMMAND(kDeleteObjects, "delete_objects");
		MODUMATE_COMMAND(kSelectObject, "select_object");
		MODUMATE_COMMAND(kSelectObjects, "select_objects");
		MODUMATE_COMMAND(kSelectAll, "select_all");
		MODUMATE_COMMAND(kSelectInverse, "select_inverse");
		MODUMATE_COMMAND(kDeselectAll, "deselect_all");
		MODUMATE_COMMAND(kCutSelected, "cut_selected");
		MODUMATE_COMMAND(kCopySelected, "copy_selected");
		MODUMATE_COMMAND(kPaste, "paste");
		MODUMATE_COMMAND(kMoveObjects, "move_objects");
		MODUMATE_COMMAND(kRotateObjects, "rotate_objects");
		MODUMATE_COMMAND(kSetObjectTransforms, "set_object_transforms");
		MODUMATE_COMMAND(kCloneObjects, "clone_objects");
		MODUMATE_COMMAND(kRestoreDeletedObjects, "restore_deleted_objects");

		// Undo/Redo
		MODUMATE_COMMAND(kUndo, "undo");
		MODUMATE_COMMAND(kRedo, "redo");
		MODUMATE_COMMAND(kBeginUndoRedoMacro, "begin_undoredo_macro");
		MODUMATE_COMMAND(kEndUndoRedoMacro, "end_undoredo_macro");

		// Crafting
		MODUMATE_COMMAND(kUpdateCraftingPreset, "update_crafting_preset");
		MODUMATE_COMMAND(kRemovePresetProjectAssembly, "remove_preset_project_assembly");
		MODUMATE_COMMAND(kSetCurrentAssembly, "set_current_assembly");
		MODUMATE_COMMAND(kCreateNewAssembly_DEPRECATED, "create_new_assembly");
		MODUMATE_COMMAND(kOverwriteAssembly_DEPRECATED, "overwrite_assembly");
		MODUMATE_COMMAND(kSaveCraftingBuiltins_DEPRECATED, "save_crafting_builtins");
		MODUMATE_COMMAND(kCreateOrUpdatePreset_DEPRECATED, "create_or_update_preset");
		MODUMATE_COMMAND(kRemovePreset_DEPRECATED, "remove_preset");
		MODUMATE_COMMAND(kRemoveAssembly_DEPRECATED, "remove_assembly");
	}

	namespace Parameters
	{
		MODUMATE_PARAM(kAssembly, "assembly");
		MODUMATE_PARAM(kArrangement, "arrangement");
		MODUMATE_PARAM(kBaseElevation, "base_elevation");
		MODUMATE_PARAM(kBaseKeyName, "base_key_name");
		MODUMATE_PARAM(kBatchSize, "batch_size");
		MODUMATE_PARAM(kChildPresets, "child_presets");
		MODUMATE_PARAM(kChildNodePinSetIndices, "child_node_pinset_indices");
		MODUMATE_PARAM(kChildNodePinSetPositions, "child_node_pinset_positions");
		MODUMATE_PARAM(kChildNodePinSetListIDs, "child_node_pinset_listids");
		MODUMATE_PARAM(kChildNodePinSetPresetIDs, "child_node_pinset_presetids");
		MODUMATE_PARAM(kCount, "count");
		MODUMATE_PARAM(kControlPoints, "points");
		MODUMATE_PARAM(kDelta, "delta");
		MODUMATE_PARAM(kDisplayName, "display_name");
		MODUMATE_PARAM(kDynamicLists, "dynamic_lists");
		MODUMATE_PARAM(kEdgesHaveFaces, "edges_have_faces");
		MODUMATE_PARAM(kEditViewMode, "edit_view_mode");
		MODUMATE_PARAM(kExtents, "extents");
		MODUMATE_PARAM(kFieldOfView, "fov");
		MODUMATE_PARAM(kFilename, "filename");
		MODUMATE_PARAM(kFormName, "form_name");
		MODUMATE_PARAM(kFunction, "function");
		MODUMATE_PARAM(kHeight, "height");
		MODUMATE_PARAM(kHeights, "heights");
		MODUMATE_PARAM(kIncludeConnected, "include_connected");
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
		MODUMATE_PARAM(kPassword, "password");
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
		MODUMATE_PARAM(kToolMode, "tool_mode");
		MODUMATE_PARAM(kTransversed, "transversed");
		MODUMATE_PARAM(kType, "type");
		MODUMATE_PARAM(kUserName, "user_name");
		MODUMATE_PARAM(kValues, "values");
		MODUMATE_PARAM(kWidth, "width");
		MODUMATE_PARAM(kWorldCoordinates, "world_coords");
	}
}
