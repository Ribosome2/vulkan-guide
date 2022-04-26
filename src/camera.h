
#pragma once
#include <glm/gtx/transform.hpp>
#include <glm/glm.hpp>
#include <SDL.h>

class Camera {
public:
	void handleInput();

public:
	glm::vec3 camPos = {0.f, -6.f, -10.f};
private:
	float  moveSpeed=0.1f;
};


