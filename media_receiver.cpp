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
                if (rtp_header.seqNumber() < last_seq_number) {
                    if (last_seq_number - rtp_header.seqNumber() > 32768) {
                        packets_lost += rtp_header.seqNumber();
                        last_seq_number = rtp_header.seqNumber();
                        ++seq_number_cycles;
                    }
                } else {
                    packets_lost += (int) rtp_header.seqNumber() - last_seq_number - 1;
                    last_seq_number = rtp_header.seqNumber();
                }
            } else {
                got_rtp_packets = true;
                last_seq_number = rtp_header.seqNumber();
                base_seq_number = rtp_header.seqNumber();
            }

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

                if (unsigned int bitrate = requested_bitrate) {
                    auto message = make_message(rtc::RtcpRemb::SizeWithSSRCs(1), rtc::Message::Control);
                    rtc::RtcpRemb remb;
                    memcpy(&remb, message->data(), sizeof(rtc::RtcpRemb));
                    remb.preparePacket(ssrc, 1, bitrate);
                    remb.setSsrc(0, ssrc);
                    memcpy(message->data(), &remb, sizeof(rtc::RtcpRemb));

                    send(message);
                }
            }
            break;
        }

        default:
            break;
        }
    }
    messages.swap(filtered);

    if (std::chrono::steady_clock::time_point current_time; got_rtp_packets && (current_time = std::chrono::steady_clock::now()) - last_rr_time >= std::chrono::seconds(1)) {
        unsigned int total_packets = last_seq_number + (unsigned int) seq_number_cycles * 65536 - base_seq_number + 1;
        uint32_t last_sr_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(current_time - last_sr_time).count() * 1e-9 * 65536;

        auto message = make_message(rtc::RtcpRr::SizeWithReportBlocks(1), rtc::Message::Control);
        rtc::RtcpRr rr;
        memcpy(&rr, message->data(), sizeof(rtc::RtcpRr));
        rr.preparePacket(ssrc, 1);
        rr.getReportBlock(0)->preparePacket(ssrc, 0, 0, last_seq_number, seq_number_cycles, 0, last_sr_ntp_time, last_sr_delay);
        rr.getReportBlock(0)->setPacketsLost(std::max<int>((double) (packets_lost - prev_packets_lost) / (total_packets - prev_total_packets) * 255, 0), std::max(packets_lost, 0));
        memcpy(message->data(), &rr, sizeof(rtc::RtcpRr));

        prev_packets_lost = packets_lost;
        prev_total_packets = total_packets;
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

    requested_bitrate = bitrate;
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
