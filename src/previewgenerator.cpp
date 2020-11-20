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

#include <utility.h>

#include "plugincontainer.h"

PreviewGenerator::PreviewGenerator(const PluginContainer* pluginContainer) :
  m_PluginContainer(pluginContainer) {
  m_MaxSize = QGuiApplication::primaryScreen()->size() * 0.8;
}

void PreviewGenerator::registerPlugin(MOBase::IPluginPreview *plugin)
{
  for (const QString &extension : plugin->supportedExtensions()) {
    m_PreviewPlugins.insert(std::make_pair(extension, plugin));
  }
}

bool PreviewGenerator::previewSupported(const QString &fileExtension) const
{
  auto it = m_PreviewPlugins.find(fileExtension.toLower());
  if (it == m_PreviewPlugins.end()) {
    return false;
  }
  return m_PluginContainer->isEnabled(it->second);
}

QWidget *PreviewGenerator::genPreview(const QString &fileName) const
{
  auto iter = m_PreviewPlugins.find(QFileInfo(fileName).suffix().toLower());
  if (iter != m_PreviewPlugins.end() && m_PluginContainer->isEnabled(iter->second)) {
    return iter->second->genFilePreview(fileName, m_MaxSize);
  } else {
    return nullptr;
  }
}
