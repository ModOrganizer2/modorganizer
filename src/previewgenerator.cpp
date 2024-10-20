/*
Copyright (C) 2014 Sebastian Herbord. All rights reserved.

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

#include "previewgenerator.h"

#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QTextEdit>
#include <utility.h>

#include "pluginmanager.h"

using namespace MOBase;

PreviewGenerator::PreviewGenerator(const PluginManager& pluginManager)
    : m_PluginManager(pluginManager)
{
  m_MaxSize = QGuiApplication::primaryScreen()->size() * 0.8;
}

bool PreviewGenerator::previewSupported(const QString& fileExtension,
                                        const bool& isArchive) const
{
  auto& previews = m_PluginManager.plugins<IPluginPreview>();
  for (auto* preview : previews) {
    if (preview->supportedExtensions().contains(fileExtension)) {
      if (!isArchive)
        return true;
      if (preview->supportsArchives())
        return true;
    }
  }
  return false;
}

QWidget* PreviewGenerator::genPreview(const QString& fileName) const
{
  const QString ext = QFileInfo(fileName).suffix().toLower();
  auto& previews    = m_PluginManager.plugins<IPluginPreview>();
  for (auto* preview : previews) {
    if (m_PluginManager.isEnabled(preview) &&
        preview->supportedExtensions().contains(ext)) {
      return preview->genFilePreview(fileName, m_MaxSize);
    }
  }
  return nullptr;
}

QWidget* PreviewGenerator::genArchivePreview(const QByteArray& fileData,
                                             const QString& fileName) const
{
  const QString ext = QFileInfo(fileName).suffix().toLower();
  auto& previews    = m_PluginManager.plugins<IPluginPreview>();
  for (auto* preview : previews) {
    if (m_PluginManager.isEnabled(preview) &&
        preview->supportedExtensions().contains(ext) && preview->supportsArchives()) {
      return preview->genDataPreview(fileData, fileName, m_MaxSize);
    }
  }
  return nullptr;
}
