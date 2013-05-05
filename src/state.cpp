#include "state.h"

#include <iomanip>
#include <iostream>

#include "debug.h"

#define BASECLASS_PROP "baseclass"

size_t log2(size_t n) {
  XASSERT(n >= 1, "Invalid number passed to log2 (%u).", n);

  size_t r = 0;
  size_t acc = 1;

  while (acc < n) {
    ++r;
    acc *= 2;
  }
  
  return r;
}

Class::Class(uint32_t _id, const std::string &_dt_name, const std::string &_name) :
  id(_id), dt_name(_dt_name), name(_name) {
}

SendProp::SendProp() {
}

SendProp::SendProp(SP_Types _type, const std::string &_var_name, uint32_t _flags,
    uint32_t _priority, const std::string &_dt_name, uint32_t _num_elements,
    float _low_value, float _high_value, uint32_t _num_bits) :
  type(_type),
  var_name(_var_name),
  flags(_flags),
  priority(_priority),
  dt_name(_dt_name),
  num_elements(_num_elements),
  low_value(_low_value),
  high_value(_high_value),
  num_bits(_num_bits),
  array_prop(0) {
}

SendProp::SendProp(const SendProp &that) {
  type = that.type;
  var_name = that.var_name;
  flags = that.flags;
  priority = that.priority;
  dt_name = that.dt_name;
  num_elements = that.num_elements;
  low_value = that.low_value;
  high_value = that.high_value;
  num_bits = that.num_bits;
  array_prop = that.array_prop;
}

SendProp::SendProp(SendProp &&that) {
  swap(*this, that);
}

SendProp &SendProp::operator=(SendProp that) {
  swap(*this, that);

  return *this;
}

void swap(SendProp &first, SendProp &second) {
  using std::swap;

  swap(first.type, second.type);
  swap(first.var_name, second.var_name);
  swap(first.flags, second.flags);
  swap(first.priority, second.priority);
  swap(first.dt_name, second.dt_name);
  swap(first.num_elements, second.num_elements);
  swap(first.low_value, second.low_value);
  swap(first.high_value, second.high_value);
  swap(first.num_bits, second.num_bits);
  swap(first.array_prop, second.array_prop);
}

SendTable::SendTable() {
}

SendTable::SendTable(const std::string _net_table_name, bool _needs_decoder) :
  net_table_name(_net_table_name), needs_decoder(_needs_decoder) {
}

FlatSendTable::FlatSendTable() {
}

FlatSendTable::FlatSendTable(const std::string _net_table_name) : net_table_name(_net_table_name) {
}

StringTableEntry::StringTableEntry() {
}

StringTableEntry::StringTableEntry(const std::string _key, const std::string _value) :
    key(_key), value(_value) {
}

StringTable::StringTable() {
}

StringTable::StringTable(const std::string &_name, uint32_t _max_entries,
    bool _user_data_fixed_size, uint32_t _user_data_size,
    uint32_t _user_data_size_bits, uint32_t _flags) :
  name(_name),
  max_entries(_max_entries),
  user_data_fixed_size(_user_data_fixed_size),
  user_data_size(_user_data_size),
  user_data_size_bits(_user_data_size_bits),
  flags(_flags),
  entry_bits(log2(_max_entries)) {
}

bool StringTable::contains(const std::string &key) const {
  return entries.has(key);
}

size_t StringTable::count() const {
  return entries.size();
}

StringTableEntry &StringTable::get(size_t i) {
  return entries[i];
}

StringTableEntry &StringTable::get(const std::string &key) {
  return entries[key];
}

StringTableEntry &StringTable::put(const std::string &key, const std::string &value) {
  XASSERT(!entries.has(key), "Entry %s already exists.", key.c_str());

  StringTableEntry entry(key, value);
  return entries.add(entry);
}

State::State(uint32_t _max_classes) :
    max_classes(_max_classes),
    class_bits(log2(_max_classes)),
    entities(new Entity[MAX_ENTITIES]()) {
}

State::~State() {
  delete[] entities;
}

const Class &State::create_class(uint32_t id, std::string dt_name, std::string name) {
  classes.push_back(Class(id, dt_name, name));

  return classes[classes.size() - 1];
}

SendTable &State::create_send_table(const std::string net_table_name, bool needs_decoder) {
  return send_tables.add(SendTable(net_table_name, needs_decoder));
}

