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

#include "fomodinstallerdialog.h"
#include "report.h"
#include "utility.h"
#include "ui_fomodinstallerdialog.h"

#include <QFile>
#include <QDir>
#include <QImage>
#include <QCheckBox>
#include <QRadioButton>
#include <QScrollArea>
#include <Shellapi.h>


bool ControlsAscending(QAbstractButton *LHS, QAbstractButton *RHS)
{
  return LHS->text() < RHS->text();
}


bool ControlsDescending(QAbstractButton *LHS, QAbstractButton *RHS)
{
  return LHS->text() > RHS->text();
}


bool PagesAscending(QGroupBox *LHS, QGroupBox *RHS)
{
  return LHS->title() < RHS->title();
}


bool PagesDescending(QGroupBox *LHS, QGroupBox *RHS)
{
  return LHS->title() > RHS->title();
}


FomodInstallerDialog::FomodInstallerDialog(const QString &modName, const QString &fomodPath, QWidget *parent)
  : QDialog(parent), ui(new Ui::FomodInstallerDialog), m_FomodPath(fomodPath), m_Manual(false)
{
  ui->setupUi(this);
  ui->nameEdit->setText(modName);
}

FomodInstallerDialog::~FomodInstallerDialog()
{
  delete ui;
}

#pragma message("implement module dependencies->file dependencies")

void FomodInstallerDialog::initData()
{
  { // parse provided package information
    QFile file(QDir::tempPath().append("/info.xml"));
    if (file.open(QIODevice::ReadOnly)) {
      // nmm allows files with wrong encoding and of course there are now files with broken
      // so, let's do as nmm does and ignore the standard. yay
      QByteArray header = file.readLine();
      if (strncmp(header.constData(), "<?", 2) != 0) {
        // not a header, rewind
        file.seek(0);
      }
      parseInfo(file.readAll());
    }
    file.close();
  }

  QImage screenshot(QDir::tempPath().append("/screenshot.png"));
  if (!screenshot.isNull()) {
    screenshot = screenshot.scaledToWidth(ui->screenshotLabel->width());
    ui->screenshotLabel->setPixmap(QPixmap::fromImage(screenshot));
  }

  { // parse xml installer file
    QFile file(QDir::tempPath().append("/ModuleConfig.xml"));
    if (!file.open(QIODevice::ReadOnly)) {
      throw MyException(tr("ModuleConfig.xml missing"));
    }
    // nmm allows files with wrong encoding and of course there are now files that are broken
    QByteArray header = file.readLine();
    if (strncmp(header.constData(), "<?", 2) != 0) {
      // not a header, rewind
      if (!file.seek(0)) {
        qCritical("failed to rewind file");
      }
    }
    parseModuleConfig(file.readAll());
    file.close();
  }
}


QString FomodInstallerDialog::getName() const
{
  return ui->nameEdit->text();
}


void FomodInstallerDialog::moveTree(DirectoryTree::Node *target, DirectoryTree::Node *source)
{
  for (DirectoryTree::const_node_iterator iter = source->nodesBegin(); iter != source->nodesEnd();) {
    target->addNode(*iter, true);
    iter = source->detach(iter);
  }

  for (DirectoryTree::const_leaf_reverse_iterator iter = source->leafsRBegin();
       iter != source->leafsREnd(); ++iter) {
    target->addLeaf(*iter);
  }
}


