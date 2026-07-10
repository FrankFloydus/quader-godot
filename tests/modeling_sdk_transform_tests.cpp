#include <quader/modeling/modeling.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace quader::modeling {
namespace {

constexpr float kEpsilon = 0.0001F;
constexpr float kQuarterTurnRadians = 1.57079632679489661923F;

bool near(float left, float right) {
	return std::abs(left - right) <= kEpsilon;
}

bool near(Vec3 left, Vec3 right) {
	return near(left.x, right.x) && near(left.y, right.y) && near(left.z, right.z);
}

VertexId vertex_at(const AuthoredPolygonPayload &payload, Vec3 position) {
	for (std::size_t index = 0; index < payload.vertices.size() && index < payload.positions.size(); ++index) {
		if (near(payload.positions[index], position)) {
			return payload.vertices[index];
		}
	}
	return {};
}

Vec3 vertex_position(const AuthoredPolygonPayload &payload, VertexId vertex) {
	for (std::size_t index = 0; index < payload.vertices.size() && index < payload.positions.size(); ++index) {
		if (payload.vertices[index] == vertex) {
			return payload.positions[index];
		}
	}
	return {};
}

FaceId face_with_vertices_at_y(const AuthoredPolygonPayload &payload, float y) {
	for (const AuthoredPolygonFacePayload &face : payload.faces) {
		if (face.vertices.empty()) {
			continue;
		}
		bool all_match = true;
		for (VertexId vertex : face.vertices) {
			all_match = all_match && near(vertex_position(payload, vertex).y, y);
		}
		if (all_match) {
			return face.id;
		}
	}
	return {};
}

void select_objects(ModelingApi &api, std::span<const ObjectId> objects) {
	const OperationReceipt receipt = api.meshes().only(objects).replace();
	ASSERT_TRUE(receipt.success);
}

TEST(ModelingSdkTransformTests, RotateSelectionTransformsSelectedVertexOnly) {
	ModelingApi api = ModelingApi::create({.error_policy = ErrorPolicy::StoreLastError});
	MeshHandle mesh = api.create_box({.name = "Box", .min = {0.0F, 0.0F, 0.0F}, .max = {2.0F, 2.0F, 2.0F}});
	const ObjectId object = mesh.id();
	const ObjectId objects[] = {object};
	select_objects(api, objects);

	const AuthoredPolygonPayload before = mesh.payloads().authored_polygon();
	const VertexId selected = vertex_at(before, {2.0F, 0.0F, 0.0F});
	const VertexId unselected = vertex_at(before, {0.0F, 0.0F, 0.0F});
	ASSERT_TRUE(selected.valid());
	ASSERT_TRUE(unselected.valid());

	const VertexId vertices[] = {selected};
	ASSERT_TRUE(api.mesh(object).vertices().only(vertices).replace().success);

	const OperationReceipt rotated =
			api.operations().rotate_selection({.radians = {0.0F, 0.0F, kQuarterTurnRadians}, .pivot = {}});
	ASSERT_TRUE(rotated.success);
	ASSERT_TRUE(rotated.changed);

	const AuthoredPolygonPayload after = api.mesh(object).payloads().authored_polygon();
	EXPECT_TRUE(near(vertex_position(after, selected), {0.0F, 2.0F, 0.0F}));
	EXPECT_TRUE(near(vertex_position(after, unselected), {0.0F, 0.0F, 0.0F}));
}

TEST(ModelingSdkTransformTests, ScaleSelectionTransformsSelectedFaceVerticesOnly) {
	ModelingApi api = ModelingApi::create({.error_policy = ErrorPolicy::StoreLastError});
	MeshHandle mesh = api.create_box({.name = "Box", .min = {0.0F, 0.0F, 0.0F}, .max = {2.0F, 2.0F, 2.0F}});
	const ObjectId object = mesh.id();
	const ObjectId objects[] = {object};
	select_objects(api, objects);

	const AuthoredPolygonPayload before = mesh.payloads().authored_polygon();
	const FaceId top = face_with_vertices_at_y(before, 2.0F);
	const VertexId top_vertex = vertex_at(before, {2.0F, 2.0F, 0.0F});
	const VertexId bottom_vertex = vertex_at(before, {2.0F, 0.0F, 0.0F});
	ASSERT_TRUE(top.valid());
	ASSERT_TRUE(top_vertex.valid());
	ASSERT_TRUE(bottom_vertex.valid());

	const FaceId faces[] = {top};
	ASSERT_TRUE(api.mesh(object).faces().only(faces).replace().success);

	const OperationReceipt scaled =
			api.operations().scale_selection({.scale = {2.0F, 1.0F, 1.0F}, .pivot = {}});
	ASSERT_TRUE(scaled.success);
	ASSERT_TRUE(scaled.changed);

	const AuthoredPolygonPayload after = api.mesh(object).payloads().authored_polygon();
	EXPECT_TRUE(near(vertex_position(after, top_vertex), {4.0F, 2.0F, 0.0F}));
	EXPECT_TRUE(near(vertex_position(after, bottom_vertex), {2.0F, 0.0F, 0.0F}));
}

TEST(ModelingSdkTransformTests, TranslateSelectionSkipsSelectedMeshWithoutComponents) {
	ModelingApi api = ModelingApi::create({.error_policy = ErrorPolicy::StoreLastError});
	MeshHandle edited = api.create_box({.name = "Edited", .min = {0.0F, 0.0F, 0.0F}, .max = {1.0F, 1.0F, 1.0F}});
	MeshHandle source_only =
			api.create_box({.name = "SourceOnly", .min = {4.0F, 0.0F, 0.0F}, .max = {5.0F, 1.0F, 1.0F}});
	const ObjectId objects[] = {edited.id(), source_only.id()};
	select_objects(api, objects);

	const AuthoredPolygonPayload edited_before = edited.payloads().authored_polygon();
	const AuthoredPolygonPayload source_before = source_only.payloads().authored_polygon();
	const VertexId edited_vertex = vertex_at(edited_before, {1.0F, 0.0F, 0.0F});
	const VertexId source_vertex = vertex_at(source_before, {5.0F, 0.0F, 0.0F});
	ASSERT_TRUE(edited_vertex.valid());
	ASSERT_TRUE(source_vertex.valid());

	const VertexId vertices[] = {edited_vertex};
	ASSERT_TRUE(api.mesh(edited.id()).vertices().only(vertices).replace().success);

	const OperationReceipt translated = api.operations().translate_selection({1.0F, 0.0F, 0.0F});
	ASSERT_TRUE(translated.success);
	ASSERT_TRUE(translated.changed);

	const AuthoredPolygonPayload edited_after = api.mesh(edited.id()).payloads().authored_polygon();
	const AuthoredPolygonPayload source_after = api.mesh(source_only.id()).payloads().authored_polygon();
	EXPECT_TRUE(near(vertex_position(edited_after, edited_vertex), {2.0F, 0.0F, 0.0F}));
	EXPECT_TRUE(near(vertex_position(source_after, source_vertex), {5.0F, 0.0F, 0.0F}));
}

} // namespace
} // namespace quader::modeling
