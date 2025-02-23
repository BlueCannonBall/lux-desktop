#include "media_receiver.hpp"
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <string.h>

void MediaReceiver::incoming(rtc::message_vector& messages, const rtc::message_callback& send) {
    rtc::message_vector filtered;
    for (const auto& message : messages) {
        switch (message->type) {
        case rtc::Message::Binary: {
            if (message->size() < sizeof(rtc::RtpHeader)) {
                continue;
            }

            rtc::RtpHeader rtp_header;
            memcpy(&rtp_header, message->data(), sizeof(rtc::RtpHeader));
            if (rtp_header.version() != 2 || rtp_header.payloadType() == 200 || rtp_header.payloadType() == 201) {
                continue;
            }

            ssrc = rtp_header.ssrc();

            if (got_rtp_packets) {
                if (rtp_header.seqNumber() < last_seq_number && last_seq_number - rtp_header.seqNumber() > 32768) {
                    packets_lost += rtp_header.seqNumber();
                    ++seq_number_cycles;
                } else if (last_seq_number + 1 != rtp_header.seqNumber()) {
                    packets_lost = std::max<int>(packets_lost + ((int) rtp_header.seqNumber() - last_seq_number - 1), 0);
                }
            } else {
                got_rtp_packets = true;
                base_seq_number = rtp_header.seqNumber();
            }
            last_seq_number = rtp_header.seqNumber();

            filtered.push_back(message);
            break;
        }

        case rtc::Message::Control: {
            rtc::RtcpHeader rtcp_header;
            memcpy(&rtcp_header, message->data(), sizeof(rtc::RtcpHeader));
            if (rtcp_header.payloadType() == 201) {
                rtc::RtcpRr rr;
                memcpy(&rr, message->data(), sizeof(rtc::RtcpRr));
                ssrc = rr.senderSSRC();
            } else if (rtcp_header.payloadType() == 200) {
                rtc::RtcpSr sr;
                memcpy(&sr, message->data(), sizeof(rtc::RtcpSr));
                ssrc = sr.senderSSRC();
                last_sr_ntp_time = sr.ntpTimestamp();
                last_sr_time = std::chrono::steady_clock::now();
            }
            break;
        }

        default:
            break;
        }
    }
    messages.swap(filtered);

    std::chrono::steady_clock::time_point current_time;
    if (got_rtp_packets && (current_time = std::chrono::steady_clock::now()) - last_rr_time >= std::chrono::seconds(1)) {
        auto message = make_message(rtc::RtcpRr::SizeWithReportBlocks(1), rtc::Message::Control);

        uint32_t total_packets = last_seq_number + (unsigned int) seq_number_cycles * 65536 - base_seq_number;
        uint32_t last_sr_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(current_time - last_sr_time).count() * 1e-9 * 65536;

        // std::cout << "============= Receiver Report =============" << std::endl;
        // std::cout << "SSRC:                  " << ssrc << std::endl;
        // std::cout << "Total packets:         " << total_packets << std::endl;
        // std::cout << "Packets lost:          " << packets_lost << std::endl;
        // std::cout << "Last sequence #:       " << last_seq_number << std::endl;
        // std::cout << "Sequence # cycles:     " << seq_number_cycles << std::endl;
        // std::cout << "Last SR NTP timestamp: " << last_sr_ntp_time << std::endl;
        // std::cout << "Last SR delay:         " << last_sr_delay / 65536. << std::endl;
        // std::cout << "-------------------------------------------" << std::endl;

        rtc::RtcpRr rr;
        memcpy(&rr, message->data(), sizeof(rtc::RtcpRr));
        rr.preparePacket(ssrc, 1);
        rr.getReportBlock(0)->preparePacket(ssrc, packets_lost, total_packets, last_seq_number, seq_number_cycles, 0, last_sr_ntp_time, last_sr_delay);
        rr.getReportBlock(0)->setPacketsLost((double) packets_lost / total_packets * 255, packets_lost);
        memcpy(message->data(), &rr, sizeof(rtc::RtcpRr));

        last_rr_time = current_time;
        send(message);
    }
}

bool MediaReceiver::requestBitrate(unsigned int bitrate, const rtc::message_callback& send) {
    auto message = make_message(rtc::RtcpRemb::SizeWithSSRCs(1), rtc::Message::Control);

    rtc::RtcpRemb remb;
    memcpy(&remb, message->data(), sizeof(rtc::RtcpRemb));
    remb.preparePacket(ssrc, 1, bitrate);
    remb.setSsrc(0, ssrc);
    memcpy(message->data(), &remb, sizeof(rtc::RtcpRemb));

    send(message);
    return true;
}

bool MediaReceiver::requestKeyframe(const rtc::message_callback& send) {
    auto message = make_message(rtc::RtcpPli::Size(), rtc::Message::Control);

    rtc::RtcpPli pli;
    memcpy(&pli, message->data(), sizeof(rtc::RtcpPli));
    pli.preparePacket(ssrc);
    memcpy(message->data(), &pli, sizeof(rtc::RtcpPli));

    send(message);
    return true;
}
