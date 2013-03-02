#ifndef _VALUE_H
#define _VALUE_H

#include "bitstream.h"

class SendProp;

class Value {
public:
  Value(const SendProp *prop);

  void read_from(Bitstream &stream);

private:
  const SendProp *prop;
};

#endif

