#include "smart_ptr_under_test.hpp"
#include "test_harness.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace allocation_failure {

// Позволяет детерминированно проверить ошибку выделения control block.
inline thread_local bool fail_next = false;

}  // namespace allocation_failure

void* operator new(std::size_t size) {
    if (allocation_failure::fail_next) {
        allocation_failure::fail_next = false;
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

struct LifetimeProbe {
    static inline std::atomic<int> alive{0};
    static inline std::atomic<int> destroyed{0};

    int value = 42;

    LifetimeProbe() {
        alive.fetch_add(1, std::memory_order_relaxed);
    }

    virtual ~LifetimeProbe() {
        alive.fetch_sub(1, std::memory_order_relaxed);
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }

    static void resetCounters() {
        alive.store(0, std::memory_order_relaxed);
        destroyed.store(0, std::memory_order_relaxed);
    }
};

struct DerivedProbe : LifetimeProbe {
    int derived_value = 99;
};

struct Pair {
    int first = 10;
    int second = 20;
};

struct TestException : std::runtime_error {
    TestException() : std::runtime_error("expected test exception") {
    }
};

struct CountingDeleter {
    std::atomic<int>* calls = nullptr;

    void operator()(LifetimeProbe* pointer) const noexcept {
        calls->fetch_add(1, std::memory_order_relaxed);
        delete pointer;
    }
};

struct StatefulDeleter {
    int* observed_tag = nullptr;
    int tag = -1;

    void operator()(LifetimeProbe* pointer) const noexcept {
        if (pointer != nullptr) {
            if (observed_tag != nullptr) {
                *observed_tag = tag;
            }
            delete pointer;
        }
    }
};

struct SwapTrackingDeleter {
    int* deleted_value = nullptr;

    void operator()(int* pointer) const noexcept {
        if (pointer != nullptr) {
            *deleted_value = *pointer;
            delete pointer;
        }
    }
};

struct NullTrackingDeleter {
    int* null_calls = nullptr;
    int* pointer_calls = nullptr;

    void operator()(int* pointer) const noexcept {
        if (pointer == nullptr) {
            ++*null_calls;
            return;
        }
        ++*pointer_calls;
        delete pointer;
    }
};

struct ArrayTrackingDeleter {
    int* nullary_calls = nullptr;
    int* pointer_calls = nullptr;

    void operator()() const noexcept {
        ++*nullary_calls;
    }

    void operator()(int* pointer) const noexcept {
        if (pointer != nullptr) {
            ++*pointer_calls;
            delete[] pointer;
        }
    }
};

struct ArrayStatefulDeleter {
    int* observed_tag = nullptr;
    int* calls = nullptr;
    int tag = -1;

    void operator()(int* pointer) const noexcept {
        if (pointer != nullptr) {
            if (observed_tag != nullptr) {
                *observed_tag = tag;
            }
            if (calls != nullptr) {
                ++*calls;
            }
            delete[] pointer;
        }
    }
};

struct ArraySwapTrackingDeleter {
    int* deleted_value = nullptr;

    void operator()(int* pointer) const noexcept {
        if (pointer != nullptr) {
            *deleted_value = pointer[0];
            delete[] pointer;
        }
    }
};

struct ConstructedValue {
    int first;
    int second;

    ConstructedValue(int first_value, int second_value)
        : first(first_value), second(second_value) {
    }
};

template <class Pointer>
bool arrowOperatorWorks() {
    if constexpr (requires(const Pointer& pointer) { pointer.operator->(); }) {
        Pointer pointer(new ConstructedValue(5, 8));
        return pointer->first == 5 && pointer->second == 8;
    }
    return false;
}

template <class Pointer>
bool constArrayIndexingWorks() {
    if constexpr (requires(const Pointer& pointer) { pointer[0]; }) {
        const Pointer pointer(new int[2]{12, 24});
        return pointer[0] == 12 && pointer[1] == 24;
    }
    return false;
}

template <class Pointer>
bool statefulDeleterSwapWorks() {
    if constexpr (requires(Pointer& first, Pointer& second) {
                      first.swap(second);
                  }) {
        int first_deleted_value = 0;
        int second_deleted_value = 0;
        {
            Pointer first(
                new int(11), SwapTrackingDeleter{&first_deleted_value});
            Pointer second(
                new int(22), SwapTrackingDeleter{&second_deleted_value});
            first.swap(second);
        }
        return first_deleted_value == 11 && second_deleted_value == 22;
    }
    return false;
}

template <class Pointer>
bool arrayMoveAssignmentWorks() {
    // if constexpr (requires(Pointer& destination, Pointer&& source) {
    //                   { destination = std::move(source) } ->
    //                       std::same_as<Pointer&>;
    //               }) {
        int source_observed_tag = -1;
        int destination_observed_tag = -1;
        int source_calls = 0;
        int destination_calls = 0;
        bool source_empty = false;
        bool destination_owns_source = false;
        {
            Pointer source(
                new int[2]{4, 9},
                ArrayStatefulDeleter{
                    &source_observed_tag, &source_calls, 73});
            Pointer destination(
                new int[1]{11},
                ArrayStatefulDeleter{
                    &destination_observed_tag, &destination_calls, 31});
            auto* source_raw = source.get();

            destination = std::move(source);
            source_empty = !source;
            destination_owns_source = destination.get() == source_raw;
        }

        return source_empty && destination_owns_source &&
               destination_observed_tag == 31 && destination_calls == 1 &&
               source_observed_tag == 73 && source_calls == 1;
    // }
    // return false;
}

template <class Pointer>
bool arrayDeleterSwapWorks() {
    if constexpr (requires(Pointer& first, Pointer& second) {
                      first.swap(second);
                  }) {
        int first_deleted_value = 0;
        int second_deleted_value = 0;
        {
            Pointer first(
                new int[2]{11, 12},
                ArraySwapTrackingDeleter{&first_deleted_value});
            Pointer second(
                new int[2]{22, 23},
                ArraySwapTrackingDeleter{&second_deleted_value});
            first.swap(second);
        }
        return first_deleted_value == 11 && second_deleted_value == 22;
    }
    return false;
}

template <class T>
bool makeUniqueForwardsArguments() {
    // if constexpr (requires { makeUnique<T>(17, 29); }) {
        auto pointer = makeUnique<T>(17, 29);
        return pointer && pointer.get()->first == 17 &&
               pointer.get()->second == 29;
    // }
    // return false;
}

template <class BasePointer, class DerivedPointer>
bool convertingMoveWorks() {
    if constexpr (std::is_constructible_v<BasePointer, DerivedPointer&&>) {
        DerivedPointer derived(new DerivedProbe{});
        auto* raw = derived.get();
        BasePointer base(std::move(derived));
        return !derived && base.get() == raw && base.get()->value == 42;
    }
    return false;
}

struct ThreadState {
    static inline std::atomic<int> destroyed{0};
    std::atomic<int> visits{0};
    int immutable_value = 1234;

    ~ThreadState() {
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }
};

static_assert(!std::is_copy_constructible_v<UniquePtr<int>>);
static_assert(!std::is_copy_assignable_v<UniquePtr<int>>);
static_assert(std::is_move_constructible_v<UniquePtr<int>>);
static_assert(std::is_move_assignable_v<UniquePtr<int>>);
static_assert(std::is_copy_constructible_v<SharedPtr<int>>);
static_assert(std::is_copy_constructible_v<WeakPtr<int>>);

TEST_CASE("[basic][unique] default construction and observers") {
    UniquePtr<int> empty;
    REQUIRE(empty.get() == nullptr);
    REQUIRE(!empty);

    UniquePtr<int> pointer(new int(17));
    REQUIRE(pointer);
    REQUIRE_EQ(pointer.get(), &*pointer);
    REQUIRE_EQ(*pointer, 17);
}

TEST_CASE("[basic][unique] arrow operator accesses the managed object") {
    REQUIRE(arrowOperatorWorks<UniquePtr<ConstructedValue>>());
}

TEST_CASE("[basic][unique] move transfers exclusive ownership") {
    LifetimeProbe::resetCounters();
    {
        UniquePtr<LifetimeProbe> first(new LifetimeProbe{});
        auto* raw = first.get();

        UniquePtr<LifetimeProbe> second(std::move(first));
        REQUIRE(!first);
        REQUIRE_EQ(second.get(), raw);

        UniquePtr<LifetimeProbe> third;
        third = std::move(second);
        REQUIRE(!second);
        REQUIRE_EQ(third.get(), raw);
        REQUIRE_EQ(LifetimeProbe::alive.load(), 1);
    }
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[basic][unique] release reset and swap") {
    UniquePtr<int> first(new int(1));
    UniquePtr<int> second(new int(2));

    first.swap(second);
    REQUIRE_EQ(*first, 2);
    REQUIRE_EQ(*second, 1);

    int* released = first.release();
    REQUIRE(!first);
    REQUIRE_EQ(*released, 2);
    delete released;

    second.reset(new int(3));
    REQUIRE_EQ(*second, 3);
    second.reset();
    REQUIRE(!second);
}

TEST_CASE("[basic][unique] self move assignment keeps ownership valid") {
    UniquePtr<int> pointer(new int(31));
    int* raw = pointer.get();
    pointer = std::move(pointer);
    REQUIRE_EQ(pointer.get(), raw);
    REQUIRE_EQ(*pointer, 31);
}

TEST_CASE("[basic][unique] move assignment replaces existing ownership") {
    LifetimeProbe::resetCounters();
    bool source_empty = false;
    bool destination_owns_source = false;
    {
        UniquePtr<LifetimeProbe> source(new LifetimeProbe{});
        UniquePtr<LifetimeProbe> destination(new LifetimeProbe{});
        auto* source_raw = source.get();

        destination = std::move(source);
        source_empty = !source;
        destination_owns_source = destination.get() == source_raw;
    }

    REQUIRE(source_empty);
    REQUIRE(destination_owns_source);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 2);
}

TEST_CASE("[basic][unique] assigning an empty source destroys old ownership") {
    LifetimeProbe::resetCounters();
    auto* old_pointer = new LifetimeProbe{};
    int destroyed_during_assignment = 0;
    {
        UniquePtr<LifetimeProbe> source;
        UniquePtr<LifetimeProbe> destination(old_pointer);

        destination = std::move(source);
        destroyed_during_assignment = LifetimeProbe::destroyed.load();

        // Keep a broken implementation from leaking the probe while still
        // preserving the observation made immediately after assignment.
        if (destroyed_during_assignment == 0 && destination.get() != old_pointer) {
            delete old_pointer;
        }
    }

    REQUIRE_EQ(destroyed_during_assignment, 1);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
}

TEST_CASE("[basic][shared] construction copy move and use_count") {
    SharedPtr<int> empty;
    REQUIRE(!empty);
    REQUIRE_EQ(empty.use_count(), 0L);

    SharedPtr<int> first(new int(42));
    REQUIRE(first);
    REQUIRE_EQ(first.use_count(), 1L);

    SharedPtr<int> second = first;
    REQUIRE_EQ(first.get(), second.get());
    REQUIRE_EQ(first.use_count(), 2L);

    SharedPtr<int> third = std::move(second);
    REQUIRE(!second);
    REQUIRE_EQ(third.use_count(), 2L);
    REQUIRE_EQ(*third, 42);
}

TEST_CASE("[basic][shared] reset swap and destruction") {
    LifetimeProbe::resetCounters();
    {
        SharedPtr<LifetimeProbe> first(new LifetimeProbe{});
        SharedPtr<LifetimeProbe> second;
        second.swap(first);
        REQUIRE(!first);
        REQUIRE(second);

        first = second;
        REQUIRE_EQ(first.use_count(), 2L);
        second.reset();
        REQUIRE_EQ(first.use_count(), 1L);
        REQUIRE_EQ(LifetimeProbe::alive.load(), 1);
        first.reset();
    }
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[basic][shared] self copy and self move assignment are safe") {
    SharedPtr<int> pointer(new int(64));
    int* raw = pointer.get();

    pointer = pointer;
    REQUIRE_EQ(pointer.get(), raw);
    REQUIRE_EQ(pointer.use_count(), 1L);

    pointer = std::move(pointer);
    REQUIRE_EQ(pointer.get(), raw);
    REQUIRE_EQ(pointer.use_count(), 1L);
    REQUIRE_EQ(*pointer, 64);
}

TEST_CASE("[basic][weak] observation lock and expiration") {
    LifetimeProbe::resetCounters();
    WeakPtr<LifetimeProbe> weak;
    REQUIRE(weak.expired());
    REQUIRE_EQ(weak.use_count(), 0L);
    REQUIRE(!weak.lock());

    {
        SharedPtr<LifetimeProbe> owner(new LifetimeProbe{});
        weak = owner;
        REQUIRE(!weak.expired());
        REQUIRE_EQ(weak.use_count(), 1L);

        auto locked = weak.lock();
        REQUIRE(locked);
        REQUIRE_EQ(locked.get(), owner.get());
        REQUIRE_EQ(owner.use_count(), 2L);
    }

    REQUIRE(weak.expired());
    REQUIRE(!weak.lock());
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[basic][weak] weak copies do not extend object lifetime") {
    LifetimeProbe::resetCounters();
    WeakPtr<LifetimeProbe> first;
    WeakPtr<LifetimeProbe> second;
    {
        SharedPtr<LifetimeProbe> owner(new LifetimeProbe{});
        first = owner;
        second = first;
        REQUIRE_EQ(owner.use_count(), 1L);
        REQUIRE_EQ(first.use_count(), 1L);
        REQUIRE_EQ(second.use_count(), 1L);
    }
    REQUIRE(first.expired());
    REQUIRE(second.expired());
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[basic][exceptions][unique] stack unwinding destroys owned object") {
    LifetimeProbe::resetCounters();
    REQUIRE_THROWS_AS(([] {
        UniquePtr<LifetimeProbe> pointer(new LifetimeProbe{});
        throw TestException{};
    }()), TestException);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[basic][exceptions][shared] stack unwinding releases one owner") {
    LifetimeProbe::resetCounters();
    SharedPtr<LifetimeProbe> surviving(new LifetimeProbe{});
    REQUIRE_THROWS_AS(( [&surviving] {
        SharedPtr<LifetimeProbe> local = surviving;
        REQUIRE_EQ(local.use_count(), 2L);
        throw TestException{};
    }()), TestException);
    REQUIRE_EQ(surviving.use_count(), 1L);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 1);
    surviving.reset();
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][unique] custom deleter is stored and invoked once") {
    LifetimeProbe::resetCounters();
    std::atomic<int> deleter_calls{0};
    {
        UniquePtr<LifetimeProbe, CountingDeleter> pointer(
            new LifetimeProbe{}, CountingDeleter{&deleter_calls});
        REQUIRE(pointer);
    }
    REQUIRE_EQ(deleter_calls.load(), 1);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][unique] all specializations remain non-copyable") {
    using ArrayPointer = UniquePtr<int[]>;
    using CustomPointer = UniquePtr<LifetimeProbe, CountingDeleter>;

    REQUIRE(!std::is_copy_constructible_v<ArrayPointer>);
    REQUIRE(!std::is_copy_assignable_v<ArrayPointer>);
    REQUIRE(!std::is_copy_constructible_v<CustomPointer>);
    REQUIRE(!std::is_copy_assignable_v<CustomPointer>);
}

