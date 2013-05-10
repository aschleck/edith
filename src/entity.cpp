#include "entity.h"

#include <iostream>

#include "bitstream.h"
#include "state.h"
#include "property.h"

Entity::Entity() : id(-1), clazz(0), table(0) {
}

Entity::Entity(uint32_t _id, const Class &_clazz, const FlatSendTable &_table) :
    id(_id), clazz(&_clazz), table(&_table) {
  size_t prop_count = table->props.size();
  properties.reserve(prop_count);

  for (size_t i = 0; i < prop_count; ++i) {
    properties.push_back(std::shared_ptr<Property>());
  }
}

void read_field_number(uint32_t &last_field, Bitstream &stream) {
  if (stream.get_bits(1)) {
    last_field += 1;
  } else {
    size_t start = stream.get_position();
    uint32_t value = stream.read_var_35();

    if (value == 0x3FFF) {
      last_field = 0xFFFFFFFF;
    } else {
      last_field += value + 1;
    }
  }
}

void read_field_list(std::vector<uint32_t> &fields, Bitstream &stream) {
  uint32_t last_field = -1;
  read_field_number(last_field, stream);

  while (last_field != 0xFFFFFFFF) {
    fields.push_back(last_field);

    read_field_number(last_field, stream);
  }
}

void Entity::update(Bitstream &stream) {
  std::vector<uint32_t> fields;
  read_field_list(fields, stream);

  for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
    uint32_t i = *iter;

    //std::cout << table->props[i]->var_name << " " <<
    //  table->props[i]->type << " " <<
    //  table->props[i]->flags << ": ";
    properties[i] = Property::read_prop(stream, table->props[i]);
  }
}

void swap(Entity &first, Entity &second) {
  using std::swap;

  swap(first.id, second.id);
  swap(first.table, second.table);
  swap(first.properties, second.properties);
}

