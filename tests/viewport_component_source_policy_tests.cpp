#include "viewport/quader_component_source_policy.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace quader_godot::viewport {
namespace {

quader::modeling::ObjectId object_id(std::uint32_t index) {
	return quader::modeling::make_id<quader::modeling::ObjectTag>(index);
}

bool contains_object(const std::vector<quader::modeling::ObjectId> &objects, quader::modeling::ObjectId object) {
	return std::find(objects.begin(), objects.end(), object) != objects.end();
}

TEST(ComponentSourcePolicyTests, HoverCandidateKeepsSourceWireBesideSelectedVertexSource) {
	const quader::modeling::ObjectId selected = object_id(1);
	const quader::modeling::ObjectId hovered = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = selected,
					.selected = true,
					.active = true,
					.has_selected_vertices = true,
			},
			{
					.object = hovered,
			},
	};

	const std::vector<quader::modeling::ObjectId> source_objects =
			component_source_wire_objects(objects, SelectionMode::Vertex, hovered);

	ASSERT_EQ(source_objects.size(), 2U);
	EXPECT_TRUE(contains_object(source_objects, selected));
	EXPECT_TRUE(contains_object(source_objects, hovered));
	EXPECT_TRUE(component_source_candidate_draws_unselected_vertices(objects, SelectionMode::Vertex, hovered));
}

TEST(ComponentSourcePolicyTests, VertexHandlesStayOnEverySelectedEditObject) {
	const quader::modeling::ObjectId selected = object_id(1);
	const quader::modeling::ObjectId hovered = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = selected,
					.selected = true,
					.active = true,
					.has_selected_vertices = true,
			},
			{
					.object = hovered,
			},
	};

	const quader::modeling::ObjectId candidate =
			component_hover_candidate(SelectionMode::Vertex, selected, hovered);
	const std::vector<quader::modeling::ObjectId> handle_objects =
			component_vertex_handle_objects(objects, SelectionMode::Vertex, candidate);

	EXPECT_EQ(candidate, hovered);
	ASSERT_EQ(handle_objects.size(), 2U);
	EXPECT_TRUE(contains_object(handle_objects, selected));
	EXPECT_TRUE(contains_object(handle_objects, hovered));
}

TEST(ComponentSourcePolicyTests, StickySourceWireRequiresSelectedEditObject) {
	const quader::modeling::ObjectId stale = object_id(1);
	const quader::modeling::ObjectId hovered = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = stale,
					.selected = false,
					.active = true,
					.has_selected_vertices = true,
			},
			{
					.object = hovered,
			},
	};

	const std::vector<quader::modeling::ObjectId> source_objects =
			component_source_wire_objects(objects, SelectionMode::Vertex, hovered);

	ASSERT_EQ(source_objects.size(), 1U);
	EXPECT_EQ(source_objects.front(), hovered);
}

TEST(ComponentSourcePolicyTests, SourceWireStaysOnEverySelectedComponentObject) {
	const quader::modeling::ObjectId first = object_id(1);
	const quader::modeling::ObjectId second = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = first,
					.selected = true,
					.has_selected_vertices = true,
			},
			{
					.object = second,
					.selected = true,
					.active = true,
					.has_selected_vertices = true,
			},
	};

	const std::vector<quader::modeling::ObjectId> source_objects =
			component_source_wire_objects(objects, SelectionMode::Vertex, {});

	ASSERT_EQ(source_objects.size(), 2U);
	EXPECT_TRUE(contains_object(source_objects, first));
	EXPECT_TRUE(contains_object(source_objects, second));
}

TEST(ComponentSourcePolicyTests, SourceWireSurvivesComponentModeChange) {
	const quader::modeling::ObjectId first = object_id(1);
	const quader::modeling::ObjectId second = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = first,
					.selected = true,
					.has_selected_vertices = true,
			},
			{
					.object = second,
					.selected = true,
					.active = true,
					.has_selected_vertices = true,
			},
	};

	const std::vector<quader::modeling::ObjectId> edge_sources =
			component_source_wire_objects(objects, SelectionMode::Edge, {});

	ASSERT_EQ(edge_sources.size(), 2U);
	EXPECT_TRUE(contains_object(edge_sources, first));
	EXPECT_TRUE(contains_object(edge_sources, second));
}

