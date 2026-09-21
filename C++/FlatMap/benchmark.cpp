#ifndef FLATMAP_HEADER
#define FLATMAP_HEADER "FlatMap.h"
#endif
#include FLATMAP_HEADER
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>

using Clock = std::chrono::steady_clock;
void require(bool ok) {
    if (!ok) throw std::runtime_error("benchmark correctness check failed");
}

template<std::size_t Bytes> struct Block {
    std::array<int, Bytes / sizeof(int)> words{};
    explicit Block(int id) { words.fill(id); }
};
// Nontrivial copy/destruction and noexcept move; includes an 80-character allocation.
class Record {
    Block<256> data_;
    std::string label_;
public:
    explicit Record(int id) : data_(id), label_(80, static_cast<char>('a' + id % 26)) {}
    int id() const { return data_.words[0]; }
    std::uint64_t checksum() const {
        return std::accumulate(data_.words.begin(), data_.words.end(), std::uint64_t{})
             + std::accumulate(label_.begin(), label_.end(), std::uint64_t{});
    }
    void increment() { ++data_.words[0]; }
};
int id(int v) { return v; }
template<std::size_t B> int id(const Block<B>& v) { return v.words[0]; }
int id(const Record& v) { return v.id(); }
std::uint64_t checksum(int v) { return v; }
template<std::size_t B> std::uint64_t checksum(const Block<B>& v) {
    return std::accumulate(v.words.begin(), v.words.end(), std::uint64_t{});
}
std::uint64_t checksum(const Record& v) { return v.checksum(); }
void increment(int& v) { ++v; }
template<std::size_t B> void increment(Block<B>& v) { ++v.words[0]; }
void increment(Record& v) { v.increment(); }
struct Less {
    template<class T> bool operator()(const T& a, const T& b) const { return id(a) < id(b); }
};
template<class T> void consume(const T& value) { asm volatile("" : : "g"(value) : "memory"); }

