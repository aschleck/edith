#include "property.h"

#include <iostream>
#include <xmmintrin.h>

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
  uint32_t first = stream.get_bits(1);
  uint32_t second = stream.get_bits(1);

  __m128 a = (__m128)_mm_setzero_si128();
  __m128 b = (__m128)_mm_setzero_si128();

  if (first || second) {
    uint32_t third = stream.get_bits(1);

    if (first) {
      first = stream.get_bits(0x0E) + 1;
    }

    if (second) {
      second = stream.get_bits(5);
    }

    b = (__m128)_mm_cvtsi32_si128(first);
    a = _mm_cvtsi32_ss(a, second);

    __m128 special = (__m128)_mm_setr_epi32(0x3D000000, 0, 0, 0);
    a = _mm_mul_ss(a, special);
    a = (__m128)_mm_cvtps_pd(a);

    b = (__m128)_mm_cvtepi32_pd((__m128i)b);
    
    a = (__m128)_mm_add_sd((__m128d)a, (__m128d)b);
    a = (__m128)_mm_cvtpd_ps((__m128d)a);

    if (third) {
      __m128 mask = (__m128)_mm_set1_epi32(0x80000000);
      a = _mm_xor_ps(a, mask);
    }
  }

  float f;
  _mm_store_ss(&f, a);
  return f;
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
    }
  } else {
    XERROR("Unknown coord type.");
  }

  __m128 a = (__m128)_mm_setzero_si128();
  a = _mm_cvtsi32_ss(a, value);
  float f;
  _mm_store_ss(&f, a);

  return f;
}

float read_float_no_scale(Bitstream &stream) {
  uint32_t value = stream.get_bits(32);

  __m128 a = (__m128)_mm_setzero_si128();
  a = _mm_cvtsi32_ss(a, value);
  float f;
  _mm_store_ss(&f, a);

  return f;
}

float read_float_normal(Bitstream &stream) {
  uint32_t first = stream.get_bits(1);
  uint32_t second = stream.get_bits(11);

  float f = second;

  if (second >> 31) {
    f += 4.2949673e9;
  }

  f *= 4.885197850512946e-4;

  __m128 a = _mm_load_ss(&f);
  __m128 mask = (__m128)_mm_set_epi32(0x80000000, 0x80000000, 0x80000000, 0x80000000);
  a = _mm_xor_ps(a, mask);

  _mm_store_ss(&f, a);

  return f;
}

float read_float_cell_coord(Bitstream &stream, FloatType type, uint32_t bits) {
  uint32_t value = stream.get_bits(bits);

  if (type == FT_None || type == FT_LowPrecision) {
    bool lp = type  == FT_LowPrecision;

    uint32_t second;
    if (!lp) {
      second = stream.get_bits(5);
    } else {
      second = stream.get_bits(3);
    }

    __m128 a;
    if (!lp) {
      a = (__m128)_mm_setr_epi32(0x3FA00000, 0, 0, 0);
    } else {
      a = (__m128)_mm_setr_epi32(0x3FC00000, 0, 0, 0);
    }

    __m128 b = (__m128)_mm_setzero_si128();
    b = _mm_cvtsi32_ss(b, second);
    b = (__m128)_mm_cvtps_pd(b);
    b = (__m128)_mm_mul_sd((__m128d)b, (__m128d)a);
    
    a = (__m128)_mm_setzero_si128();
    a = _mm_cvtsi32_ss(a, value);
    
    b = (__m128)_mm_add_sd((__m128d)b, (__m128d)a);
    a = (__m128)_mm_cvtpd_ps((__m128d)b);

    float f;
    _mm_store_ss(&f, a);

    return f;
  } else if (type == FT_Integral) {
    __m128 a = (__m128)_mm_setzero_si128();
    a = _mm_cvtsi32_ss(a, value);
    float f;
    _mm_store_ss(&f, a);

    if (value >> 31) {
      f += 4.2949673e9;
    } 

    return f;
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
    unsigned int value = stream.get_bits(prop->num_bits);
    __m128 a = (__m128)_mm_setzero_si128();
    a = _mm_cvtsi32_ss(a, value);

    __m128 b = (__m128)_mm_setzero_si128();
    b = _mm_cvtsi32_ss(b, (1 << prop->num_bits) - 1);

    a = _mm_div_ss(a, b);

    float range = prop->high_value - prop->low_value;
    b = _mm_load_ss(&range);
    a = _mm_mul_ss(a, b);

    range = prop->low_value;
    b = _mm_load_ss(&range);
    a = _mm_add_ss(a, b);

    float f;
    _mm_store_ss(&f, a);

    return f;
  }
}

void read_vector(float vector[3], Bitstream &stream, const SendProp *prop) {
  vector[0] = read_float(stream, prop);
  vector[1] = read_float(stream, prop);

  if (prop->flags & SP_Normal) {
    uint32_t first = stream.get_bits(1);

    __m128 a = _mm_load_ss(&vector[0]);
    __m128 b = _mm_load_ss(&vector[1]);

    a = _mm_mul_ss(a, a);
    b = _mm_mul_ss(b, b);

    a = _mm_add_ss(a, b);

    b = (__m128)_mm_setr_epi32(0x3D000000, 0, 0, 0);

    if (_mm_comile_ss(b, a)) {
      a = (__m128)_mm_setzero_si128();
    } else {
      b = _mm_sub_ss(b, a);
      a = _mm_sqrt_ss(b);
    }

    if (first) {
      __m128 special = (__m128)_mm_setr_epi32(0x3D000000, 0, 0, 0);
      a = _mm_mul_ss(a, special);
    }

    _mm_store_ss(&vector[2], a);
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

  for (uint32_t i = 0; i < count; ++i) {
    elements.push_back(Property::read_prop(stream, prop->array_prop));
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

std::shared_ptr<Property> Property::read_prop(Bitstream &stream, const SendProp *prop) {
  Property *out;

  std::string var_name = prop->in_table->net_table_name + "." + prop->var_name;

  if (prop->type == SP_Int) {
    out = new IntProperty(var_name, read_int(stream, prop));
  } else if (prop->type == SP_Float) {
    out = new FloatProperty(var_name, read_float(stream, prop));
  } else if (prop->type == SP_Vector) {
    float value[3];
    read_vector(value, stream, prop);

    out = new VectorProperty(var_name, value);
  } else if (prop->type == SP_VectorXY) {
    float value[2];
    read_vector_xy(value, stream, prop);

    out = new VectorXYProperty(var_name, value);
  } else if (prop->type == SP_String) {
    char str[MAX_STRING_LENGTH + 1];
    size_t length = read_string(str, MAX_STRING_LENGTH, stream, prop);

    out = new StringProperty(var_name, std::string(str, length));
  } else if (prop->type == SP_Array) {
    std::vector<ArrayPropertyElement> elements;
    read_array(elements, stream, prop);

    out = new ArrayProperty(var_name, elements, prop->array_prop->type);
  } else if (prop->type == SP_Int64) {
    out = new Int64Property(var_name, read_int64(stream, prop));
  } else {
    XERROR("Unknown send prop type %d", prop->type);
  }

  return std::shared_ptr<Property>(out);
}

Property::Property(const std::string &_name, SP_Types _type) : name(_name), type(_type) {
}

Property::~Property() {
}

