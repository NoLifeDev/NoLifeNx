//////////////////////////////////////////////////////////////////////////////
// NoLifeNx - Part of the NoLifeStory project                               //
// Copyright (C) 2013 Peter Atashian                                        //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "NX.hpp"
#include "lz4.hpp"
#include <cstring>
#include <vector>
#ifdef NL_WINDOWS
#include <Windows.h>
#elif defined NL_POSIX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace NL {
#pragma region Structs
#pragma pack(push, 1)
    struct File::Header {
        uint32_t const magic;
        uint32_t const ncount;
        uint64_t const noffset;
        uint32_t const scount;
        uint64_t const soffset;
        uint32_t const bcount;
        uint64_t const boffset;
        uint32_t const acount;
        uint64_t const aoffset;
    };
    struct Node::Data {
        uint32_t const name;
        uint32_t const children;
        uint16_t const num;
        Type const type;
        union {
            int64_t const ireal;
            double const dreal;
            uint32_t const string;
            int32_t const vector[2];
            struct {
                uint32_t index;
                uint16_t width;
                uint16_t height;
            } const bitmap;
            struct {
                uint32_t index;
                uint32_t length;
            } const audio;
        };
    };
#pragma pack(pop)
#pragma endregion
#pragma region Bitmap
    Bitmap & Bitmap::operator=(Bitmap const & o) {
        d = o.d;
        w = o.w;
        h = o.h;
        return *this;
    }
    std::vector<uint8_t> buf;
    void const * Bitmap::Data() const {
        if (!d) return nullptr;
        size_t const l = Length() + 0x10;
        if (l > buf.size()) buf.resize(Length() + 0x10);
        LZ4::Uncompress(reinterpret_cast<uint8_t const *>(d) + 4, buf.data(), Length());
        return buf.data();
    }
    uint16_t Bitmap::Width() const {
        return w;
    }
    uint16_t Bitmap::Height() const {
        return h;
    }
    uint32_t Bitmap::Length() const {
        return w * h * 4;
    }
    size_t Bitmap::ID() const {
        return reinterpret_cast<size_t>(d);
    }
#pragma endregion
#pragma region Audio
    Audio & Audio::operator=(Audio const & o) {
        d = o.d;
        l = o.l;
        return *this;
    }
    void const * Audio::Data() const {
        return d;
    }
    uint32_t Audio::Length() const {
        return l;
    }
    Audio::operator bool() const {
        return d ? true : false;
    }
    bool Audio::operator==(Audio o) const {
        return d == o.d;
    }