template<class K, class V> void measure(const char* kn, const char* vn, int n, bool verify) {
    using Map = FlatMap<K, V, Less>;
    std::mt19937 rng(20260921);
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    auto report = [&](const char* op, auto start, std::uint64_t count) {
        const double ns = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        if (!verify) std::cout << kn << ',' << vn << ',' << sizeof(K) << ',' << sizeof(V)
            << ',' << n << ',' << op << ',' << count << ',' << ns / count << '\n';
    };
    Map map;
    require(map.begin() == map.end());
    require(!map.find(K(1)) && !map.erase(K(1)));
    std::uint64_t expected_keys = 0, expected_values = 0, expected_full = 0;
    for (int i = 0; i < n; ++i) {
        require(map.insert(K(2*i), V(2*i+1)));
        expected_keys += 2*i;
        expected_values += 2*i+1;
        expected_full += checksum(K(2*i)) + checksum(V(2*i+1));
    }
    require(!map.insert(K(0), V(999)));
    require(map.size() == static_cast<std::size_t>(n));
    require(map.end() - map.begin() == n);
    require(id(map.cend()[-1].first) == 2*(n-1));
    const int queries = verify ? 256 : 100000;
    std::vector<K> hits, misses;
    std::uint64_t expected_hits = 0;
    for (int i = 0; i < queries; ++i) {
        int key = 2 * static_cast<int>(rng() % n);
        hits.emplace_back(key); misses.emplace_back(key+1); expected_hits += key+1;
    }
    for (const auto& key : hits) consume(map.find(key));
    auto start = Clock::now();
    std::uint64_t sum = 0;
    for (const auto& key : hits) { const auto* p = map.find(key); consume(p); if(p) sum += id(*p); }
    report("find_hit", start, queries); require(sum == expected_hits);
    start = Clock::now(); sum = 0;
    for (const auto& key : misses) { const auto* p = map.find(key); consume(p); sum += p != nullptr; }
    report("find_miss", start, queries); require(sum == 0);

    const auto& read = map;
    const int repeats = verify ? 2 : std::max(4, 1048576 / n);
    // Barrier once per traversal: preserves repeated work without blocking inner-loop vectorization.
    auto traversal = [&](const char* name, auto body, std::uint64_t expected) {
        auto t = Clock::now(); std::uint64_t total = 0;
        for (int r = 0; r < repeats; ++r) { asm volatile("" ::: "memory"); auto s = body(); consume(s); total += s; }
        report(name, t, static_cast<std::uint64_t>(repeats)*n);
        require(total == expected * repeats);
    };
    traversal("iter_keys", [&] { std::uint64_t s=0; for(auto it=read.begin();it!=read.end();++it) s+=id(it->first); return s; }, expected_keys);
    traversal("iter_values", [&] { std::uint64_t s=0; for(auto it=read.begin();it!=read.end();++it) s+=id(it->second); return s; }, expected_values);
    traversal("iter_pairs", [&] { std::uint64_t s=0; for(auto [k,v]:read) s+=id(k)+id(v); return s; }, expected_keys+expected_values);
    traversal("iter_full", [&] { std::uint64_t s=0; for(auto [k,v]:read) s+=checksum(k)+checksum(v); return s; }, expected_full);
    traversal("iter_reverse", [&] { std::uint64_t s=0; for(auto it=read.rbegin();it!=read.rend();++it) s+=id(it->first)+id(it->second); return s; }, expected_keys+expected_values);
    traversal("iter_random", [&] { std::uint64_t s=0; auto it=read.begin(); for(int i:order) { auto p=it[i]; s+=id(p.first)+id(p.second); } return s; }, expected_keys+expected_values);
    start=Clock::now();
    for(int r=0;r<repeats;++r) { asm volatile("" ::: "memory"); for(auto it=map.begin();it!=map.end();++it) increment(it->second); }
    asm volatile("" ::: "memory");
    report("iter_mutate", start, static_cast<std::uint64_t>(repeats)*n);
    for(int i=0;i<n;++i) { auto p=read.begin()[i]; require(id(p.first)==2*i && id(p.second)==2*i+1+repeats); }

    // Quadratic mutations limited to N <= 4096, including large types.
    if(n>4096) return;
    int batches=verify ? 1 : std::clamp(8192/n, 1, 16);
    std::vector<Map> maps(batches);
    start=Clock::now(); sum=0;
    for(auto& target:maps) for(int i:order) sum+=target.insert(K(2*i), V(2*i+1));
    report("insert_random", start, static_cast<std::uint64_t>(batches)*n);
    require(sum==static_cast<std::uint64_t>(batches)*n);
    for(const auto& target:maps) for(int i:order) { auto* p=target.find(K(2*i)); require(p && checksum(*p)==checksum(V(2*i+1))); }
    start=Clock::now(); sum=0;
    for(auto& target:maps) for(int i:order) sum+=target.erase(K(2*i));
    report("erase_random", start, static_cast<std::uint64_t>(batches)*n);
    require(sum==static_cast<std::uint64_t>(batches)*n);
    for(auto& target:maps) require(target.empty() && target.begin()==target.end());
}
int main(int argc, char**) {
    const bool verify=argc>1;
    if(!verify) std::cout << "key_type,value_type,key_bytes,value_bytes,size,operation,operations,ns_per_op\n";
    for(int n : (verify ? std::vector<int>{1,17,256} : std::vector<int>{256,4096,16384})) {
        measure<int,int>("int","int",n,verify);
#define CASE(T, NAME) \
        measure<int,T>("int",NAME,n,verify); \
        measure<T,int>(NAME,"int",n,verify); \
        measure<T,T>(NAME,NAME,n,verify);
        CASE(Block<64>, "struct64")
        CASE(Block<1024>, "struct1024")
        CASE(Record, "class_record")
#undef CASE
    }
}
