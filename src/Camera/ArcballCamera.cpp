#include "ArcballCamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

ArcballCamera::ArcballCamera(glm::vec3 position, glm::vec3 target, float viewportWidth, float viewportHeight, float nearPlane, float farPlane)
	: position_(position), target_(target), Camera(viewportWidth, viewportHeight, nearPlane, farPlane)
{
	updateInternalVectors();
}

glm::mat4 ArcballCamera::calculateView() const
{
	return glm::lookAt(position_, target_, up_);
}

glm::mat4 ArcballCamera::calculateProjection() const
{
	return glm::perspective(glm::radians(zoom), viewportWidth / viewportHeight, nearPlane, farPlane);
}

void ArcballCamera::movePosition(Direction direction, float deltaTime)
{
}

void ArcballCamera::moveDirection(float dx, float dy)
{
	const glm::vec3 front = target_ - position_;

	glm::vec4 position(position_, 1.0);
	glm::vec4 pivot(target_, 1.0);

	float dxAngle = dx * (2 * glm::pi<float>() / viewportWidth);
	float dyAngle = dy * (glm::pi<float>() / viewportHeight);

	// TODO: fix this
	//float cosAngle = glm::dot(front, up_);
	//if (cosAngle * glm::sign(dyAngle) > 0.99f)
	//	dyAngle = 0;

	glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), dxAngle, up_);
	position = (rotationX * (position - pivot)) + pivot;

	glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), dyAngle, right_);
	position = (rotationY * (position - pivot)) + pivot;
	
	position_ = position;
}

glm::vec3 ArcballCamera::getPosition() const
{
	return position_;
}

void ArcballCamera::scrollWheel(float dx, float dy)
{
	zoom -= dy;
}

void ArcballCamera::updateInternalVectors()
{
	const glm::vec3 front = target_ - position_;
	right_ = glm::normalize(glm::cross(front, globalUp));
	up_ = glm::normalize(glm::cross(right_, front));
}
