#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_events.h>

void Camera::update()
{
    if (IsFlying)
    {
        const float lookSpeed = 0.03f;
        yaw += rightStick.x * lookSpeed;
        pitch -= rightStick.y * lookSpeed;
    }

    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    float speed = 0.01f;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT)
    {
        // RMB just pressed
        IsFlying = true;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_RIGHT)
    {
        // RMB released
        IsFlying = false;
    }
    if (e.type == SDL_EVENT_KEY_DOWN)
    {
        if ((e.key.key == SDLK_W) && (IsFlying))
        {
            velocity.z = -speed;
        }
        if ((e.key.key == SDLK_S) && (IsFlying))
        {
            velocity.z = speed;
        }
        if ((e.key.key == SDLK_A) && (IsFlying))
        {
            velocity.x = -speed;
        }
        if ((e.key.key == SDLK_D) && (IsFlying))
        {
            velocity.x = speed;
        }
    }

    if (e.type == SDL_EVENT_KEY_UP)
    {
        if (e.key.key == SDLK_W)
        {
            velocity.z = 0;
        }
        if (e.key.key == SDLK_S)
        {
            velocity.z = 0;
        }
        if (e.key.key == SDLK_A)
        {
            velocity.x = 0;
        }
        if (e.key.key == SDLK_D)
        {
            velocity.x = 0;
        }
    }

    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
    {
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
        {
            IsFlying = true;
            velocity.z = -speed;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
        {
            IsFlying = true;
            velocity.z = speed;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
        {
            IsFlying = true;
            velocity.x = -speed;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
        {
            IsFlying = true;
            velocity.x = speed;
        }
    }

    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
    {
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
        {
            velocity.z = 0;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
        {
            velocity.z = 0;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
        {
            velocity.x = 0;
        }
        if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
        {
            velocity.x = 0;
        }
    }

    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        const float deadzone = 8000.f;
        const float maxRange = 32767.f;
        auto apply = [&](Sint16 raw) -> float
        {
            float v = (float)raw;
            if (fabsf(v) < deadzone)
            {
                IsFlying = false;
                return 0.f;
            }
            float sign = v < 0 ? -1.f : 1.f;
            IsFlying = true;
            return sign * (fabsf(v) - deadzone) / (maxRange - deadzone);
        };
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX)
        {
            rightStick.x = apply(e.gaxis.value) * 0.1f;
        }
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY)
        {
            rightStick.y = apply(e.gaxis.value) * 0.1f;
        }
    }

    if ((e.type == SDL_EVENT_MOUSE_MOTION) && (IsFlying))
    {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}
