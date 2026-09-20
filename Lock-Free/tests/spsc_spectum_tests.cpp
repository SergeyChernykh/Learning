#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "../SPSCQueueSpectum.h"

template <std::size_t N>
void boundaries_and_fifo() {
  SPSCQueue<std::uint64_t, N> queue;
  assert(queue.Top() == nullptr);
  // Repeated laps exercise reuse of every physical slot.
  for (std::uint64_t lap = 0; lap < 100; ++lap) {
    for (std::size_t i = 0; i < N; ++i) {
      auto* slot = static_cast<std::uint64_t*>(queue.GetFreeSlot());
      assert(slot != nullptr);
      *slot = lap * N + i;
      queue.Submit();
    }
    assert(queue.GetFreeSlot() == nullptr);
    for (std::size_t i = 0; i < N; ++i) {
      auto* item = queue.Top();
      assert(item != nullptr);
      assert(*item == lap * N + i);
      queue.Pop();
    }
    assert(queue.Top() == nullptr);
  }
}

void publication_and_reuse() {
  struct Item { std::uint64_t sequence; std::uint64_t inverse; };
  SPSCQueue<Item, 2> queue;
  constexpr std::uint64_t count = 200000;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  auto wait = [&] {
    assert(std::chrono::steady_clock::now() < deadline);
    std::this_thread::yield();
  };
  std::thread producer([&] {
    for (std::uint64_t i = 0; i < count; ++i) {
      Item* slot;
      while (!(slot = static_cast<Item*>(queue.GetFreeSlot()))) wait();
      slot->sequence = i;
      if ((i & 127) == 0) std::this_thread::yield();
      slot->inverse = ~i;
      queue.Submit();
    }
  });
  std::thread consumer([&] {
    for (std::uint64_t i = 0; i < count; ++i) {
      Item* item;
      while (!(item = queue.Top())) wait();
      assert(item->sequence == i);
      if ((i & 127) == 0) std::this_thread::yield();
      assert(item->inverse == ~i);
      queue.Pop();
    }
  });
  producer.join();
  consumer.join();
  assert(queue.Top() == nullptr);
}

int main() {
  boundaries_and_fifo<1>();
  boundaries_and_fifo<2>();
  boundaries_and_fifo<4>();
  publication_and_reuse();
}
