#include "sf/assets/gmd_model.hpp"

#include "sf/core/error.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace sf::assets {
namespace {

constexpr std::size_t header_size = 0x18;
constexpr std::size_t triangle_size = 0x10;
constexpr std::uint32_t identifier = 0x7b;

std::uint16_t readLe16(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw core::Error{core::ErrorCode::invalid_format, "Truncated GMD 16-bit value"};
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes[offset]) |
        (std::to_integer<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

std::uint32_t readLe32(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw core::Error{core::ErrorCode::invalid_format, "Truncated GMD 32-bit value"};
    }
    return std::to_integer<std::uint32_t>(bytes[offset]) |
           (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

std::int16_t readSignedLe16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int16_t>(readLe16(bytes, offset));
}

std::int16_t unpackSigned(std::uint32_t value, unsigned int shift, unsigned int bits) {
    const auto mask = (1U << bits) - 1U;
    const auto sign = 1U << (bits - 1U);
    auto component = static_cast<std::int32_t>((value >> shift) & mask);
    if ((static_cast<std::uint32_t>(component) & sign) != 0) {
        component -= static_cast<std::int32_t>(1U << bits);
    }
    return static_cast<std::int16_t>(component);
}

bool degenerateTriangle(const GmdVertex& first, const GmdVertex& second,
                        const GmdVertex& third) noexcept {
    const auto first_x = static_cast<std::int64_t>(second.x) - first.x;
    const auto first_y = static_cast<std::int64_t>(second.y) - first.y;
    const auto first_z = static_cast<std::int64_t>(second.z) - first.z;
    const auto second_x = static_cast<std::int64_t>(third.x) - first.x;
    const auto second_y = static_cast<std::int64_t>(third.y) - first.y;
    const auto second_z = static_cast<std::int64_t>(third.z) - first.z;
    const auto cross_x = first_y * second_z - first_z * second_y;
    const auto cross_y = first_z * second_x - first_x * second_z;
    const auto cross_z = first_x * second_y - first_y * second_x;
    return cross_x == 0 && cross_y == 0 && cross_z == 0;
}

EmdUv decodeUv(std::uint16_t packed) {
    return EmdUv{
        static_cast<std::uint8_t>(packed),
        static_cast<std::uint8_t>(packed >> 8U),
    };
}

GmdVertex decodePackedVector(std::uint32_t packed) {
    return GmdVertex{
        unpackSigned(packed, 0, 10),
        unpackSigned(packed, 10, 10),
        unpackSigned(packed, 20, 12),
    };
}

struct WorkingNormal {
    double x{};
    double y{};
    double z{};
};

struct GmdFaceNormalData {
    WorkingNormal normal{};
    std::array<double, 3> corner_angles{};
    bool usable{};
};

struct GmdCornerReference {
    std::size_t triangle{};
    std::size_t corner{};
};

WorkingNormal subtract(const GmdVertex& left, const GmdVertex& right) noexcept {
    return WorkingNormal{
        static_cast<double>(left.x) - right.x,
        static_cast<double>(left.y) - right.y,
        static_cast<double>(left.z) - right.z,
    };
}

double dot(const WorkingNormal& left, const WorkingNormal& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

WorkingNormal cross(const WorkingNormal& left,
                    const WorkingNormal& right) noexcept {
    return WorkingNormal{
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

double length(const WorkingNormal& value) noexcept {
    return std::sqrt(dot(value, value));
}

WorkingNormal normalized(const WorkingNormal& value) noexcept {
    const auto magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= 0.000001) {
        return {};
    }
    return WorkingNormal{value.x / magnitude, value.y / magnitude,
                         value.z / magnitude};
}

double cornerAngle(const GmdVertex& origin, const GmdVertex& first,
                   const GmdVertex& second) noexcept {
    const auto edge_a = subtract(first, origin);
    const auto edge_b = subtract(second, origin);
    const auto denominator = length(edge_a) * length(edge_b);
    if (!std::isfinite(denominator) || denominator <= 0.000001) {
        return 0.0;
    }
    return std::acos(std::clamp(dot(edge_a, edge_b) / denominator, -1.0, 1.0));
}

bool sameSmoothingMaterial(const GmdTriangle& left,
                           const GmdTriangle& right) noexcept {
    return left.texture_page == right.texture_page && left.clut == right.clut &&
           left.flags == right.flags &&
           left.semi_transparent == right.semi_transparent;
}

bool compatibleIndexedUvs(const GmdTriangle& left,
                          const GmdTriangle& right) noexcept {
    auto shared = false;
    for (auto left_corner = std::size_t{}; left_corner < 3U; ++left_corner) {
        for (auto right_corner = std::size_t{}; right_corner < 3U;
             ++right_corner) {
            if (left.vertex_indices[left_corner] !=
                right.vertex_indices[right_corner]) {
                continue;
            }
            shared = true;
            if (left.uv[left_corner].u != right.uv[right_corner].u ||
                left.uv[left_corner].v != right.uv[right_corner].v) {
                return false;
            }
        }
    }
    return shared;
}

bool hasUsableAuthoredNormals(std::span<const GmdNormal> normals,
                              std::span<const GmdTriangle> triangles) noexcept {
    for (const auto& triangle : triangles) {
        if (triangle.flags == 0U || triangle.degenerate) {
            continue;
        }
        for (const auto index : triangle.normal_indices) {
            if (index >= normals.size()) {
                continue;
            }
            const auto& normal = normals[index];
            if (normal.x != 0 || normal.y != 0 || normal.z != 0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::vector<std::array<GmdPreparedNormal, 3>> prepareGmdFallbackNormals(
    std::span<const GmdVertex> vertices,
    std::span<const GmdTriangle> triangles, double crease_cosine) {
    crease_cosine = std::clamp(crease_cosine, -1.0, 1.0);
    auto faces = std::vector<GmdFaceNormalData>(triangles.size());
    auto incident =
        std::vector<std::vector<GmdCornerReference>>(vertices.size());
    auto has_usable_face = false;
    for (auto triangle_index = std::size_t{};
         triangle_index < triangles.size(); ++triangle_index) {
        const auto& triangle = triangles[triangle_index];
        if (triangle.flags == 0U || triangle.degenerate ||
            std::ranges::any_of(triangle.vertex_indices,
                                [&](std::uint8_t index) {
                                    return index >= vertices.size();
                                })) {
            continue;
        }
        const auto& first = vertices[triangle.vertex_indices[0]];
        const auto& second = vertices[triangle.vertex_indices[1]];
        const auto& third = vertices[triangle.vertex_indices[2]];
        const auto face_normal =
            normalized(cross(subtract(second, first), subtract(third, first)));
        if (length(face_normal) <= 0.000001) {
            continue;
        }
        auto& face = faces[triangle_index];
        face.normal = face_normal;
        face.corner_angles = {
            cornerAngle(first, second, third),
            cornerAngle(second, third, first),
            cornerAngle(third, first, second),
        };
        face.usable = true;
        has_usable_face = true;
        for (auto corner = std::size_t{}; corner < 3U; ++corner) {
            incident[triangle.vertex_indices[corner]].push_back(
                GmdCornerReference{triangle_index, corner});
        }
    }
    if (!has_usable_face) {
        return {};
    }

    auto result =
        std::vector<std::array<GmdPreparedNormal, 3>>(triangles.size());
    for (auto triangle_index = std::size_t{};
         triangle_index < triangles.size(); ++triangle_index) {
        const auto& face = faces[triangle_index];
        if (!face.usable) {
            continue;
        }
        const auto& triangle = triangles[triangle_index];
        for (auto corner = std::size_t{}; corner < 3U; ++corner) {
            auto accumulated = WorkingNormal{};
            for (const auto candidate :
                 incident[triangle.vertex_indices[corner]]) {
                const auto& candidate_face = faces[candidate.triangle];
                const auto& candidate_triangle = triangles[candidate.triangle];
                if (!candidate_face.usable ||
                    dot(face.normal, candidate_face.normal) < crease_cosine ||
                    !sameSmoothingMaterial(triangle, candidate_triangle) ||
                    !compatibleIndexedUvs(triangle, candidate_triangle)) {
                    continue;
                }
                const auto weight = candidate_face.corner_angles[candidate.corner];
                accumulated.x += candidate_face.normal.x * weight;
                accumulated.y += candidate_face.normal.y * weight;
                accumulated.z += candidate_face.normal.z * weight;
            }
            auto prepared = normalized(accumulated);
            if (length(prepared) <= 0.000001) {
                prepared = face.normal;
            }
            result[triangle_index][corner] =
                GmdPreparedNormal{prepared.x, prepared.y, prepared.z};
        }
    }
    return result;
}

GmdModel::GmdModel(
    std::vector<GmdVertex> vertices,
    std::vector<GmdNormal> normals,
    std::vector<GmdTriangle> triangles,
    EmdBounds bounds,
    std::uint32_t texture_page_mask,
    std::uint32_t renderable_texture_page_mask)
    : vertices_(std::move(vertices)),
      normals_(std::move(normals)),
      triangles_(std::move(triangles)),
      bounds_(bounds),
      texture_page_mask_(texture_page_mask),
      renderable_texture_page_mask_(renderable_texture_page_mask) {
    // Authored corner normals remain byte-exact. Only all-zero/unavailable
    // presentation tables receive the conservative generated fallback.
    if (!hasUsableAuthoredNormals(normals_, triangles_)) {
        generated_corner_normals_ =
            prepareGmdFallbackNormals(vertices_, triangles_);
    }
}

GmdModel GmdModel::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size || readLe32(bytes, 0) != identifier) {
        throw core::Error{core::ErrorCode::invalid_format, "Invalid GMD header"};
    }

    const auto triangle_count = static_cast<std::size_t>(readLe16(bytes, 4));
    const auto vertex_offset = static_cast<std::size_t>(readLe16(bytes, 6));
    const auto normal_offset = static_cast<std::size_t>(readLe16(bytes, 8));
    if (triangle_count == 0 ||
        triangle_count > (std::numeric_limits<std::size_t>::max() - header_size) / triangle_size ||
        vertex_offset != header_size + triangle_count * triangle_size ||
        normal_offset < vertex_offset || normal_offset > bytes.size() ||
        (normal_offset - vertex_offset) % sizeof(std::uint32_t) != 0) {
        throw core::Error{core::ErrorCode::invalid_format, "Invalid GMD table offsets"};
    }
    const auto vertex_count = (normal_offset - vertex_offset) / sizeof(std::uint32_t);
    if (vertex_count == 0 || vertex_count > 256U) {
        throw core::Error{core::ErrorCode::invalid_format,
                          "Invalid GMD vertex count"};
    }

    const EmdBounds bounds{
        readSignedLe16(bytes, 0x0a),
        readSignedLe16(bytes, 0x0c),
        readSignedLe16(bytes, 0x0e),
        readSignedLe16(bytes, 0x10),
        readSignedLe16(bytes, 0x12),
        readSignedLe16(bytes, 0x14),
    };
    if (bounds.minimum_x > bounds.maximum_x ||
        bounds.minimum_y > bounds.maximum_y ||
        bounds.minimum_z > bounds.maximum_z) {
        throw core::Error{core::ErrorCode::invalid_format, "Invalid GMD bounds"};
    }

    std::vector<GmdVertex> vertices;
    vertices.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index) {
        const auto packed = readLe32(bytes, vertex_offset + index * sizeof(std::uint32_t));
        vertices.push_back(decodePackedVector(packed));
    }

    std::vector<GmdTriangle> triangles;
    triangles.reserve(triangle_count);
    std::size_t normal_count{};
    std::uint32_t texture_page_mask{};
    std::uint32_t renderable_texture_page_mask{};
    for (std::size_t index = 0; index < triangle_count; ++index) {
        const auto offset = header_size + index * triangle_size;
        const auto word0 = readLe32(bytes, offset);
        const auto word1 = readLe32(bytes, offset + 4U);
        const auto word2 = readLe32(bytes, offset + 8U);
        const auto word3 = readLe32(bytes, offset + 12U);
        if ((word3 & 0xff000000U) != 0U) {
            throw core::Error{core::ErrorCode::invalid_format,
                              "Invalid GMD normal-index padding"};
        }
        GmdTriangle triangle;
        triangle.vertex_indices = {
            static_cast<std::uint8_t>(word2),
            static_cast<std::uint8_t>(word2 >> 8U),
            static_cast<std::uint8_t>(word2 >> 16U),
        };
        triangle.normal_indices = {
            static_cast<std::uint8_t>(word3),
            static_cast<std::uint8_t>(word3 >> 8U),
            static_cast<std::uint8_t>(word3 >> 16U),
        };
        for (const auto normal_index : triangle.normal_indices) {
            normal_count =
                std::max(normal_count, static_cast<std::size_t>(normal_index) + 1U);
        }
        if (std::ranges::any_of(triangle.vertex_indices, [vertex_count](std::uint8_t value) {
                return value >= vertex_count;
            })) {
            throw core::Error{core::ErrorCode::invalid_format, "GMD vertex index is out of range"};
        }
        triangle.degenerate = degenerateTriangle(
            vertices[triangle.vertex_indices[0]],
            vertices[triangle.vertex_indices[1]],
            vertices[triangle.vertex_indices[2]]);
        triangle.uv = {
            decodeUv(static_cast<std::uint16_t>(word0)),
            decodeUv(static_cast<std::uint16_t>(word1)),
            decodeUv(static_cast<std::uint16_t>(word1 >> 16U)),
        };
        triangle.clut = static_cast<std::uint16_t>(
            0x7830U | (((word0 >> 24U) & 0x7fU) << 6U));
        triangle.texture_page = static_cast<std::uint16_t>((word0 >> 16U) & 0xffU);
        triangle.flags = static_cast<std::uint8_t>((word2 >> 24U) & 0x7fU);
        triangle.semi_transparent = static_cast<std::int32_t>(word0) < 0;
        const auto texture_page_bit =
            1U << (triangle.texture_page & 0x1fU);
        texture_page_mask |= texture_page_bit;
        if (triangle.flags != 0U && !triangle.degenerate) {
            renderable_texture_page_mask |= texture_page_bit;
        }
        triangles.push_back(triangle);
    }

    // GMD does not store a normal count. Triangle word 3 is authoritative;
    // some HOG entries retain unrelated bytes after the referenced table.
    if (normal_count == 0U ||
        normal_count > (bytes.size() - normal_offset) /
                           sizeof(std::uint32_t)) {
        throw core::Error{core::ErrorCode::invalid_format,
                          "GMD normal index is out of range"};
    }
    std::vector<GmdNormal> normals;
    normals.reserve(normal_count);
    for (std::size_t index = 0; index < normal_count; ++index) {
        const auto offset = normal_offset + index * sizeof(std::uint32_t);
        if (bytes[offset + 3U] != std::byte{}) {
            throw core::Error{core::ErrorCode::invalid_format,
                              "Invalid GMD normal padding"};
        }
        normals.push_back(GmdNormal{
            static_cast<std::int8_t>(
                std::to_integer<std::uint8_t>(bytes[offset])),
            static_cast<std::int8_t>(
                std::to_integer<std::uint8_t>(bytes[offset + 1U])),
            static_cast<std::int8_t>(
                std::to_integer<std::uint8_t>(bytes[offset + 2U])),
        });
    }
    return GmdModel{std::move(vertices), std::move(normals),
                    std::move(triangles), bounds, texture_page_mask,
                    renderable_texture_page_mask};
}

} // namespace sf::assets
