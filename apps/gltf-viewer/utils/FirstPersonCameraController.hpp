#pragma once

#include "CameraController.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "cameras.hpp"


class FirstPersonCameraController : public CameraController
{
public:
  FirstPersonCameraController(GLFWwindow *window, float speed = 1.f,
      const glm::vec3 &worldUpAxis = glm::vec3(0, 1, 0)) :
      m_pWindow(window),
      m_fSpeed(speed),
      m_worldUpAxis(worldUpAxis),
      m_camera{glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)}
  {
  }

  // Controller attributes, if put in a GUI, should be adapted
  void setSpeed(float speed) { m_fSpeed = speed; }

  float getSpeed() const { return m_fSpeed; }

  void increaseSpeed(float delta)
  {
    m_fSpeed += delta;
    m_fSpeed = glm::max(m_fSpeed, 0.f);
  }

  const glm::vec3 &getWorldUpAxis() const { return m_worldUpAxis; }

  void setWorldUpAxis(const glm::vec3 &worldUpAxis)
  {
    m_worldUpAxis = worldUpAxis;
  }

  // Update the view matrix based on input events and elapsed time
  // Return true if the view matrix has been modified
  bool update (float elapsedTime) override;

  // Get the view matrix
  const Camera &getCamera() const override { return m_camera; }

  void setCamera(const Camera &camera) override { m_camera = camera; }

private:
  GLFWwindow *m_pWindow = nullptr;
  float m_fSpeed = 0.f;
  glm::vec3 m_worldUpAxis;

  // Input event state
  bool m_LeftButtonPressed = false;
  glm::dvec2 m_LastCursorPosition;

  // Current camera
  Camera m_camera;
};
