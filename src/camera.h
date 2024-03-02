#pragma once

#include <SDL_events.h>
#include <glm/gtx/quaternion.hpp>

#include <camera.h>
#include <vk_types.h>

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
    
    MovementMode movementMode;

    void init();

    glm::mat4 getViewMatrix() const;
    glm::quat getPitchMatrix() const;
    glm::quat getYawMatrix() const;
    glm::mat4 getRotationMatrix() const;

    void processSDLEvent(const SDL_Event& e);

    void update();
};
