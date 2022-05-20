#include "thememanager.h"

#include <QDir>
#include <QDirIterator>
#include <QProxyStyle>

#include <log.h>
#include <utility.h>

#include "shared/appconfig.h"

using namespace MOBase;

// style proxy that changes the appearance of drop indicators
//
class ProxyStyle : public QProxyStyle
{
public:
  ProxyStyle(QStyle* baseStyle = 0) : QProxyStyle(baseStyle) {}

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const override
  {
    if (element == QStyle::PE_IndicatorItemViewItemDrop) {

      // 0. Fix a bug that made the drop indicator sometimes appear on top
      // of the mod list when selecting a mod.
      if (option->rect.height() == 0 && option->rect.bottomRight() == QPoint(-1, -1)) {
        return;
      }

      // 1. full-width drop indicator
      QRect rect(option->rect);
      if (auto* view = qobject_cast<const QTreeView*>(widget)) {
        rect.setLeft(view->indentation());
        rect.setRight(widget->width());
      }

      // 2. stylish drop indicator
      painter->setRenderHint(QPainter::Antialiasing, true);

      QColor col(option->palette.windowText().color());
      QPen pen(col);
      pen.setWidth(2);
      col.setAlpha(50);

      painter->setPen(pen);
      painter->setBrush(QBrush(col));
      if (rect.height() == 0) {
        QPoint tri[3] = {rect.topLeft(), rect.topLeft() + QPoint(-5, 5),
                         rect.topLeft() + QPoint(-5, -5)};
        painter->drawPolygon(tri, 3);
        painter->drawLine(rect.topLeft(), rect.topRight());
      } else {
        painter->drawRoundedRect(rect, 5, 5);
      }
    } else {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }
};

ThemeManager::ThemeManager(QApplication* application) : m_app{application}
{
  // add built-in themes
  addQtThemes();

  // TODO: remove this
  addOldFormatThemes();

  // find the default theme
  m_defaultTheme =
      m_baseThemesByIdentifier.at(m_app->style()->objectName().toStdString());

  // for ease, we set the empty identifier to the default theme
  m_baseThemesByIdentifier[""] = m_defaultTheme;

  // load the default theme
  load(m_defaultTheme);

  // connect the style watcher
  m_app->connect(&m_watcher, &QFileSystemWatcher::fileChanged, [this](auto&&) {
    reload();
  });
}

// TODO: remove this
void ThemeManager::addOldFormatThemes()
{
  QDirIterator iter(QCoreApplication::applicationDirPath() + "/" +
                        QString::fromStdWString(AppConfig::stylesheetsPath()),
                    QStringList("*.qss"), QDir::Files);

  while (iter.hasNext()) {
    iter.next();
    registerTheme(
        std::make_shared<Theme>(iter.fileInfo().completeBaseName().toStdString(),
                                iter.fileInfo().baseName().toStdString(),
                                iter.fileInfo().filesystemFilePath()));
  }
}

bool ThemeManager::load(std::shared_ptr<const Theme> theme)
{
  // no theme -> default
  if (!theme) {
    theme = m_defaultTheme;
  }

  // do not reload the current theme
  if (theme == m_currentTheme) {
    return true;
  }

  // set the current theme
  m_currentTheme = theme;

  if (isBuiltIn(theme)) {
    loadQtTheme(theme->identifier());
    watchThemeFiles(nullptr);
  } else {
    loadExtensionTheme(theme);
    watchThemeFiles(theme);
  }

  return true;
}

bool ThemeManager::load(std::string_view themeIdentifier)
{
  auto it = m_baseThemesByIdentifier.find(themeIdentifier);
  if (it == m_baseThemesByIdentifier.end()) {
    log::error("theme '{}' not found", themeIdentifier);
    return false;
  }

  return load(it->second);
}

void ThemeManager::loadQtTheme(std::string_view themeIdentifier)
{
  m_app->setStyleSheet("");
  m_app->setStyle(new ProxyStyle(QStyleFactory::create(ToQString(themeIdentifier))));
}

void ThemeManager::loadExtensionTheme(std::shared_ptr<const Theme> const& theme)
{
  // load the default theme
  m_app->setStyle(
      new ProxyStyle(QStyleFactory::create(ToQString(m_defaultTheme->identifier()))));

  // build the stylesheet and set it
  m_app->setStyleSheet(buildStyleSheet(theme));
}

void ThemeManager::unload()
{
  // load the default style
  load(m_defaultTheme);
}

void ThemeManager::reload()
{
  // cannot reload if there is no theme or builtin themes
  if (!m_currentTheme || m_currentTheme->stylesheet().empty()) {
    return;
  }

  if (isBuiltIn(m_currentTheme)) {
    loadQtTheme(m_currentTheme->identifier());
  } else {
    loadExtensionTheme(m_currentTheme);
  }
}

void ThemeManager::registerTheme(std::shared_ptr<const Theme> const& theme)
{
  // two themes with same identifier, skip (+ warn)
  auto it = m_baseThemesByIdentifier.find(theme->identifier());
  if (it != m_baseThemesByIdentifier.end()) {
    log::warn("found existing theme with identifier '{}', skipping",
              theme->identifier());
    return;
  }

  m_baseThemes.push_back(theme);
  m_baseThemesByIdentifier[theme->identifier()] = theme;
}

void ThemeManager::addQtThemes()
{
  for (const auto& key : QStyleFactory::keys()) {
    registerTheme(std::make_shared<Theme>(key.toStdString(), key.toStdString(),
                                          std::filesystem::path{}));
  }
}

namespace
{
QString readWholeFile(std::filesystem::path const& path)
{
  QFile file(path);
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    return {};
  }

