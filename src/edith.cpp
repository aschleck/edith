#include <stdint.h>

#include "demo.pb.h"
#include "netmessages.pb.h"

#include "bitstream.h"
#include "debug.h"
#include "demo.h"
#include "entity.h"
#include "state.h"
#include "visitor.h"

#define INSTANCE_BASELINE_TABLE "instancebaseline"
#define KEY_HISTORY_SIZE 32
#define MAX_EDICTS 0x800
#define MAX_KEY_SIZE 0x400
#define MAX_VALUE_SIZE 0x4000

enum UpdateFlag {
  UF_LeavePVS = 1,
  UF_Delete = 2,
  UF_EnterPVS = 4,
};

State *state = 0;

uint32_t read_var_int(const char *data, size_t length, size_t *offset) {
  uint32_t b;
  int count = 0;
  uint32_t result = 0;

  do {
    XASSERT(count != 5, "Corrupt data.");
    XASSERT(*offset <= length, "Premature end of stream.");

    b = data[(*offset)++];
    result |= (b & 0x7F) << (7 * count);
    ++count;
  } while (b & 0x80);

  return result;
}

void dump_SVC_SendTable(const CSVCMsg_SendTable &table) {
  XASSERT(state, "SVC_SendTable but no state created.");

  SendTable &converted = state->create_send_table(table.net_table_name(), table.needs_decoder());

  size_t prop_count = table.props_size();
  for (size_t i = 0; i < prop_count; ++i) {
    const CSVCMsg_SendTable_sendprop_t &prop = table.props(i);

    SendProp c(
        static_cast<SP_Types>(prop.type()),
        prop.var_name(),
        prop.flags(),
        prop.priority(),
        prop.dt_name(),
        prop.num_elements(),
        prop.low_value(),
        prop.high_value(),
        prop.num_bits());

    converted.props.add(c);
  }
}

void dump_DEM_SendTables(const CDemoSendTables &tables) {
  const char *data = tables.data().c_str();
  size_t offset = 0;
  size_t length = tables.data().length();

  while (offset < length) {
    uint32_t command = read_var_int(data, length, &offset);
    uint32_t size = read_var_int(data, length, &offset);
    XASSERT(offset + size <= length, "Reading data outside of table.");

    XASSERT(command == svc_SendTable, "Command %u in DEM_SendTables.", command);

    CSVCMsg_SendTable send_table;
    send_table.ParseFromArray(&(data[offset]), size);
    dump_SVC_SendTable(send_table);

    offset += size;
  }
}

const StringTableEntry &get_baseline_for(int class_i) {
  XASSERT(state, "No state created.");

  StringTable &instance_baseline = state->get_string_table(INSTANCE_BASELINE_TABLE);

  char buf[32];
  sprintf(buf, "%d", class_i);

  return instance_baseline.get(buf);
}

uint32_t read_entity_header(uint32_t *base, Bitstream &stream) {
  uint32_t value = stream.get_bits(6);

  if (value & 0x30) {
    uint32_t a = (value >> 4) & 3;
    uint32_t b = (a == 3) ? 16 : 0;

    value = stream.get_bits(4 * a + b) << 4 | (value & 0xF);
  }

  *base += value + 1;

  unsigned int update_flags = 0;

  if (!stream.get_bits(1)) {
    if (stream.get_bits(1)) {
      update_flags |= UF_EnterPVS;
    }
  } else {
    update_flags |= UF_LeavePVS;
    if (stream.get_bits(1)) {
      update_flags |= UF_Delete;
    }
  }

  return update_flags;
}

void read_entity_enter_pvs(uint32_t entity_id, Bitstream &stream, Visitor& visitor) {
  uint32_t class_i = stream.get_bits(state->class_bits);
  uint32_t serial = stream.get_bits(10);

  XASSERT(entity_id < MAX_EDICTS, "Entity %ld exceeds max edicts.", entity_id);

  const Class &clazz = state->get_class(class_i);
  const FlatSendTable &flat_send_table = state->flat_send_tables[clazz.dt_name];

  Entity &entity = state->entities[entity_id];

  if (entity.id != -1) {
    visitor.visit_entity_deleted(entity);
  }

  entity = Entity(entity_id, clazz, flat_send_table);

  const StringTableEntry &baseline = get_baseline_for(class_i);
  Bitstream baseline_stream(baseline.value);
  entity.update(baseline_stream);

  entity.update(stream);

  visitor.visit_entity_created(entity);
}

