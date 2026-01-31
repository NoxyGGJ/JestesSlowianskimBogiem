#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <cstddef>
#include <string>

class UdpSender {
public:
    // адрес по умолчанию: localhost (127.0.0.1)
    explicit UdpSender(uint16_t port, const char* ip = "127.0.0.1");

    // перемещение ок, копирование запрещено
    UdpSender(UdpSender&& other) noexcept;
    UdpSender& operator=(UdpSender&& other) noexcept;
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    ~UdpSender();

    // true если сокет готов к отправке
    bool isValid() const;

    // Отправка сырых байт. Возвращает кол-во отправленных байт, либо -1 при ошибке.
    int sendBytes(const void* data, size_t size);

    // Удобная отправка строки (без добавления '\0')
    int sendText(const std::string& text);

    // Код последней ошибки Winsock (WSAGetLastError), либо 0
    int lastError() const;

    // Человекочитаемая строка для lastError() (если есть)
    std::string lastErrorMessage() const;

private:
    void* sock_ = nullptr;         // SOCKET, но без включения winsock2.h в header публично
    uint32_t destAddr_ = 0;        // IPv4 in network byte order
    uint16_t destPort_ = 0;        // network byte order
    int lastErr_ = 0;
};