TEST_CASE("[advanced][unique] move construction transfers deleter state") {
    LifetimeProbe::resetCounters();
    int observed_tag = -1;
    bool source_empty = false;
    {
        UniquePtr<LifetimeProbe, StatefulDeleter> source(
            new LifetimeProbe{}, StatefulDeleter{&observed_tag, 73});
        UniquePtr<LifetimeProbe, StatefulDeleter> destination(std::move(source));
        source_empty = !source;

        // A shallow copy would otherwise leave two owners of the same object.
        if (!source_empty) {
            static_cast<void>(source.release());
        }
    }

    REQUIRE(source_empty);
    REQUIRE_EQ(observed_tag, 73);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][unique] swap exchanges stateful deleters") {
    using Pointer = UniquePtr<int, SwapTrackingDeleter>;
    REQUIRE(statefulDeleterSwapWorks<Pointer>());
}

TEST_CASE("[advanced][unique] empty ownership never invokes the deleter") {
    int null_calls = 0;
    int pointer_calls = 0;
    {
        UniquePtr<int, NullTrackingDeleter> pointer(
            nullptr, NullTrackingDeleter{&null_calls, &pointer_calls});
        pointer.reset();
    }

    REQUIRE_EQ(null_calls, 0);
    REQUIRE_EQ(pointer_calls, 0);
}

