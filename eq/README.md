
## Mouse wheel on the graph (songsmithstudio#11)

What the wheel changes depends on the filter type of the band under the pointer (or the selected band), not on one fixed parameter:

| Band | The wheel changes | Step |
|---|---|---|
| High Pass, Low Pass (Low Cut, High Cut) | the slope | one supported value a notch: 6, 12, 18, 24, 36, 48 dB/oct (stops at both ends) |
| Bell, Low Shelf, High Shelf | the gain | 0.5 dB a notch, 0.1 dB with Shift, in the gain you see (it follows All Band Gains); stops at the limits |
| Notch | the Q (its only adjustable shape) | as before |

Wheel up makes the slope steeper or the gain higher; wheel down the opposite. Frequency, on/off, the Q of a Bell or shelf and every other band are never touched. A touchpad's many small movements add up to one step; turning back starts again. One burst of wheel is one Undo step, and the change is a normal parameter change, so host automation, presets and the graph follow it. Tests: `MouseWheelTests` in `Tests/EQTests.cpp`. The on-screen hint reads "Mouse wheel: slope of a High Pass or Low Pass, gain of a Bell or shelf."
