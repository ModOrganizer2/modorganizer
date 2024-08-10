#ifndef SETTINGSUTILITIES_H
#define SETTINGSUTILITIES_H

#include <uibase/log.h>

#include <QAbstractButton>
#include <QHeaderView>

namespace MOBase
{
class ExpanderWidget;
}

bool shouldLogSetting(const QString& displayName);

template <class T>
void logChange(const QString& displayName, std::optional<T> oldValue, const T& newValue)
{
  if (!shouldLogSetting(displayName)) {
    return;
  }

  if (oldValue) {
    MOBase::log::debug("setting '{}' changed from '{}' to '{}'", displayName, *oldValue,
                       newValue);
  } else {
    MOBase::log::debug("setting '{}' set to '{}'", displayName, newValue);
  }
}

void logRemoval(const QString& name);

QString settingName(const QString& section, const QString& key);

template <class T>
void setImpl(QSettings& settings, const QString& displayName, const QString& section,
             const QString& key, const T& value)
{
  const auto current = getOptional<T>(settings, section, key);

  if (current && *current == value) {
    // no change
    return;
  }

  const auto name = settingName(section, key);

  logChange(displayName, current, value);

  if constexpr (std::is_enum_v<T>) {
    settings.setValue(name, static_cast<std::underlying_type_t<T>>(value));
  } else {
    settings.setValue(name, value);
  }
}

void removeImpl(QSettings& settings, const QString& displayName, const QString& section,
                const QString& key);

template <class T>
std::optional<T> getOptional(const QSettings& settings, const QString& section,
                             const QString& key, std::optional<T> def = {})
{
  if (settings.contains(settingName(section, key))) {
    const auto v = settings.value(settingName(section, key));

    if constexpr (std::is_enum_v<T>) {
      return static_cast<T>(v.value<std::underlying_type_t<T>>());
    } else {
      return v.value<T>();
    }
  }

  return def;
}

template <class T>
T get(const QSettings& settings, const QString& section, const QString& key, T def)
{
  if (auto v = getOptional<T>(settings, section, key)) {
    return *v;
  } else {
    return def;
  }
}

template <class T>
void set(QSettings& settings, const QString& section, const QString& key,
         const T& value)
{
  setImpl(settings, settingName(section, key), section, key, value);
}

void remove(QSettings& settings, const QString& section, const QString& key);
void removeSection(QSettings& settings, const QString& section);

class ScopedGroup
{
public:
  ScopedGroup(QSettings& s, const QString& name);
  ~ScopedGroup();

  ScopedGroup(const ScopedGroup&)            = delete;
  ScopedGroup& operator=(const ScopedGroup&) = delete;

  template <class T>
  void set(const QString& key, const T& value)
  {
    setImpl(m_settings, settingName(m_name, key), "", key, value);
  }

  void remove(const QString& key);

  QStringList keys() const;

  template <class F>
  void for_each(F&& f) const
  {
    for (const QString& key : keys()) {
      f(key);
    }
  }

  template <class T>
  std::optional<T> getOptional(const QString& key, std::optional<T> def = {}) const
  {
    return ::getOptional<T>(m_settings, "", key, def);
  }

  template <class T>
  T get(const QString& key, T def = {}) const
  {
    return ::get<T>(m_settings, "", key, def);
  }

private:
  QSettings& m_settings;
  QString m_name;
};

class ScopedReadArray
{
public:
  ScopedReadArray(QSettings& s, const QString& section);
  ~ScopedReadArray();

  ScopedReadArray(const ScopedReadArray&)            = delete;
  ScopedReadArray& operator=(const ScopedReadArray&) = delete;

  template <class F>
  void for_each(F&& f) const
  {
    for (int i = 0; i < count(); ++i) {
      m_settings.setArrayIndex(i);
      f();
    }
  }

  template <class T>
  std::optional<T> getOptional(const QString& key, std::optional<T> def = {}) const
  {
    return ::getOptional<T>(m_settings, "", key, def);
  }

  template <class T>
  T get(const QString& key, T def = {}) const
  {
    return ::get<T>(m_settings, "", key, def);
  }

  int count() const;
  QStringList keys() const;

private:
  QSettings& m_settings;
  int m_count;
};

class ScopedWriteArray
{
public:
  static const auto NoSize = std::numeric_limits<std::size_t>::max();

  ScopedWriteArray(QSettings& s, const QString& section, std::size_t size = NoSize);
  ~ScopedWriteArray();

  ScopedWriteArray(const ScopedWriteArray&)            = delete;
  ScopedWriteArray& operator=(const ScopedWriteArray&) = delete;

  void next();

  template <class T>
  void set(const QString& key, const T& value)
  {
    const auto displayName = QString("%1/%2\\%3").arg(m_section).arg(m_i).arg(key);

    setImpl(m_settings, displayName, "", key, value);
  }

private:
  QSettings& m_settings;
  QString m_section;
  int m_i;
};

QString widgetNameWithTopLevel(const QWidget* widget);
QString widgetName(const QMainWindow* w);
QString widgetName(const QHeaderView* w);
QString widgetName(const MOBase::ExpanderWidget* w);
QString widgetName(const QWidget* w);

template <class Widget>
QString geoSettingName(const Widget* widget)
{
  return widgetName(widget) + "_geometry";
}

template <class Widget>
QString stateSettingName(const Widget* widget)
{
  return widgetName(widget) + "_state";
}

template <class Widget>
QString visibilitySettingName(const Widget* widget)
{
  return widgetName(widget) + "_visibility";
}

QString dockSettingName(const QDockWidget* dock);
QString indexSettingName(const QWidget* widget);
QString checkedSettingName(const QAbstractButton* b);

void warnIfNotCheckable(const QAbstractButton* b);

bool setWindowsCredential(const QString& key, const QString& data);
QString getWindowsCredential(const QString& key);

#endif  // SETTINGSUTILITIES_H
