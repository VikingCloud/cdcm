//===========================================================================
// VikingCloud, Inc. @{SRCH}
//								zmq_message.cpp
//
//---------------------------------------------------------------------------
// DESCRIPTION: @{HDES}
// -----------
//---------------------------------------------------------------------------
// CHANGES LOG: @{HREV}
// -----------
// Revision: 01.00
// By      : Assaf Cohen
// Date    : 26 Jun 2019
// Comments:
#include "zmq_message.hpp"
#include <algorithm> // for copy
#include <cassert> // for assert
#include <cstring> // for memcpy, size_t, strlen, NULL
#include <functional> // for function
#include <iomanip> // for operator<<, setfill, setw
#include <iostream> // for operator<<, basic_ostream, basic_ostre...
#include <zmq.h> // for ZMQ_SNDMORE
#include <zmq.hpp> // for message_t, error_t, socket_t
namespace std {
    void for_each_n(...) {}
} // namespace std

namespace trustwave {
    constexpr bool is_defined()
    {
        auto l = [](int) {};
        using ftype = decltype(std::for_each_n(std::begin({0}), 1, l));
        return std::is_same<ftype, void>();
    }

    template<typename InputIt, typename Size, typename UnaryFunction>
    InputIt for_each_n(InputIt first, Size n, UnaryFunction f)
    {
        if constexpr(!is_defined()) {
            return std::for_each_n(first, n, f);
        }
        for(Size i = 0; i < n; ++first, (void)++i) {
            f(*first);
        }
        return first;
    }

} // namespace trustwave

namespace {
    static constexpr uint16_t uuid_str_len = 17;
    static constexpr uint16_t uuid_binary_len = 34;

    //  --------------------------------------------------------------------------
    //  Formats 17-byte UUID as 33-char string starting with '@'
    //  Lets us print UUIDs as C strings and use them as addresses
    //
    std::array<char, uuid_binary_len> encode_uuid(const unsigned char* data)
    {
        static constexpr char hex_char[] = "0123456789ABCDEF";

        assert(data[0] == 0);

        std::array<char, uuid_binary_len> uuidstr = {0};
        uuidstr[0] = '@';
        uuidstr[uuid_binary_len - 1] = 0;

        for(int byte_nbr = 0; byte_nbr < 16; byte_nbr++) {
            uuidstr[byte_nbr * 2 + 1] = hex_char[data[byte_nbr + 1] >> 4];
            uuidstr[byte_nbr * 2 + 2] = hex_char[data[byte_nbr + 1] & 15];
        }
        return uuidstr;
    }

    std::array<char, uuid_str_len> decode_uuid(char* uuidstr)
    {
        static constexpr char hex_to_bin[128]
            = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* */
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* */
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* */
               0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, /* 0..9 */
               -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* A..F */
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* */
               -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* a..f */
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; /* */

        assert(strlen(uuidstr) == uuid_binary_len - 1);
        assert(uuidstr[0] == '@');
        std::array<char, uuid_str_len> data = {0};
        int byte_nbr;
        data[0] = 0;
        for(byte_nbr = 0; byte_nbr < uuid_str_len - 1; byte_nbr++)
            data[byte_nbr + 1]
                = (hex_to_bin[uuidstr[byte_nbr * 2 + 1] & 127] << 4) + (hex_to_bin[uuidstr[byte_nbr * 2 + 2] & 127]);
        return data;
    }
} // namespace

//  --------------------------------------------------------------------------
//  Constructor, sets initial body
zmsg::zmsg(char const* body) { body_set(body); }

//  --------------------------------------------------------------------------
//  Copy Constructor, equivalent to zmsg_dup
zmsg::zmsg(const zmsg& msg)
{
    m_part_data.resize(msg.m_part_data.size());
    std::copy(msg.m_part_data.begin(), msg.m_part_data.end(), m_part_data.begin());
}

//  --------------------------------------------------------------------------
//  Erases all messages
void zmsg::clear() noexcept { m_part_data.clear(); }

bool zmsg::recv(zmq::socket_t& socket)
{
    clear();
    while(true) {
        zmq::message_t message(0);
        try {
            if(!socket.recv(&message, 0)) {
                return false;
            }
        }
        catch(zmq::error_t& error) {
            return false;
        }
        if(message.size() == uuid_str_len && reinterpret_cast<unsigned char*>((message.data()))[0] == 0) {
            auto uuidstr = encode_uuid(reinterpret_cast<unsigned char*>(message.data()));
            emplace_back(uuidstr.data());
        }
        else {
            m_part_data.emplace_back(reinterpret_cast<unsigned char*>(message.data()), message.size());
        }
        if(!message.more()) {
            break;
        }
    }
    return true;
}