  return QTextStream(&file).readAll();
}
}  // namespace

QString ThemeManager::buildStyleSheet(std::shared_ptr<const Theme> const& theme) const
{
  // create the base stylesheet
  QString stylesheet = patchStyleSheet(readWholeFile(theme->stylesheet()),
                                       theme->stylesheet().parent_path());

  for (auto&& globalExtension : m_globalExtensions) {
    stylesheet += "\n" + patchStyleSheet(readWholeFile(globalExtension->stylesheet()),
                                         globalExtension->stylesheet().parent_path());
  }

  auto it = m_themeExtensionsByIdentifier.find(theme->identifier());
  if (it != m_themeExtensionsByIdentifier.end()) {
    for (auto&& themExtension : m_themeExtensionsByIdentifier.at(theme->identifier())) {
      stylesheet += "\n" + patchStyleSheet(readWholeFile(themExtension->stylesheet()),
                                           themExtension->stylesheet().parent_path());
    }
  }

  return stylesheet;
}

QString ThemeManager::patchStyleSheet(QString stylesheet,
                                      std::filesystem::path const& folder) const
{
  // we try to extract url() from the stylesheet and replace them
  QRegularExpression urlRegex(R"re((:|\s+)url\("?([^")]+)"?\))re");

  QString newStyleSheet = "";
  while (!stylesheet.isEmpty()) {
    auto match = urlRegex.match(stylesheet);

    if (match.hasMatch()) {
      QFileInfo path(match.captured(2));
      if (path.isRelative()) {
        path = QFileInfo(QDir(folder), match.captured(2));
      }
      newStyleSheet += stylesheet.left(match.capturedStart()) + match.captured(1) +
                       "url(\"" + path.absoluteFilePath() + "\")";
      stylesheet = stylesheet.mid(match.capturedEnd());
    } else {
      newStyleSheet += stylesheet;
      stylesheet = "";
    }
  }

  return newStyleSheet;
}

void ThemeManager::watchThemeFiles(std::shared_ptr<const Theme> const& theme)
{
  // clear previous files
  QStringList currentWatch = m_watcher.files();
  if (currentWatch.count() != 0) {
    m_watcher.removePaths(currentWatch);
  }

  if (!theme) {
    return;
  }

  // find theme files
  QStringList themeFiles;

  themeFiles.append(ToQString(absolute(theme->stylesheet()).native()));

  for (auto&& themeExtension : m_globalExtensions) {
    themeFiles.append(ToQString(absolute(themeExtension->stylesheet()).native()));
  }

  for (auto&& themeExtension : m_themeExtensionsByIdentifier[theme->identifier()]) {
    themeFiles.append(ToQString(absolute(themeExtension->stylesheet()).native()));
  }

  // add all files
  m_watcher.addPaths(themeFiles);
}

void ThemeManager::extensionLoaded(IExtension const& extension)
{
  for (const auto& theme : extension.themes()) {
    registerTheme(theme);
  }

  for (const auto& themeExtension : extension.themeExtensions()) {
    const auto identifier = themeExtension->baseIdentifier();

    if (identifier.has_value()) {
      m_themeExtensionsByIdentifier[*identifier].push_back(themeExtension);
    } else {
      m_globalExtensions.push_back(themeExtension);
    }
  }
}

void ThemeManager::extensionUnloaded(IExtension const& extension)
{
  // remove theme, unload if needed
  for (const auto& theme : extension.themes()) {
    if (m_currentTheme == theme) {
      unload();
    }

    if (std::erase(m_baseThemes, theme) > 0) {
      m_baseThemesByIdentifier.erase(theme->identifier());
    }
  }

  for (const auto& themeExtension : extension.themeExtensions()) {
    std::erase(m_globalExtensions, themeExtension);

    if (themeExtension->baseIdentifier().has_value() &&
        m_themeExtensionsByIdentifier.contains(*themeExtension->baseIdentifier())) {
      std::erase(m_themeExtensionsByIdentifier[*themeExtension->baseIdentifier()],
                 themeExtension);
    }
  }
}

void ThemeManager::extensionEnabled(IExtension const& extension)
{
  extensionLoaded(extension);
}

void ThemeManager::extensionDisabled(IExtension const& extension)
{
  extensionUnloaded(extension);
}
