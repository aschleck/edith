#include <stdint.h>
#include <iostream>

#include "generated_proto/demo.pb.h"
#include "generated_proto/netmessages.pb.h"

#include "bitstream.h"
#include "debug.h"
#include "decoder.h"
#include "demo.h"
#include "entity.h"
#include "state.h"

#define INSTANCE_BASELINE_TABLE "instancebaseline"
#define MAX_EDICTS 0x800

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

  printf("Done with SendTables.\n");
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

void read_entity_enter_pvs(uint32_t entity_id, Bitstream &stream) {
  uint32_t class_i = stream.get_bits(state->class_bits);
  uint32_t serial = stream.get_bits(10);

  XASSERT(entity_id < MAX_EDICTS, "Entity %ld exceeds max edicts.", entity_id);

  const Class &clazz = state->get_class(class_i);
  const FlatSendTable &flat_send_table = state->flat_send_tables[clazz.dt_name];

  Entity &entity = state->entities[entity_id];
  XASSERT(entity.id == -1, "Entity %d already exists.", entity_id);
  entity = Entity(entity_id, flat_send_table);

  const StringTableEntry &baseline = get_baseline_for(class_i);
  Bitstream baseline_stream(baseline.value);
  entity.update(baseline_stream);

  entity.update(stream);

  std::cout << "Created a " << clazz.name << " (id=" << entity_id << ", serial=" << serial << ")" << std::endl;
}

void read_entity_update(uint32_t entity_id, Bitstream &stream) {
  XASSERT(entity_id < MAX_ENTITIES, "Entity id too big");

  Entity &entity = state->entities[entity_id];
  XASSERT(entity.id != -1, "Entity %d is not set up.", entity_id);

  entity.update(stream);

  std::cout << "Updated a " << entity.table->net_table_name << std::endl;
}

void dump_SVC_PacketEntities(const CSVCMsg_PacketEntities &entities) {
  static size_t num = 0;
  if (++num == 1) {
    return;
  }

  std::cout << num << std::endl;
  printf("pe is_delta? %d update_baseline? %d baseline? %d delta_from? %d updated_entries? %d size? %ld\n", entities.is_delta(),
      entities.update_baseline(), entities.baseline(), entities.delta_from(), entities.updated_entries(), entities.entity_data().length());

  Bitstream stream(entities.entity_data());

  uint32_t entity_id = -1;
  size_t found = 0;
  uint32_t update_type;

  while (found < entities.updated_entries()) {
    update_type = read_entity_header(&entity_id, stream);

    //printf("Got %d %d.\n", entity_id, update_type);

    if (update_type & UF_EnterPVS) {
      read_entity_enter_pvs(entity_id, stream);
    } else if (update_type & UF_LeavePVS) {
      XASSERT(entities.is_delta(), "Leave PVS on full update");

      std::cout << "Leave PVS?" << std::endl;

      if (update_type & UF_Delete) {
        std::cout << "Deleting entity " << entity_id << std::endl;
        state->entities[entity_id].id = -1;
      }
    } else {
      read_entity_update(entity_id, stream);
    }

    ++found;
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

      if (prop.type == SP_Array) {
        XASSERT(i > 0, "Array prop %s is at index zero.", prop.var_name.c_str());
        prop.array_prop = &(table.props[i - 1]);
      }
    }
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

    char *key = 0;
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

    char *value = 0;
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

void clear_entities() {
  for (size_t i = 0; i < MAX_ENTITIES; ++i) {
    state->entities[i].id = -1;
  }
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

      clear_entities();
      dump_DEM_Packet(packet.packet());
    } else if (command == DEM_Packet || command == DEM_SignonPacket) {
      CDemoPacket packet;
      packet.ParseFromArray(demo.expose_buffer(), uncompressed_size);

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

