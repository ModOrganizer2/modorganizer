/*
Copyright (C) 2020 Mikaël Capelle. All rights reserved.

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

#ifndef DISABLEPROXYPLUGINDIALOG_H
#define DISABLEPROXYPLUGINDIALOG_H

#include <QDialog>

#include "ipluginproxy.h"

namespace Ui
{
class DisableProxyPluginDialog;
}

class DisableProxyPluginDialog : public QDialog
{
public:
  DisableProxyPluginDialog(MOBase::IPlugin* proxyPlugin,
                           std::vector<MOBase::IPlugin*> const& required,
                           QWidget* parent = nullptr);

private slots:

  Ui::DisableProxyPluginDialog* ui;
};

#endif
