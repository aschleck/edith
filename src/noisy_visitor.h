#ifndef _NOISY_VISITOR_H
#define _NOISY_VISITOR_H

#include <iostream>

#include "visitor.h"

class NoisyVisitor : public Visitor {
public:
  virtual void visit_tick(uint32_t tick) {
    std::cout << "Tick " << tick << std::endl;
  }

  virtual void visit_entity_created(const Entity &entity) {
    std::cout << "Created " << entity.id << " (" << entity.clazz->name << ")" << std::endl;
  }

  virtual void visit_entity_updated(const Entity &entity) {
    std::cout << "Updated " << entity.id << " (" << entity.clazz->name << ")" << std::endl;
  }

  virtual void visit_entity_deleted(const Entity &entity) {
    std::cout << "Deleted " << entity.id << " (" << entity.clazz->name << ")" << std::endl;
  }
};

#endif