void read_entity_update(uint32_t entity_id, Bitstream &stream, Visitor& visitor) {
  XASSERT(entity_id < MAX_ENTITIES, "Entity id too big");

  Entity &entity = state->entities[entity_id];
  XASSERT(entity.id != -1, "Entity %d is not set up.", entity_id);

  entity.update(stream);

  visitor.visit_entity_updated(entity);
}

void dump_SVC_PacketEntities(const CSVCMsg_PacketEntities &entities, Visitor& visitor) {
  Bitstream stream(entities.entity_data());

  uint32_t entity_id = -1;
  size_t found = 0;
  uint32_t update_type;

  while (found < entities.updated_entries()) {
    update_type = read_entity_header(&entity_id, stream);

    if (update_type & UF_EnterPVS) {
      read_entity_enter_pvs(entity_id, stream, visitor);
    } else if (update_type & UF_LeavePVS) {
      XASSERT(entities.is_delta(), "Leave PVS on full update");

      if (update_type & UF_Delete) {
        visitor.visit_entity_deleted(state->entities[entity_id]);

        state->entities[entity_id].id = -1;
      }
    } else {
      read_entity_update(entity_id, stream, visitor);
    }

    ++found;
  }

  if (entities.is_delta()) {
    while (stream.get_bits(1)) {
      entity_id = stream.get_bits(11);
      visitor.visit_entity_deleted(state->entities[entity_id]);
      state->entities[entity_id].id = -1;
    }
  }
}

void dump_SVC_ServerInfo(const CSVCMsg_ServerInfo &info) {
  XASSERT(!state, "Already seen SVC_ServerInfo.");

  state = new State(info.max_classes());
}

void dump_DEM_ClassInfo(const CDemoClassInfo &info) {
  XASSERT(state, "DEM_ClassInfo but no state.");

  for (size_t i = 0; i < info.classes_size(); ++i) {
    const CDemoClassInfo_class_t &clazz = info.classes(i);
    state->create_class(clazz.class_id(), clazz.table_name(), clazz.network_name());
  }

  for (auto iter = state->send_tables.begin(); iter != state->send_tables.end(); ++iter) {
    SendTable &table = *iter;

    for (size_t i = 0; i < table.props.size(); ++i) {
      SendProp &prop = table.props[i];

      prop.in_table = &table;

      if (prop.type == SP_Array) {
        XASSERT(i > 0, "Array prop %s is at index zero.", prop.var_name.c_str());
        prop.array_prop = &(table.props[i - 1]);
      }
    }
  }

  state->compile_send_tables();
}

void read_string_table_key(uint32_t first_bit, Bitstream &stream, char *buf,
    size_t buf_length, std::vector<std::string> &key_history) {
  if (first_bit && stream.get_bits(1)) {
    XERROR("Not sure how to read this key");
  } else {
    uint32_t is_substring = stream.get_bits(1);

    if (is_substring) {
      uint32_t from_index = stream.get_bits(5);
      uint32_t from_length = stream.get_bits(5);
      key_history[from_index].copy(buf, from_length, 0);

      stream.read_string(buf + from_length, buf_length - from_length);
    } else {
      stream.read_string(buf, buf_length);
    }
  }
}

