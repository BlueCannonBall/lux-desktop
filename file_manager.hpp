#pragma once

#include "util.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Progress.H>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <stdint.h>
#include <string>
#include <unordered_map>

class ProgressWindow : public Fl_Double_Window {
public:
    ProgressWindow(const std::string& path, uint64_t value, uint64_t size, std::function<void()> cancel_cb);

    float value() const {
        return progress->value();
    }

    void value(float value);

protected:
    Fl_Progress* progress;
};

// progress_window is written on the main thread and read on libdatachannel's
// threads, so it has to be atomic
struct IncomingTransfer {
    std::ofstream file;
    std::string path;
    uint64_t size;
    std::atomic<uint64_t> received = 0;
    std::atomic<ProgressWindow*> progress_window = nullptr;
    std::chrono::steady_clock::time_point last_progress_update = std::chrono::steady_clock::now();

    ~IncomingTransfer() {
        if (auto window = progress_window.load()) {
            awake([window]() {
                Fl::delete_widget(window);
            });
        }
    }
};

struct OutgoingTransfer {
    std::ifstream file;
    std::string path;
    uint64_t size;
    std::atomic<uint64_t> sent = 0;
    std::atomic<bool> started = false; // Set once the peer has acknowledged the transfer
    std::atomic<ProgressWindow*> progress_window = nullptr;
    std::chrono::steady_clock::time_point last_progress_update = std::chrono::steady_clock::now();

    ~OutgoingTransfer() {
        if (auto window = progress_window.load()) {
            awake([window]() {
                Fl::delete_widget(window);
            });
        }
    }
};

class FileManager {
protected:
    std::shared_ptr<rtc::DataChannel> channel;

    // upload() and download() run a modal file chooser, which pumps the event
    // loop, which can destroy this FileManager along with the connection. They
    // hold a copy of this flag across the chooser to detect that
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    std::mutex mutex;
    uint64_t chunk_size;
    uint32_t transfer_id = 0;
    std::unordered_map<uint32_t, std::shared_ptr<IncomingTransfer>> incoming_transfers;
    std::unordered_map<uint32_t, std::shared_ptr<OutgoingTransfer>> outgoing_transfers;
    std::atomic<bool> buffered_amount_low_running = false;

    void on_buffered_amount_low();
    void on_binary_message(rtc::binary message);
    void on_string_message(rtc::string message);
    void cancel_transfer(uint32_t id);

public:
    FileManager(std::shared_ptr<rtc::DataChannel> channel, uint64_t chunk_size = 16384):
        channel(std::move(channel)),
        chunk_size(chunk_size) {
        this->channel->setBufferedAmountLowThreshold(256 * 1024);
        this->channel->onBufferedAmountLow(std::bind(&FileManager::on_buffered_amount_low, this));
        this->channel->onMessage(std::bind(&FileManager::on_binary_message, this, std::placeholders::_1), std::bind(&FileManager::on_string_message, this, std::placeholders::_1));
    }

    ~FileManager();

    void upload();
    void download();

    bool ready() const {
        return channel->isOpen();
    }
};
