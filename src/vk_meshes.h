#pragma once

#include <vk_materials.h>
#include <vk_types.h>

struct DrawContext;

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
    Bounds bounds;
};

// Base class for a renderable dynamic object
class IRenderable {
    virtual void ToRenderObject(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// Implementation of a drawable scene node.
// The scene node can hold children and will also keep a transform to propagate to them (ie all children nodes also get transformed).
struct Node : public IRenderable {
    // Parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform; // proj * view * localTransform

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (const auto& c : children)
            c->refreshTransform(worldTransform);
    }
    virtual void ToRenderObject(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        for (const auto& c : children)
            c->ToRenderObject(topMatrix, ctx);
    }
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces; // Mesh primitives, one material per primitve
    MeshBuffers meshBuffers;
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    void ToRenderObject(const glm::mat4& topMatrix, DrawContext& ctx) override;
};
