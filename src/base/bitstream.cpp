/*
 * Danger from the Deep - Open source submarine simulation
 * Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "bitstream.hpp"

//#define DEBUG_WRITE
//#define DEBUG_READ_BYTE
//#define DEBUG_TO_BUFFER

uint8_t obitstream::bitmask[] = {0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};
uint8_t ibitstream::bitmask[] = {0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};

inline void obitstream::to_buffer(uint8_t bits, uint8_t len)
{
#ifdef DEBUG_TO_BUFFER
    std::cout << "old byte_pos: " << byte_pos << std::endl;
    std::cout << "old bit_pos: " << bit_pos << std::endl;

    std::cout << "len: " << (uint16_t) len << std::endl;
#endif
    buffer[byte_pos] |= (bits << (8 - len - bit_pos));
    bit_pos += len;

    if (bit_pos > 7) {
        bit_pos -= 8;
        byte_pos++;
        if (byte_pos >= buffer.size() - 1) {
            flush();
        }
    }
#ifdef DEBUG_TO_BUFFER
    std::cout << "new byte_pos: " << byte_pos << std::endl;
    std::cout << "new bit_pos: " << bit_pos << std::endl;
#endif
}

void obitstream::last_write()
{
    flush();
    if (bit_pos > 0) {
        outstream->write(reinterpret_cast<char*>((buffer).data()), 1);
    }
}

void obitstream::flush()
{
    outstream->write(reinterpret_cast<char*>((buffer).data()), byte_pos);
    buffer[0] = buffer[byte_pos];
    for (uint8_t i = 1; i < buffer.size(); i++) {
        buffer[i] = 0;
    }
    byte_pos = 0;
}

auto obitstream::write(uint8_t bits, uint8_t len) -> bool
{
#ifdef DEBUG_WRITE
    std::cout << "Write" << std::endl;
    std::cout << "bits: " << (uint16_t) bits << std::endl;
    std::cout << "len: " << (uint16_t) len << std::endl;
    std::cout << "bit_pos: " << bit_pos << std::endl;
#endif
    if (len < 1 || len > 8) {
        return false;
    }

    bits &= bitmask[len - 1];
#ifdef DEBUG_WRITE
    std::cout << "masked bits: " << (uint16_t) bits << std::endl;
#endif
    if ((len + bit_pos) > 8) { // need to split
#ifdef DEBUG_WRITE
        std::cout << "split" << std::endl;
#endif
        if (static_cast<uint16_t>(byte_pos + 1) >= buffer.size()) {
            flush();
        }

        uint8_t high_bits_len = 8 - bit_pos;
        uint8_t low_bits_len  = len - high_bits_len;
#ifdef DEBUG_WRITE
        std::cout << "high_bits_len: " << (uint16_t) high_bits_len << std::endl;
        std::cout << "low_bits_len: " << (uint16_t) low_bits_len << std::endl;
#endif
        uint8_t high_bits = ((bitmask[high_bits_len] << (len - high_bits_len)) & bits) >> (len - high_bits_len);
        uint8_t low_bits  = (bitmask[low_bits_len]) & bits;
#ifdef DEBUG_WRITE
        std::cout << "high_bits: " << (uint16_t) high_bits << std::endl;
        std::cout << "low_bits: " << (uint16_t) low_bits << std::endl;
        std::cout << std::endl;
#endif
        to_buffer(high_bits, high_bits_len);
        to_buffer(low_bits, low_bits_len);
    } else {
        to_buffer(bits, len);
    }
#ifdef DEBUG_WRITE
    std::cout << std::endl;
#endif
    return true;
}

auto obitstream::write(uint16_t bits, uint8_t len) -> bool
{
    if (len < 1 || len > 16) {
        return false;
    }

    uint8_t* high_byte;
    uint8_t* low_byte;

    if (IS_BENDIAN) {
        high_byte = reinterpret_cast<uint8_t*>(&bits);
        low_byte  = high_byte + 1;
    } else {
        low_byte  = reinterpret_cast<uint8_t*>(&bits);
        high_byte = low_byte + 1;
    }

    bool ret_val;
    if (len > 8) {
        ret_val = write(*high_byte, len - 8);
        ret_val &= write(*low_byte, len - (len - 8));
    } else {
        ret_val = write(*low_byte, len);
    }

    return ret_val;
}

ibitstream::ibitstream(std::istream* is, long bufsize)
    : byte_pos(0)
    , bit_pos(0)
    , end_pos(0)
    , buffer(bufsize, 0)
    , instream(is)
{
    if (buffer.size() < 4) {
        buffer.resize(4, 0);
    }

    instream->read(reinterpret_cast<char*>((buffer).data()), buffer.size());
    end_pos += instream->gcount() - 1;
}

inline void ibitstream::update_position(const uint8_t& len)
{
    bit_pos += len;
    if (bit_pos > 7) {
        bit_pos -= 8;
        byte_pos++;
    }
    // std::cout << "new bit pos: " << (uint16_t)bit_pos << std::endl;
    if ((byte_pos == end_pos) && (instream->tellg() >= 0)) {
        fill_buffer();
    }
}

inline void ibitstream::fill_buffer()
{
    buffer[0] = buffer[byte_pos];

    byte_pos = 0;

    instream->read(reinterpret_cast<char*>(&buffer[1]), buffer.size() - 1);
    end_pos = instream->gcount();
    // std::cout << "filled: " << end_pos << std::endl;
}

auto ibitstream::read_byte(uint8_t len) -> uint8_t
{
    uint8_t ret_val = 0;
#ifdef DEBUG_READ_BYTE
    std::cout << "Read Byte" << std::endl;
#endif
    if (len < 1 || len > 8) {
        return ret_val;
    }

#ifdef DEBUG_READ_BYTE
    std::cout << "len: " << (uint16_t) len << std::endl;
    std::cout << "byte_pos: " << (uint16_t) byte_pos << std::endl;
    std::cout << "bit_pos: " << (uint16_t) bit_pos << std::endl;
#endif

    if (len > (8 - bit_pos)) {
        uint8_t high_bits_len = 8 - bit_pos;
        uint8_t low_bits_len  = len - high_bits_len;

        uint8_t high_bits = (buffer[byte_pos] & bitmask[high_bits_len - 1]) << low_bits_len;
        uint8_t low_bits =
            (buffer[byte_pos + 1] & (bitmask[low_bits_len - 1] << (8 - low_bits_len))) >> (8 - low_bits_len);

        ret_val = high_bits | low_bits;
    } else {
        ret_val = (buffer[byte_pos] & (bitmask[len - 1] << ((8 - len) - bit_pos))) >> ((8 - len) - bit_pos);
    }

    update_position(len);
#ifdef DEBUG_READ_BYTE
    std::cout << "ret_val: " << (uint16_t) ret_val << std::endl;
#endif
    return ret_val;
}

auto ibitstream::read(uint8_t len) -> uint16_t
{
    uint8_t high_byte = 0;
    uint8_t low_byte  = 0;
#ifdef DEBUG_READ
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "Read" << std::endl;
    std::cout << "len: " << (uint16_t) len << std::endl;
#endif
    if (len > 8) {

        high_byte = read_byte(len - 8);
        low_byte  = read_byte(8);
#ifdef DEBUG_READ
        std::cout << "high_byte: " << (uint16_t) high_byte << std::endl;
        std::cout << "low_byte: " << (uint16_t) low_byte << std::endl;
#endif
        uint16_t ret_val = 0;
        if (IS_BENDIAN) {
#ifdef DEBUG_READ
            std::cout << "big endian" << std::endl;
#endif
            ret_val = low_byte;
            ret_val <<= 8;
            ret_val |= high_byte;
        } else {
#ifdef DEBUG_READ
            std::cout << "little endian" << std::endl;
#endif
            ret_val = high_byte;
            ret_val <<= 8;
            ret_val |= low_byte;
        }
#ifdef DEBUG_READ
        std::cout << "ret_val: " << ret_val << std::endl;
        std::cout << std::endl;
#endif
        return ret_val;
    }
    return static_cast<uint16_t>(read_byte(len));
}
