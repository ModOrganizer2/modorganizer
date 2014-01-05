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
#include <QGLPixelBuffer>
#include <QDesktopWidget>
#include <utility.h>

PreviewGenerator::PreviewGenerator()
{
  // set up image reader to be used for all image types qt (the current installation) supports
  auto imageReader = std::bind(&PreviewGenerator::genImagePreview, this, std::placeholders::_1);

  foreach (const QByteArray &fileType, QImageReader::supportedImageFormats()) {
    m_PreviewGenerators[QString(fileType).toLower()] = imageReader;
  }

  m_PreviewGenerators["txt"]
    = std::bind(&PreviewGenerator::genTxtPreview, this, std::placeholders::_1);

  m_PreviewGenerators["dds"]
    = std::bind(&PreviewGenerator::genDDSPreview, this, std::placeholders::_1);

  QDesktopWidget desk;
  m_MaxSize = desk.screenGeometry().size() * 0.8;
}

bool PreviewGenerator::previewSupported(const QString &fileExtension) const
{
  return m_PreviewGenerators.find(fileExtension.toLower()) != m_PreviewGenerators.end();
}

QWidget *PreviewGenerator::genPreview(const QString &fileName) const
{
  auto iter = m_PreviewGenerators.find(QFileInfo(fileName).completeSuffix().toLower());
  if (iter != m_PreviewGenerators.end()) {
    return iter->second(fileName);
  } else {
    return nullptr;
  }
}

QWidget *PreviewGenerator::genImagePreview(const QString &fileName) const
{
  QLabel *label = new QLabel();
  label->setPixmap(QPixmap(fileName));
  return label;
}

QWidget *PreviewGenerator::genTxtPreview(const QString &fileName) const
{
  QTextEdit *edit = new QTextEdit();
  edit->setText(MOBase::readFileText(fileName));
  edit->setReadOnly(true);
  return edit;
}

QWidget *PreviewGenerator::genDDSPreview(const QString &fileName) const
{
  QGLWidget glWidget;
  glWidget.makeCurrent();

  GLuint texture = glWidget.bindTexture(fileName);
  if (!texture)
    return nullptr;

  // Determine the size of the DDS image
  GLint width, height;
  glBindTexture(GL_TEXTURE_2D, texture);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

  if (width == 0 || height == 0)
    return nullptr;

  QGLPixelBuffer pbuffer(QSize(width, height), glWidget.format(), &glWidget);
  if (!pbuffer.makeCurrent())
    return nullptr;

  pbuffer.drawTexture(QRectF(-1, -1, 2, 2), texture);

  QLabel *label = new QLabel();
  QImage image = pbuffer.toImage();
  if ((image.size().width() > m_MaxSize.width()) ||
      (image.size().height() > m_MaxSize.height())) {
    image = image.scaled(m_MaxSize, Qt::KeepAspectRatio);
  }

  label->setPixmap(QPixmap::fromImage(image));
  return label;
}
