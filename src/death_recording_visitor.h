#ifndef _DEATH_RECORDING_VISITOR_H
#define _DEATH_RECORDING_VISITOR_H

// Maps player names to the entity of the hero they selected and outputs the position
// of that hero everytime they are updated and their health is set to zero.
//
// This shows an example usage of this API but is hilariously inefficient and terrible.

#include <iostream>
#include <map>
#include <string>

#include "debug.h"
#include "property.h"
#include "visitor.h"

uint32_t tick = 0;

std::array<std::string, 10> player_names;
std::map<uint32_t, uint32_t> selected_hero_id;

// This handles the CDOTA_PlayerResource entitiy.
// We want the 24 send props from the m_iszPlayerNames table named 0000-0023
// that contain the names of the (up to) 24 people connected and we want the 24
// send props from m_hSelectedHero that contain the entity ID of the heroes they
// selected.
void update_name_map(const Entity &player_resource) {
  using std::dynamic_pointer_cast;
  using std::shared_ptr;

  // TODO: go up to 23 (inclusive) if interested in others, like casters.
  player_names[0] = player_resource.properties.at("m_iszPlayerNames.0000")->value_as<StringProperty>();
  player_names[1] = player_resource.properties.at("m_iszPlayerNames.0001")->value_as<StringProperty>();
  player_names[2] = player_resource.properties.at("m_iszPlayerNames.0002")->value_as<StringProperty>();
  player_names[3] = player_resource.properties.at("m_iszPlayerNames.0003")->value_as<StringProperty>();
  player_names[4] = player_resource.properties.at("m_iszPlayerNames.0004")->value_as<StringProperty>();
  player_names[5] = player_resource.properties.at("m_iszPlayerNames.0005")->value_as<StringProperty>();
  player_names[6] = player_resource.properties.at("m_iszPlayerNames.0006")->value_as<StringProperty>();
  player_names[7] = player_resource.properties.at("m_iszPlayerNames.0007")->value_as<StringProperty>();
  player_names[8] = player_resource.properties.at("m_iszPlayerNames.0008")->value_as<StringProperty>();
  player_names[9] = player_resource.properties.at("m_iszPlayerNames.0009")->value_as<StringProperty>();
      // Valve packs some additional data in the upper bits, we only care about the lower
      // ones.
  int selected_hero_count = 0;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0000")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0001")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0002")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0003")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0004")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0005")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0006")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0007")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0008")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;
  selected_hero_id[player_resource.properties.at("m_hSelectedHero.0009")->value_as<IntProperty>() & 0x7FF] = selected_hero_count++;

  // XASSERT(player_names.size() == 24, "Player names not complete");
  // XASSERT(selected_hero_count == 24, "Selected heroes not complete");
}

// This handles CDOTA_Unit_Hero_* entities.
// We first check if it's in our name map, and if it isn't then it must be an illusion
// or something. After that we make sure the hero has 0 health and if it does we output
// it.
void update_hero(const Entity &hero) {
  using std::dynamic_pointer_cast;
  using std::shared_ptr;
  using std::cout;
  using std::endl;

  if (selected_hero_id.count(hero.id) == 0) {
    // An illusion.
    return;
  }

  try {
    int life = hero.properties.at("DT_DOTA_BaseNPC.m_iHealth")->value_as<IntProperty>();
    if (life > 0) {
      return;
    }

    int cell_x = hero.properties.at("DT_DOTA_BaseNPC.m_cellX")->value_as<IntProperty>();
    int cell_y = hero.properties.at("DT_DOTA_BaseNPC.m_cellY")->value_as<IntProperty>();
    int cell_z = hero.properties.at("DT_DOTA_BaseNPC.m_cellZ")->value_as<IntProperty>();
    auto origin_prop = dynamic_pointer_cast<VectorXYProperty>(hero.properties.at("DT_DOTA_BaseNPC.m_vecOrigin"));

    cout << tick << "," << hero.id << "," << hero.clazz->name << ",";
    cout << "\"" << player_names[selected_hero_id[hero.id]] << "\",";
    cout << life << ",";
    cout << origin_prop->values[0] << ",";
    cout << origin_prop->values[1] << ",";
    cout << cell_x << ",";
    cout << cell_y << ",";
    cout << cell_z << endl;
  } catch(const std::out_of_range& e) {
    XASSERT(false, "%s", e.what());
  } catch(const std::bad_cast& e) {
    XASSERT(false, "%s", e.what());
  }
}

void handle_entity(const Entity &entity) {
  if (entity.clazz->name == "CDOTA_PlayerResource") {
    update_name_map(entity);
  } else if (entity.clazz->name.find("CDOTA_Unit_Hero_") == 0) {
    update_hero(entity);
  }
}

class DeathRecordingVisitor : public Visitor {
public:
  DeathRecordingVisitor() {
    std::cout << "tick,entity_id,class_name,player_name,health,x,y,cx,cy,cz" << std::endl;
  }

  virtual void visit_tick(uint32_t t) {
    tick = t;
  }

  virtual void visit_entity_created(const Entity &entity) {
    handle_entity(entity);
  }

  virtual void visit_entity_updated(const Entity &entity) {
    handle_entity(entity);
  }

  virtual void visit_entity_deleted(const Entity &entity) {
  }
};

#endif
