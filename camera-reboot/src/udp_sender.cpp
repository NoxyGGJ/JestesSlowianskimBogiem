#include "udp_sender.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

static LONG g_wsaRefCount = 0;

static bool EnsureWsaStartup(int& outErr) {
    outErr = 0;
    if (InterlockedIncrement(&g_wsaRefCount) == 1) {
        WSADATA wsa{};
        const int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rc != 0) {
            outErr = rc;
            InterlockedDecrement(&g_wsaRefCount);
            return false;
        }
    }
    return true;
}

static void EnsureWsaCleanup() {
    if (InterlockedDecrement(&g_wsaRefCount) == 0) {
        WSACleanup();
    }
}

static std::string FormatWinSockError(int err) {
    if (err == 0) return {};
    LPSTR msg = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageA(flags, nullptr, (DWORD)err, 0, (LPSTR)&msg, 0, nullptr);
    std::string s;
    if (len && msg) {
        s.assign(msg, msg + len);
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    }
    if (msg) LocalFree(msg);
    return s;
}

UdpSender::UdpSender(uint16_t port, const char* ip) {
    // WSA
    if (!EnsureWsaStartup(lastErr_)) {
        return;
    }

    // socket
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        lastErr_ = WSAGetLastError();
        EnsureWsaCleanup();
        return;
    }

    // parse ip
    in_addr addr{};
    if (InetPtonA(AF_INET, ip, &addr) != 1) {
        lastErr_ = WSAGetLastError();
        ::closesocket(s);
        EnsureWsaCleanup();
        return;
    }

    sock_ = (void*)s;
    destAddr_ = addr.S_un.S_addr;          // already network order
    destPort_ = htons(port);
    lastErr_ = 0;
}

UdpSender::UdpSender(UdpSender&& other) noexcept {
    *this = std::move(other);
}

UdpSender& UdpSender::operator=(UdpSender&& other) noexcept {
    if (this == &other) return *this;

    // destroy current
    if (sock_) {
        ::closesocket((SOCKET)sock_);
        EnsureWsaCleanup();
    }

    sock_ = other.sock_;
    destAddr_ = other.destAddr_;
    destPort_ = other.destPort_;
    lastErr_ = other.lastErr_;

    other.sock_ = nullptr;
    other.destAddr_ = 0;
    other.destPort_ = 0;
    other.lastErr_ = 0;
    return *this;
}

UdpSender::~UdpSender() {
    if (sock_) {
        ::closesocket((SOCKET)sock_);
        sock_ = nullptr;
        EnsureWsaCleanup();
    }
}

bool UdpSender::isValid() const {
    return sock_ != nullptr;
}

int UdpSender::sendBytes(const void* data, size_t size) {
    if (!sock_) {
        lastErr_ = WSAENOTSOCK;
        return -1;
    }
    if (!data && size != 0) {
        lastErr_ = WSAEINVAL;
        return -1;
    }

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.S_un.S_addr = destAddr_;
    to.sin_port = destPort_;

    // sendto takes int length
    if (size > static_cast<size_t>(INT32_MAX)) {
        lastErr_ = WSAEMSGSIZE;
        return -1;
    }

    const int sent = ::sendto(
        (SOCKET)sock_,
        reinterpret_cast<const char*>(data),
        static_cast<int>(size),
        0,
        reinterpret_cast<const sockaddr*>(&to),
        sizeof(to)
    );

    if (sent == SOCKET_ERROR) {
        lastErr_ = WSAGetLastError();
        return -1;
    }

    lastErr_ = 0;
    return sent;
}

int UdpSender::sendText(const std::string& text) {
    return sendBytes(text.data(), text.size());
}

int UdpSender::lastError() const {
    return lastErr_;
}

std::string UdpSender::lastErrorMessage() const {
    return FormatWinSockError(lastErr_);
}