DirectoryTree::Node *FomodInstallerDialog::findNode(DirectoryTree::Node *node, const QString &path, bool create)
{
  if (path.length() == 0) {
    return node;
  }

//  static QRegExp pathSeparator("[/\\]");
  int pos = path.indexOf('\\');
  if (pos == -1) {
    pos = path.indexOf('/');
  }
  QString subPath = path;
  if (pos > 0) {
    subPath = path.mid(0, pos);
  }
  for (DirectoryTree::const_node_iterator iter = node->nodesBegin(); iter != node->nodesEnd(); ++iter) {
    if ((*iter)->getData().name.compare(subPath, Qt::CaseInsensitive) == 0) {
      if (pos <= 0) {
        return *iter;
      } else {
        return findNode(*iter, path.mid(pos + 1), create);
      }
    }
  }
  if (create) {
    DirectoryTree::Node *newNode = new DirectoryTree::Node;
    newNode->setData(subPath);
    node->addNode(newNode, false);
    if (pos <= 0) {
      return newNode;
    } else {
      return findNode(newNode, path.mid(pos + 1), create);
    }
  } else {
    throw MyException(QString("%1 not found in archive").arg(path));
  }
}

void FomodInstallerDialog::copyLeaf(DirectoryTree::Node *sourceTree, const QString &sourcePath,
                                    DirectoryTree::Node *destinationTree, const QString &destinationPath)
{
  int sourceFileIndex = sourcePath.lastIndexOf('\\');
  if (sourceFileIndex == -1) {
    sourceFileIndex = sourcePath.lastIndexOf('/');
    if (sourceFileIndex == -1) {
      sourceFileIndex = 0;
    }
  }
  DirectoryTree::Node *sourceNode = sourceFileIndex == 0 ? sourceTree : findNode(sourceTree, sourcePath.mid(0, sourceFileIndex), false);

  int destinationFileIndex = destinationPath.lastIndexOf('\\');
  if (destinationFileIndex == -1) {
    destinationFileIndex = destinationPath.lastIndexOf('/');
    if (destinationFileIndex == -1) {
      destinationFileIndex = 0;
    }
  }

  DirectoryTree::Node *destinationNode =
      destinationFileIndex == 0 ? destinationTree
                                : findNode(destinationTree, destinationPath.mid(0, destinationFileIndex), true);

  QString sourceName = sourcePath.mid((sourceFileIndex != 0) ? sourceFileIndex + 1 : 0);
  QString destinationName = (destinationFileIndex != 0) ? destinationPath.mid(destinationFileIndex + 1) : destinationPath;
  if (destinationName.length() == 0) {
    destinationName = sourceName;
  }

  bool found = false;
  for (DirectoryTree::const_leaf_reverse_iterator iter = sourceNode->leafsRBegin();
       iter != sourceNode->leafsREnd(); ++iter) {
    if (iter->getName().compare(sourceName, Qt::CaseInsensitive) == 0) {
      destinationNode->addLeaf(*iter);
      found = true;
    }
  }
  if (!found) {
    qCritical("%s not found!", sourceName.toUtf8().constData());
  }
}


void dumpTree(DirectoryTree::Node *node, int indent)
{
  for (DirectoryTree::const_leaf_reverse_iterator iter = node->leafsRBegin();
       iter != node->leafsREnd(); ++iter) {
    qDebug("%.*s%s", indent, " ", iter->getName().toUtf8().constData());
  }

  for (DirectoryTree::const_node_iterator iter = node->nodesBegin(); iter != node->nodesEnd(); ++iter) {
    qDebug("%.*s-- %s", indent, " ", (*iter)->getData().name.toUtf8().constData());
    dumpTree(*iter, indent + 2);
  }
}


bool FomodInstallerDialog::copyFileIterator(DirectoryTree *sourceTree, DirectoryTree *destinationTree, FileDescriptor *descriptor)
{
  QString source = (m_FomodPath.length() != 0) ? m_FomodPath.mid(0).append("\\").append(descriptor->m_Source)
                                               : descriptor->m_Source;
  QString destination = descriptor->m_Destination;
  try {
    if (descriptor->m_IsFolder) {
      DirectoryTree::Node *sourceNode = findNode(sourceTree, source, false);
      DirectoryTree::Node *targetNode = findNode(destinationTree, destination, true);
      moveTree(targetNode, sourceNode);
    } else {
      copyLeaf(sourceTree, source, destinationTree, destination);
    }
    return true;
  } catch (const MyException &e) {
    qCritical("failed to extract %s to %s: %s",
              source.toUtf8().constData(), destination.toUtf8().constData(), e.what());
    return false;
  }
}


