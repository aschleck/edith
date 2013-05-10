#ifndef _STATE_H
#define _STATE_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "dictionary_list.h"
#include "entity.h"

// I'm not strict about these but if someone specially crafted a replay it could probably
// do some damage.
#define MAX_ENTITIES 0x3FFF
#define MAX_SEND_TABLES 0xFFFF
#define MAX_NONDATATABLE_PROPS 0x800

class Class {
public:
  Class(uint32_t id, const std::string &dt_name, const std::string &name);

  uint32_t id;
  std::string dt_name;
  std::string name;
};

enum SP_Flags {
  SP_Unsigned = 1 << 0,
  SP_Coord = 1 << 1,
  SP_NoScale = 1 << 2,
  SP_RoundDown = 1 << 3,
  SP_RoundUp = 1 << 4,
  SP_Normal = 1 << 5,
  SP_Exclude = 1 << 6,
  SP_Xyze = 1 << 7,
  SP_InsideArray = 1 << 8,

  SP_Collapsible = 1 << 11,
  SP_CoordMp = 1 << 12,
  SP_CoordMpLowPrecision = 1 << 13,
  SP_CoordMpIntegral = 1 << 14,
  SP_CellCoord = 1 << 15,
  SP_CellCoordLowPrecision = 1 << 16,
  SP_CellCoordIntegral = 1 << 17,
  SP_ChangesOften = 1 << 18,
  SP_EncodedAgainstTickcount = 1 << 19,
};

enum SP_Types {
  SP_Int = 0,
  SP_Float = 1,
  SP_Vector = 2,
  SP_VectorXY = 3,
  SP_String = 4,
  SP_Array = 5,
  SP_DataTable = 6,
  SP_Int64 = 7,
};

class SendProp {
public:
  SendProp();
  SendProp(SP_Types type, const std::string &var_name, uint32_t flags, uint32_t priority,
      const std::string &dt_name, uint32_t num_elements, float low_value, float high_value,
      uint32_t num_bits);

  SP_Types type;
  std::string var_name;
  uint32_t flags;
  uint32_t priority;

  std::string dt_name;
  uint32_t num_elements;

  float low_value;
  float high_value;
  uint32_t num_bits;

  SendProp *array_prop;
};

class SendTable {
public:
  SendTable();
  SendTable(const std::string net_table_name, bool needs_decoder);

  std::string net_table_name;
  bool needs_decoder;

private:
  struct GetSendPropName {
    const std::string &operator()(const SendProp &prop) {
      return prop.var_name;
    }
  };

public:
  DictionaryList<SendProp, std::string, GetSendPropName> props;
};

struct DTProp {
  const SendTable *send_table;
  std::vector<DTProp> dt_props;
  std::vector<const SendProp *> non_dt_props;

  size_t prop_start;
  size_t prop_count;
};

class FlatSendTable {
public:
  FlatSendTable();
  FlatSendTable(const std::string net_table_name);

  std::string net_table_name;
  std::vector<const SendProp *> props;
  DTProp dt_prop;
};

enum ST_Flags {
  ST_Something = 2,
  ST_FixedLength = 8,
};

class StringTableEntry {
public:
  StringTableEntry();
  StringTableEntry(const std::string key, const std::string value);

  std::string key;
  std::string value;
};

class StringTable {
public:
  StringTable();
  StringTable(const std::string &name, uint32_t max_entries, bool user_data_fixed_size,
      uint32_t user_data_size, uint32_t user_data_size_bits, uint32_t flags);

  bool contains(const std::string &key) const;
  size_t count() const;
  StringTableEntry &get(size_t i);
  StringTableEntry &get(const std::string &key);
  StringTableEntry &put(const std::string &key, const std::string &value);

  std::string name;
  uint32_t max_entries;
  bool user_data_fixed_size;
  uint32_t user_data_size;
  uint32_t user_data_size_bits;
  uint32_t flags;

  size_t entry_bits;
  
private:
  struct GetEntryKey {
    const std::string &operator()(const StringTableEntry &entry) {
      return entry.key;
    }
  };

  DictionaryList<StringTableEntry, std::string, GetEntryKey> entries;
};

class State {
public:
  State(uint32_t max_classes);
  ~State();

  const Class &create_class(uint32_t id, std::string dt_name, std::string name);
  SendTable &create_send_table(const std::string net_table_name, bool needs_decoder);
  StringTable &create_string_table(const std::string &name, uint32_t max_entries,
      bool user_data_fixed_size, uint32_t user_data_size, uint32_t user_data_size_bits,
      uint32_t flags);

  void compile_send_tables();

  const Class &get_class(size_t i) const;
  StringTable &get_string_table(size_t i);
  StringTable &get_string_table(const std::string &name);

  uint32_t max_classes;

  uint32_t class_bits;

private:
  struct CompileState {
    std::set<std::string> excluding;
    std::vector<const SendProp *> props;
  };

  void build_hierarchy(const SendTable &table, DTProp &dt_prop, CompileState &state);
  void compile_send_table(const SendTable &table);
  void gather(const SendTable &from, DTProp &dt_prop, CompileState &state);
  void gather_excludes(const SendTable &table, std::set<std::string> &excluding);

private:
  struct GetSendTableName {
    const std::string &operator()(const SendTable &table) {
      return table.net_table_name;
    }
  };

  struct GetFlatSendTableName {
    const std::string &operator()(const FlatSendTable &table) {
      return table.net_table_name;
    }
  };

  struct GetStringTableName {
    const std::string &operator()(const StringTable &table) {
      return table.name;
    }
  };

public:
  DictionaryList<SendTable, std::string, GetSendTableName> send_tables;
  DictionaryList<FlatSendTable, std::string, GetFlatSendTableName> flat_send_tables;

  DictionaryList<StringTable, std::string, GetStringTableName> string_tables;

  std::vector<Class> classes;
  Entity *entities;
};

#endif

