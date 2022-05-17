#ifndef MODORGANIZER_JSON_INCLUDED
#define MODORGANIZER_JSON_INCLUDED

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <log.h>

namespace json
{

class failed
{};

namespace details
{

  QString typeName(const QJsonValue& v)
  {
    if (v.isUndefined()) {
      return "undefined";
    } else if (v.isNull()) {
      return "null";
    } else if (v.isArray()) {
      return "an array";
    } else if (v.isBool()) {
      return "a bool";
    } else if (v.isDouble()) {
      return "a double";
    } else if (v.isObject()) {
      return "an object";
    } else if (v.isString()) {
      return "a string";
    } else {
      return "an unknown type";
    }
  }

  QString typeName(const QJsonDocument& doc)
  {
    if (doc.isEmpty()) {
      return "empty";
    } else if (doc.isNull()) {
      return "null";
    } else if (doc.isArray()) {
      return "an array";
    } else if (doc.isObject()) {
      return "an object";
    } else {
      return "an unknown type";
    }
  }

  template <class T>
  T convert(const QJsonValue& v) = delete;

  template <>
  bool convert<bool>(const QJsonValue& v)
  {
    if (!v.isBool()) {
      throw failed();
    }

    return v.toBool();
  }

  template <>
  QJsonObject convert<QJsonObject>(const QJsonValue& v)
  {
    if (!v.isObject()) {
      throw failed();
    }

    return v.toObject();
  }

  template <>
  QString convert<QString>(const QJsonValue& v)
  {
    if (!v.isString()) {
      throw failed();
    }

    return v.toString();
  }

  template <>
  QJsonArray convert<QJsonArray>(const QJsonValue& v)
  {
    if (!v.isArray()) {
      throw failed();
    }

    return v.toArray();
  }

  template <>
  qint64 convert<qint64>(const QJsonValue& v)
  {
    if (!v.isDouble()) {
      throw failed();
    }

    return static_cast<qint64>(v.toDouble());
  }

}  // namespace details

template <class T>
T convert(const QJsonValue& value, const char* what)
{
  try {
    return details::convert<T>(value);
  } catch (failed&) {
    MOBase::log::error("'{}' is a {}, not a {}", what, details::typeName(value),
                       typeid(T).name);

    throw;
  }
}

template <class T>
T convertWarn(const QJsonValue& value, const char* what, T def = {})
{
  try {
    return details::convert<T>(value);
  } catch (failed&) {
    MOBase::log::warn("'{}' is a {}, not a {}", what, details::typeName(value),
                      typeid(T).name());

    return def;
  }
}

template <class T>
T get(const QJsonObject& o, const char* e)
{
  if (!o.contains(e)) {
    MOBase::log::error("property '{}' is missing", e);
    throw failed();
  }

  return convert<T>(o[e], e);
}

template <class T>
T getWarn(const QJsonObject& o, const char* e, T def = {})
{
  if (!o.contains(e)) {
    MOBase::log::warn("property '{}' is missing", e);
    return def;
  }

  return convertWarn<T>(o[e], e);
}

template <class T>
T getOpt(const QJsonObject& o, const char* e, T def = {})
{
  if (!o.contains(e)) {
    return def;
  }

  return convertWarn<T>(o[e], e);
}

template <class Value>
void requireObject(const Value& v, const char* what)
{
  if (!v.isObject()) {
    MOBase::log::error("{} is {}, not an object", what, details::typeName(v));
    throw failed();
  }
}

}  // namespace json

#endif  // MODORGANIZER_JSON_INCLUDED
