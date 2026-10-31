#pragma once
// FEng packages as data: the chunks of a menu package, parsed and written back (see quit_item.cpp)
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace eon::fng {

inline uint32_t Tag(const char* t) {
    uint32_t v;
    std::memcpy(&v, t, 4);
    return v;
}
inline uint32_t Rd(const std::vector<uint8_t>& b, size_t at) {
    uint32_t v;
    std::memcpy(&v, &b[at], 4);
    return v;
}
inline void Put(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    out.insert(out.end(), b, b + 4);
}

// a record of an ObjD or MsgR body: two letters, a 16 bit length, the value
inline void Record(std::vector<uint8_t>& out, const char* name, uint32_t v) {
    out.push_back(uint8_t(name[0]));
    out.push_back(uint8_t(name[1]));
    out.push_back(4);
    out.push_back(0);
    Put(out, v);
}

// FEng chunks: a tag, a length, the body; a tag whose last byte is 0x80 or more holds chunks
struct Chunk {
    uint32_t tag;
    std::vector<uint8_t> body;
    std::vector<Chunk> kids;
    bool container() const { return (tag >> 24) >= 0x80; }
};
inline std::vector<Chunk> Parse(const uint8_t* p, size_t n) {
    std::vector<Chunk> out;
    size_t at = 0;
    while (at + 8 <= n) {
        Chunk c;
        uint32_t size;
        std::memcpy(&c.tag, p + at, 4);
        std::memcpy(&size, p + at + 4, 4);
        if (at + 8 + size > n) break;
        if (c.container())
            c.kids = Parse(p + at + 8, size);
        else
            c.body.assign(p + at + 8, p + at + 8 + size);
        out.push_back(std::move(c));
        at += 8 + size;
    }
    return out;
}
inline void Write(const Chunk& c, std::vector<uint8_t>& out) {
    std::vector<uint8_t> body;
    if (c.container())
        for (const Chunk& k : c.kids) Write(k, body);
    else
        body = c.body;
    Put(out, c.tag);
    Put(out, uint32_t(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

// an object's ObjD: its Ot record's words (type, name, guid, name, flags, order, parent, string, font for text)
inline size_t OtWord(const Chunk& objd, int word) {
    size_t p = 0;
    while (p + 4 <= objd.body.size()) {
        size_t len = objd.body[p + 2] | objd.body[p + 3] << 8;
        if (objd.body[p] == 'O' && objd.body[p + 1] == 't') return p + 4 + size_t(word) * 4;
        p += 4 + len;
    }
    return SIZE_MAX;
}

inline uint32_t GuestString(const char* s) {
    size_t n = std::strlen(s) + 1;
    uint32_t va = mem::AllocSystem(uint32_t(n));
    if (va) std::memcpy(kernel::HostPtr(va), s, n);
    return va;
}

}  // namespace eon::fng
