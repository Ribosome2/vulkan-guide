
#include "camera.h"

void Camera::handleInput() {
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	float horizontalSpeed = 0;
	float forwardSpeed = 0;
	float verticalSpeed = 0;
	if (state[SDL_SCANCODE_A]) {
		horizontalSpeed = moveSpeed;
	}

	if (state[SDL_SCANCODE_D]) {
		horizontalSpeed = -moveSpeed;
	}

	if (state[SDL_SCANCODE_Q]) {
		verticalSpeed = moveSpeed;
	}

	if (state[SDL_SCANCODE_E]) {
		verticalSpeed = -moveSpeed;
	}

	if (state[SDL_SCANCODE_W]) {
		forwardSpeed = moveSpeed;
	}

	if (state[SDL_SCANCODE_S]) {
		forwardSpeed = -moveSpeed;
	}

	camPos += glm::vec3(horizontalSpeed, verticalSpeed, forwardSpeed);

}
