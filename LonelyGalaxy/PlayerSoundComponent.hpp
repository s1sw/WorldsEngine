#pragma once

namespace lg {
    struct PlayerSoundComponent {
        float timeSinceLastJump = 0.0f;
        bool groundedLast = false;
        bool dblJumpUsedLast = false;
        float stepTimer = 0.0f;
    };
}
