#ifndef INIBAKERY_H
#define INIBAKERY_H

#include <QString>

#include <filemapping.h>

class OrganizerCore;

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
