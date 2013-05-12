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

std::vector<std::string> player_names;
std::map<uint32_t, uint32_t> selected_hero_id;

// This handles the CDOTA_PlayerResource entitiy.
// We want the 23 send props from the m_iszPlayerNames table named 0000-0023
// that contain the names of the (up to) 24 people connected and we want the 23
// send props from m_hSelectedHero that contain the entity ID of the heroes they
// selected.
void update_name_map(const Entity &player_resource) {
  using std::dynamic_pointer_cast;
  using std::shared_ptr;

  player_names.clear();
  selected_hero_id.clear();

  size_t selected_hero_count = 0;

  for (auto iter = player_resource.properties.begin();
      iter != player_resource.properties.end();
      ++iter) {
    shared_ptr<Property> prop = *iter;

    if (prop->name.find("m_iszPlayerNames.") == 0) {
      shared_ptr<StringProperty> name_prop = dynamic_pointer_cast<StringProperty>(prop);

      player_names.push_back(name_prop->value);
    } else if (prop->name.find("m_hSelectedHero.") == 0) {
      shared_ptr<IntProperty> selected = dynamic_pointer_cast<IntProperty>(prop);

      // Valve packs some additional data in the upper bits, we only care about the lower
      // ones.
      selected_hero_id[selected->value & 0x3FF] = selected_hero_count;

      ++selected_hero_count;
    }
  }

  XASSERT(player_names.size() == 24, "Player names not complete");
  XASSERT(selected_hero_count == 24, "Selected heroes not complete");
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

  shared_ptr<IntProperty> life_prop;
  shared_ptr<VectorXYProperty> origin_prop;
  shared_ptr<IntProperty> cell_x_prop;
  shared_ptr<IntProperty> cell_y_prop;
  shared_ptr<IntProperty> cell_z_prop;

  if (selected_hero_id.count(hero.id) == 0) {
    // An illusion.
    return;
  }

  for (auto iter = hero.properties.begin();
      iter != hero.properties.end();
      ++iter) {
    shared_ptr<Property> prop = *iter;

    if (prop->name == "DT_DOTA_BaseNPC.m_iHealth") {
      life_prop = dynamic_pointer_cast<IntProperty>(prop);
    } else if (prop->name == "DT_DOTA_BaseNPC.m_vecOrigin") {
      origin_prop = dynamic_pointer_cast<VectorXYProperty>(prop);
    } else if (prop->name == "DT_DOTA_BaseNPC.m_cellX") {
      cell_x_prop = dynamic_pointer_cast<IntProperty>(prop);
    } else if (prop->name == "DT_DOTA_BaseNPC.m_cellY") {
      cell_y_prop = dynamic_pointer_cast<IntProperty>(prop);
    } else if (prop->name == "DT_DOTA_BaseNPC.m_cellZ") {
      cell_z_prop = dynamic_pointer_cast<IntProperty>(prop);
    }
  }

  XASSERT(life_prop, "Unable to find m_iHealth");
  XASSERT(origin_prop, "Unable to find m_vecOrigin");
  XASSERT(cell_x_prop, "Unable to find m_cellX");
  XASSERT(cell_y_prop, "Unable to find m_cellY");
  XASSERT(cell_z_prop, "Unable to find m_cellZ");

  if (life_prop->value > 0) {
    return;
  }

  cout << tick << "," << hero.id << "," << hero.clazz->name << ",";
  cout << "\"" << player_names[selected_hero_id[hero.id]] << "\",";
  cout << life_prop->value << ",";
  cout << origin_prop->values[0] << ",";
  cout << origin_prop->values[1] << ",";
  cout << cell_x_prop->value << ",";
  cout << cell_y_prop->value << ",";
  cout << cell_z_prop->value << endl;
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
