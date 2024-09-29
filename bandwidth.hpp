#pragma once

#include <algorithm>
#include <stddef.h>
#include <stdint.h>

class BandwidthEstimator {
protected:
    size_t data_received = 0;
    uint16_t packets_lost = 0;
    uint16_t last_seq_number = 0;
    int last_empirical_bitrate;
    int last_optimistic_bitrate;

public:
    BandwidthEstimator(int starting_bitrate = 4000):
        last_empirical_bitrate(starting_bitrate),
        last_optimistic_bitrate(starting_bitrate) {}

    void update(size_t size, uint16_t seq_number) {
        data_received += size;
        if (seq_number != last_seq_number + 1) {
            ++packets_lost;
        }
        last_seq_number = seq_number;
    }

    // Given the interval since last call (in seconds), returns a new bitrate value
    int estimate(double interval) {
        int ret = data_received * 8 / interval / 1000.;
        if (!packets_lost && last_empirical_bitrate - ret < 250) {
            last_empirical_bitrate = ret;
            ret = last_optimistic_bitrate + 150;
        } else {
            last_empirical_bitrate = ret;
            ret = std::max(last_optimistic_bitrate - std::max((last_optimistic_bitrate - 2000) / 3, 150), ret);
        }
        ret = std::min(std::max(ret, 2000), 7000);
        last_optimistic_bitrate = ret;
        data_received = 0;
        packets_lost = 0;
        return ret;
    }
};
