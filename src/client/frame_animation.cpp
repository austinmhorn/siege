#include "client/frame_animation.hpp"

#include <stdexcept>
#include <utility>

namespace siege {

FrameAnimation::FrameAnimation(std::vector<std::size_t> ordered_frames,
                               const double seconds_per_frame, const bool looping)
    : frames_(std::move(ordered_frames)), seconds_per_frame_(seconds_per_frame),
      looping_(looping) {
    if (frames_.empty()) {
        throw std::invalid_argument("an animation requires at least one frame");
    }
    if (seconds_per_frame_ <= 0.0) {
        throw std::invalid_argument("animation frame duration must be positive");
    }
}

void FrameAnimation::update(const double delta_seconds) noexcept {
    if (finished_ || delta_seconds <= 0.0) {
        return;
    }

    elapsed_ += delta_seconds;
    while (elapsed_ >= seconds_per_frame_) {
        elapsed_ -= seconds_per_frame_;
        if (current_index_ + 1 < frames_.size()) {
            ++current_index_;
        } else if (looping_) {
            current_index_ = 0;
        } else {
            finished_ = true;
            break;
        }
    }
}

void FrameAnimation::reset() noexcept {
    elapsed_ = 0.0;
    current_index_ = 0;
    finished_ = false;
}

std::size_t FrameAnimation::current_frame() const noexcept {
    return frames_[current_index_];
}

bool FrameAnimation::finished() const noexcept {
    return finished_;
}

} // namespace siege