TEST_CASE("[advanced][unique] array specialization uses delete array") {
    LifetimeProbe::resetCounters();
    {
        UniquePtr<LifetimeProbe[]> array(new LifetimeProbe[3]);
        array[1].value = 77;
        REQUIRE_EQ(array[1].value, 77);
        REQUIRE_EQ(LifetimeProbe::alive.load(), 3);
    }
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 3);
}

TEST_CASE("[advanced][unique] array move transfers exclusive ownership") {
    UniquePtr<int[]> source(new int[2]{4, 9});
    auto* raw = source.get();
    UniquePtr<int[]> destination(std::move(source));

    const bool source_empty = !source;
    const bool destination_owns_source = destination.get() == raw;

    // Avoid double deletion if a broken specialization performs a shallow copy.
    int* source_pointer = source.release();
    int* destination_pointer = destination.release();
    if (source_pointer == destination_pointer) {
        delete[] source_pointer;
    } else {
        delete[] source_pointer;
        delete[] destination_pointer;
    }

    REQUIRE(source_empty);
    REQUIRE(destination_owns_source);
}

TEST_CASE("[advanced][unique] array move construction transfers deleter state") {
    int observed_tag = -1;
    int calls = 0;
    bool source_empty = false;
    {
        UniquePtr<int[], ArrayStatefulDeleter> source(
            new int[2]{4, 9}, ArrayStatefulDeleter{&observed_tag, &calls, 73});
        UniquePtr<int[], ArrayStatefulDeleter> destination(std::move(source));
        source_empty = !source;
    }

    REQUIRE(source_empty);
    REQUIRE_EQ(observed_tag, 73);
    REQUIRE_EQ(calls, 1);
}

