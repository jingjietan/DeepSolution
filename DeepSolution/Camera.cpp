#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

Camera::Camera(glm::vec3 position, float pitch, float yaw, float viewportWidth, float viewportHeight, float nearPlane, float farPlane)
	:position(position), pitch(pitch), yaw(yaw), viewportWidth(viewportWidth), viewportHeight(viewportHeight), nearPlane(nearPlane), farPlane(farPlane)
{
	updateFront();
}

glm::mat4 Camera::calculateView() const
{
	return glm::lookAt(position, position + front, glm::normalize(glm::cross(right, front)));
}

glm::mat4 Camera::calculateProjection()
{
	return glm::perspective(glm::radians(45.0f), viewportWidth / viewportHeight, nearPlane, farPlane);
}

void Camera::updateViewport(float width, float height)
{
	viewportWidth = width;
	viewportHeight = height;
}

void Camera::movePosition(Direction direction, float deltaTime)
{
	switch (direction)
	{
	case Direction::Forward:
		position += front * deltaTime;
		break;
	case Direction::Backward:
		position -= front * deltaTime;
		break;
	case Direction::Left:
		position -= right * deltaTime;
		break;
	case Direction::Right:
		position += right * deltaTime;
		break;
	default:
		break;
	}
}

void Camera::moveDirection(float dx, float dy)
{
	dx *= 0.1f;
	dy *= 0.1f;

	yaw += dx;
	pitch += dy;

	if (pitch > 89.0f)
	{
		pitch = 89.0f;
	}

	if (pitch < -89.0f)
	{
		pitch = -89.0f;
	}

	updateFront();
}

const glm::vec3& Camera::getFront() const
{
	return front;
}

void Camera::updateFront()
{
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	front = glm::normalize(front);

	right = glm::normalize(glm::cross(front, up));
}