StringTable &State::create_string_table(const std::string &name, uint32_t max_entries,
    bool user_data_fixed_size, uint32_t user_data_size, uint32_t user_data_size_bits,
    uint32_t flags) {
  XASSERT(!string_tables.has(name), "StringTable %s already exists.", name.c_str());

  StringTable table(name, max_entries, user_data_fixed_size, user_data_size,
      user_data_size_bits, flags);
  return string_tables.add(table);
}

const Class &State::get_class(size_t i) const {
  XASSERT(i < classes.size(), "Class %ld does not exist (%ld).", i, classes.size());

  return classes[i];
}

StringTable &State::get_string_table(size_t i) {
  return string_tables[i];
}

StringTable &State::get_string_table(const std::string &name) {
  return string_tables[name];
}

void State::gather_excludes(const SendTable &table, std::set<std::string> &excluding) {
  for (auto iter = table.props.begin(); iter != table.props.end(); ++iter) {
    const SendProp &prop = *iter;

    if (SP_Exclude & prop.flags) {
      excluding.insert(prop.dt_name + prop.var_name);
    } else if (SP_DataTable == prop.type) {
      gather_excludes(send_tables[prop.dt_name], excluding);
    }
  }
}

void State::gather(const SendTable &from, DTProp &dt_prop, CompileState &state) {
  for (auto iter = from.props.begin();
      iter != from.props.end();
      ++iter) {
    const SendProp &prop = *iter;

    if (SP_InsideArray & prop.flags) {
      continue;
    } else if (state.excluding.count(from.net_table_name + prop.var_name)) {
      continue;
    }

    if (SP_DataTable == prop.type) {
      const SendTable &dt_table = send_tables[prop.dt_name];

      if (SP_Collapsible & prop.flags) {
        gather(dt_table, dt_prop, state);
      } else {
        DTProp new_dt_prop;
        dt_prop.dt_props.push_back(new_dt_prop);

        build_hierarchy(dt_table, new_dt_prop, state);
      }
    } else {
      dt_prop.non_dt_props.push_back(&prop);
    }
  }
}

void State::build_hierarchy(const SendTable &send_table, DTProp &dt_prop,
    CompileState &state) {
  dt_prop.send_table = &send_table;

  dt_prop.prop_start = state.props.size();
  gather(send_table, dt_prop, state);

  for (auto iter = dt_prop.non_dt_props.begin(); iter != dt_prop.non_dt_props.end(); ++iter) {
    state.props.push_back(*iter);
  }

  dt_prop.prop_count = state.props.size() - dt_prop.prop_start;
}

void State::compile_send_table(const SendTable &table) {
  CompileState state;
  gather_excludes(table, state.excluding);

  DTProp dt_prop;
  build_hierarchy(table, dt_prop, state);

  std::vector<uint32_t> priorities;
  priorities.push_back(64);
  for (auto iter = state.props.begin(); iter != state.props.end(); ++iter) {
    uint32_t priority = (*iter)->priority;

    auto priority_iter = priorities.begin();
    while (priority_iter != priorities.end() && *priority_iter != priority) {
      ++priority_iter;
    }

    if (priority_iter == priorities.end()) {
      priorities.push_back(priority);
    }
  }

  std::sort(priorities.begin(), priorities.end());

  size_t prop_offset = 0;
  for (size_t priority_index = 0; priority_index < priorities.size(); ++priority_index) {
    size_t priority = priorities[priority_index];

    size_t hole = prop_offset;
    size_t cursor = hole;
    while (cursor < state.props.size()) {
      const SendProp *prop = state.props[cursor];

      if (prop->priority == priority ||
          (priority == 64 && (SP_ChangesOften & prop->flags))) {
        const SendProp *temp = state.props[hole];
        state.props[hole] = state.props[cursor];
        state.props[cursor] = temp;

        ++hole;
        ++prop_offset;
      }

      ++cursor;
    }
  }

  FlatSendTable flat_table(table.net_table_name);
  flat_table.props = state.props;
  flat_table.dt_prop = dt_prop;

  flat_send_tables.add(flat_table);
}

void State::compile_send_tables() {
  for (auto iter = send_tables.begin();
      iter != send_tables.end();
      ++iter) {
    compile_send_table(*iter);
  }
}