void update_string_table(StringTable &table, size_t num_entries, const std::string &data) {
  // These do something with precaches. This isn't a client so I'm assuming this
  // is irrelevant.
  if (table.flags & 2) {
    return;
  }

  Bitstream stream(data);

  uint32_t first_bit = stream.get_bits(1);

  std::vector<std::string> key_history;

  uint32_t entry_id = -1;
  size_t entries_read = 0;
  while (entries_read < num_entries) {
    if (!stream.get_bits(1)) {
      entry_id = stream.get_bits(table.entry_bits);
    } else {
      entry_id += 1;
    }

    XASSERT(entry_id < table.max_entries, "Entry id too large");

    char key_buffer[MAX_KEY_SIZE];
    char *key = 0;
    if (stream.get_bits(1)) {
      read_string_table_key(first_bit, stream, key_buffer, MAX_KEY_SIZE, key_history);

      key = key_buffer;

      // So technically we should only store the first 32 characters but I'm lazy.
      if (key_history.size() == KEY_HISTORY_SIZE) {
        key_history.erase(key_history.begin());
      }

      key_history.push_back(key);
    }

    char value_buffer[MAX_VALUE_SIZE];
    char *value = 0;
    size_t bit_length = 0;
    size_t length = 0;
    if (stream.get_bits(1)) {
      if (table.flags & ST_FixedLength) {
        length = table.user_data_size;
        bit_length = table.user_data_size_bits;
      } else {
        length = stream.get_bits(14);
        bit_length = 8 * length;
      }

      XASSERT(length < MAX_VALUE_SIZE, "Message too long.");

      stream.read_bits(value_buffer, bit_length);
    }

    if (entry_id < table.count()) {
      StringTableEntry &item = table.get(entry_id);

      if (key) {
        XASSERT(item.key == key, "Entry's keys don't match.");
      }

      if (value) {
        // This is kind of bad because if the server sends us a zero length string we'll consider
        // it to be uninitialized.
        XASSERT(item.value.size() == 0, "String table already has this value");
        item.value = std::string(value, length);
      }
    } else {
      XASSERT(key, "Creating a new string table entry but no key specified.");

      table.put(key, std::string(value_buffer, length));
    }

    ++entries_read;
  }
}

void handle_SVC_CreateStringTable(const CSVCMsg_CreateStringTable &table) {
  XASSERT(state, "SVC_CreateStringTable but no state.");

  StringTable &converted = state->create_string_table(table.name(),
      (size_t) table.max_entries(), table.flags(), table.user_data_fixed_size(),
      table.user_data_size(), table.user_data_size_bits());

  update_string_table(converted, table.num_entries(), table.string_data());
}

void handle_SVC_UpdateStringTable(const CSVCMsg_UpdateStringTable &update) {
  XASSERT(state, "SVC_UpdateStringTable but no state.");

  StringTable &table = state->get_string_table(update.table_id());

  update_string_table(table, update.num_changed_entries(), update.string_data());
}

void dump_DEM_Packet(const CDemoPacket &packet, Visitor& visitor) {
  const char *data = packet.data().c_str();
  size_t offset = 0;
  size_t length = packet.data().length();

  while (offset < length) {
    uint32_t command = read_var_int(data, length, &offset);
    uint32_t size = read_var_int(data, length, &offset);
    XASSERT(offset + size <= length, "Reading data outside of packet.");

    if (command == svc_ServerInfo) {
      CSVCMsg_ServerInfo info;
      info.ParseFromArray(&(data[offset]), size);

      dump_SVC_ServerInfo(info);
    } else if (command == svc_PacketEntities) {
      CSVCMsg_PacketEntities entities;
      entities.ParseFromArray(&(data[offset]), size);

      dump_SVC_PacketEntities(entities, visitor);
    } else if (command == svc_CreateStringTable) {
      CSVCMsg_CreateStringTable table;
      table.ParseFromArray(&(data[offset]), size);

      handle_SVC_CreateStringTable(table);
    } else if (command == svc_UpdateStringTable) {
      CSVCMsg_UpdateStringTable table;
      table.ParseFromArray(&(data[offset]), size);

      handle_SVC_UpdateStringTable(table);
    }

    offset += size;
  }
}

void dump(const char *file, Visitor& visitor) {
  Demo demo(file);

  for (int frame = 0; !demo.eof(); ++frame) {
    int tick = 0;
    size_t size;
    bool compressed;
    size_t uncompressed_size;

    EDemoCommands command = demo.get_message_type(&tick, &compressed);
    demo.read_message(compressed, &size, &uncompressed_size);

    visitor.visit_tick(tick);

    if (command == DEM_ClassInfo) {
      CDemoClassInfo info;
      info.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      dump_DEM_ClassInfo(info);
    } else if (command == DEM_SendTables) {
      CDemoSendTables tables;
      tables.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      dump_DEM_SendTables(tables);
    } else if (command == DEM_Packet || command == DEM_SignonPacket) {
      CDemoPacket packet;
      packet.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      dump_DEM_Packet(packet, visitor);
    }
  }
}

