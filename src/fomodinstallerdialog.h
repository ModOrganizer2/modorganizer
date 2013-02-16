/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOMODINSTALLERDIALOG_H
#define FOMODINSTALLERDIALOG_H

#include "installdialog.h"
#include <QDialog>
#include <QAbstractButton>
#include <QXmlStreamReader>
#include <QGroupBox>
#include <QSharedPointer>

namespace Ui {
class FomodInstallerDialog;
}


class Condition : public QObject {
  Q_OBJECT
public:
  Condition(QObject *parent = NULL) : QObject(parent) { }
  Condition(const Condition &reference) : QObject(reference.parent()), m_Name(reference.m_Name), m_Value(reference.m_Value) { }
  Condition(const QString &name, const QString &value) : QObject(), m_Name(name), m_Value(value) { }
  QString m_Name;
  QString m_Value;
private:
  Condition &operator=(const Condition&);
};

Q_DECLARE_METATYPE(Condition)


class FileDescriptor : public QObject {
  Q_OBJECT
public:
  FileDescriptor(QObject *parent)
    : QObject(parent), m_Source(), m_Destination(), m_Priority(0), m_IsFolder(false), m_AlwaysInstall(false),
      m_InstallIfUsable(false) {}
  FileDescriptor(const FileDescriptor &reference)
    : QObject(reference.parent()), m_Source(reference.m_Source), m_Destination(reference.m_Destination),
      m_Priority(reference.m_Priority), m_IsFolder(reference.m_IsFolder), m_AlwaysInstall(reference.m_AlwaysInstall),
      m_InstallIfUsable(reference.m_InstallIfUsable) {}
  QString m_Source;
  QString m_Destination;
  int m_Priority;
  bool m_IsFolder;
  bool m_AlwaysInstall;
  bool m_InstallIfUsable;
private:
  FileDescriptor &operator=(const FileDescriptor&);
};

Q_DECLARE_METATYPE(FileDescriptor*)


class FomodInstallerDialog : public QDialog
{
  Q_OBJECT
  
public:
  explicit FomodInstallerDialog(const QString &modName, bool nameWasGuessed, const QString &fomodPath, QWidget *parent = 0);
  ~FomodInstallerDialog();

  void initData();

  /**
   * @return bool true if the user requested the manual dialog
   **/
  bool manualRequested() const { return m_Manual; }

  /**
   * @return QString the (user-modified) name to be used for the mod
   **/
  QString getName() const;

  /**
   * @brief retrieve the updated archive tree from the dialog. The caller is responsible to delete the returned tree.
   *
   * @note This call is destructive on the input tree!
   *
   * @param tree input tree. (TODO isn't this the same as the tree passed in the constructor?)
   * @return DataTree* a new tree with only the selected options and directories arranged correctly. The caller takes custody of this pointer!
   **/
  DirectoryTree *updateTree(DirectoryTree *tree);

protected:

  virtual bool eventFilter(QObject *object, QEvent *event);

private slots:

  void on_cancelBtn_clicked();

  void on_manualBtn_clicked();

  void on_websiteLabel_linkActivated(const QString &link);

  void on_nextBtn_clicked();

  void on_prevBtn_clicked();

private:

  enum ItemOrder {
    ORDER_ASCENDING,
    ORDER_DESCENDING,
    ORDER_EXPLICIT
  };

  enum GroupType {
    TYPE_SELECTATLEASTONE,
    TYPE_SELECTATMOSTONE,
    TYPE_SELECTEXACTLYONE,
    TYPE_SELECTANY,
    TYPE_SELECTALL
  };

  enum PluginType {
    TYPE_REQUIRED,
    TYPE_RECOMMENDED,
    TYPE_OPTIONAL,
    TYPE_NOTUSABLE,
    TYPE_COULDBEUSABLE
  };

  struct Plugin {
    QString m_Name;
    QString m_Description;
    QString m_ImagePath;
    PluginType m_Type;
    std::vector<Condition> m_Conditions;
    std::vector<FileDescriptor*> m_Files;
  };

  struct ConditionalInstall {
    enum {
      OP_AND,
      OP_OR
    } m_Operator;
    std::vector<Condition> m_Conditions;
    std::vector<FileDescriptor*> m_Files;
  };

private:

  static int bomOffset(const QByteArray &buffer);

  QString readContent(QXmlStreamReader &reader);
  void parseInfo(const QByteArray &data);

  static ItemOrder getItemOrder(const QString &orderString);
  static GroupType getGroupType(const QString &typeString);
  static PluginType getPluginType(const QString &typeString);
  static bool byPriority(const FileDescriptor *LHS, const FileDescriptor *RHS);

  bool copyFileIterator(DirectoryTree *sourceTree, DirectoryTree *destinationTree, FileDescriptor *descriptor);
  void readFileList(QXmlStreamReader &reader, std::vector<FileDescriptor*> &fileList);
  void readPluginType(QXmlStreamReader &reader, Plugin &plugin);
  void readConditionFlags(QXmlStreamReader &reader, Plugin &plugin);
  FomodInstallerDialog::Plugin readPlugin(QXmlStreamReader &reader);
  void readPlugins(QXmlStreamReader &reader, GroupType groupType, QLayout *layout);
  void readGroup(QXmlStreamReader &reader, QLayout *layout);
  void readGroups(QXmlStreamReader &reader, QLayout *layout);
  void readVisible(QXmlStreamReader &reader, QVariantList &conditions);
  QGroupBox *readInstallerStep(QXmlStreamReader &reader);
  ConditionalInstall readConditionalPattern(QXmlStreamReader &reader);
  void readConditionalFileInstalls(QXmlStreamReader &reader);
  void readInstallerSteps(QXmlStreamReader &reader);
  void parseModuleConfig(const QByteArray &data);
  void highlightControl(QAbstractButton *button);

  bool testCondition(int maxIndex, const QString &flag, const QString &value);
  bool testVisible(int pageIndex);
  bool nextPage();
  void activateCurrentPage();
  void moveTree(DirectoryTree::Node *target, DirectoryTree::Node *source);
  DirectoryTree::Node *findNode(DirectoryTree::Node *node, const QString &path, bool create);
  void copyLeaf(DirectoryTree::Node *sourceTree, const QString &sourcePath,
                DirectoryTree::Node *destinationTree, const QString &destinationPath);

private:

  Ui::FomodInstallerDialog *ui;

  bool m_NameWasGuessed;

  QString m_FomodPath;
  bool m_Manual;

//  ItemOrder m_StepOrder;
//  std::vector<InstallationStep> m_Steps;
  std::vector<FileDescriptor*> m_RequiredFiles;
  std::vector<ConditionalInstall> m_ConditionalInstalls;

};

#endif // FOMODINSTALLERDIALOG_H