#pragma endregion
#pragma region Node
    Node::Node() : d(nullptr), f(nullptr) {}
    Node::Node(Node const & o) : d(o.d), f(o.f) {}
    Node::Node(Node && o) : d(o.d), f(o.f) {}
    Node::Node(Data const * d, File const * f) : d(d), f(f) {}
    Node & Node::operator=(Node const & o) {
        d = o.d;
        f = o.f;
        return *this;
    }
    Node Node::begin() const {
        if (d) return Node(f->ntable + d->children, f);
        return Node();
    }
    Node Node::end() const {
        if (d) return Node(f->ntable + d->children + d->num, f);
        return Node();
    }
    Node Node::operator*() const {
        return *this;
    }
    Node & Node::operator++() {
        ++d;
        return *this;
    }
    Node Node::operator++(int) {
        return Node(d++, f);
    }
    bool Node::operator==(Node o) const {
        return d == o.d;
    }
    bool Node::operator!=(Node o) const {
        return d != o.d;
    }
    std::string operator+(std::string s, Node n) {
        s.append(n.GetString());
        return move(s);
    }
    std::string operator+(char const * s, Node n) {
        return s + n.GetString();
    }
    std::string Node::operator+(std::string s) const {
        s.insert(0, GetString());
        return move(s);
    }
    std::string Node::operator+(char const * s) const {
        return GetString() + s;
    }
    Node Node::operator[](std::string const & o) const {
        return GetChild(o.c_str(), o.length());
    }
    Node Node::operator[](char const * o) const {
        return GetChild(o, strlen(o));
    }
    Node Node::operator[](Node o) const {
        return operator[](o.GetString());
    }
    Node::operator int64_t () const {
        return static_cast<int64_t >(GetInt());
    }
    Node::operator uint64_t() const {
        return static_cast<uint64_t>(GetInt());
    }
    Node::operator int32_t () const {
        return static_cast<int32_t >(GetInt());
    }
    Node::operator uint32_t() const {
        return static_cast<uint32_t>(GetInt());
    }
    Node::operator int16_t () const {
        return static_cast<int16_t >(GetInt());
    }
    Node::operator uint16_t() const {
        return static_cast<uint16_t>(GetInt());
    }
    Node::operator int8_t  () const {
        return static_cast<int8_t  >(GetInt());
    }
    Node::operator uint8_t () const {
        return static_cast<uint8_t >(GetInt());
    }
    Node::operator double() const {
        return static_cast<double>(GetFloat());
    }
    Node::operator float() const {
        return static_cast<float>(GetFloat());
    }
    Node::operator std::string() const {
        return GetString();
    }
    Node::operator std::pair<int32_t, int32_t>() const {
        return GetVector();
    }
    Node::operator Bitmap() const {
        return GetBitmap();
    }
    Node::operator Audio() const {
        return GetAudio();
    }
    Node::operator bool() const {
        return d ? true : false;
    }
    int64_t Node::GetInt() const {
        if (d) switch (d->type) {
        case ireal: return ToInt();
        case dreal: return static_cast<int64_t>(ToFloat());
        case string: return std::stoll(ToString());
        }
        return 0;
    }
    double Node::GetFloat() const {
        if (d) switch (d->type) {
        case dreal: return ToFloat();
        case ireal: return static_cast<double>(ToInt());
        case string: return std::stod(ToString());
        }
        return 0;
    }
    std::string Node::GetString() const {
        if (d) switch (d->type) {
        case string: return ToString();
        case ireal: return std::to_string(ToInt());
        case dreal: return std::to_string(ToFloat());
        case vector: return "Vector";
        case bitmap: return "Bitmap";
        case audio: return "Audio";
        }
        return std::string();
    }
    std::pair<int32_t, int32_t> Node::GetVector() const {
        if (d) if (d->type == vector) return ToVector();
        return std::make_pair(0, 0);
    }
    Bitmap Node::GetBitmap() const {
        if (d) if (d->type == bitmap) return ToBitmap();
        return Bitmap();
    }
    Audio Node::GetAudio() const {
        if (d) if (d->type == audio) return ToAudio();
        return Audio();
    }
    bool Node::GetBool(bool def) const {
        if (d) if (d->type == ireal) return ToInt() ? true : false;
        return def; 
    }
    int32_t Node::X() const {
        if (d) if (d->type == vector) return d->vector[0];
        return 0;
    }
    int32_t Node::Y() const {
        if (d) if (d->type == vector) return d->vector[1];
        return 0;
    }
    std::string Node::Name() const {
        if (d) return f->GetString(d->name);
        return std::string();
    }
    size_t Node::Size() const {
        if (d) return d->num;
        return 0;
    }
    Node::Type Node::T() const {
        if (d) return d->type;
        return Type::none;
    }
    Node Node::GetChild(char const * o, size_t l) const {
        if (!d) return Node();
        Data const * p = f->ntable + d->children;
        size_t n = d->num;
        char const * const b = reinterpret_cast<const char *>(f->base);
        uint64_t const * const t = f->stable;
bloop:
        if (!n) return Node();
        size_t const n2 = n >> 1;
        Data const * const p2 = p + n2;
        char const * const sl = b + t[p2->name];
        size_t const l1 = *reinterpret_cast<uint16_t const *>(sl);
        uint8_t const * s = reinterpret_cast<uint8_t const *>(sl + 2);
        uint8_t const * os = reinterpret_cast<uint8_t const *>(o);
        for (size_t i = l1 < l ? l1 : l; i; --i, ++s, ++os) {
            if (*s > *os) goto greater;
            if (*s < *os) goto lesser;
        }
        if (l1 < l) goto lesser;
        if (l1 > l) goto greater;
        return Node(p2, f);
lesser:
        p = p2 + 1;
        n -= n2 + 1;
        goto bloop;
greater:
        n = n2;
        goto bloop;
    }
    int64_t Node::ToInt() const {
        return d->ireal;
    }
    double Node::ToFloat() const {
        return d->dreal;
    }
    std::string Node::ToString() const {
        return f->GetString(d->string);
    }
    std::pair<int32_t, int32_t> Node::ToVector() const {
        return std::make_pair(d->vector[0], d->vector[1]);
    }
    Bitmap Node::ToBitmap() const {
        return Bitmap(d->bitmap.width, d->bitmap.height, reinterpret_cast<char const *>(f->base) + f->btable[d->bitmap.index]);
    }
    Audio Node::ToAudio() const {
        return Audio(d->audio.length, reinterpret_cast<char const *>(f->base) + f->atable[d->audio.index]);
    }
#pragma endregion
#pragma region File
    File::File(char const * name) {
#ifdef NL_WINDOWS
        file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
        if (file == INVALID_HANDLE_VALUE) throw;
        map = CreateFileMappingA(file, 0, PAGE_READONLY, 0, 0, 0);
        if (!map) throw;
        base = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
        if (!base) throw;
#elif defined NL_POSIX
        file = open(name, O_RDONLY);
        if (file == -1) throw;
        struct stat finfo;
        if (fstat(file, &finfo) == -1) throw;
        size = finfo.st_size;
        base = mmap(nullptr, size, PROT_READ, MAP_SHARED, file, 0);
        if (reinterpret_cast<size_t>(base) == -1) throw;
#endif
        head = reinterpret_cast<Header const *>(base);
        if (head->magic != 0x34474B50) throw;
        ntable = reinterpret_cast<Node::Data const *>(reinterpret_cast<char const *>(base) + head->noffset);
        stable = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(base) + head->soffset);
        btable = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(base) + head->boffset);
        atable = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(base) + head->aoffset);
    }
    File::~File() {
#ifdef NL_WINDOWS
        UnmapViewOfFile(base);
        CloseHandle(map);
        CloseHandle(file);
#elif defined NL_POSIX
        munmap(const_cast<void *>(base), size);
        close(file);
#endif
    }
    Node File::Base() const {
        return Node(ntable, this);
    }
    uint32_t File::StringCount() const {
        return head->scount;
    }
    uint32_t File::BitmapCount() const {
        return head->bcount;
    }
    uint32_t File::AudioCount() const {
        return head->acount;
    }
    uint32_t File::NodeCount() const {
        return head->ncount;
    }
    std::string File::GetString(uint32_t i) const {
        char const * const s = reinterpret_cast<char const *>(base) + stable[i];
        return std::string(s + 2, *reinterpret_cast<uint16_t const *>(s));
    }
#pragma endregion
}
