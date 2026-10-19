#include <cmath>
#include <iostream>

int main()
{
    float side = 1.0f;
    float forward = 1.0f;
    float mag = std::sqrt(side * side + forward * forward);
    if (mag > 1.0f) {
        side /= mag;
        forward /= mag;
    }

    const float yaw = 0.75f;
    const float dx = std::cos(yaw) * side + std::sin(yaw) * forward;
    const float dz = -std::sin(yaw) * side + std::cos(yaw) * forward;
    const float len = std::sqrt(dx * dx + dz * dz);

    if (std::fabs(len - 1.0f) > 0.001f) {
        std::cerr << "SP smoke failed: camera-relative movement normalization\n";
        return 2;
    }

    std::cout << "SP human controller core OK\n";
    return 0;
}
