#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Progress.H>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <sstream>
#include <stdint.h>
#include <unordered_map>

class ProgressWindow : public Fl_Double_Window {
public:
    ProgressWindow(uint64_t value, uint64_t size, std::function<void()> cancel_cb);

    float value() const {
        return progress->value();
    }

    void value(float value) {
        progress->value(value);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << value / progress->maximum() * 100 << "% (" << std::setprecision(2) << value / 1e+6f << '/' << progress->maximum() / 1e+6f << " MB)";
        progress->copy_label(ss.str().c_str());
    }

protected:
    Fl_Progress* progress;
};

struct IncomingTransfer {
    std::ofstream file;
    uint64_t size;
    std::atomic<uint64_t> received = 0;
    ProgressWindow* progress_window = nullptr;

    ~IncomingTransfer() {
        if (progress_window) {
            Fl::awake([](void* data) {
                Fl::delete_widget((Fl_Widget*) data);
            },
                progress_window);
        }
    }
};

struct OutgoingTransfer {
    std::ifstream file;
    uint64_t size;
    std::atomic<uint64_t> sent = 0;
    ProgressWindow* progress_window = nullptr;

    ~OutgoingTransfer() {
        if (progress_window) {
            Fl::awake([](void* data) {
                Fl::delete_widget((Fl_Widget*) data);
            },
                progress_window);
        }
    }
};

class FileManager {
protected:
    std::shared_ptr<rtc::DataChannel> channel;
    uint64_t chunk_size;

    std::mutex mutex;
    uint32_t transfer_id = 0;
    std::unordered_map<uint32_t, std::shared_ptr<IncomingTransfer>> incoming_transfers;
    std::unordered_map<uint32_t, std::shared_ptr<OutgoingTransfer>> outgoing_transfers;

    void on_buffered_amount_low();
    void on_binary_message(rtc::binary message);
    void on_string_message(rtc::string message);
    void cancel_incoming_transfer(uint32_t id, bool erase = true);
    void cancel_outgoing_transfer(uint32_t id, bool erase = true);

public:
    FileManager(std::shared_ptr<rtc::DataChannel> channel, uint64_t chunk_size = 16384):
        channel(std::move(channel)),
        chunk_size(chunk_size) {
        this->channel->setBufferedAmountLowThreshold(chunk_size * 2);
        this->channel->onBufferedAmountLow(std::bind(&FileManager::on_buffered_amount_low, this));
        this->channel->onMessage(std::bind(&FileManager::on_binary_message, this, std::placeholders::_1), std::bind(&FileManager::on_string_message, this, std::placeholders::_1));
    }

    ~FileManager();

    void upload();
    void download();
};