DirectoryTree *FomodInstallerDialog::updateTree(DirectoryTree *tree)
{
  DirectoryTree *newTree = new DirectoryTree;

  for (std::vector<FileDescriptor*>::iterator iter = m_RequiredFiles.begin(); iter != m_RequiredFiles.end(); ++iter) {
    copyFileIterator(tree, newTree, *iter);
  }

  for (std::vector<ConditionalInstall>::iterator installIter = m_ConditionalInstalls.begin();
       installIter != m_ConditionalInstalls.end(); ++installIter) {
    bool match = installIter->m_Operator == ConditionalInstall::OP_AND;
    for (std::vector<Condition>::iterator conditionIter = installIter->m_Conditions.begin();
         conditionIter != installIter->m_Conditions.end(); ++conditionIter) {
      bool conditionMatches = testCondition(ui->stepsStack->count(), conditionIter->m_Name, conditionIter->m_Value);
      if (conditionMatches && (installIter->m_Operator == ConditionalInstall::OP_OR)) {
        match = true;
        break;
      } else if (!conditionMatches && (installIter->m_Operator == ConditionalInstall::OP_AND)) {
        match = false;
        break;
      }
    }
    if (match) {
      for (std::vector<FileDescriptor*>::iterator fileIter = installIter->m_Files.begin();
           fileIter != installIter->m_Files.end(); ++fileIter) {
        copyFileIterator(tree, newTree, *fileIter);
      }
    }
  }

  QList<QAbstractButton*> choices = ui->stepsStack->findChildren<QAbstractButton*>("choice");
  foreach (QAbstractButton* choice, choices) {
    if (choice->isChecked()) {
      QVariantList fileList = choice->property("files").toList();
      foreach (QVariant fileVariant, fileList) {
        copyFileIterator(tree, newTree, fileVariant.value<FileDescriptor*>());
      }
    }
  }

//  dumpTree(newTree, 0);

  return newTree;
}


void FomodInstallerDialog::highlightControl(QAbstractButton *button)
{
  QVariant screenshotName = button->property("screenshot");
  if (screenshotName.isValid()) {
    QString screenshotFileName = screenshotName.toString();
    if (!screenshotFileName.isEmpty()) {
      QString temp = QFileInfo(screenshotFileName).fileName();
      QImage screenshot(QDir::tempPath().append("/").append(temp));
      if (screenshot.isNull()) {
        qWarning(">%s< is a null image", screenshotName.toString().toUtf8().constData());
      }
      screenshot = screenshot.scaledToWidth(ui->screenshotLabel->width());
      ui->screenshotLabel->setPixmap(QPixmap::fromImage(screenshot));
    } else {
      ui->screenshotLabel->setPixmap(QPixmap());
    }
  }
  ui->descriptionText->setText(button->property("description").toString());
}


bool FomodInstallerDialog::eventFilter(QObject *object, QEvent *event)
{
  QAbstractButton *button = qobject_cast<QAbstractButton*>(object);
  if ((button != NULL) && (event->type() == QEvent::HoverEnter)) {
    highlightControl(button);

  }
  return QDialog::eventFilter(object, event);
}


QString FomodInstallerDialog::readContent(QXmlStreamReader &reader)
{
  if (reader.readNext() == QXmlStreamReader::Characters) {
    return reader.text().toString();
  } else {
    return QString();
  }
}


