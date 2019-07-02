#ifndef MODINFODIALOGTAB_H
#define MODINFODIALOGTAB_H

#include "modinfodialogfwd.h"
#include <QObject>

namespace MOShared { class FilesOrigin; }
namespace Ui { class ModInfoDialog; }

class Settings;
class OrganizerCore;

// helper struct to avoid passing too much stuff to tab constructors
//
struct ModInfoDialogTabContext
{
  OrganizerCore& core;
  PluginContainer& plugin;
  QWidget* parent;
  Ui::ModInfoDialog* ui;
  ModInfoTabIDs id;
  ModInfoPtr mod;
  MOShared::FilesOrigin* origin;

  ModInfoDialogTabContext(
    OrganizerCore& core,
    PluginContainer& plugin,
    QWidget* parent,
    Ui::ModInfoDialog* ui,
    ModInfoTabIDs id,
    ModInfoPtr mod,
    MOShared::FilesOrigin* origin) :
      core(core), plugin(plugin), parent(parent), ui(ui), id(id),
      mod(mod), origin(origin)
  {
  }
};


// base class for all tabs in the mod info dialog
//
// when the dialog is opened or when next/previous is clicked, the sequence is:
// setMod(), clear(), feedFile() an update()
//
// when the dialog is closed, canClose() is called on all tabs
//
// when a tab is selected for the first time for the current mod,
// firstActivation() is called; this is used by NexusTab to refresh stuff
//
// when the dialog is first shown, restoreState() is called on all tabs and
// saveState() is called when the dialog is closed
//
// there isn't a good framework for keyboard shortcuts because only the delete
// key is used for now, which calls deletedRequested() on all tabs until one
// returns true
//
// each tab override canHandleSeparators() and canHandleUnmanaged() to return
// true if they can handle separators or unmanaged mods; if these return false
// (which they do by default), the tabs will be removed from the widget entirely
//
// when tabs modify the origin and call emitOriginModified() (such as the
// conflicts tabs), all tabs that return true in usesOriginFiles() will go
// through the full update sequence as above
//
// tabs can call emitModOpen() to request showing a different mod
//
// hasDataChanged() should be called when a tab goes from having data to being
// empty or vice versa; this will update the tab text colour
//
class ModInfoDialogTab : public QObject
{
  Q_OBJECT;

public:
  ModInfoDialogTab(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab& operator=(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab(ModInfoDialogTab&&) = default;
  ModInfoDialogTab& operator=(ModInfoDialogTab&&) = default;
  virtual ~ModInfoDialogTab() = default;

  // called by ModInfoDialog every time this tab is selected; this will call
  // firstActivation() the first time it's called, until resetFirstActivation()
  // is called
  //
  void activated();

  // called by ModInfoDialog when the selected mod has changed, to make sure
  // activated() will call firstActivation() next time
  //
  void resetFirstActivation();


  // called when the selected mod changed, `mod` can never be empty, but
  // `origin` can (if the mod is not active, for example)
  //
  // derived classes can override this to connect to events on the mod for
  // examples (see NexusTab), but must call the base class implementation
  //
  virtual void setMod(ModInfoPtr mod, MOShared::FilesOrigin* origin);

  // this tab should clear its user interface; clear() will always be called
  // before feedFile() and update()
  //
  virtual void clear() = 0;

  // the dialog will go through each file in the mod and call feedFile()
  // with it on all tabs; if a tab handles the file, it should return true to
  // prevent other tabs from displaying it
  //
  // this prevents individual tabs from having to go through the filesystem
  // independently, which would kill performance, but it cannot be the only way
  // to update tabs because some of them don't actually use files (like
  // NotesTab) or they use the internal structures such as `FilesOrigin` (like
  // ConflictsTab)
  //
  // once all the files have been fed to tabs, update() will be called; tabs
  // that need to do additional work after being fed files, or tabs that are
  // unable to use feedFile() at all can use update() to do work
  //
  virtual bool feedFile(const QString& rootPath, const QString& filename);

  // called after all the files on the filesystem have been sent through
  // feedFile()
  //
  // this can be used to do a final processing of the files handled by
  // feedFile() (ImagesTab will set up the scrollbars, for example) or something
  // completely different (CategoriesTab will fill up the lists)
  //
  virtual void update();

  // called when a tab is visible for the first time for the current mod, can
  // be used to do expensive work that's not worth doing until the tab is
  // actually selected by the user
  //
  virtual void firstActivation();

  // called when closing the dialog, can return false to stop the dialog from
  // closing
  //
  // this is typically used by tabs that require manual saving, like text files;
  // tabs that refuse to close should focus themselves before showing whatever
  // confirmation they have
  //
  virtual bool canClose();


  // called after the dialog is closed, tabs should save whatever UI state they
  // want
  //
  virtual void saveState(Settings& s);

  // called before the is shown, tabs should restore whatever UI state they
  // saved in saveState()
  //
  virtual void restoreState(const Settings& s);


  // called on the selected tab when the Delete key is pressed on the keyboard;
  // tabs _must_ check which widget currently has focus to decide whether this
  // should be handled or not; do not blindly delete stuff when this is called
  //
  // if the delete request was handled, this should return true
  //
  virtual bool deleteRequested();


  // return true if this tab can handle a separator mod, defaults to false;
  // when this returns false, the tab is removed from the widget entirely
  //
  // if a tab can show meaningful information about a separator (like
  // categories or notes), it should return true
  //
  virtual bool canHandleSeparators() const;

  // return true if this tab can handle unmanaged mods, defaults to false;
  // when this returns false, the tab is removed from the widget entirely
  //
  virtual bool canHandleUnmanaged() const;

  // return true if this tab uses the files from the mod's origin, defaults to
  // false
  //
  // tabs that do not care about the files inside a mod should return false,
  // such as the notes or categories tab
  //
  // mods that return true will be updated anytime a tab calls
  // emitOriginModifed()
  //
  virtual bool usesOriginFiles() const;


  // returns the currently selected mod
  //
  ModInfo& mod() const;

  // returns the currently selected mod, can never be empty
  //
  ModInfoPtr modPtr() const;

  // returns the origin of the selected mod; this can be null for mods that
  // don't have an origin, like deactivated mods
  //
  MOShared::FilesOrigin* origin() const;


  ModInfoTabIDs tabID() const;
  bool hasData() const;

signals:
  void originModified(int originID);
  void modOpen(QString name);
  void hasDataChanged();
  void wantsFocus();

protected:
  Ui::ModInfoDialog* ui;

  ModInfoDialogTab(ModInfoDialogTabContext cx);

  OrganizerCore& core();
  PluginContainer& plugin();

  QWidget* parentWidget();

  void emitOriginModified();
  void emitModOpen(QString name);
  void setHasData(bool b);

  void setFocus();

  // this needs to be a template because saveState() and restoreState() are
  // not in QWidget, but they're in various widgets
  //
  template <class Widget>
  void saveWidgetState(QSettings& s, Widget* w)
  {
    s.setValue(settingName(w), w->saveState());
  }

  template <class Widget>
  void restoreWidgetState(const QSettings& s, Widget* w)
  {
    if (s.contains(settingName(w))) {
      w->restoreState(s.value(settingName(w)).toByteArray());
    }
  }

private:
  OrganizerCore& m_core;
  PluginContainer& m_plugin;
  QWidget* m_parent;
  ModInfoPtr m_mod;
  MOShared::FilesOrigin* m_origin;
  ModInfoTabIDs m_tabID;
  bool m_hasData;
  bool m_firstActivation;


  QString settingName(QWidget* w)
  {
    return "geometry/modinfodialog_" + w->objectName();
  }
};


class NotesTab : public ModInfoDialogTab
{
public:
  NotesTab(ModInfoDialogTabContext cx);

  void clear() override;
  void update() override;
  bool canHandleSeparators() const override;
  bool usesOriginFiles() const override;

private:
  void onComments();
  void onNotes();
  void checkHasData();
};

#endif // MODINFODIALOGTAB_H
