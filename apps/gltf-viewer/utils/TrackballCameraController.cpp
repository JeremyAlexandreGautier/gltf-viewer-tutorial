#include "TrackballCameraController.hpp"
#include "glfw.hpp"

#include <iostream>

using namespace glm;


bool TrackballCameraController::update(float elapsedTime) {
    if (glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
        !m_MiddleButtonPressed) {
        m_MiddleButtonPressed = true;
        glfwGetCursorPos(
                m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
    } else if (!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
               m_MiddleButtonPressed) {
        m_MiddleButtonPressed = false;
    }

    const auto cursorDelta = ([&]() {
        if (m_MiddleButtonPressed) {
            dvec2 cursorPosition;
            glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
            const auto delta = cursorPosition - m_LastCursorPosition;
            m_LastCursorPosition = cursorPosition;
            return delta;
        }
        return dvec2(0);
    })();

    float truckLeft = 0.f;
    float pedestalUp = 0.f;
    float dollyIn = 0.f;

    if(glfwGetKey(m_pWindow, GLFW_KEY_LEFT_SHIFT)){
        truckLeft = -0.01f * float(cursorDelta.x); // the tuto say's pan, but i'm pretty sure this is a truck
        pedestalUp = -0.01f * float(cursorDelta.y);
        m_camera.moveLocal(truckLeft, pedestalUp, 0.f);
        return true;
    }

    if(glfwGetKey(m_pWindow, GLFW_KEY_LEFT_CONTROL)){
        const auto viewVector = m_camera.center() - m_camera.eye();
        dollyIn += cursorDelta.y * m_fSpeed * elapsedTime;



        auto newEye = m_camera.eye() + dollyIn
                * (viewVector/ glm::distance(m_camera.center(), m_camera.eye()));
        if(newEye == m_camera.center()){
            return false;
        }
        m_camera = Camera(newEye, m_camera.center(), m_worldUpAxis);
        return true;
    }



    const auto rotationX = -0.01f * float(cursorDelta.x);
    const auto rotationY = 0.01f * float(cursorDelta.y);

    const auto depthAxis = m_camera.eye() - m_camera.center();
    //X and Y are reversed in the tutorial
    const auto upMovementY = rotate(glm::mat4(1), rotationY, m_camera.left());
    const auto leftMovementX = rotate(glm::mat4(1), rotationX, m_worldUpAxis);

    const auto rotatedDepthAxis = glm::vec3(upMovementY * glm::vec4(depthAxis, 0));

    const auto finalDepthAxis = glm::vec3(leftMovementX * glm::vec4(rotatedDepthAxis, 0));

    const auto newEye = m_camera.center() + finalDepthAxis;
    m_camera = Camera(newEye, m_camera.center(), m_worldUpAxis);

    return true;

}