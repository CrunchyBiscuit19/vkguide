#pragma once

#include <vk_materials.h>
#include <vk_types.h>

struct DrawContext;

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t vertexCount;
    std::shared_ptr<PbrMaterial> material;
    Bounds bounds;
};

// Implementation of a drawable scene node.
// The scene node can hold children and will also keep a transform to propagate to them (ie all children nodes also get transformed).
struct Node {
    // Parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform; // Original file data
    glm::mat4 worldTransform; // Modified transform to whole model

    void refreshTransform(const glm::mat4& parentMatrix);

    virtual ~Node() = default;
};

struct MeshData {
    std::string name;
    std::vector<Primitive> primitives; // Mesh primitives, one material per primitve
    MeshBuffers meshBuffers;
};

struct MeshNode : Node {
    std::shared_ptr<MeshData> mesh;
};
