#include "camera.h"

void Camera::init()
{
    velocity = glm::vec3(0.f);
    position = glm::vec3(0, 0, 5);
    pitch = 0;
    yaw = 0;
    keyState = SDL_GetKeyboardState(nullptr);
    movementMode = MINECRAFT;
    movementFuns[MINECRAFT] = [this]() -> void {
        const SDL_Keymod modState = SDL_GetModState();
        if (keyState[SDL_SCANCODE_W]) {
            if (modState & KMOD_LSHIFT)
                velocity.y = 1;
            else
                velocity.z = -1;
        }
        if (keyState[SDL_SCANCODE_S]) {
            if (modState & KMOD_LSHIFT)
                velocity.y = -1;
            else
                velocity.z = 1;
        }
        if (keyState[SDL_SCANCODE_A])
            velocity.x = -1;
        if (keyState[SDL_SCANCODE_D])
            velocity.x = 1;
    };
    movementFuns[LOOKANDGO] = [this]() -> void {
        const SDL_Keymod modState = SDL_GetModState();
        if (keyState[SDL_SCANCODE_W])
            velocity.z = -1;
        if (keyState[SDL_SCANCODE_S])
            velocity.z = 1;
        if (keyState[SDL_SCANCODE_A])
            velocity.x = -1;
        if (keyState[SDL_SCANCODE_D])
            velocity.x = 1;
    };
}

glm::mat4 Camera::getViewMatrix() const
{
    // To create a correct model view, we need to move the world in opposite direction to the camera.
    // Invert the translation and rotation.
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::quat Camera::getPitchMatrix() const
{
    return glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
}

glm::quat Camera::getYawMatrix() const
{
    return glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f }); // Negative Y to flip OpenGL rotation?;
}

glm::mat4 Camera::getRotationMatrix() const
{
    // Join the pitch and yaw rotations into the final rotation matrix.
    const glm::quat pitchRotation = getPitchMatrix();
    const glm::quat yawRotation = getYawMatrix();
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::update()
{
    switch (movementMode) {
    case MINECRAFT:
        position += glm::vec3(getYawMatrix() * glm::vec4(velocity * 0.5f, 0.f));
        break;
    case LOOKANDGO:
        position += glm::vec3(getRotationMatrix() * glm::vec4(velocity * 0.5f, 0.f));
        break;
    }
}

void Camera::processSDLEvent(const SDL_Event& e)
{
    const SDL_Keymod modState = SDL_GetModState();
    velocity = glm::vec3(0.f);

    movementFuns[movementMode]();

    if (keyState[SDL_SCANCODE_F1]) {
        switch (movementMode) {
        case MINECRAFT:
            movementMode = LOOKANDGO;
            break;
        case LOOKANDGO:
            movementMode = MINECRAFT;
            break;
        }
    }

    if (keyState[SDL_SCANCODE_F2])
        relativeMode = static_cast<SDL_bool>(!relativeMode);

    if (e.type == SDL_MOUSEMOTION && relativeMode) {
        yaw += static_cast<float>(e.motion.xrel) / 200.f;
        pitch -= static_cast<float>(e.motion.yrel) / 200.f;
    }
}
