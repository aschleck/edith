#include "decoder.h"

#include "debug.h"

#define MAX_DATATABLE_PROPS 4096

Decoder::Decoder(Bitstream &_stream) : eof(false), stream(&_stream), last_prop_i(-1) {
}

Decoder::~Decoder() {
}

int Decoder::read_next_prop_i() {
  XASSERT(!eof, "Already read last prop i.");

  if (!stream->get_bits(1)) {
    eof = true;
    return -1;
  }

  int prop = stream->read_var_uint() + 1 + last_prop_i;
  last_prop_i = prop;

  XASSERT(0 <= prop && prop <= MAX_DATATABLE_PROPS, "Prop index %d is out of bounds.", prop);

  return prop;
}

