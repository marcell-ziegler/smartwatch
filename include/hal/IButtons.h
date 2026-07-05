#pragma once
#include <optional>

// Physical navigation buttons. The real device has a 5-way d-pad (the four
// directions + a center press = Select) plus, later, some extra buttons.
enum class Button
{
    Up,
    Down,
    Left,
    Right,
    Select
};

// Edge-triggered button input. poll() returns the next *press* (button-down
// edge) since the last call, or nullopt when nothing is queued -- the caller
// drains it each tick. Each concrete driver handles its own debouncing /
// edge detection (SDL keydown events on desktop, GPIO reads on hardware).
class IButtons
{
public:
    virtual ~IButtons() = default;
    virtual void begin() = 0;
    virtual std::optional<Button> poll() = 0;
};