void FomodInstallerDialog::parseInfo(const QByteArray &data)
{
  QXmlStreamReader reader(data);
/*  while (reader.readNext() != QXmlStreamReader::StartDocument) {}
  QTextDecoder *decoder = QTextCodec::codecForName(reader.documentEncoding().toLocal8Bit())->makeDecoder(QTextCodec::ConvertInvalidToNull);
  QString test(decoder->toUnicode(data));
  qDebug("test: %d, %s", test.isNull(), test.toUtf8().constData());

  qDebug(">%s<", reader.documentEncoding().toUtf8().constData());*/
  while (!reader.atEnd() && !reader.hasError()) {
    switch (reader.readNext()) {
      case QXmlStreamReader::StartElement: {
        if (reader.name() == "Name") {
          ui->nameEdit->setText(readContent(reader));
        } else if (reader.name() == "Author") {
          ui->authorLabel->setText(readContent(reader));
        } else if (reader.name() == "Version") {
          ui->versionLabel->setText(readContent(reader));
        } else if (reader.name() == "Website") {
          QString url = readContent(reader);
          ui->websiteLabel->setText(tr("<a href=\"%1\">Link</a>").arg(url));
          ui->websiteLabel->setToolTip(url);
        }
      } break;
      default: {} break;
    }
  }
  if (reader.hasError()) {
    throw MyException(tr("failed to parse info.xml: %1 (%2) (line %3, column %4)")
                        .arg(reader.errorString())
                        .arg(reader.error())
                        .arg(reader.lineNumber())
                        .arg(reader.columnNumber()));

  }
}


FomodInstallerDialog::ItemOrder FomodInstallerDialog::getItemOrder(const QString &orderString)
{
  if (orderString == "Ascending") {
    return ORDER_ASCENDING;
  } else if (orderString == "Descending") {
    return ORDER_DESCENDING;
  } else if (orderString == "Explicit") {
    return ORDER_EXPLICIT;
  } else {
    throw MyException(tr("unsupported order type %1").arg(orderString));
  }
}


FomodInstallerDialog::GroupType FomodInstallerDialog::getGroupType(const QString &typeString)
{
  if (typeString == "SelectAtLeastOne") {
    return TYPE_SELECTATLEASTONE;
  } else if (typeString == "SelectAtMostOne") {
    return TYPE_SELECTATMOSTONE;
  } else if (typeString == "SelectExactlyOne") {
    return TYPE_SELECTEXACTLYONE;
  } else if (typeString == "SelectAny") {
    return TYPE_SELECTANY;
  } else if (typeString == "SelectAll") {
    return TYPE_SELECTALL;
  } else {
    throw MyException(tr("unsupported group type %1").arg(typeString));
  }
}


FomodInstallerDialog::PluginType FomodInstallerDialog::getPluginType(const QString &typeString)
{
  if (typeString == "Required") {
    return FomodInstallerDialog::TYPE_REQUIRED;
  } else if (typeString == "Optional") {
    return FomodInstallerDialog::TYPE_OPTIONAL;
  } else if (typeString == "Recommended") {
    return FomodInstallerDialog::TYPE_RECOMMENDED;
  } else if (typeString == "NotUsable") {
    return FomodInstallerDialog::TYPE_NOTUSABLE;
  } else if (typeString == "CouldBeUsable") {
    return FomodInstallerDialog::TYPE_COULDBEUSABLE;
  } else {
    qCritical("invalid plugin type %s", typeString.toUtf8().constData());
    return FomodInstallerDialog::TYPE_OPTIONAL;
  }
}


void FomodInstallerDialog::readFileList(QXmlStreamReader &reader, std::vector<FileDescriptor*> &fileList)
{
  QStringRef openTag = reader.name();
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == openTag))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if ((reader.name() == "folder") ||
          (reader.name() == "file")) {
        QXmlStreamAttributes attributes = reader.attributes();
        FileDescriptor *file = new FileDescriptor(this);
        file->m_Source = attributes.value("source").toString();
        file->m_Destination = attributes.hasAttribute("destination") ? attributes.value("destination").toString()
                                                                     : file->m_Source;
        file->m_Priority = attributes.hasAttribute("priority") ? attributes.value("priority").string()->toInt()
                                                               : 0;
        file->m_IsFolder = reader.name() == "folder";
        file->m_InstallIfUsable = attributes.hasAttribute("installIfUsable") ? (attributes.value("installIfUsable").compare("true") == 0)
                                                                             : false;
        file->m_AlwaysInstall = attributes.hasAttribute("alwaysInstall") ? (attributes.value("alwaysInstall").compare("true") == 0)
                                                                         : false;

        fileList.push_back(file);
      }
    }
  }
}


