#include <stdint.h>
#include <iostream>

#include "generated_proto/demo.pb.h"
#include "generated_proto/netmessages.pb.h"

#include "bitstream.h"
#include "debug.h"
#include "decoder.h"
#include "demo.h"
#include "state.h"
#include "value.h"

#define INSTANCE_BASELINE_TABLE "instancebaseline"
#define MAX_EDICTS 0x800

enum UpdateFlag {
  UF_LeavePVS = 1,
  UF_Delete = 2,
  UF_EnterPVS = 4,
};

State *state = NULL;

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

  printf("Got sendtable %s.\n", table.net_table_name().c_str());

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

    converted.add(c);
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

  printf("Done with SendTables.\n");
}

const StringTableEntry &get_baseline_for(int class_i) {
  XASSERT(state, "No state created.");

  StringTable &instance_baseline = state->get_string_table(INSTANCE_BASELINE_TABLE);

  char buf[32];
  sprintf(buf, "%d", class_i);

  return instance_baseline.get(buf);
}

unsigned int read_entity_header(int *base, Bitstream &stream) {
  int new_entity = *base + 1 + stream.read_var_uint();
  *base = new_entity;

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

void read_field(uint32_t &last_field, Bitstream &stream) {
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
  read_field(last_field, stream);

  while (last_field != 0xFFFFFFFF) {
    fields.push_back(last_field);

    read_field(last_field, stream);
  }
}

void read_entity_enter_pvs(uint32_t update_flags, uint32_t entity, Bitstream &stream) {
  uint32_t class_i = stream.get_bits(state->class_bits);
  uint32_t serial = stream.get_bits(10);

  XASSERT(entity < MAX_EDICTS, "Entity %ld exceeds max edicts.", entity);

  const Class &clazz = state->get_class(class_i);
  const StringTableEntry &baseline = get_baseline_for(class_i);

  const FlatSendTable &flat_send_table = state->flat_send_tables[clazz.dt_name];

  std::cout << flat_send_table.net_table_name << std::endl;

  Bitstream baseline_stream(baseline.value);

  std::vector<uint32_t> fields;
  read_field_list(fields, baseline_stream);

  for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
    Value value(flat_send_table.props[*iter]);
    value.read_from(baseline_stream);
  }

  std::cout << "Done with baseline" << std::endl;

  fields.clear();
  read_field_list(fields, stream);

  for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
    Value value(flat_send_table.props[*iter]);
    value.read_from(stream);
  }

  std::cout << "Done with stream" << std::endl;

  printf("Created a %s (serial=%u).\n", clazz.name.c_str(), serial);
}

