#include <srat/core-types.hpp>
#include <srat/core-math.hpp>

#include <srat/gfx-command-buffer.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include "cgltf.h"
#pragma GCC diagnostic pop

#include <cfloat>
#include <vector>

struct SratModel {
	struct Mesh {
		std::vector<u32> indices;
		std::vector<f32v3> positions;
		std::vector<f32v4> colors;
		std::vector<f32v3> normals;
		std::vector<f32v2> uvcoords;
		srat::gfx::DrawInfo drawInfo;
	};
	std::vector<Mesh> meshes;

	f32v3 boundsMin { FLT_MAX, FLT_MAX, FLT_MAX };
	f32v3 boundsMax { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	[[nodiscard]] f32v3 center() const { return (boundsMin + boundsMax) * 0.5f; }
	[[nodiscard]] f32v3 size() const { return boundsMax - boundsMin; }
};

SratModel load_gltf_model_from_file(char const * const filepath);