void FomodInstallerDialog::readPluginType(QXmlStreamReader &reader, Plugin &plugin)
{
  plugin.m_Type = TYPE_OPTIONAL;
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "typeDescriptor"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "type") {
        plugin.m_Type = getPluginType(reader.attributes().value("name").toString());
      }
    }
  }
}


void FomodInstallerDialog::readConditionFlags(QXmlStreamReader &reader, Plugin &plugin)
{
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "conditionFlags"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "flag") {
        QString name = reader.attributes().value("name").toString();
        plugin.m_Conditions.push_back(Condition(name, readContent(reader)));
      }
    }
  }
}


bool FomodInstallerDialog::byPriority(const FileDescriptor *LHS, const FileDescriptor *RHS)
{
  return LHS->m_Priority < RHS->m_Priority;
}


FomodInstallerDialog::Plugin FomodInstallerDialog::readPlugin(QXmlStreamReader &reader)
{
  Plugin result;
  result.m_Name = reader.attributes().value("name").toString();

  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "plugin"))) {
//    QXmlStreamReader::TokenType type = reader.tokenType();
//    QString name = reader.name().toUtf8();
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "description") {
        result.m_Description = readContent(reader).trimmed();
      } else if (reader.name() == "image") {
        result.m_ImagePath = reader.attributes().value("path").toString();
      } else if (reader.name() == "typeDescriptor") {
        readPluginType(reader, result);
      } else if (reader.name() == "conditionFlags") {
        readConditionFlags(reader, result);
      } else if (reader.name() == "files") {
        readFileList(reader, result.m_Files);
      }
    }
  }

  std::sort(result.m_Files.begin(), result.m_Files.end(), byPriority);

  return result;
}