void zmsg::send(zmq::socket_t& socket)
{
    for(size_t part_nbr = 0; part_nbr < m_part_data.size(); part_nbr++) {
        zmq::message_t message;
        ustring data = m_part_data[part_nbr];
        if(data.size() == uuid_binary_len - 1 && data[0] == '@') {
            auto uuidbin = decode_uuid(const_cast<char*>(reinterpret_cast<const char*>(data.c_str())));
            message.rebuild(uuid_str_len);
            memcpy(message.data(), uuidbin.data(), uuid_str_len);
        }
        else {
            message.rebuild(data.size());
            memcpy(message.data(), data.c_str(), data.size());
        }
        try {
            socket.send(message, part_nbr < m_part_data.size() - 1 ? ZMQ_SNDMORE : 0);
        }
        catch(zmq::error_t& error) {
            assert(error.num() != 0);
        }
    }
    clear();
}

size_t zmsg::parts() { return m_part_data.size(); }

void zmsg::body_set(const char* body)
{
    if(!m_part_data.empty()) {
        m_part_data.erase(m_part_data.end() - 1);
    }
    emplace_back(body);
}

char* zmsg::body()
{
    if(!m_part_data.empty()) {
        return (const_cast<char*>(reinterpret_cast<const char*>(m_part_data[m_part_data.size() - 1].c_str())));
    }
    return nullptr;
}

// zmsg_push
void zmsg::push_front(const char* part)
{
    m_part_data.insert(m_part_data.begin(), reinterpret_cast<const unsigned char*>(part));
}

// zmsg_append
zmsg::ustring& zmsg::emplace_back(const char* part)
{
    return m_part_data.emplace_back(reinterpret_cast<const unsigned char*>(part));
}

// zmsg_pop
zmsg::ustring zmsg::pop_front()
{
    if(m_part_data.empty()) {
        return ustring();
    }
    ustring part = m_part_data.front();
    m_part_data.erase(m_part_data.begin());
    return part;
}

void zmsg::append(const char* part)
{
    assert(part);
    emplace_back(const_cast<char*>(part));
}

char* zmsg::address()
{
    if(!m_part_data.empty()) {
        return reinterpret_cast<char*>(const_cast<unsigned char*>(m_part_data[0].c_str()));
    }
    return nullptr;
}

void zmsg::wrap(const char* address, const char* delim)
{
    if(nullptr != delim) {
        push_front(delim);
    }
    push_front(address);
}

std::string zmsg::unwrap()
{
    if(m_part_data.empty()) {
        return std::string();
    }
    std::string addr = reinterpret_cast<const char*>(pop_front().c_str());
    if(nullptr != address() && *address() == 0) {
        pop_front();
    }
    return addr;
}

void zmsg::dump()
{
    std::cerr << "--------------------------------------" << std::endl;
    for(const auto& data: m_part_data) {
        // Dump the message as text or binary
        bool is_text = true;
        for(unsigned char char_nbr: data) {
            if(char_nbr < 32 || char_nbr > 127) {
                is_text = false;
            }
        }

        std::cerr << "[" << std::setw(3) << std::setfill('0') << data.size() << "] ";
        for(unsigned char char_nbr: data) {
            if(is_text) {
                std::cerr << static_cast<char>(char_nbr);
            }
            else {
                std::cerr << std::hex << std::setw(2) << std::setfill(' ') << static_cast<uint16_t>(char_nbr);
            }
        }
        std::cerr << std::endl;
    }
}

std::string zmsg::to_str(bool with_header, bool with_body, bool full)
{
    static constexpr size_t non_full_output_len = 30;
    if(!with_header && !with_body && !full) {
        return std::string();
    }
    auto check_is_text = [](const ustring& data) -> bool {
        for(unsigned char char_nbr: data) {
            if(char_nbr < 32 || char_nbr > 127) {
                return false;
            }
        }
        return true;
    };
    std::stringstream ss;
    using printer = std::function<void(unsigned char c)>;
    printer text_print = [&](unsigned char c) -> void { ss << (char)c; };

    printer binary_print
        = [&](unsigned char c) -> void { ss << std::hex << std::setw(2) << std::setfill('0') << (int16_t)c; };
    auto part_print = [&](const ustring& data) {
        bool is_text = check_is_text(data);
        ss << "[" << std::setw(3) << std::setfill('0') << data.size() << "] ";
        trustwave::for_each_n(data.begin(), full ? data.size() : std::max(non_full_output_len, data.size()),
                              is_text ? text_print : binary_print);
        ss << std::endl;
    };
    if(with_header) {
        trustwave::for_each_n(m_part_data.begin(), m_part_data.size() - 1, part_print);
    }
    if(with_body) {
        part_print(m_part_data[m_part_data.size() - 1]);
    }

    return ss.str();
}
