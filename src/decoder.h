#ifndef _DECODER_H
#define _DECODER_H

#include <stdint.h>

#include "bitstream.h"

class Decoder {
  public:
    Decoder(Bitstream &stream);
    ~Decoder();

    int read_next_prop_i();

  private:
    bool eof;
    Bitstream *stream;

    int last_prop_i;
};

#endif
