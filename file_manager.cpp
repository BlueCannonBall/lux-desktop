#include "file_manager.hpp"
#include "Polyweb/polyweb.hpp"
#include "json.hpp"
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_callback_macros.H>
#include <exception>
#include <inttypes.h>
#include <iostream>
#include <type_traits>
#include <utility>
#ifdef _WIN32
    #include "theme.hpp"
    #include <FL/x.H>
#endif

using nlohmann::json;

template <typename F>
void awake(F&& handler) {
    Fl::awake([](void* data) {
        auto handler = (std::decay_t<F>*) data;
        (*handler)();
        delete handler;
    },
        new std::decay_t<F>(std::forward<F>(handler)));
}

ProgressWindow::ProgressWindow(uint64_t value, uint64_t size, std::function<void()> cancel_cb):
    Fl_Double_Window(300, 85, "File Transfer") {
    progress = new Fl_Progress(10, 10, w() - 20, 30);
    progress->selection_color(FL_SELECTION_COLOR);
    progress->maximum(size);
    this->value(value);

    auto cancel_button = new Fl_Button(w() - 85, h() - 40, 75, 30, "Cancel");
    FL_INLINE_CALLBACK_2(cancel_button, Fl_Window*, window, this, std::function<void()>, cancel_cb, cancel_cb, {
        window->hide();
        cancel_cb();
    });

    FL_INLINE_CALLBACK_2(this, Fl_Window*, window, this, std::function<void()>, cancel_cb, cancel_cb, {
        window->hide();
        cancel_cb();
    });

    end();
}

void FileManager::on_buffered_amount_low() {
    std::lock_guard<std::mutex> lock(mutex);
    while (channel->bufferedAmount() <= chunk_size * 2) {
        for (auto transfer_it = outgoing_transfers.begin(); transfer_it != outgoing_transfers.end();) {
            rtc::binary message(chunk_size + 4);
            if (transfer_it->second->file.read((char*) message.data() + 4, chunk_size).fail()) {
                transfer_it = outgoing_transfers.erase(transfer_it);
                cancel_outgoing_transfer(transfer_it->first, false);
                continue;
            }
            message.resize(transfer_it->second->file.gcount() + 4);
#if BYTE_ORDER == BIG_ENDIAN
            memcpy(message.data(), &transfer_it->first, 4);
#else
            pw::reverse_memcpy(message.data(), &transfer_it->first, 4);
#endif
            channel->send(std::move(message));
            transfer_it->second->sent += transfer_it->second->file.gcount();

            if (transfer_it->second->progress_window) {
                awake([weak_transfer = std::weak_ptr<OutgoingTransfer>(transfer_it->second)]() {
                    if (auto transfer = weak_transfer.lock()) {
                        transfer->progress_window->value(transfer->sent);
                    }
                });
            }

            if (transfer_it->second->file.eof() || transfer_it->second->sent >= transfer_it->second->size) {
                transfer_it = outgoing_transfers.erase(transfer_it);
                continue;
            }

            ++transfer_it;
        }
    }
}

void FileManager::on_binary_message(rtc::binary message) {
    if (message.size() > 4) {
        uint32_t id;
#if BYTE_ORDER == BIG_ENDIAN
        memcpy(&id, message.data(), 4);
#else
        pw::reverse_memcpy(&id, message.data(), 4);
#endif

        std::lock_guard<std::mutex> lock(mutex);
        if (auto transfer_it = incoming_transfers.find(id); transfer_it != incoming_transfers.end()) {
            if (transfer_it->second->file.write((const char*) message.data() + 4, message.size() - 4).fail()) {
                cancel_incoming_transfer(id);
                return;
            }
            transfer_it->second->received += message.size() - 4;

            if (transfer_it->second->progress_window) {
                awake([weak_transfer = std::weak_ptr<IncomingTransfer>(transfer_it->second)]() {
                    if (auto transfer = weak_transfer.lock()) {
                        transfer->progress_window->value(transfer->received);
                    }
                });
            }

            if (transfer_it->second->received >= transfer_it->second->size) {
                transfer_it = incoming_transfers.erase(transfer_it);
                return;
            }
        }
    }
}

