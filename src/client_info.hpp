#pragma once
struct ClientInfo {
    std::string ip_addr;
    unsigned int port;
    unsigned int fd;
    unsigned int age;
    unsigned int idle;
    unsigned int flags;
    static constexpr SLAVE_MONITOR_MODE_MASK = 01;
    static constexpr SLAVE_NORMAL_MODE_MASK = 02;
    static constexpr MASTER_MODE_MASK = 04;
    static constexpr MULTI_MODE_MASK = 010;
    static constexpr WAIT_BLOCK_MASK = 020;
    static constexpr VM_IO_MASK = 040;
    static constexpr WATCHED_KEY_MODIFIED_MASK = 0100;
    static constexpr CLOSE_AFTER_WRITE_MASK = 0200;
    static constexpr UNBLOCKED_MASK = 0400;
    static constexpr CLOSED_ASAP_MASK = 01000;

};