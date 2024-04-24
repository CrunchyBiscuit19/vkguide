#pragma once

#include <vk_types.h>

#include <SDL_events.h>
#include <glm/gtx/quaternion.hpp>

#include <functional>

enum MovementMode
{
	MINECRAFT,
    LOOKANDGO,
};

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch { 0.f };
    float yaw { 0.f };

    const Uint8* keyState;
    SDL_bool relativeMode { SDL_TRUE };
    MovementMode movementMode;
    std::unordered_map<MovementMode, std::function<void()>> movementFuns;

    void init();

    glm::mat4 getViewMatrix() const;
    glm::quat getPitchMatrix() const;
    glm::quat getYawMatrix() const;
    glm::mat4 getRotationMatrix() const;

    void processSDLEvent(const SDL_Event& e);

    void updatePosition(float deltaTime, float expectedDeltaTime);
};
