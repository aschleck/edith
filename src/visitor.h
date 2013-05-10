#ifndef _VISITOR_H
#define _VISITOR_H

class Entity;

class Visitor {
public:
  virtual ~Visitor() { }

  virtual void visit_tick(uint32_t tick) { }

  virtual void visit_entity_created(const Entity &entity) { }
  virtual void visit_entity_updated(const Entity &entity) { }
  virtual void visit_entity_deleted(const Entity &entity) { }
};

#endif