TEST_CASE("[advanced][unique] array move assignment transfers ownership and deleter") {
    using Pointer = UniquePtr<int[], ArrayStatefulDeleter>;
    REQUIRE(arrayMoveAssignmentWorks<Pointer>());
}

TEST_CASE("[advanced][unique] array swap exchanges pointers and deleters") {
    using Pointer = UniquePtr<int[], ArraySwapTrackingDeleter>;
    REQUIRE(arrayDeleterSwapWorks<Pointer>());
}

TEST_CASE("[advanced][unique] array reset deletes the previous allocation") {
    int nullary_calls = 0;
    int pointer_calls = 0;
    int pointer_calls_during_reset = 0;
    auto* old_pointer = new int[2]{1, 2};
    {
        UniquePtr<int[], ArrayTrackingDeleter> pointer(
            old_pointer, ArrayTrackingDeleter{&nullary_calls, &pointer_calls});
        pointer.reset(new int[3]{3, 4, 5});
        pointer_calls_during_reset = pointer_calls;

        // Clean up an allocation missed by an incorrect nullary deleter call.
        if (pointer_calls_during_reset == 0) {
            delete[] old_pointer;
        }
    }

    REQUIRE_EQ(nullary_calls, 0);
    REQUIRE_EQ(pointer_calls_during_reset, 1);
    REQUIRE_EQ(pointer_calls, 2);
}