void dump_SVC_PacketEntities(const CSVCMsg_PacketEntities &entities) {
  printf("pe is_delta? %d update_baseline? %d baseline? %d delta_from? %d updated_entries? %d size? %ld\n", entities.is_delta(),
      entities.update_baseline(), entities.baseline(), entities.delta_from(), entities.updated_entries(), entities.entity_data().length());

  Bitstream stream(entities.entity_data());

  int entity_id = -1;
  int found = 0;
  uint32_t update_type = read_entity_header(&entity_id, stream);

  while (found < entities.updated_entries()) {
    printf("Got %d %d.\n", entity_id, update_type);

    if (update_type & UF_EnterPVS) {
      read_entity_enter_pvs(update_type, entity_id, stream);
    } else if (update_type & UF_LeavePVS) {
      XERROR("Not implemented.");
    } else {
      XERROR("Not implemented.");
    }

    ++found;
    update_type = read_entity_header(&entity_id, stream);
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

  state->compile_send_tables();
}

void update_string_table(StringTable &table, size_t num_entries, std::string data) {
  Bitstream stream(data);

  size_t entries_read = 0;
  
  uint32_t some_var = stream.get_bits(1);
  uint32_t some_other_var_4854 = 0;

  int32_t entry_id = -1;
  while (entries_read < num_entries) {
    if (!stream.get_bits(1)) {
      entry_id = stream.get_bits(table.entry_bits);
    } else {
      entry_id = entry_id + 1;
    }

    XASSERT(entry_id >= 0, "Entry id is less than zero!");

    char *key = NULL;
    if (stream.get_bits(1)) {
      key = new char[MAX_KEY_SIZE];

      if (!some_var) {
        if (!stream.get_bits(1)) {
          stream.read_string(key, MAX_KEY_SIZE);
        } else {
          XERROR("no");
        }
      } else {
        XERROR("please no");
      }
    }

    char *value = NULL;
    size_t length;
    if (stream.get_bits(1)) {
      if (!(table.flags & STF_FixedLength)) {
        length = stream.get_bits(14);

        XASSERT(length < MAX_MESSAGE_SIZE, "Message too long.");
      } else {
        length = table.user_data_size;
      }

      value = new char[length];
      stream.read_bits(value, 8 * length);
    }

    if (some_other_var_4854 >= 31) {
      --some_other_var_4854;

      if (some_other_var_4854 > 0) {
        XERROR("no stop");
      }
    }

    StringTableEntry *item;
    if (entry_id < table.count()) {
      item = &(table.get(entry_id));

      XASSERT(item->key == key, "Entry's keys don't match.");
    } else {
      XASSERT(key, "Creating a new string table entry but no key specified.");

      // value should be null? may cause issues?
      item = &(table.put(key, ""));
    }

    if (key) {
      delete[] key;
    }

    if (value) {
      item->value = std::string(value, length);
      delete[] value;
    }

    ++entries_read;
  }
}

void handle_SVC_CreateStringTable(const CSVCMsg_CreateStringTable &table) {
  XASSERT(state, "SVC_CreateStringTable but no state.");

  printf("Got string table %s\n", table.name().c_str());

  StringTable &converted = state->create_string_table(table.name(),
      (size_t) table.max_entries(), table.flags(), table.user_data_fixed_size(),
      table.user_data_size(), table.user_data_size_bits());

  // I don't want to deal with other string tables right now.
  if (table.name() == "instancebaseline") {
    update_string_table(converted, table.num_entries(), table.string_data());
  }
}

void handle_SVC_UpdateStringTable(const CSVCMsg_UpdateStringTable &update) {
  XASSERT(state, "SVC_UpdateStringTable but no state.");

  StringTable &table = state->get_string_table(update.table_id());
  update_string_table(table, update.num_changed_entries(), update.string_data());
}

void dump_DEM_Packet(const CDemoPacket &packet) {
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

      dump_SVC_PacketEntities(entities);
    } else if (command == svc_CreateStringTable) {
      CSVCMsg_CreateStringTable table;
      table.ParseFromArray(&(data[offset]), size);

      handle_SVC_CreateStringTable(table);
    } else if (command == svc_UpdateStringTable) {
      CSVCMsg_UpdateStringTable table;
      table.ParseFromArray(&(data[offset]), size);

      handle_SVC_UpdateStringTable(table);
    } else {
      //printf("Skipped command %u with %u bytes.\n", command, size);
    }

    offset += size;
  }
}

void dump(const char *file) {
  Demo demo(file);

  for (int frame = 0; !demo.eof(); ++frame) {
    int tick = 0; 
    size_t size;
    bool compressed;
    size_t uncompressed_size;

    EDemoCommands command = demo.get_message_type(&tick, &compressed);
    demo.read_message(compressed, &size, &uncompressed_size);

    if (command == DEM_ClassInfo) {
      CDemoClassInfo info;
      info.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      dump_DEM_ClassInfo(info);
    } else if (command == DEM_SendTables) {
      CDemoSendTables tables;
      tables.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      dump_DEM_SendTables(tables);
    } else if (command == DEM_FullPacket) {
      CDemoFullPacket packet;
      packet.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      printf("Read full packet (%lu/%lu).\n", size, uncompressed_size);
      printf("%s\n", packet.string_table().DebugString().c_str());
      dump_DEM_Packet(packet.packet());
    } else if (command == DEM_Packet || command == DEM_SignonPacket) {
      CDemoPacket packet;
      packet.ParseFromArray(demo.expose_buffer(), uncompressed_size);

      printf("Read packet (%lu/%lu).\n", size, uncompressed_size);
      dump_DEM_Packet(packet);
    } else {
      //printf("Skipped command %u %lu (%lu) bytes.\n", command, size, uncompressed_size);
    }
  }
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("Usage: %s something.dem\n", argv[0]);
    exit(1);
  } else {
    dump(argv[1]);
  }

  return 0;
}

