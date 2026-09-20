struct Tick {
    uint64_t timestamp_us;
    double price;
    int64_t quantity;
};
//vwap = sum(qty_i * price_i) / sum(qty_i)
class VWAPWindow {
    double a {0};
    double b {0};
    uint64_t window_us_ {0};
    std::queue<Tick> queue;
public:
    VWAPWindow(uint64_t window_us): window_us_ (window_us);

    void add(const Tick& tick) {
        if (queue.empty()) {
            a += tick.price*tick.quantity;
            b += tick.quantity;
            queue.insert(tick);
            return;
        }
        Tick t = queue.front();
        while (tick.timestamp_us - t.timestamp_us > window_us_) {
            a -= t.price*t.quantity;
            b -= t.quantity;
            queue.pop();
            if (queue.empty()) {
                break;
            }
            t = queue.front();
        }
        a += tick.price*tick.quantity;
        b += tick.quantity;
        queue.insert(tick);
    }

    double vwap() const {
        return a / b;
    }
};