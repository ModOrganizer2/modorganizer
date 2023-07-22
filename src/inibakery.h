#ifndef INIBAKERY_H
#define INIBAKERY_H

#include <QString>

#include <filemapping.h>

class OrganizerCore;

// small classes that deal with preparing profiles before runs for local saves, bsa
// invalidation, etc., and providing mapping for local profile files when needed
//
// this class replaces the old INI Bakery plugin
//
class IniBakery
{
public:
  IniBakery(OrganizerCore& core);

  MappingType mappings() const;

private:
  bool prepareIni() const;

private:
  OrganizerCore& m_core;
};

#endif
