#pragma once

#include <string>
#include <optional>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

namespace demo_daemon::ipc {

/**
 * @brief Статус сессии соединения
 */
enum class SessionState {
    Active,      ///< Сессия активна, принимает команды
    Reading,     ///< Чтение данных из сокета
    Writing,     ///< Запись данных в сокет
    Closing,     ///< Сессия закрывается
    Closed       ///< Сессия закрыта
};

/**
 * @brief Буфер для чтения/записи данных
 */
struct IOBuffer {
    std::vector<char> data;
    size_t read_pos{0};
    size_t write_pos{0};
    
    IOBuffer() : data(4096) {}
    explicit IOBuffer(size_t initial_size) : data(initial_size) {}
    
    [[nodiscard]] size_t available_for_read() const {
        return write_pos - read_pos;
    }
    
    [[nodiscard]] size_t available_for_write() const {
        return data.size() - write_pos;
    }
    
    void compact() {
        if (read_pos > 0) {
            std::memmove(data.data(), data.data() + read_pos, write_pos - read_pos);
            write_pos -= read_pos;
            read_pos = 0;
        }
    }
    
    void clear() {
        read_pos = 0;
        write_pos = 0;
    }
    
    [[nodiscard]] std::string_view get_read_view() const {
        return std::string_view(data.data() + read_pos, available_for_read());
    }
    
    char* get_write_ptr() {
        return data.data() + write_pos;
    }
    
    void advance_write(size_t bytes) {
        write_pos += bytes;
    }
    
    void advance_read(size_t bytes) {
        read_pos += bytes;
    }
};

} // namespace demo_daemon::ipc