void FomodInstallerDialog::readPlugins(QXmlStreamReader &reader, GroupType groupType, QLayout *layout)
{
  ItemOrder pluginOrder = reader.attributes().hasAttribute("order") ? getItemOrder(reader.attributes().value("order").toString())
                                                                    : ORDER_ASCENDING;
  bool first = true;
  bool maySelectMore = true;

  std::vector<QAbstractButton*> controls;

  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "plugins"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "plugin") {
        Plugin plugin = readPlugin(reader);
        QAbstractButton *newControl = NULL;
        switch (groupType) {
          case TYPE_SELECTATLEASTONE:
          case TYPE_SELECTANY: {
            newControl = new QCheckBox(plugin.m_Name);
          } break;
          case TYPE_SELECTATMOSTONE: {
            newControl = new QRadioButton(plugin.m_Name);
          } break;
          case TYPE_SELECTEXACTLYONE: {
            newControl = new QRadioButton(plugin.m_Name);
            if (first) {
              newControl->setChecked(true);
            }
          } break;
          case TYPE_SELECTALL: {
            newControl = new QCheckBox(plugin.m_Name);
            newControl->setChecked(true);
            newControl->setEnabled(false);
          } break;
        }
        newControl->setObjectName("choice");
        switch (plugin.m_Type) {
          case TYPE_REQUIRED: {
            newControl->setChecked(true);
            newControl->setEnabled(false);
            newControl->setToolTip(tr("This component is required"));
          } break;
          case TYPE_RECOMMENDED: {
            if (maySelectMore) {
              newControl->setChecked(true);
            }
            newControl->setToolTip(tr("It is recommended you enable this component"));
            if ((groupType == TYPE_SELECTATMOSTONE) || (groupType == TYPE_SELECTEXACTLYONE)) {
              maySelectMore = false;
            }
          } break;
          case TYPE_OPTIONAL: {
            newControl->setToolTip(tr("Optional component"));
          } break;
          case TYPE_NOTUSABLE: {
            newControl->setChecked(false);
            newControl->setEnabled(false);
            newControl->setToolTip(tr("This component is not usable in combination with other installed plugins"));
          } break;
          case TYPE_COULDBEUSABLE: {
            newControl->setCheckable(true);
            newControl->setIcon(QIcon(":/new/guiresources/resources/dialog-warning_16.png"));
            newControl->setToolTip(tr("You may be experiencing instability in combination with other installed plugins"));
          } break;
        }

        newControl->setProperty("plugintype", plugin.m_Type);
        newControl->setProperty("screenshot", plugin.m_ImagePath);
        newControl->setProperty("description", plugin.m_Description);
        QVariantList fileList;
        for (std::vector<FileDescriptor*>::iterator iter = plugin.m_Files.begin(); iter != plugin.m_Files.end(); ++iter) {
          fileList.append(qVariantFromValue(*iter));
        }
        newControl->setProperty("files", fileList);
        QVariantList conditionFlags;
        for (std::vector<Condition>::const_iterator iter = plugin.m_Conditions.begin(); iter != plugin.m_Conditions.end(); ++iter) {
          if (iter->m_Name.length() != 0) {
            conditionFlags.append(qVariantFromValue(Condition(iter->m_Name, iter->m_Value)));
          }
        }
        newControl->setProperty("conditionFlags", conditionFlags);
        newControl->installEventFilter(this);
        controls.push_back(newControl);
        first = false;
      }
    }
  }

  if (pluginOrder == ORDER_ASCENDING) {
    std::sort(controls.begin(), controls.end(), ControlsAscending);
  } else if (pluginOrder == ORDER_DESCENDING) {
    std::sort(controls.begin(), controls.end(), ControlsDescending);
  }

  for (std::vector<QAbstractButton*>::const_iterator iter = controls.begin(); iter != controls.end(); ++iter) {
    layout->addWidget(*iter);
  }

  if (groupType == TYPE_SELECTATMOSTONE) {
    QRadioButton *newButton = new QRadioButton(tr("None"));
    if (maySelectMore) {
      newButton->setChecked(true);
    }
    layout->addWidget(newButton);
  }
}


void FomodInstallerDialog::readGroup(QXmlStreamReader &reader, QLayout *layout)
{
  //FileGroup result;
  QString name = reader.attributes().value("name").toString();
  GroupType type = getGroupType(reader.attributes().value("type").toString());

  if (type == TYPE_SELECTATLEASTONE) {
    QLabel *label = new QLabel(tr("Select one or more of these options:"));
    layout->addWidget(label);
  }

  QGroupBox *groupBox = new QGroupBox(name);

  QVBoxLayout *groupLayout = new QVBoxLayout;

  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "group"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "plugins") {
        readPlugins(reader, type, groupLayout);
      }
    }
  }

  groupBox->setLayout(groupLayout);
  layout->addWidget(groupBox);
}


void FomodInstallerDialog::readGroups(QXmlStreamReader &reader, QLayout *layout)
{
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "optionalFileGroups"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "group") {
        readGroup(reader, layout);
      }
    }
  }
}


void FomodInstallerDialog::readVisible(QXmlStreamReader &reader, QVariantList &conditions)
{
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "visible"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "flagDependency") {
        Condition condition(reader.attributes().value("flag").toString(),
                            reader.attributes().value("value").toString());
        conditions.append(qVariantFromValue(condition));
      }
    }
  }
}

