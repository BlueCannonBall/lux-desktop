#include "file_manager.hpp"
#include "Polyweb/polyweb.hpp"
#include "json.hpp"
#include "util.hpp"
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_callback_macros.H>
#include <exception>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#ifdef _WIN32
    #include "theme.hpp"
    #include <FL/x.H>
#endif

using nlohmann::json;

constexpr auto PROGRESS_UPDATE_INTERVAL = std::chrono::milliseconds(250);

ProgressWindow::ProgressWindow(const std::string& path, uint64_t value, uint64_t size, std::function<void()> cancel_cb):
    Fl_Double_Window(400, 120, "File Transfer") {
    auto path_box = new Fl_Box(10, 10, w() - 20, 30);
    path_box->copy_label(("Copying " + path + "...").c_str());

    progress = new Fl_Progress(10, 45, w() - 20, 30);
    progress->selection_color(FL_SELECTION_COLOR);
    progress->maximum(size);
    this->value(value);

    auto cancel_button = new Fl_Button(w() - 85, h() - 40, 75, 30, "Cancel");
    FL_INLINE_CALLBACK_2(cancel_button, Fl_Double_Window*, window, this, std::function<void()>, cancel_cb, cancel_cb, {
        window->hide();
        cancel_cb();
    });

    FL_INLINE_CALLBACK_2(this, Fl_Double_Window*, window, this, std::function<void()>, cancel_cb, cancel_cb, {
        window->hide();
        cancel_cb();
    });

    end();
}

void ProgressWindow::value(float value) {
    progress->value(value);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << value / progress->maximum() * 100 << "% (" << std::setprecision(2) << value / 1e+6f << '/' << progress->maximum() / 1e+6f << " MB)";
    progress->copy_label(ss.str().c_str());
}

