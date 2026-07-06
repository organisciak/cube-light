#pragma once

// Public control surface for the snake pattern (port of the TS exports
// queueSnakeInput / getSnakeState). The firmware's network layer calls
// queueSnakeInput when a controller (WS message, D-pad page, gamepad)
// sends a direction.

namespace cube {

enum class SnakeDir : int { XP = 0, XN = 1, YP = 2, YN = 3, ZP = 4, ZN = 5 };

void queueSnakeInput(SnakeDir dir);

struct SnakeState {
  int score;
  int highScore;
  bool alive;
  int length;
};

/** Returns false if the game hasn't been initialized yet. */
bool getSnakeState(SnakeState& out);

}  // namespace cube
