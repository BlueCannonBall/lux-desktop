#pragma once

#include <atomic>
#include <chrono>
#include <rtc/rtc.hpp>

class MediaReceiver : public rtc::MediaHandler {
protected:
    uint32_t ssrc = 0;
    unsigned int packets_lost = 0;
    uint16_t last_seq_number = 0;
    uint16_t seq_number_cycles = 0;
    uint64_t last_sr_ntp_time = 0;

    bool got_rtp_packets = false;
    uint16_t base_seq_number = 0;
    unsigned int prev_packets_lost = 0;
    unsigned int prev_total_packets = 0;
    std::chrono::steady_clock::time_point last_sr_time;
    std::chrono::steady_clock::time_point last_rr_time = std::chrono::steady_clock::now();

    std::atomic<unsigned int> requested_bitrate = 0;

public:
    void incoming(rtc::message_vector& messages, const rtc::message_callback& send) override;
    bool requestBitrate(unsigned int bitrate, const rtc::message_callback& send) override;
    bool requestKeyframe(const rtc::message_callback& send) override;
};