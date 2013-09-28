// Maps player names to the entity of the hero they selected and outputs the position
// of that hero everytime they are updated and their health is set to zero.
//
// This shows an example usage of this API but is hilariously inefficient and terrible.

#include <stdexcept>
#include <iostream>
#include <string>
#include <array>
#include <map>

#include "debug.h"
#include "property.h"
#include "visitor.h"
#include "edith.h"

// TODO: go up to 24 if interested in others, like casters.
const size_t NUM_PLAYERS_TO_TRACK = 10;

uint32_t tick = 0;

std::array<std::string, NUM_PLAYERS_TO_TRACK> player_name_prop_names;
std::array<std::string, NUM_PLAYERS_TO_TRACK> selected_hero_prop_names;

std::map<uint32_t, std::string> hero_to_playername;
std::map<uint32_t, int> hero_previous_life;

// This generates the prop names we're interested in. These are formatted like
// m_iszPlayerNames.0000 and m_hSelectedHero.0000
void generate_prop_names() {
  for (size_t i = 0; i < NUM_PLAYERS_TO_TRACK; ++i) {
    char as_string[30];

    sprintf(as_string, "m_iszPlayerNames.%04lu", i);
    player_name_prop_names[i] = as_string;

    sprintf(as_string, "m_hSelectedHero.%04lu", i);
    selected_hero_prop_names[i] = as_string;
  }
}

// This handles the CDOTA_PlayerResource entitiy.
// We want the 24 send props from the m_iszPlayerNames table named 0000-0023
// that contain the names of the (up to) 24 people connected and we want the 24
// send props from m_hSelectedHero that contain the entity ID of the heroes they
// selected.
void update_name_map(const Entity &player_resource) {
  for (size_t iPlayer = 0; iPlayer < NUM_PLAYERS_TO_TRACK; ++iPlayer) {
    auto name_prop = player_resource.properties.at(player_name_prop_names[iPlayer]);
    auto selected_prop = player_resource.properties.at(selected_hero_prop_names[iPlayer]);

    // Valve packs some additional data in the upper bits, we only care about the lower
    // ones.
    int heroid = selected_prop->value_as<IntProperty>() & 0x7FF;
    hero_to_playername[heroid] = name_prop->value_as<StringProperty>();
  }
}

// This handles CDOTA_Unit_Hero_* entities.
// We first check if it's in our name map, and if it isn't then it must be an illusion
// or something. After that we make sure the hero has 0 health and if it does we output
// it.
void update_hero(const Entity &hero) {
  using std::dynamic_pointer_cast;
  using std::cout;
  using std::endl;

  if (hero_to_playername.count(hero.id) == 0) {
    // An illusion.
    return;
  }

  try {
    int life = hero.properties.at("DT_DOTA_BaseNPC.m_iHealth")->value_as<IntProperty>();
    // Note that we get multiple entity updates even when it died, but we
    // only want one output per death. That's why we only output stuff when
    // the life drops below zero for the first time.
    // (Think of someone dying from the hook, its corpse gets carried along but
    //  we are only interested in the death moment!)
    if (life > 0 || hero_previous_life[hero.id] <= 0) {
      hero_previous_life[hero.id] = life;
      return;
    }
    hero_previous_life[hero.id] = life;

    int cell_x = hero.properties.at("DT_DOTA_BaseNPC.m_cellX")->value_as<IntProperty>();
    int cell_y = hero.properties.at("DT_DOTA_BaseNPC.m_cellY")->value_as<IntProperty>();
    int cell_z = hero.properties.at("DT_DOTA_BaseNPC.m_cellZ")->value_as<IntProperty>();
    auto origin_prop = dynamic_pointer_cast<VectorXYProperty>(hero.properties.at("DT_DOTA_BaseNPC.m_vecOrigin"));

    cout << tick << "," << hero.id << "," << hero.clazz->name << ",";
    cout << "\"" << hero_to_playername[hero.id] << "\",";
    cout << life << ",";
    cout << origin_prop->values[0] << ",";
    cout << origin_prop->values[1] << ",";
    cout << cell_x << ",";
    cout << cell_y << ",";
    cout << cell_z << endl;
  } catch(const std::out_of_range& e) {
    XERROR("%s", e.what());
  } catch(const std::bad_cast& e) {
    XERROR("%s", e.what());
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

    generate_prop_names();
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

int main(int argc, char **argv) {
    if (argc <= 1) {
        std::cerr << "Usage: " << argv[0] << " something.dem" << std::endl;
        return 1;
    }

    DeathRecordingVisitor visitor;
    dump(argv[1], visitor);
    return 0;
}

