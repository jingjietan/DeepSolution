#pragma once

#include "Camera.h"
#include <glm/glm.hpp>

class FreeCamera: public Camera
{
public:
	glm::vec3 position;
	const glm::vec3 up = glm::vec3(0, 1, 0);

	FreeCamera(glm::vec3 position, float pitch, float yaw, float viewportWidth, float viewportHeight, float nearPlane, float farPlane);

	[[nodiscard]] glm::mat4 calculateView() const override;
	[[nodiscard]] glm::mat4 calculateProjection() const override;

	void movePosition(Direction direction, float deltaTime) override;
	void moveDirection(float dx, float dy) override;
	glm::vec3 getPosition() const override;
	void scrollWheel(float dx, float dy) override;

	const glm::vec3& getFront() const;
private:
	float pitch;
	float yaw;

	glm::vec3 front;
	glm::vec3 right;

	void updateFront();
};