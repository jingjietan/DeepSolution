#include "Camera.h"

Camera::Camera(float viewportWidth, float viewportHeight, float nearPlane, float farPlane): viewportWidth(viewportWidth), viewportHeight(viewportHeight), nearPlane(nearPlane), farPlane(farPlane)
{
}

void Camera::updateViewport(float width, float height)
{
	viewportWidth = width;
	viewportHeight = height;
}