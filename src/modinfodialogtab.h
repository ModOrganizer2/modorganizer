#ifndef MODINFODIALOGTAB_H
#define MODINFODIALOGTAB_H

#include "modinfo.h"
#include <QObject>

namespace MOShared { class FilesOrigin; }
namespace Ui { class ModInfoDialog; }

class Settings;

class ModInfoDialogTab : public QObject
{
  Q_OBJECT;

public:
  ModInfoDialogTab() = default;
  ModInfoDialogTab(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab& operator=(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab(ModInfoDialogTab&&) = default;
  ModInfoDialogTab& operator=(ModInfoDialogTab&&) = default;
  virtual ~ModInfoDialogTab() = default;

  virtual void clear() = 0;
  virtual bool feedFile(const QString& rootPath, const QString& filename) = 0;
  virtual bool canClose();
  virtual void saveState(Settings& s);
  virtual void restoreState(const Settings& s);

  virtual void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  virtual void update();

signals:
  void originModified(int originID);
  void modOpen(QString name);

protected:
  void emitOriginModified(int originID);
  void emitModOpen(QString name);
};

#endif // MODINFODIALOGTAB_H
