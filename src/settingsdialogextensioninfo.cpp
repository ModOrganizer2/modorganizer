#include "settingsdialogextensioninfo.h"

#include "ui_settingsdialogextensioninfo.h"

#include <format>

#include <QComboBox>

#include <uibase/formatters.h>

#include "extensionmanager.h"
#include "pluginmanager.h"
#include "settings.h"

using namespace MOBase;

ExtensionSettingWidget::ExtensionSettingWidget(Setting const& setting,
                                               QVariant const& value, QWidget* parent)
    : QWidget(parent), m_value(value)
{
  setLayout(new QVBoxLayout());

  {
    auto* titleLabel = new QLabel(setting.title());
    auto font        = titleLabel->font();
    font.setBold(true);
    titleLabel->setFont(font);
    layout()->addWidget(titleLabel);
  }

  if (!setting.description().isEmpty()) {
    auto* descriptionLabel = new QLabel(setting.description());
    auto font              = descriptionLabel->font();
    font.setItalic(true);
    font.setPointSize(static_cast<int>(font.pointSize() * 0.85));
    descriptionLabel->setFont(font);
    layout()->addWidget(descriptionLabel);
  }

  {
    QWidget* valueWidget = nullptr;
    switch (setting.defaultValue().typeId()) {
    case QMetaType::Bool: {
      auto* comboBox = new QComboBox();
      comboBox->addItems({tr("False"), tr("True")});
      comboBox->setCurrentIndex(value.toBool());
      valueWidget = comboBox;
    } break;
    case QMetaType::QString: {
      auto* lineEdit = new QLineEdit(value.toString());
      valueWidget    = lineEdit;
    } break;
    case QMetaType::Float:
    case QMetaType::Double: {
      auto* lineEdit = new QLineEdit(QString::number(value.toInt()));
      lineEdit->setValidator(new QDoubleValidator(lineEdit));
      valueWidget = lineEdit;
    } break;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong: {
      auto* lineEdit = new QLineEdit(QString::number(value.toInt()));
      lineEdit->setValidator(new QIntValidator(lineEdit));
      valueWidget = lineEdit;
    } break;
    case QMetaType::QColor: {
      valueWidget  = new QWidget(this);
      auto* layout = new QHBoxLayout();
      valueWidget->setLayout(layout);

      const auto color  = m_value.value<QColor>();
      auto* textWidget  = new QLabel(color.name());
      auto* colorWidget = new QLabel("");
      colorWidget->setStyleSheet(
          QString("QLabel { background-color: %1; }").arg(color.name()));
      auto* button = new QPushButton(tr("Edit"));
      connect(button, &QPushButton::clicked, [textWidget, colorWidget, this]() {
        const auto newColor = QColorDialog::getColor(m_value.value<QColor>());
        if (newColor.isValid()) {
          m_value = newColor;
          textWidget->setText(newColor.name());
          colorWidget->setStyleSheet(
              QString("QLabel { background-color: %1; }").arg(newColor.name()));
        }
      });

      layout->addWidget(textWidget);
      layout->addWidget(colorWidget, 1);
      layout->addWidget(button);
    } break;
    }

    if (valueWidget) {
      layout()->addWidget(valueWidget);
    }
  }
}

ExtensionListInfoWidget::ExtensionListInfoWidget(QWidget* parent)
    : QWidget(parent), ui{new Ui::ExtensionListInfoWidget()}
{
  ui->setupUi(this);

  ui->authorLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->authorLabel->setOpenExternalLinks(true);

  ui->pluginSettingsList->setRootIsDecorated(true);
  ui->pluginSettingsList->setSelectionMode(QAbstractItemView::NoSelection);
  ui->pluginSettingsList->setItemsExpandable(true);
  ui->pluginSettingsList->setColumnCount(1);
  ui->pluginSettingsList->header()->setSectionResizeMode(
      0, QHeaderView::ResizeMode::Stretch);
}

void ExtensionListInfoWidget::setup(Settings& settings,
                                    ExtensionManager& extensionManager,
                                    PluginManager& pluginManager)
{
  m_settings         = &settings;
  m_extensionManager = &extensionManager;
  m_pluginManager    = &pluginManager;
}

void ExtensionListInfoWidget::setExtension(const IExtension& extension)
{
  m_extension = &extension;

  // update the header for the extension

  const auto& metadata = m_extension->metadata();
  const auto& author   = metadata.author();

  if (author.homepage().isEmpty()) {
    ui->authorLabel->setText(metadata.author().name());
  } else {
    ui->authorLabel->setText(QString::fromStdString(
        std::format("<a href=\"{}\">{}</a>", author.homepage(), author.name())));
  }
  ui->descriptionLabel->setText(metadata.description());
  ui->versionLabel->setText(metadata.version().string(Version::FormatCondensed));

  if (metadata.type() == ExtensionType::THEME ||
      metadata.type() == ExtensionType::TRANSLATION) {
    ui->enabledCheckbox->setChecked(true);
    ui->enabledCheckbox->setEnabled(false);
    ui->enabledCheckbox->setToolTip(
        tr("Translation and theme extensions cannot be disabled."));
  } else {
    ui->enabledCheckbox->setChecked(m_extensionManager->isEnabled(extension));
    ui->enabledCheckbox->setEnabled(true);
    ui->enabledCheckbox->setToolTip(QString());
  }

  ui->pluginSettingsList->clear();

  // update the list of settings
  if (const auto* pluginExtension = dynamic_cast<const PluginExtension*>(m_extension)) {

    // TODO: refactor code somewhere to have direct access of the plugins for a given
    // extension
    for (auto& plugin : m_pluginManager->plugins()) {
      if (&m_pluginManager->details(plugin).extension() != m_extension) {
        continue;
      }

      const auto settings = plugin->settings();
      if (settings.isEmpty()) {
        continue;
      }

      QTreeWidgetItem* pluginItem = new QTreeWidgetItem({plugin->localizedName()});
      ui->pluginSettingsList->addTopLevelItem(pluginItem);

      for (auto& setting : settings) {
        auto* settingItem   = new QTreeWidgetItem();
        auto* settingWidget = new ExtensionSettingWidget(
            setting, m_settings->plugins().setting(plugin->name(), setting.name(),
                                                   setting.defaultValue()));
        pluginItem->addChild(settingItem);
        ui->pluginSettingsList->setItemWidget(settingItem, 0, settingWidget);
        settingItem->setSizeHint(0, settingWidget->sizeHint());
      }

      pluginItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
      pluginItem->setExpanded(true);
    }
  }
}
