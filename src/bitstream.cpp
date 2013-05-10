#include "bitstream.h"

#include <iostream>

#include "debug.h"

Bitstream::Bitstream(const std::string &bytes) :
    position(0),
    end(bytes.length() * 8) {
  uint32_t *buffer = new uint32_t[(bytes.length() + 3) / 4 + 1];
  memcpy(buffer, bytes.c_str(), bytes.length());
  data = buffer;
}

Bitstream::~Bitstream() {
  delete[] data;
}

bool Bitstream::eof() const {
  return position >= end;
}

size_t Bitstream::get_end() const {
  return end;
}

size_t Bitstream::get_position() const {
  return position;
}

void Bitstream::set_position(size_t new_position) {
  position = new_position;
}

uint32_t Bitstream::get_bits(size_t n) {
  XASSERT(n <= 32, "Only 32 or fewer bits are supported.");
  XASSERT(end - position >= n, "Bitstream overflow %d - %d < %d", end, position, n);

  uint32_t a = data[position / 32];
  uint32_t b = data[(position + n - 1) / 32];

  uint32_t read = position & 31;

  a >>= read;
  b <<= 32 - read;

  uint32_t mask = (1 << n) - 1;
  uint32_t ret = (a | b) & mask;
  
  position += n;
  return ret;
}

void Bitstream::read_bits(void *buffer, size_t bit_length) {
  XASSERT(position + bit_length <= end, "Buffer will overflow: %d + %d > %d", position,
      bit_length, end);

  size_t remaining = bit_length;

  size_t i = 0;
  char *data = reinterpret_cast<char *>(buffer);

  while (remaining >= 8) {
    data[i++] = get_bits(8);
    remaining -= 8;
  }

  if (remaining > 0) {
    data[i++] = get_bits(remaining);
  }
}

void Bitstream::read_string(char *buffer, size_t size) {
  for (size_t i = 0; i < size - 1; ++i) {
    buffer[i] = (char) get_bits(8);

    if (buffer[i] == '\0') {
      return;
    }
  }

  buffer[size - 1] = '\0';
}

uint32_t Bitstream::read_var_35() {
  size_t read = 0;

  uint32_t value = 0;
  uint32_t got;
  do {
    got = get_bits(8);

    uint32_t lower = got & 0x7F;
    uint32_t upper = got >> 7;

    value |= lower << read;
    read += 7;
  } while ((got >> 7) && read < 35);

  return value;
}

uint32_t Bitstream::read_var_uint() {
  XASSERT(!eof(), "End stream.");

  uint32_t ret = get_bits(6);

  switch (ret & (16 | 32)) {
    case 16: ret = (ret & 15) | (get_bits(4) << 4); break;
    case 32: ret = (ret & 15) | (get_bits(8) << 4); break;
    case 48: ret = (ret & 15) | (get_bits(28) << 4); break;
  }

  return ret;
}

