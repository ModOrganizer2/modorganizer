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
#include <QLabel>
#include <QImageReader>
#include <QTextEdit>
#include <utility.h>

#include "plugincontainer.h"

using namespace MOBase;

PreviewGenerator::PreviewGenerator(const PluginContainer& pluginContainer) :
  m_PluginContainer(pluginContainer) {
  m_MaxSize = QGuiApplication::primaryScreen()->size() * 0.8;
}

bool PreviewGenerator::previewSupported(const QString &fileExtension) const
{
  auto& previews = m_PluginContainer.plugins<IPluginPreview>();
  for (auto* preview : previews) {
    if (preview->supportedExtensions().contains(fileExtension)) {
      return true;
    }
  }
  return false;
}

QWidget *PreviewGenerator::genPreview(const QString &fileName) const
{
  const QString ext = QFileInfo(fileName).suffix().toLower();
  auto& previews = m_PluginContainer.plugins<IPluginPreview>();
  for (auto* preview : previews) {
    if (m_PluginContainer.isEnabled(preview) && preview->supportedExtensions().contains(ext)) {
      return preview->genFilePreview(fileName, m_MaxSize);
    }
  }
  return nullptr;
}