void FileManager::on_string_message(rtc::string message) {
    try {
        json message_json = json::parse(message);
        if (message_json["type"] == "transferready") {
            if (message_json.contains("size")) {
                std::lock_guard<std::mutex> lock(mutex);
                if (auto transfer_it = incoming_transfers.find(message_json["id"]); transfer_it != incoming_transfers.end()) {
                    transfer_it->second->size = message_json["size"];
                    awake([this, id = transfer_it->first, weak_transfer = std::weak_ptr<IncomingTransfer>(transfer_it->second)]() {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window = new ProgressWindow(transfer->received, transfer->size, [this, id]() {
                                mutex.lock();
                                incoming_transfers.erase(id);
                                mutex.unlock();
                            });
                            transfer->progress_window->show();
#ifdef _WIN32
                            Fl::flush();
                            set_window_dark_mode(fl_xid(transfer->progress_window));
#endif
                        }
                    });
                }
            } else {
                std::lock_guard<std::mutex> lock(mutex);
                if (auto transfer_it = outgoing_transfers.find(message_json["id"]); transfer_it != outgoing_transfers.end()) {
                    awake([this, id = transfer_it->first, weak_transfer = std::weak_ptr<OutgoingTransfer>(transfer_it->second)]() {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window = new ProgressWindow(transfer->sent, transfer->size, [this, id]() {
                                mutex.lock();
                                outgoing_transfers.erase(id);
                                mutex.unlock();
                            });
                            transfer->progress_window->show();
#ifdef _WIN32
                            Fl::flush();
                            set_window_dark_mode(fl_xid(transfer->progress_window));
#endif

                            while (channel->bufferedAmount() <= chunk_size * 2) {
                                rtc::binary message(chunk_size + 4);
                                if (transfer->file.read((char*) message.data() + 4, chunk_size).fail()) {
                                    cancel_outgoing_transfer(id);
                                    break;
                                }
                                message.resize(transfer->file.gcount() + 4);
#if BYTE_ORDER == BIG_ENDIAN
                                memcpy(message.data(), &id, 4);
#else
                                pw::reverse_memcpy(message.data(), &id, 4);
#endif
                                channel->send(std::move(message));
                                transfer->sent += transfer->file.gcount();

                                transfer->progress_window->value(transfer->sent);

                                if (transfer->file.eof() || transfer->sent >= transfer->size) {
                                    outgoing_transfers.erase(id);
                                    break;
                                }
                            }
                        }
                    });
                }
            }
        } else if (message_json["type"] == "canceltransfer") {
            std::unique_lock<std::mutex> lock(mutex);
            if (message_json["id"] < transfer_id) {
                incoming_transfers.erase(message_json["id"]);
                outgoing_transfers.erase(message_json["id"]);
                lock.unlock();

                awake([id = message_json["id"].get<uint32_t>()]() {
                    fl_alert("File transfer #%" PRIu32 " has been cancelled.", id);
                });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
    }
}

void FileManager::cancel_incoming_transfer(uint32_t id, bool erase) {
    if (erase) incoming_transfers.erase(id);
    awake([id]() {
        fl_alert("Error: File transfer #%" PRIu32 " failed", id);
    });
}

void FileManager::cancel_outgoing_transfer(uint32_t id, bool erase) {
    if (erase) outgoing_transfers.erase(id);
    awake([id]() {
        fl_alert("Error: File transfer #%" PRIu32 " failed", id);
    });
}

FileManager::~FileManager() {
    if (channel) {
        channel->onMessage(nullptr, nullptr);
        channel->onBufferedAmountLow(nullptr);

        if (channel->isOpen()) {
            for (const auto& transfer : incoming_transfers) {
                json message = {
                    {"type", "canceltransfer"},
                    {"id", transfer.first},
                };
                channel->send(message.dump());
            }

            for (const auto& transfer : outgoing_transfers) {
                json message = {
                    {"type", "canceltransfer"},
                    {"id", transfer.first},
                };
                channel->send(message.dump());
            }
        }
    }
}

void FileManager::upload() {
    if (const char* filename = fl_file_chooser("Choose File", nullptr, nullptr, 0); filename) {
        auto transfer = std::make_unique<OutgoingTransfer>();
        transfer->file.open(filename, std::ios::binary | std::ios::ate);
        if (!transfer->file.is_open()) {
            fl_alert("Error: Failed to open file");
            return;
        }

        uint64_t size = transfer->size = transfer->file.tellg();
        transfer->file.seekg(0, std::ios::beg);

        mutex.lock();
        uint32_t id = transfer_id++;
        outgoing_transfers[id] = std::move(transfer);
        mutex.unlock();

        json message = {
            {"type", "requesttransfer"},
            {"id", id},
            {"size", size},
        };
        channel->send(message.dump());
    }
}

void FileManager::download() {
    if (const char* filename = fl_file_chooser("Save File", nullptr, nullptr, 0); filename) {
        auto transfer = std::make_unique<IncomingTransfer>();
        transfer->file.open(filename, std::ios::binary);
        if (!transfer->file.is_open()) {
            fl_alert("Error: Failed to open file");
            return;
        }

        mutex.lock();
        uint32_t id = transfer_id++;
        incoming_transfers[id] = std::move(transfer);
        mutex.unlock();

        json message = {
            {"type", "requesttransfer"},
            {"id", id},
        };
        channel->send(message.dump());
    }
}
