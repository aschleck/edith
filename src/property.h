#ifndef _PROPERTY_H
#define _PROPERTY_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "bitstream.h"
#include "state.h"

class Property {
public:
  static std::shared_ptr<Property> read_prop(Bitstream &stream, const SendProp *prop);

  Property(const std::string &name, SP_Types type);
  virtual ~Property();

  std::string name;
  SP_Types type;
};

template<SP_Types SendPropType, typename T>
class TypedProperty : public Property {
public:
  TypedProperty(const std::string &_name, T _value) :
      Property(_name, SendPropType), value(_value) {
  }

  T value;
};

template<SP_Types SendPropType, typename T, size_t C>
class FixedTypedProperty : public Property {
public:
  FixedTypedProperty(const std::string &_name, T _values[C]) :
      Property(_name, SendPropType) {
    std::copy(_values, _values + C, values);
  }

  T values[C];
};

typedef std::shared_ptr<Property> ArrayPropertyElement;

template<>
class TypedProperty<SP_Array, ArrayPropertyElement> : public Property {
public:
  TypedProperty(const std::string &_name, std::vector<ArrayPropertyElement> _elements,
      SP_Types _value_type) :

      Property(_name, SP_Array),
      elements(_elements),
      value_type(_value_type) {
  }

  std::vector<ArrayPropertyElement> elements;
  SP_Types value_type;
};

typedef TypedProperty<SP_Int, uint32_t> IntProperty;
typedef TypedProperty<SP_Float, float> FloatProperty;
typedef FixedTypedProperty<SP_Vector, float, 3> VectorProperty;
typedef FixedTypedProperty<SP_VectorXY, float, 2> VectorXYProperty;
typedef TypedProperty<SP_String, std::string> StringProperty;
typedef TypedProperty<SP_Array, ArrayPropertyElement> ArrayProperty;
typedef TypedProperty<SP_Int64, uint64_t> Int64Property;

#endif