TEST_CASE("[advanced][unique] const array supports indexing") {
    REQUIRE(constArrayIndexingWorks<UniquePtr<int[]>>());
}

TEST_CASE("[advanced][unique] makeUnique forwards constructor arguments") {
    REQUIRE(makeUniqueForwardsArguments<ConstructedValue>());
}

TEST_CASE("[advanced][unique] converting move supports polymorphism") {
    LifetimeProbe::resetCounters();
    const bool move_worked = convertingMoveWorks<
        UniquePtr<LifetimeProbe>, UniquePtr<DerivedProbe>>();
    REQUIRE(move_worked);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][shared] converting copies share one control block") {
    LifetimeProbe::resetCounters();
    {
        SharedPtr<DerivedProbe> derived(new DerivedProbe{});
        SharedPtr<LifetimeProbe> base = derived;
        WeakPtr<LifetimeProbe> weak = derived;
        REQUIRE_EQ(base.get(), derived.get());
        REQUIRE_EQ(base.use_count(), 2L);
        REQUIRE_EQ(weak.use_count(), 2L);
    }
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][shared] custom deleter is invoked once") {
    LifetimeProbe::resetCounters();
    std::atomic<int> deleter_calls{0};
    {
        SharedPtr<LifetimeProbe> first(
            new LifetimeProbe{}, CountingDeleter{&deleter_calls});
        auto second = first;
        first.reset();
        REQUIRE_EQ(deleter_calls.load(), 0);
        second.reset();
    }
    REQUIRE_EQ(deleter_calls.load(), 1);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][shared][weak] aliasing preserves the subobject pointer") {
    SharedPtr<Pair> owner(new Pair{});
    SharedPtr<int> alias(owner, &owner->second);
    WeakPtr<int> weak_alias(alias);

    REQUIRE_EQ(owner.use_count(), 2L);
    owner.reset();
    REQUIRE_EQ(alias.use_count(), 1L);
    REQUIRE_EQ(*alias, 20);

    auto locked = weak_alias.lock();
    REQUIRE_EQ(locked.get(), alias.get());
    *locked = 55;
    REQUIRE_EQ(*alias, 55);
}

