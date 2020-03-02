#pragma once

#include "cameras.hpp"

class CameraController{
public :
    virtual const Camera &getCamera() const = 0;
    virtual void setCamera(const Camera&camera) = 0;
    virtual bool update(float elapsedTime) = 0;
};