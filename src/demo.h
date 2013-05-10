#ifndef _DEMO_H
#define _DEMO_H

#include <stdint.h>

#include <fstream>

#include "demo.pb.h"

#define DEMO_BUFFER_SIZE (512 * 1024)

class Demo {
  public:
    Demo(const char *file);
    ~Demo();

    bool eof();
    EDemoCommands get_message_type(int *tick, bool *compressed);
    void read_message(bool compressed, size_t *size, size_t *uncompressed_size);

    char *expose_buffer();
    size_t get_buffer_len();

  private:
    char buffer[DEMO_BUFFER_SIZE];
    size_t buffer_len;
    char scratch[DEMO_BUFFER_SIZE];
    size_t scratch_len;

    std::ifstream stream;
};

#endif
