#pragma once

// Public control surface for the pacman pattern (port of queuePacmanInput).
// Shares the direction enum with the snake.

#include "cube_snake.h"

namespace cube {

void queuePacmanInput(SnakeDir dir);

}  // namespace cube