TEST(ComponentSourcePolicyTests, VertexHandlesDoNotLeakToSelectedObjectsWithoutModeComponents) {
	const quader::modeling::ObjectId selected_with_vertex = object_id(1);
	const quader::modeling::ObjectId selected_without_vertex = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = selected_with_vertex,
					.selected = true,
					.has_selected_vertices = true,
			},
			{
					.object = selected_without_vertex,
					.selected = true,
					.active = true,
			},
	};

	const std::vector<quader::modeling::ObjectId> handle_objects =
			component_vertex_handle_objects(objects, SelectionMode::Vertex, {});

	ASSERT_EQ(handle_objects.size(), 1U);
	EXPECT_EQ(handle_objects.front(), selected_with_vertex);
}

TEST(ComponentSourcePolicyTests, HoverCandidateShowsVerticesEvenWhenObjectSelectedWithoutVertexComponents) {
	const quader::modeling::ObjectId selected_with_vertex = object_id(1);
	const quader::modeling::ObjectId hovered_object_selected = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = selected_with_vertex,
					.selected = true,
					.has_selected_vertices = true,
			},
			{
					.object = hovered_object_selected,
					.selected = true,
			},
	};

	const std::vector<quader::modeling::ObjectId> handle_objects =
			component_vertex_handle_objects(objects, SelectionMode::Vertex, hovered_object_selected);

	ASSERT_EQ(handle_objects.size(), 2U);
	EXPECT_TRUE(contains_object(handle_objects, selected_with_vertex));
	EXPECT_TRUE(contains_object(handle_objects, hovered_object_selected));
	EXPECT_TRUE(component_source_candidate_draws_unselected_vertices(
			objects, SelectionMode::Vertex, hovered_object_selected));
}

TEST(ComponentSourcePolicyTests, VertexHandlesUseOnlyActiveFallbackBeforeComponentSelection) {
	const quader::modeling::ObjectId inactive_selected = object_id(1);
	const quader::modeling::ObjectId active_selected = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = inactive_selected,
					.selected = true,
			},
			{
					.object = active_selected,
					.selected = true,
					.active = true,
			},
	};

	const std::vector<quader::modeling::ObjectId> handle_objects =
			component_vertex_handle_objects(objects, SelectionMode::Vertex, {});

	ASSERT_EQ(handle_objects.size(), 1U);
	EXPECT_EQ(handle_objects.front(), active_selected);
}

TEST(ComponentSourcePolicyTests, HoverCandidateReplacesFallbackActiveOnlyWhenActiveHasNoComponents) {
	const quader::modeling::ObjectId active = object_id(1);
	const quader::modeling::ObjectId hovered = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = active,
					.selected = true,
					.active = true,
			},
			{
					.object = hovered,
			},
	};

	const std::vector<quader::modeling::ObjectId> source_objects =
			component_source_wire_objects(objects, SelectionMode::Vertex, hovered);

	ASSERT_EQ(source_objects.size(), 1U);
	EXPECT_EQ(source_objects.front(), hovered);
}

TEST(ComponentSourcePolicyTests, SourceWireKeepsFaceSelectionAcrossModesWithoutVertexHandles) {
	const quader::modeling::ObjectId selected = object_id(1);
	const quader::modeling::ObjectId hovered = object_id(2);
	const ComponentSourceObjectState objects[] = {
			{
					.object = selected,
					.selected = true,
					.active = true,
					.has_selected_faces = true,
			},
			{
					.object = hovered,
			},
	};

	const std::vector<quader::modeling::ObjectId> vertex_sources =
			component_source_wire_objects(objects, SelectionMode::Vertex, hovered);
	ASSERT_EQ(vertex_sources.size(), 2U);
	EXPECT_TRUE(contains_object(vertex_sources, selected));
	EXPECT_TRUE(contains_object(vertex_sources, hovered));

	const std::vector<quader::modeling::ObjectId> vertex_handles =
			component_vertex_handle_objects(objects, SelectionMode::Vertex, {});
	EXPECT_TRUE(vertex_handles.empty());

	const std::vector<quader::modeling::ObjectId> face_sources =
			component_source_wire_objects(objects, SelectionMode::Face, hovered);
	ASSERT_EQ(face_sources.size(), 2U);
	EXPECT_TRUE(contains_object(face_sources, selected));
	EXPECT_TRUE(contains_object(face_sources, hovered));
}

} // namespace
} // namespace quader_godot::viewport
