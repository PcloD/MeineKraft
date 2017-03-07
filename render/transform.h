#ifndef MEINEKRAFT_TRANSFORM_H
#define MEINEKRAFT_TRANSFORM_H

#include "../math/vector.h"

struct Transform {
    bool finished;
    bool repeat;

    const Vec3<float> from_position;
    Vec3<float> current_position;
    const Vec3<float> to_position;

    /// Time in milliseconds
    uint64_t time_elapsed;
    const uint64_t duration;

    Transform(): from_position{}, current_position{}, to_position{}, duration(0), time_elapsed(0), repeat(false), finished(false) {};
    // std::function ...

    void start() const {
        // Start the transform by adding it to the runloop?
    }

    void update(uint64_t delta) {
        if (time_elapsed >= duration && !repeat) {
            finished = true;
            return;
        }
        time_elapsed += delta;
        current_position.x = 30 * sinf(time_elapsed * M_PI_2 / 1000);
        current_position.z = 30 * cosf(time_elapsed * M_PI_2 / 1000);

    }
};

#endif //MEINEKRAFT_TRANSFORM_H
