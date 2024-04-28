#include <vk_meshes.h>
#include "vk_engine.h"

void MeshNode::ToRenderObject(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // If the function gets called multiple times, we can draw the same multiple times with different transforms
    const glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (const auto& s : mesh->primitives) {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.bounds = s.bounds;
        def.meshNode = *this;
        def.material = &s.material->data;
        def.transform = nodeMatrix;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        if (s.material->data.passType == MaterialPass::Transparent)
            ctx.TransparentSurfaces.push_back(def);
        else
            ctx.OpaqueSurfaces.push_back(def);
    }

    // Call the Node version of the function to continue recursive drawing
    Node::ToRenderObject(topMatrix, ctx);
}
