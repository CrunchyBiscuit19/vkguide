#include <vk_meshes.h>
#include "vk_engine.h"

void Node::refreshTransform(const glm::mat4& parentTransform)
{
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& c : mChildren)
        c->refreshTransform(mWorldTransform);
}
