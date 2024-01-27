#pragma once

#include "Camera.h"

/**
 * @brief https://asliceofrendering.com/page3/
*/
class ArcballCamera: public Camera
{
public:
	const glm::vec3 globalUp = glm::vec3(0, 1, 0);
	ArcballCamera(glm::vec3 position, glm::vec3 target, float viewportWidth, float viewportHeight, float nearPlane, float farPlane);

	[[nodiscard]] glm::mat4 calculateView() const override;
	[[nodiscard]] glm::mat4 calculateProjection() const override;

	void movePosition(Direction direction, float deltaTime) override;
	void moveDirection(float dx, float dy) override;
	glm::vec3 getPosition() const override;
	void scrollWheel(float dx, float dy) override;

private:
	glm::vec3 position_;
	glm::vec3 target_;
	glm::vec3 up_;
	glm::vec3 right_;

	float zoom = 45.0f;

	void updateInternalVectors();
};