// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

// mumlib
#include "mumlib2/exceptions.h"
#include "mumlib2_private/varint.h"

namespace mumlib2 {
    VarInt::VarInt(const uint8_t* buf)
    {
        parse(buf);
    }

    VarInt::VarInt(int8_t val) {
        _val = val;
    }

    VarInt::VarInt(int16_t val) {
        _val = val;
    }

    VarInt::VarInt(int32_t val)
    {
        _val = val;
    }

    VarInt::VarInt(uint32_t val)
    {
        _val = val;
    }

    VarInt::VarInt(int64_t val)
    {
        _val = val;
    }

    size_t VarInt::Size() const
    {
        return _size;
    }

    size_t VarInt::Value() const
    {
        return _val;
    }

    std::vector<uint8_t> VarInt::Encode() const
    {
        std::vector<uint8_t> result;

        if (_val < 0) {
            throw VarIntException("currently negative not supported");
        }

        //7 bit positive (0xxxxxxx)
        if (_val < 0x80) {
            result.reserve(1);
            result.push_back(_val & 0x7F);
            return result;
        }

        //14 bit positive (10xxxxxx + 1b)
        if (_val < 0x4000) {
            result.reserve(2);
            result.push_back(static_cast<uint8_t&&>(0x80 | (_val >> 8)));
            result.push_back(static_cast<uint8_t&&>(_val & 0xFF));
            return result;
        }

        //21 bit positive (110xxxxx + 2b)
        if (_val < 0x200000) {
            result.reserve(3);
            result.push_back(static_cast<uint8_t&&>(0xC0 | (_val >> 16)));
            result.push_back(static_cast<uint8_t&&>((_val >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t&&>(_val & 0xFF));
            return result;
        }

        //28 bit positive (1110xxxx + 3b)
        else if (_val < 0x10000000) {
            result.reserve(4);
            result.push_back(static_cast<uint8_t&&>(0xE0 | (_val >> 24)));
            result.push_back(static_cast<uint8_t&&>((_val >> 16) & 0xFF));
            result.push_back(static_cast<uint8_t&&>((_val >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t&&>(_val & 0xFF));
            return result;
        }

        //64 bit positive (111100__ + int64)
        result.reserve(9);
        result.push_back(0xF4);
        result.push_back((_val >> 56) & 0xFF);
        result.push_back((_val >> 48) & 0xFF);
        result.push_back((_val >> 40) & 0xFF);
        result.push_back((_val >> 32) & 0xFF);
        result.push_back((_val >> 24) & 0xFF);
        result.push_back((_val >> 16) & 0xFF);
        result.push_back((_val >> 8) & 0xFF);
        result.push_back((_val >> 0) & 0xFF);

        return result;
    }

    void VarInt::parse(const uint8_t* buf)
    {
        _val = buf[0];
        _size = 0;

        //7 bit positive (0xxxxxxx)
        if ((buf[0] & 0b1000'0000) == 0b0000'0000) {
            _val = _val & 0b0111'1111;
            _size = 1;
            return;
        }

        //14 bit positive (10xxxxxx + 1b)
        if ((buf[0] & 0b1100'0000) == 0b1000'0000) {
            _val = ((_val & 0b0011'1111) << 8) | buf[1];
            _size = 2;
            return;
        }

        //21 bit positive (110xxxxx + 2b)
        if ((buf[0] & 0b1110'0000) == 0b1100'0000) {
            _val = ((_val & 0b0001'1111) << 16) | (buf[1] << 8) | buf[2];
            _size = 3;
            return;
        }

        //28 bit positive (1110xxxx + 3b)
        if ((buf[0] & 0b1111'0000) == 0b1110'0000) {
            _val = ((_val & 0b0000'1111) << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
            _size = 4;
            return;
        }

        //32 bit positive (111100__ + int)
        if ((buf[0] & 0b1111'1100) == 0b1111'0000) {
            _val = __builtin_bswap64((uint64_t)*reinterpret_cast<const int32_t*>(buf + 1));
            _size = 5;
            return;
        }

        //64 bit positive (111100__ + int64)
        if ((buf[0] & 0b1111'1100) == 0b1111'0100) {
            _val = __builtin_bswap64((uint64_t)*reinterpret_cast<const int64_t*>(buf + 1));
            _size = 9;
            return;
        }

        //negative recursive
        if ((buf[0] & 0b1111'1100) == 0b1111'1000) {
            _val = 0;
            _size = 0;
            throw VarIntException("currently negative recursive aren't supported");
            return;
        }

        //negative two bit
        if ((buf[0] & 0b1111'1100) == 0b1111'1100) {
            _val = 0;
            _size = 0;
            throw VarIntException("currently negative twobits aren't supported");
            return;
        } 
    }
}
