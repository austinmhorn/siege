#pragma once

#include <cstddef>
#include <vector>

namespace siege {

class FrameAnimation {
public:
    FrameAnimation(std::vector<std::size_t> ordered_frames,
                   double seconds_per_frame, bool looping);

    void update(double delta_seconds) noexcept;
    void reset() noexcept;

    [[nodiscard]] std::size_t current_frame() const noexcept;
    [[nodiscard]] bool finished() const noexcept;

private:
    std::vector<std::size_t> frames_;
    double seconds_per_frame_{};
    double elapsed_{};
    std::size_t current_index_{};
    bool looping_{};
    bool finished_{};
};

} // namespace siege
