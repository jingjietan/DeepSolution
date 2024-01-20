#pragma once

#include <glm/glm.hpp>

enum class Direction
{
	Forward,
	Backward,
	Left,
	Right
};

class Camera
{
public:
	glm::vec3 position;
	const glm::vec3 up = glm::vec3(0, 1, 0);

	float viewportWidth;
	float viewportHeight;
	float nearPlane;
	float farPlane;

	Camera(glm::vec3 position, float pitch, float yaw, float viewportWidth, float viewportHeight, float nearPlane, float farPlane);

	[[nodiscard]] glm::mat4 calculateView() const;
	[[nodiscard]] glm::mat4 calculateProjection();

	void updateViewport(float width, float height);
	void movePosition(Direction direction, float deltaTime);
	void moveDirection(float dx, float dy);

	const glm::vec3& getFront() const;
private:
	float pitch;
	float yaw;

	glm::vec3 front;
	glm::vec3 right;

	void updateFront();
};