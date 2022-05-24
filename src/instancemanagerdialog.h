#ifndef MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
#define MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED

#include <QDialog>
#include <filterwidget.h>

namespace Ui
{
class InstanceManagerDialog;
};

class Instance;
class PluginManager;

// a dialog to manage existing instances
//
class InstanceManagerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit InstanceManagerDialog(PluginManager& pc, QWidget* parent = nullptr);

  ~InstanceManagerDialog();

  // selects the instance having the given index in the list
  //
  void select(std::size_t i);

  // selects the instance by name
  //
  void select(const QString& name);

  // select the instance that is currently in use in MO
  //
  void selectActiveInstance();

  // switches to the selected instance; restarts MO, unless
  // was called setRestartOnSelect(false)
  //
  void openSelectedInstance();

  // renames the currently selected instance
  //
  void rename();

  // explores the directory of the selected instance
  //
  void exploreLocation();

  // explores the base directory of the selected instance
  //
  void exploreBaseDirectory();

  // explores the game directory of the selected instance
  //
  void exploreGame();

  // converts the selected, portable instance to a global one; not implemented
  //
  void convertToGlobal();

  // converts the selected, global instance to a portable one; not implemented
  //
  void convertToPortable();

  // opens the ini of the selected instance in the shell
  //
  void openINI();

  // deletes the selected instance
  //
  void deleteInstance();

  // sets whether the dialog should restart MO when selecting an instance; this
  // is false on startup when no instances exist
  //
  void setRestartOnSelect(bool b);

  // saves geometry
  //
  void done(int r) override;

protected:
  // restores geometry
  //
  void showEvent(QShowEvent* e) override;

private:
  static const std::size_t NoSelection = -1;

  std::unique_ptr<Ui::InstanceManagerDialog> ui;
  PluginManager& m_pc;
  std::vector<std::unique_ptr<Instance>> m_instances;
  MOBase::FilterWidget m_filter;
  QStandardItemModel* m_model;
  bool m_restartOnSelect;

  // refreshes the list instances from disk
  //
  void updateInstances();

  // updates the ui for the selected instance
  //
  void onSelection();

  // opens the create instance dialog
  //
  void createNew();

  // shows a confirmation to the user before switching
  //
  bool confirmSwitch(const Instance& to);

  // returns the index of selected instance, NoSelection if none
  //
  std::size_t singleSelectionIndex() const;

  // returns the InstanceInfo associated with the selected instance, null if
  // none
  //
  const Instance* singleSelection() const;

  // fills the instance list on the ui
  //
  void updateList();

  // fills the ui for the selected instance
  //
  void fillData(const Instance& ii);

  // clears the ui when there's no selection
  //
  void clearData();

  // enables/disables buttons like rename, explore...
  //
  void setButtonsEnabled(bool b);

  // deletes the given files, returns false on error
  //
  bool doDelete(const QStringList& files, bool recycle);
};

#endif  // MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
