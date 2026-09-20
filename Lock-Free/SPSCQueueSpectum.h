template <typename T, size_t N>
class SPSCQueue {
  static_assert(!N & (N-1));
  std::array<T, N> buffer_;
  alignas(64) std::atomic<size_t> head_ {0};
  alignas(64) std::atomic<size_t> tail_ {0};
public:
    // Producer side
    void *GetFreeSlot() {
      size_t head = head_.load(std::memory_order::relaxed);
      size_t tail = tail_.load(std::memory_order::relaxed);
      if (head - tail == N) {
        return nullptr;
      }
      return &buffer[tail & (N-1)];
    }

    void Submit() {
      size_t i = tail_.load(std::memory_order::relaxed);
      i++;
      tail_.store(i, std::memory_order::release);
    }

    // Consumer side
    T* Top() {
      size_t head = head_.load(std::memory_order::relaxed);
      size_t tail = tail_.load(std::memory_order::aquire);
      if (head == tail) {
        return nullptr;
      }
      return &buffer_[head & (N-1)]);
    }

    void Pop() {
      size_t i = head_.load(std::memory_order::relaxed);
      i++;
      head_.store(i, std::memory_order::relaxed);
    }
};
