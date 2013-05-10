#ifndef _BITSTREAM_H
#define _BITSTREAM_H

#include <cstring>
#include <stdint.h>
#include <string>

class Bitstream {
  public:
    Bitstream(const std::string &bytes);
    ~Bitstream();

    bool eof() const;

    size_t get_end() const;
    size_t get_position() const;
    void set_position(size_t new_position);

    uint32_t get_bits(size_t n);
    void read_bits(void *buffer, size_t bit_length);
    void read_string(char *buffer, size_t size);
    uint32_t read_var_35();
    uint32_t read_var_uint();

  private:
    const uint32_t *data;

    size_t position;
    size_t end;
};

#endif
