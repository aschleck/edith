#include "property.h"

#include <cmath>

#define MAX_STRING_LENGTH 0x200

uint32_t read_int(Bitstream &stream, const SendProp *prop) {
  if (prop->flags & SP_EncodedAgainstTickcount) {
    if (prop->flags & SP_Unsigned) {
      return stream.read_var_35();
    } else {
      uint32_t value = stream.read_var_35();
      return (-(value & 1)) ^ (value >> 1);
    }
  } else {
    uint32_t value = stream.get_bits(prop->num_bits);
    uint32_t signer = (0x80000000 >> (32 - prop->num_bits)) & ((prop->flags & SP_Unsigned) - 1);

    value = value ^ signer;
    return value - signer;
  }
}

float read_float_coord(Bitstream &stream) {
  uint32_t integer = stream.get_bits(1);
  uint32_t fraction = stream.get_bits(1);

  if (integer || fraction) {
    uint32_t sign = stream.get_bits(1);

    if (integer) {
      integer = stream.get_bits(0x0E) + 1;
    }

    if (fraction) {
      fraction = stream.get_bits(5);
    }

    double d = 0.03125 * fraction;
    d += integer;

    if (sign) {
      d *= -1;
    }

    return (float) d;
  } else {
    return 0;
  }
}

enum FloatType {
  FT_None,
  FT_LowPrecision,
  FT_Integral,
};

float read_float_coord_mp(Bitstream &stream, FloatType type) {
  uint32_t value;

  if (type == FT_LowPrecision || type == FT_None) {
    uint32_t a = stream.get_bits(1);
    uint32_t b = stream.get_bits(1);
    uint32_t c = stream.get_bits(1);

    value = 4 * c + 2 * b + a;

    XERROR("please no");
  } else if (type == FT_Integral) {
    uint32_t a = stream.get_bits(1);
    uint32_t b = stream.get_bits(1);
    a = a + 2 * b;

    if (!b) {
      return 0;
    } else {
      if (a) {
        value = stream.get_bits(12);
      } else {
        value = stream.get_bits(15);
      }

      if (value & 1) {
        value = -((value >> 1) + 1);
      } else {
        value = (value >> 1) + 1;
      }

      return value;
    }
  } else {
    XERROR("Unknown coord type.");
  }
}

float read_float_no_scale(Bitstream &stream) {
  return stream.get_bits(32);
}

float read_float_normal(Bitstream &stream) {
  uint32_t sign = stream.get_bits(1);
  uint32_t value = stream.get_bits(11);

  float f = value;

  if (value >> 31) {
    f += 4.2949673e9;
  }

  f *= 4.885197850512946e-4;

  if (sign) {
    f = -1 * f;
  }

  return f;
}

float read_float_cell_coord(Bitstream &stream, FloatType type, uint32_t bits) {
  uint32_t value = stream.get_bits(bits);

  if (type == FT_None || type == FT_LowPrecision) {
    bool lp = type == FT_LowPrecision;

    uint32_t fraction;
    if (!lp) {
      fraction = stream.get_bits(5);
    } else {
      fraction = stream.get_bits(3);
    }

    double d = value + (lp ? 0.125 : 0.03125) * fraction;
    return (float) d;
  } else if (type == FT_Integral) {
    double d = value;

    if (value >> 31) {
      d += 4.2949673e9;
    } 

    return (float) d;
  } else {
    XERROR("Unknown float type");
    return 0;
  }
}

float read_float(Bitstream &stream, const SendProp *prop) {
  if (prop->flags & SP_Coord) {
    return read_float_coord(stream);
  } else if (prop->flags & SP_CoordMp) {
    return read_float_coord_mp(stream, FT_None);
  } else if (prop->flags & SP_CoordMpLowPrecision) {
    return read_float_coord_mp(stream, FT_LowPrecision);
  } else if (prop->flags & SP_CoordMpIntegral) {
    return read_float_coord_mp(stream, FT_Integral);
  } else if (prop->flags & SP_NoScale) {
    return read_float_no_scale(stream);
  } else if (prop->flags & SP_Normal) {
    return read_float_normal(stream);
  } else if (prop->flags & SP_CellCoord) {
    return read_float_cell_coord(stream, FT_None, prop->num_bits);
  } else if (prop->flags & SP_CellCoordLowPrecision) {
    return read_float_cell_coord(stream, FT_LowPrecision, prop->num_bits);
  } else if (prop->flags & SP_CellCoordIntegral) {
    return read_float_cell_coord(stream, FT_Integral, prop->num_bits);
  } else {
    uint32_t dividend = stream.get_bits(prop->num_bits);
    uint32_t divisor = (1 << prop->num_bits) - 1;

    float f = ((float) dividend) / divisor;
    float range = prop->high_value - prop->low_value;

    return f * range + prop->low_value;
  }
}

