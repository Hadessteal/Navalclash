// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_section.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

struct MeshVertex {
    Vec3d position{};
    Vec3d normal{};
    double u = 0.0;
    double v = 0.0;
};

struct ConstructMeshBuffer {
    std::string material;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct ConstructSectionMesh {
    ConstructSectionPos section_position{};
    std::uint64_t section_revision = 0;
    std::vector<ConstructMeshBuffer> buffers;
    std::size_t visible_faces = 0;
};

class ConstructMesher final {
public:
    [[nodiscard]] static ConstructSectionMesh buildSection(
        const DynamicConstruct &construct,
        const ConstructSection &section);
};

} // namespace navycraft