TEST_CASE("[advanced][weak] construct shared from live weak pointer") {
    SharedPtr<int> owner(new int(8));
    WeakPtr<int> weak(owner);
    SharedPtr<int> second(weak);
    REQUIRE_EQ(second.get(), owner.get());
    REQUIRE_EQ(owner.use_count(), 2L);
}

TEST_CASE("[advanced][exceptions][weak] expired weak constructor throws") {
    WeakPtr<int> weak;
    {
        SharedPtr<int> owner(new int(8));
        weak = owner;
    }
    REQUIRE(weak.expired());
    REQUIRE_THROWS_AS(SharedPtr<int>(weak), std::bad_weak_ptr);
    REQUIRE(!weak.lock());
}

TEST_CASE("[advanced][exceptions][shared] allocation failure deletes raw pointer") {
    LifetimeProbe::resetCounters();
    auto* raw = new LifetimeProbe{};

    bool caught = false;
    allocation_failure::fail_next = true;
    try {
        SharedPtr<LifetimeProbe> pointer(raw);
        static_cast<void>(pointer);
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    allocation_failure::fail_next = false;

    REQUIRE(caught);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[advanced][exceptions][shared] allocation failure invokes custom deleter") {
    LifetimeProbe::resetCounters();
    std::atomic<int> deleter_calls{0};
    auto* raw = new LifetimeProbe{};

    bool caught = false;
    allocation_failure::fail_next = true;
    try {
        SharedPtr<LifetimeProbe> pointer(
            raw, CountingDeleter{&deleter_calls});
        static_cast<void>(pointer);
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    allocation_failure::fail_next = false;

    REQUIRE(caught);
    REQUIRE_EQ(deleter_calls.load(), 1);
    REQUIRE_EQ(LifetimeProbe::alive.load(), 0);
    REQUIRE_EQ(LifetimeProbe::destroyed.load(), 1);
}

TEST_CASE("[threads][shared] reference count tolerates concurrent copies") {
    constexpr int thread_count = 8;
    constexpr int iterations = 25'000;

    ThreadState::destroyed.store(0, std::memory_order_relaxed);
    SharedPtr<ThreadState> owner(new ThreadState{});
    WeakPtr<ThreadState> weak(owner);
    std::atomic<bool> start{false};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([copy = owner, &start, &errors] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < iterations; ++i) {
                SharedPtr<ThreadState> first = copy;
                SharedPtr<ThreadState> second;
                second = first;
                SharedPtr<ThreadState> third = std::move(second);
                if (!third || third->immutable_value != 1234) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
                third->visits.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    owner.reset();
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(errors.load(), 0);
    REQUIRE(weak.expired());
    REQUIRE_EQ(ThreadState::destroyed.load(), 1);
}

TEST_CASE("[threads][shared][weak] lock races with last owners releasing") {
    constexpr int holder_count = 4;
    constexpr int locker_count = 4;
    constexpr int lock_attempts = 40'000;

    ThreadState::destroyed.store(0, std::memory_order_relaxed);
    SharedPtr<ThreadState> owner(new ThreadState{});
    WeakPtr<ThreadState> weak(owner);
    std::atomic<bool> release{false};
    std::atomic<int> invalid_values{0};
    std::vector<std::thread> threads;
    threads.reserve(holder_count + locker_count);

    for (int i = 0; i < holder_count; ++i) {
        threads.emplace_back([holder = owner, &release]() mutable {
            while (!release.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            holder.reset();
        });
    }

    for (int i = 0; i < locker_count; ++i) {
        threads.emplace_back([observer = weak, &release, &invalid_values] {
            while (!release.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int attempt = 0; attempt < lock_attempts; ++attempt) {
                auto locked = observer.lock();
                if (locked && locked->immutable_value != 1234) {
                    invalid_values.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    owner.reset();
    release.store(true, std::memory_order_release);
    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(invalid_values.load(), 0);
    REQUIRE(weak.expired());
    REQUIRE(!weak.lock());
    REQUIRE_EQ(ThreadState::destroyed.load(), 1);
}

}  // namespace

int main(int argc, char** argv) {
    return test_harness::run(argc, argv);
}
