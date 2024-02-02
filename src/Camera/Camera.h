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
	float viewportWidth;
	float viewportHeight;
	float nearPlane;
	float farPlane;

	Camera(float viewportWidth, float viewportHeight, float nearPlane, float farPlane);

	[[nodiscard]] virtual glm::mat4 calculateView() const = 0;
	[[nodiscard]] virtual glm::mat4 calculateProjection() const = 0;

	void updateViewport(float width, float height);
	virtual void movePosition(Direction direction, float deltaTime) = 0;
	virtual void moveDirection(float dx, float dy) = 0;
	virtual void scrollWheel(float dx, float dy) = 0;
	virtual glm::vec3 getPosition() const = 0;

	virtual ~Camera() = default;
};