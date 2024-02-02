#include "FreeCamera.h"

#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

FreeCamera::FreeCamera(glm::vec3 position, float pitch, float yaw, float viewportWidth, float viewportHeight, float nearPlane, float farPlane)
	:position(position), pitch(pitch), yaw(yaw), Camera(viewportWidth, viewportHeight, nearPlane, farPlane)
{
	updateFront();
}

glm::mat4 FreeCamera::calculateView() const
{
	return glm::lookAt(position, position + front, glm::normalize(glm::cross(right, front)));
}

glm::mat4 FreeCamera::calculateProjection() const
{
	return glm::perspective(glm::radians(45.0f), viewportWidth / viewportHeight, nearPlane, farPlane);
}

void FreeCamera::movePosition(Direction direction, float deltaTime)
{
	constexpr float movementSpeed = 5.f;
	switch (direction)
	{
	case Direction::Forward:
		position += front * movementSpeed * deltaTime;
		break;
	case Direction::Backward:
		position -= front * movementSpeed * deltaTime;
		break;
	case Direction::Left:
		position -= right * movementSpeed * deltaTime;
		break;
	case Direction::Right:
		position += right * movementSpeed * deltaTime;
		break;
	default:
		break;
	}
}

void FreeCamera::moveDirection(float dx, float dy)
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

glm::vec3 FreeCamera::getPosition() const
{
	return position;
}

void FreeCamera::scrollWheel(float dx, float dy)
{
}

const glm::vec3& FreeCamera::getFront() const
{
	return front;
}

void FreeCamera::updateFront()
{
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	front = glm::normalize(front);

	right = glm::normalize(glm::cross(front, up));
}