void read_vector(float vector[3], Bitstream &stream, const SendProp *prop) {
  vector[0] = read_float(stream, prop);
  vector[1] = read_float(stream, prop);

  if (prop->flags & SP_Normal) {
    uint32_t sign = stream.get_bits(1);

    float f = vector[0] * vector[0] + vector[1] * vector[1];

    if (1 >= f) {
      vector[2] = 0;
    } else {
      vector[2] = sqrt(1 - f);
    }

    if (sign) {
      vector[2] = -1 * vector[2];
    }
  } else {
    vector[2] = read_float(stream, prop);
  }
}

void read_vector_xy(float vector[2], Bitstream &stream, const SendProp *prop) {
  vector[0] = read_float(stream, prop);
  vector[1] = read_float(stream, prop);
}

size_t read_string(char *buf, size_t max_length, Bitstream &stream, const SendProp *prop) {
  uint32_t length = stream.get_bits(9);
  XASSERT(length <= max_length, "String too long %d > %d", length, max_length);

  stream.read_bits(buf, 8 * length);

  return length;
}

uint32_t get_array_length_bits(const SendProp *prop) {
  uint32_t n = prop->num_elements;
  uint32_t bits = 0;

  while (n) {
    ++bits;
    n >>= 1;
  }

  return bits;
}

void read_array(std::vector<ArrayPropertyElement> &elements, Bitstream &stream,
    const SendProp *prop) {
  XASSERT(prop->array_prop, "Array prop has no inner prop.");

  uint32_t count = stream.get_bits(get_array_length_bits(prop));

  // The element's names are usually the name of the container + "element"
  // such as "player_array_element", which is useless.
  std::string dummy_name;
  for (uint32_t i = 0; i < count; ++i) {
    elements.push_back(Property::read_prop(stream, prop->array_prop, dummy_name));
  }
}

uint64_t read_int64(Bitstream &stream, const SendProp *prop) {
  if (SP_EncodedAgainstTickcount & prop->flags) {
    XERROR("this sounds scary");
  } else {
    bool negate = false;
    size_t second_bits = prop->num_bits - 32;

    if (!(SP_Unsigned & prop->flags)) {
      --second_bits;

      if (stream.get_bits(1)) {
        negate = true;
      }
    }

    XASSERT(prop->num_bits >= second_bits, "Invalid number of bits");

    uint64_t a = stream.get_bits(32);
    uint64_t b = stream.get_bits(second_bits);
    uint64_t value = (a << 32) | b;

    if (negate) {
      value *= -1;
    }

    return value;
  }
}

std::shared_ptr<Property> Property::read_prop(Bitstream &stream, const SendProp *prop, std::string& name) {
  Property *out;

  name = prop->in_table->net_table_name + "." + prop->var_name;

  if (prop->type == SP_Int) {
    out = new IntProperty(read_int(stream, prop));
  } else if (prop->type == SP_Float) {
    out = new FloatProperty(read_float(stream, prop));
  } else if (prop->type == SP_Vector) {
    float value[3];
    read_vector(value, stream, prop);

    out = new VectorProperty(value);
  } else if (prop->type == SP_VectorXY) {
    float value[2];
    read_vector_xy(value, stream, prop);

    out = new VectorXYProperty(value);
  } else if (prop->type == SP_String) {
    char str[MAX_STRING_LENGTH + 1];
    size_t length = read_string(str, MAX_STRING_LENGTH, stream, prop);

    out = new StringProperty(std::string(str, length));
  } else if (prop->type == SP_Array) {
    std::vector<ArrayPropertyElement> elements;
    read_array(elements, stream, prop);

    out = new ArrayProperty(elements, prop->array_prop->type);
  } else if (prop->type == SP_Int64) {
    out = new Int64Property(read_int64(stream, prop));
  } else {
    XERROR("Unknown send prop type %d", prop->type);
  }

  return std::shared_ptr<Property>(out);
}

Property::Property(SP_Types _type) : type(_type) {
}

Property::~Property() {
}