void FileManager::on_buffered_amount_low() {
    std::lock_guard<std::mutex> lock(mutex);
    while (channel->bufferedAmount() <= chunk_size * 2 && !outgoing_transfers.empty()) {
        for (auto transfer_it = outgoing_transfers.begin(); transfer_it != outgoing_transfers.end();) {
            rtc::binary message(chunk_size + 4);
            if (transfer_it->second->file.read((char*) message.data() + 4, chunk_size).bad() && !transfer_it->second->file.eof()) {
                uint32_t id = transfer_it->first;
                outgoing_transfers.erase(transfer_it);
                cancel_transfer(id);
                awake([id]() {
                    fl_alert("Error: File transfer #%" PRIu32 " failed", id);
                });
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
                if (auto now = std::chrono::steady_clock::now(); now - transfer_it->second->last_progress_update >= PROGRESS_UPDATE_INTERVAL) {
                    awake([weak_transfer = std::weak_ptr<OutgoingTransfer>(transfer_it->second)]() {
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window->value(transfer->sent);
                        }
                    });
                    transfer_it->second->last_progress_update = now;
                }
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
                uint32_t id = transfer_it->first;
                incoming_transfers.erase(transfer_it);
                cancel_transfer(id);
                awake([id]() {
                    fl_alert("Error: File transfer #%" PRIu32 " failed", id);
                });
                return;
            }
            transfer_it->second->received += message.size() - 4;

            if (transfer_it->second->progress_window) {
                if (auto now = std::chrono::steady_clock::now(); now - transfer_it->second->last_progress_update >= PROGRESS_UPDATE_INTERVAL) {
                    awake([weak_transfer = std::weak_ptr<IncomingTransfer>(transfer_it->second)]() {
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window->value(transfer->received);
                        }
                    });
                    transfer_it->second->last_progress_update = now;
                }
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
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window = new ProgressWindow(transfer->path, transfer->received, transfer->size, [this, id]() {
                                std::lock_guard<std::mutex> lock(mutex);
                                incoming_transfers.erase(id);
                                cancel_transfer(id);
                            });
                            transfer->progress_window->show();
#ifdef _WIN32
                            Fl::flush();
                            set_window_dark_mode(fl_xid(transfer->progress_window));
#endif
                        }
                    },
                        true);
                }
            } else {
                std::lock_guard<std::mutex> lock(mutex);
                if (auto transfer_it = outgoing_transfers.find(message_json["id"]); transfer_it != outgoing_transfers.end()) {
                    awake([this, id = transfer_it->first, weak_transfer = std::weak_ptr<OutgoingTransfer>(transfer_it->second)]() {
                        if (auto transfer = weak_transfer.lock()) {
                            transfer->progress_window = new ProgressWindow(transfer->path, transfer->sent, transfer->size, [this, id]() {
                                std::lock_guard<std::mutex> lock(mutex);
                                outgoing_transfers.erase(id);
                                cancel_transfer(id);
                            });
                            transfer->progress_window->show();
#ifdef _WIN32
                            Fl::flush();
                            set_window_dark_mode(fl_xid(transfer->progress_window));
#endif
                        }
                    },
                        true);

                    while (channel->bufferedAmount() <= chunk_size * 2) {
                        rtc::binary message(chunk_size + 4);
                        if (transfer_it->second->file.read((char*) message.data() + 4, chunk_size).bad() && !transfer_it->second->file.eof()) {
                            uint32_t id = transfer_it->first;
                            outgoing_transfers.erase(transfer_it);
                            cancel_transfer(id);
                            awake([id]() {
                                fl_alert("Error: File transfer #%" PRIu32 " failed", id);
                            });
                            break;
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
                            if (auto now = std::chrono::steady_clock::now(); now - transfer_it->second->last_progress_update >= PROGRESS_UPDATE_INTERVAL) {
                                awake([weak_transfer = std::weak_ptr<OutgoingTransfer>(transfer_it->second)]() {
                                    if (auto transfer = weak_transfer.lock()) {
                                        transfer->progress_window->value(transfer->sent);
                                    }
                                });
                                transfer_it->second->last_progress_update = now;
                            }
                        }

                        if (transfer_it->second->file.eof() || transfer_it->second->sent >= transfer_it->second->size) {
                            outgoing_transfers.erase(transfer_it);
                            break;
                        }
                    }
                }
            }
        } else if (message_json["type"] == "canceltransfer") {
            std::unique_lock<std::mutex> lock(mutex);
            if (message_json["id"] < transfer_id) {
                incoming_transfers.erase(message_json["id"]);
                outgoing_transfers.erase(message_json["id"]);
                awake([id = message_json["id"].get<uint32_t>()]() {
                    fl_alert("File transfer #%" PRIu32 " has been cancelled.", id);
                });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
    }
}

bool FileManager::cancel_transfer(uint32_t id) {
    json message = {
        {"type", "canceltransfer"},
        {"id", id},
    };
    return channel->send(message.dump());
}

FileManager::~FileManager() {
    channel->onMessage(nullptr, nullptr);
    channel->onBufferedAmountLow(nullptr);

    std::lock_guard<std::mutex> lock(mutex);
    for (auto& transfer : incoming_transfers) {
        if (transfer.second->progress_window) {
            awake([&transfer]() {
                delete transfer.second->progress_window;
                transfer.second->progress_window = nullptr;
            },
                true);
        }
        if (channel->isOpen()) cancel_transfer(transfer.first);
    }
    for (auto& transfer : outgoing_transfers) {
        if (transfer.second->progress_window) {
            awake([&transfer]() {
                delete transfer.second->progress_window;
                transfer.second->progress_window = nullptr;
            },
                true);
        }
        if (channel->isOpen()) cancel_transfer(transfer.first);
    }
}

void FileManager::upload() {
    if (const char* filename = fl_file_chooser("Choose File", nullptr, nullptr, 0); filename) {
        auto transfer = std::make_shared<OutgoingTransfer>();
        transfer->file.open((transfer->path = filename), std::ios::binary | std::ios::ate);
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
        auto transfer = std::make_shared<IncomingTransfer>();
        transfer->file.open((transfer->path = filename), std::ios::binary);
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
