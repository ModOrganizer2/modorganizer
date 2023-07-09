#include "csvbuilder.h"

CSVBuilder::CSVBuilder(QIODevice* target)
    : m_Out(target), m_Separator(','), m_LineBreak(BREAK_CRLF)
{
  m_Out.setEncoding(QStringConverter::Encoding::Utf8);

  m_QuoteMode[TYPE_INTEGER] = QUOTE_NEVER;
  m_QuoteMode[TYPE_FLOAT]   = QUOTE_NEVER;
  m_QuoteMode[TYPE_STRING]  = QUOTE_ONDEMAND;
}

CSVBuilder::~CSVBuilder() {}

void CSVBuilder::setFieldSeparator(char sep)
{
  char oldSeparator = m_Separator;
  m_Separator       = sep;
  try {
    checkFields(m_Fields);
  } catch (const CSVException&) {
    m_Separator = oldSeparator;
    throw;
  }
}

void CSVBuilder::setLineBreak(CSVBuilder::ELineBreak lineBreak)
{
  m_LineBreak = lineBreak;
}

void CSVBuilder::setEscapeMode(CSVBuilder::EFieldType type, CSVBuilder::EQuoteMode mode)
{
  m_QuoteMode[type] = mode;
}

void CSVBuilder::setFields(const std::vector<std::pair<QString, EFieldType>>& fields)
{
  std::vector<QString> fieldNames;
  std::map<QString, EFieldType> fieldTypes;

  for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
    fieldNames.push_back(iter->first);
    fieldTypes[iter->first] = iter->second;
  }

  checkFields(fieldNames);

  m_Fields     = fieldNames;
  m_FieldTypes = fieldTypes;
  m_Defaults.clear();
  m_RowBuffer.clear();
}

void CSVBuilder::checkValue(const QString& field, const QVariant& value)
{
  auto typeIter = m_FieldTypes.find(field);
  if (typeIter == m_FieldTypes.end()) {
    throw CSVException(QObject::tr("invalid field name \"%1\"").arg(field));
  }

  switch (typeIter->second) {
  case TYPE_INTEGER: {
    if (!value.canConvert<int>()) {
      throw CSVException(
          QObject::tr("invalid type for \"%1\" (should be integer)").arg(field));
    }
  } break;
  case TYPE_STRING: {
    if (!value.canConvert<QString>()) {
      throw CSVException(
          QObject::tr("invalid type for \"%1\" (should be string)").arg(field));
    }
  } break;
  case TYPE_FLOAT: {
    if (!value.canConvert<float>()) {
      throw CSVException(
          QObject::tr("invalid type for \"%1\" (should be float)").arg(field));
    }
  } break;
  }
}

void CSVBuilder::setDefault(const QString& field, const QVariant& value)
{
  checkValue(field, value);
  m_Defaults[field] = value;
}

void CSVBuilder::writeHeader()
{
  if (m_Fields.size() == 0) {
    throw CSVException(QObject::tr("no fields set up yet!"));
  }

  for (auto iter = m_Fields.begin(); iter != m_Fields.end(); ++iter) {
    if (iter != m_Fields.begin()) {
      m_Out << m_Separator;
    }
    m_Out << *iter;
  }
  m_Out << lineBreak();
  m_Out.flush();
}

void CSVBuilder::setRowField(const QString& field, const QVariant& value)
{
  checkValue(field, value);
  m_RowBuffer[field] = value;
}

void CSVBuilder::writeData(const std::map<QString, QVariant>& data, bool check)
{
  QString line;
  QTextStream temp(&line);

  for (auto iter = m_Fields.begin(); iter != m_Fields.end(); ++iter) {
    if (iter != m_Fields.begin()) {
      temp << separator();
    }

    QVariant val;

    auto valIter = data.find(*iter);
    if (valIter == data.end()) {
      auto defaultIter = m_Defaults.find(*iter);
      if (defaultIter == m_Defaults.end()) {
        throw CSVException(QObject::tr("field not set \"%1\"").arg(*iter));
      } else {
        val = defaultIter->second;
      }
    } else {
      val = valIter->second;
    }

    if (check) {
      checkValue(*iter, val);
    }

    switch (m_FieldTypes[*iter]) {
    case TYPE_INTEGER: {
      quoteInsert(temp, val.toInt());
    } break;
    case TYPE_FLOAT: {
      quoteInsert(temp, val.toFloat());
    } break;
    case TYPE_STRING: {
      quoteInsert(temp, val.toString());
    } break;
    }
  }
  m_Out << line << lineBreak();
  m_Out.flush();
}

void CSVBuilder::quoteInsert(QTextStream& stream, int value)
{
  switch (m_QuoteMode[TYPE_INTEGER]) {
  case QUOTE_NEVER:
  case QUOTE_ONDEMAND: {
    stream << value;
  } break;
  case QUOTE_ALWAYS: {
    stream << "\"" << value << "\"";
  } break;
  }
}

void CSVBuilder::quoteInsert(QTextStream& stream, float value)
{
  switch (m_QuoteMode[TYPE_FLOAT]) {
  case QUOTE_NEVER:
  case QUOTE_ONDEMAND: {
    stream << value;
  } break;
  case QUOTE_ALWAYS: {
    stream << "\"" << value << "\"";
  } break;
  }
}

void CSVBuilder::quoteInsert(QTextStream& stream, const QString& value)
{
  switch (m_QuoteMode[TYPE_STRING]) {
  case QUOTE_NEVER: {
    stream << value;
  } break;
  case QUOTE_ONDEMAND: {
    if (value.contains("[,\r\n]")) {
      stream << "\"" << value.mid(0).replace("\"", "\"\"") << "\"";
    } else {
      stream << value;
    }
  } break;
  case QUOTE_ALWAYS: {
    stream << "\"" << value.mid(0).replace("\"", "\"\"") << "\"";
  } break;
  }
}

void CSVBuilder::writeRow()
{
  writeData(m_RowBuffer, false);  // data was tested on input
  m_RowBuffer.clear();
}

void CSVBuilder::addRow(const std::map<QString, QVariant>& data)
{
  writeData(data, true);
}

void CSVBuilder::checkFields(const std::vector<QString>& fields)
{
  for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
    if (iter->contains(m_Separator) || iter->contains('\r') || iter->contains('\n') ||
        iter->contains('"')) {
      throw CSVException(QObject::tr("invalid character in field \"%1\"").arg(*iter));
    }
    if (iter->length() == 0) {
      throw CSVException(QObject::tr("empty field name"));
    }
  }
}

/*

if(cell.contains(KDefaultEscapeChar) || cell.contains(KDefaultNewLine)
    || cell.contains(KDefaultDelimiter)) {
    m_currentLine->append(cell.replace(KDefaultNewLine,
                                      QString(KDefaultEscapeChar) + KDefaultEscapeChar)
                         .prepend(KDefaultEscapeChar)
                         .append(KDefaultEscapeChar));
} else {
m_currentLine->append(cell);
}*/

const char* CSVBuilder::lineBreak()
{
  switch (m_LineBreak) {
  case BREAK_CR:
    return "\r";
  case BREAK_CRLF:
    return "\r\n";
  case BREAK_LF:
    return "\n";
  default:
    return "\n";  // default shouldn't be necessary
  }
}

const char CSVBuilder::separator()
{
  return m_Separator;
}