QGroupBox *FomodInstallerDialog::readInstallerStep(QXmlStreamReader &reader)
{
  QString name = reader.attributes().value("name").toString();
  QGroupBox *page = new QGroupBox(name);
  QVBoxLayout *pageLayout = new QVBoxLayout;
  QScrollArea *scrollArea = new QScrollArea;
  QFrame *scrolledArea = new QFrame;
  QVBoxLayout *scrollLayout = new QVBoxLayout;

  QVariantList conditions;

  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "installStep"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "optionalFileGroups") {
        readGroups(reader, scrollLayout);
      } else if (reader.name() == "visible") {
        readVisible(reader, conditions);
      }
    }
  }
  if (conditions.length() != 0) {
    page->setProperty("conditions", conditions);
  }

  scrolledArea->setLayout(scrollLayout);
  scrollArea->setWidget(scrolledArea);
  scrollArea->setWidgetResizable(true);
  pageLayout->addWidget(scrollArea);
  page->setLayout(pageLayout);
  return page;
}


void FomodInstallerDialog::readInstallerSteps(QXmlStreamReader &reader)
{
  ItemOrder stepOrder = reader.attributes().hasAttribute("order") ? getItemOrder(reader.attributes().value("order").toString())
                                                                    : ORDER_ASCENDING;

  std::vector<QGroupBox*> pages;

  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "installSteps"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "installStep") {
        pages.push_back(readInstallerStep(reader));
      }
    }
  }

  if (stepOrder == ORDER_ASCENDING) {
    std::sort(pages.begin(), pages.end(), PagesAscending);
  } else if (stepOrder == ORDER_DESCENDING) {
    std::sort(pages.begin(), pages.end(), PagesDescending);
  }

  for (std::vector<QGroupBox*>::const_iterator iter = pages.begin(); iter != pages.end(); ++iter) {
    ui->stepsStack->addWidget(*iter);
  }
}


FomodInstallerDialog::ConditionalInstall FomodInstallerDialog::readConditionalPattern(QXmlStreamReader &reader)
{
  ConditionalInstall result;
  result.m_Operator = ConditionalInstall::OP_AND;
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "pattern"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "dependencies") {
        QStringRef dependencyOperator = reader.attributes().value("operator");
        if (dependencyOperator == "And") {
          result.m_Operator = ConditionalInstall::OP_AND;
        } else if (dependencyOperator == "Or") {
          result.m_Operator = ConditionalInstall::OP_OR;
        } // otherwise operator is not set (which we can ignore) or invalid (which we should report actually)

        while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
                 (reader.name() == "dependencies"))) {
          if (reader.tokenType() == QXmlStreamReader::StartElement) {
            if (reader.name() == "flagDependency") {
              result.m_Conditions.push_back(Condition(reader.attributes().value("flag").toString(),
                                                      reader.attributes().value("value").toString()));
            }
          }
        }
      } else if (reader.name() == "files") {
        readFileList(reader, result.m_Files);
      }
    }
  }
  return result;
}


void FomodInstallerDialog::readConditionalFileInstalls(QXmlStreamReader &reader)
{
  while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
           (reader.name() == "conditionalFileInstalls"))) {
    if (reader.tokenType() == QXmlStreamReader::StartElement) {
      if (reader.name() == "patterns") {
        while (!((reader.readNext() == QXmlStreamReader::EndElement) &&
                 (reader.name() == "patterns"))) {
          if (reader.tokenType() == QXmlStreamReader::StartElement) {
            if (reader.name() == "pattern") {
              m_ConditionalInstalls.push_back(readConditionalPattern(reader));
            }
          }
        }
      }
    }
  }
}


