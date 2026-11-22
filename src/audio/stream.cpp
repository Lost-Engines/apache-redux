#include "stream.h"
#include <algorithm>
#include <stdexcept>

namespace apache::audio {
void stream::begin(stream_source next, size_t const offset) {
  if(!next.pcm || next.pcm->empty() || (next.channels != 1 && next.channels != 2) || next.pcm->size() % next.channels || offset > next.pcm->size() || (next.repeat && next.loop >= next.pcm->size())) throw std::invalid_argument{"Invalid PCM stream"};
  source = std::move(next); file_position = offset;
  head = tail = position = 0; space = capacity; waiting = false; open = playing = true;
  std::ranges::fill(ring, 128); preread();
  if(started) started(source, clock);
}
void stream::start(stream_source next) {
  if((open || playing) && !saved && source.repeat) store();
  begin(std::move(next), 0);
}
void stream::store() {
  if(!open) return;
  size_t unread{capacity - space};
  while(unread > source.pcm->size()) unread -= source.pcm->size();
  // A stale or wrapping file position must never become a negative seek.
  saved = saved_stream{source, file_position >= unread ? file_position - unread : 0};
}
void stream::stop() {
  open = waiting = playing = false;
}
void stream::restore() {
  stop();
  if(saved) { auto next{std::move(*saved)}; saved.reset(); begin(std::move(next.source), next.position); }
}
void stream::read_block() {
  size_t count{std::min(block, source.pcm->size() - file_position)};
  std::copy_n(source.pcm->begin() + static_cast<ptrdiff_t>(file_position), count, ring.begin() + static_cast<ptrdiff_t>(head));
  file_position += count;
  if(count < block) {
    if(source.repeat) {
      while(count < block) {
        file_position = source.loop;
        auto const rest{std::min(block - count, source.pcm->size() - file_position)};
        std::copy_n(source.pcm->begin() + static_cast<ptrdiff_t>(file_position), rest, ring.begin() + static_cast<ptrdiff_t>(head + count));
        file_position += rest; count += rest;
      }
    } else {
      open = false; waiting = true;
      for(unsigned int i{0}; i != 5; ++i) {
        std::fill_n(ring.begin() + static_cast<ptrdiff_t>(head), block, 0);
        head = (head + block) % capacity; space -= block;
      }
      return;
    }
  }
  head = (head + block) % capacity; space -= block;
}
void stream::preread() {
  if(!open) return;
  read_block();
  while(open && space > 0x20000) read_block();
}
void stream::update_space() {
  space += (position + capacity - tail) % capacity; tail = position;
  space = std::min(space, capacity);
}
void stream::reader() {
  if(open) {
    update_space();
    if(space < 0x3000) due = clock + 2640;                                     // 100 ms wait followed by the 20 ms reader sleep.
    else { preread(); due = clock + 440; }
  } else if(waiting) {
    update_space();
    if(space < 0xfd001) due = clock + 440;
    else { restore(); due = clock + 2200; }
  } else due = clock + 2200;
}
void stream::advance(size_t frames, pcm_sink const &sink) {
  while(frames) {
    if(clock >= due) reader();
    auto const step{std::min<uint64_t>(frames, due - clock)};
    if(playing) {
      size_t count{static_cast<size_t>(step) * source.channels};
      while(count) {
        auto const bytes{std::min(count, capacity - position)};
        if(sink) sink(std::span<uint8_t const>{ring}.subspan(position, bytes), source.channels);
        position = (position + bytes) % capacity; count -= bytes;
      }
    }
    clock += step; frames -= static_cast<size_t>(step);
    if(clock == due) reader();
  }
}
std::array<float, 2> stream::sample(size_t const offset) const {
  if(!playing) return {};
  size_t const index{(position + offset * source.channels) % capacity};
  float const left{(static_cast<float>(ring[index]) - 128) / 128};
  float const right{source.channels == 1 ? left : (static_cast<float>(ring[(index + 1) % capacity]) - 128) / 128};
  return {left, right};
}
} // namespace apache::audio
