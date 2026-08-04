// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_mesh.h"

#include <array>
#include <map>
#include <utility>

namespace navycraft {
namespace {
struct FaceDefinition {
    LocalNodePos neighbour;
    Vec3d normal;
    std::array<Vec3d, 4> corners;
};

const std::array<FaceDefinition, 6> FACES{{
    {{-1, 0, 0}, {-1.0, 0.0, 0.0}, {{{0.0, 0.0, 1.0}, {0.0, 0.0, 0.0},
                                                {0.0, 1.0, 0.0}, {0.0, 1.0, 1.0}}}},
    {{1, 0, 0}, {1.0, 0.0, 0.0}, {{{1.0, 0.0, 0.0}, {1.0, 0.0, 1.0},
                                              {1.0, 1.0, 1.0}, {1.0, 1.0, 0.0}}}},
    {{0, -1, 0}, {0.0, -1.0, 0.0}, {{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0},
                                                {1.0, 0.0, 1.0}, {1.0, 0.0, 0.0}}}},
    {{0, 1, 0}, {0.0, 1.0, 0.0}, {{{0.0, 1.0, 1.0}, {0.0, 1.0, 0.0},
                                              {1.0, 1.0, 0.0}, {1.0, 1.0, 1.0}}}},
    {{0, 0, -1}, {0.0, 0.0, -1.0}, {{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
                                                {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}}}},
    {{0, 0, 1}, {0.0, 0.0, 1.0}, {{{1.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
                                              {0.0, 1.0, 1.0}, {1.0, 1.0, 1.0}}}},
}};

LocalNodePos added(const LocalNodePos &left, const LocalNodePos &right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}
}

ConstructSectionMesh ConstructMesher::buildSection(
    const DynamicConstruct &construct,
    const ConstructSection &section)
{
    ConstructSectionMesh result;
    result.section_position = section.position;
    result.section_revision = section.revision;

    std::map<std::string, ConstructMeshBuffer> buffers;
    for (const auto &entry : section.nodes) {
        const std::string material = entry.node.node_name.empty() ?
            std::string("unknown") : entry.node.node_name;
        auto &buffer = buffers[material];
        buffer.material = material;
        const Vec3d base{static_cast<double>(entry.position.x),
            static_cast<double>(entry.position.y), static_cast<double>(entry.position.z)};
        for (const auto &face : FACES) {
            if (construct.getNode(added(entry.position, face.neighbour)))
                continue;
            const std::uint32_t first = static_cast<std::uint32_t>(buffer.vertices.size());
            static constexpr std::array<std::array<double, 2>, 4> UV{{
                {{0.0, 1.0}}, {{1.0, 1.0}}, {{1.0, 0.0}}, {{0.0, 0.0}},
            }};
            for (std::size_t index = 0; index < face.corners.size(); ++index) {
                buffer.vertices.push_back({base + face.corners[index], face.normal,
                    UV[index][0], UV[index][1]});
            }
            buffer.indices.insert(buffer.indices.end(), {
                first, first + 1U, first + 2U,
                first, first + 2U, first + 3U,
            });
            ++result.visible_faces;
        }
    }
    result.buffers.reserve(buffers.size());
    for (auto &[material, buffer] : buffers) {
        (void)material;
        result.buffers.push_back(std::move(buffer));
    }
    return result;
}

} // namespace navycraft