void FomodInstallerDialog::parseModuleConfig(const QByteArray &data)
{
  QXmlStreamReader reader(data);
  while (!reader.atEnd() && !reader.hasError()) {
    switch (reader.readNext()) {
      case QXmlStreamReader::StartElement: {
        if (reader.name() == "installSteps") {
          readInstallerSteps(reader);
        } else if (reader.name() == "requiredInstallFiles") {
          readFileList(reader, m_RequiredFiles);
        } else if (reader.name() == "conditionalFileInstalls") {
          readConditionalFileInstalls(reader);
        }
      } break;
      default: {} break;
    }
  }
  if (reader.hasError()) {
    reportError(tr("failed to parse ModuleConfig.xml: %1 - %2").arg(reader.errorString()).arg(reader.lineNumber()));
  }
  activateCurrentPage();
}


void FomodInstallerDialog::on_manualBtn_clicked()
{
  m_Manual = true;
  this->reject();
}

void FomodInstallerDialog::on_cancelBtn_clicked()
{
  this->reject();
}


void FomodInstallerDialog::on_websiteLabel_linkActivated(const QString &link)
{
  ::ShellExecuteW(NULL, L"open", ToWString(link).c_str(), NULL, NULL, SW_SHOWNORMAL);
}


void FomodInstallerDialog::activateCurrentPage()
{
  QList<QAbstractButton*> choices = ui->stepsStack->currentWidget()->findChildren<QAbstractButton*>("choice");
  if (choices.count() > 0) {
    highlightControl(choices.at(0));
  }
}


bool FomodInstallerDialog::testCondition(int maxIndex, const QString &flag, const QString &value)
{
  // iterate through all set condition flags on all activated controls on all visible pages if one of them matches the condition
  for (int i = 0; i < maxIndex; ++i) {
    if (testVisible(i)) {
      QWidget *page = ui->stepsStack->widget(i);
      QList<QAbstractButton*> choices = page->findChildren<QAbstractButton*>("choice");
      foreach (QAbstractButton* choice, choices) {
        if (choice->isChecked()) {
          QVariant temp = choice->property("conditionFlags");
          if (temp.isValid()) {
            QVariantList conditionFlags = temp.toList();
            for (QVariantList::const_iterator iter = conditionFlags.begin(); iter != conditionFlags.end(); ++iter) {
              Condition condition = iter->value<Condition>();
              if ((condition.m_Name == flag) && (condition.m_Value == value)) {
                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}


bool FomodInstallerDialog::testVisible(int pageIndex)
{
  QWidget *page = ui->stepsStack->widget(pageIndex);
  QVariant temp = page->property("conditions");
  if (temp.isValid()) {
    QVariantList conditions = temp.toList();
    for (QVariantList::const_iterator iter = conditions.begin(); iter != conditions.end(); ++iter) {
      Condition condition = iter->value<Condition>();
      if (!testCondition(pageIndex, condition.m_Name, condition.m_Value)) {
        return false;
      }
    }
    return true;
  } else {
    return true;
  }
}


bool FomodInstallerDialog::nextPage()
{
  int index = ui->stepsStack->currentIndex() + 1;
  while (index < ui->stepsStack->count()) {
    if (testVisible(index)) {
      ui->stepsStack->setCurrentIndex(index);
      return true;
    }
    ++index;
  }
  // no more visible pages -> install
  return false;
}


void FomodInstallerDialog::on_nextBtn_clicked()
{
  if (ui->stepsStack->currentIndex() == ui->stepsStack->count() - 1) {
    this->accept();
  } else {
    if (nextPage()) {
      if (ui->stepsStack->currentIndex() == ui->stepsStack->count() - 1) {
        ui->nextBtn->setText(tr("Install"));
      }
      ui->prevBtn->setEnabled(true);
      activateCurrentPage();
    } else {
      this->accept();
    }
  }
}

void FomodInstallerDialog::on_prevBtn_clicked()
{
  if (ui->stepsStack->currentIndex() != 0) {
    ui->stepsStack->setCurrentIndex(ui->stepsStack->currentIndex() - 1);
    ui->nextBtn->setText(tr("Next"));
  }
  if (ui->stepsStack->currentIndex() == 0) {
    ui->prevBtn->setEnabled(false);
  }
  activateCurrentPage();
}
