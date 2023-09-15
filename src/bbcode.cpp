/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

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

#include "bbcode.h"
#include <QRegularExpression>
#include <log.h>
#include <map>

namespace BBCode
{

namespace log = MOBase::log;

class BBCodeMap
{

  typedef std::map<QString, std::pair<QRegularExpression, QString>> TagMap;

public:
  static BBCodeMap& instance()
  {
    static BBCodeMap s_Instance;
    return s_Instance;
  }

  QString convertTag(QString input, int& length)
  {
    // extract the tag name
    auto match      = m_TagNameExp.match(input, 1, QRegularExpression::NormalMatch,
                                         QRegularExpression::AnchoredMatchOption);
    QString tagName = match.captured(0).toLower();
    TagMap::iterator tagIter = m_TagMap.find(tagName);
    if (tagIter != m_TagMap.end()) {
      // recognized tag
      if (tagName.endsWith('=')) {
        tagName.chop(1);
      }

      int closeTagPos        = 0;
      int nextTagPos         = 0;
      int nextTagSearchIndex = input.indexOf("]");
      int closeTagLength     = 0;
      if (tagName == "*") {
        // ends at the next bullet point
        closeTagPos =
            input.indexOf(QRegularExpression("(\\[\\*\\]|</ul>)",
                                             QRegularExpression::CaseInsensitiveOption),
                          3);
        // leave closeTagLength at 0 because we don't want to "eat" the next bullet
        // point
      } else if (tagName == "line") {
        // ends immediately after the tag
        closeTagPos = 6;
        // leave closeTagLength at 0 because there is no close tag to skip over
      } else {
        QRegularExpression nextTag(QString("\\[%1[=\\]]?").arg(tagName),
                                   QRegularExpression::CaseInsensitiveOption);
        QString closeTag = QString("[/%1]").arg(tagName);
        closeTagPos      = input.indexOf(closeTag, 0, Qt::CaseInsensitive);
        nextTagPos       = nextTag.match(input, nextTagSearchIndex).capturedStart(0);
        while (nextTagPos != -1 && closeTagPos != -1 && nextTagPos < closeTagPos) {
          closeTagPos        = input.indexOf(closeTag, closeTagPos + closeTag.size(),
                                             Qt::CaseInsensitive);
          nextTagSearchIndex = input.indexOf("]", nextTagPos);
          nextTagPos = nextTag.match(input, nextTagSearchIndex).capturedStart(0);
        }
        if (closeTagPos == -1) {
          // workaround to improve compatibility: add fake closing tag
          input.append(closeTag);
          closeTagPos = input.size() - closeTag.size();
        }
        closeTagLength = closeTag.size();
      }

      if (closeTagPos > -1) {
        length       = closeTagPos + closeTagLength;
        QString temp = input.mid(0, length);
        tagIter->second.first.setPatternOptions(
            QRegularExpression::PatternOption::DotMatchesEverythingOption);
        auto match = tagIter->second.first.match(temp);
        if (match.hasMatch()) {
          if (tagIter->second.second.isEmpty()) {
            if (tagName == "color") {
              QString color   = match.captured(1);
              QString content = match.captured(2);
              if (color.at(0) == '#') {
                return temp.replace(tagIter->second.first,
                                    QString("<font style=\"color: %1;\">%2</font>")
                                        .arg(color, content));
              } else {
                auto colIter = m_ColorMap.find(color.toLower());
                if (colIter != m_ColorMap.end()) {
                  color = colIter->second;
                }
                return temp.replace(tagIter->second.first,
                                    QString("<font style=\"color: #%1;\">%2</font>")
                                        .arg(color, content));
              }
            } else {
              log::warn("don't know how to deal with tag {}", tagName);
            }
          } else {
            if (tagName == "*") {
              temp.remove(QRegularExpression("(\\[/\\*\\])?(<br/>)?$"));
            }
            return temp.replace(tagIter->second.first, tagIter->second.second);
          }
        } else {
          // expression doesn't match. either the input string is invalid
          // or the expression is
          log::warn("{} doesn't match the expression for {}", temp, tagName);
          length = 0;
          return QString();
        }
      }
    }

    // not a recognized tag or tag invalid
    length = 0;
    return QString();
  }

private:
  BBCodeMap() : m_TagNameExp("[a-zA-Z*]*=?")
  {
    m_TagMap["b"] =
        std::make_pair(QRegularExpression("\\[b\\](.*)\\[/b\\]"), "<b>\\1</b>");
    m_TagMap["i"] =
        std::make_pair(QRegularExpression("\\[i\\](.*)\\[/i\\]"), "<i>\\1</i>");
    m_TagMap["u"] =
        std::make_pair(QRegularExpression("\\[u\\](.*)\\[/u\\]"), "<u>\\1</u>");
    m_TagMap["s"] =
        std::make_pair(QRegularExpression("\\[s\\](.*)\\[/s\\]"), "<s>\\1</s>");
    m_TagMap["sub"] =
        std::make_pair(QRegularExpression("\\[sub\\](.*)\\[/sub\\]"), "<sub>\\1</sub>");
    m_TagMap["sup"] =
        std::make_pair(QRegularExpression("\\[sup\\](.*)\\[/sup\\]"), "<sup>\\1</sup>");
    m_TagMap["size="] =
        std::make_pair(QRegularExpression("\\[size=([^\\]]*)\\](.*)\\[/size\\]"),
                       "<font size=\"\\1\">\\2</font>");
    m_TagMap["color="] =
        std::make_pair(QRegularExpression("\\[color=([^\\]]*)\\](.*)\\[/color\\]"), "");
    m_TagMap["font="] =
        std::make_pair(QRegularExpression("\\[font=([^\\]]*)\\](.*)\\[/font\\]"),
                       "<font style=\"font-family: \\1;\">\\2</font>");
    m_TagMap["center"] =
        std::make_pair(QRegularExpression("\\[center\\](.*)\\[/center\\]"),
                       "<div align=\"center\">\\1</div>");
    m_TagMap["right"] =
        std::make_pair(QRegularExpression("\\[right\\](.*)\\[/right\\]"),
                       "<div align=\"right\">\\1</div>");
    m_TagMap["quote"] =
        std::make_pair(QRegularExpression("\\[quote\\](.*)\\[/quote\\]"),
                       "<figure class=\"quote\"><blockquote>\\1</blockquote></figure>");
    m_TagMap["quote="] =
        std::make_pair(QRegularExpression("\\[quote=([^\\]]*)\\](.*)\\[/quote\\]"),
                       "<figure class=\"quote\"><blockquote>\\2</blockquote></figure>");
    m_TagMap["spoiler"] =
        std::make_pair(QRegularExpression("\\[spoiler\\](.*)\\[/spoiler\\]"),
                       "<details><summary>Spoiler:  <div "
                       "class=\"bbc_spoiler_show\">Show</div></summary><div "
                       "class=\"spoiler_content\">\\1</div></details>");
    m_TagMap["code"] = std::make_pair(QRegularExpression("\\[code\\](.*)\\[/code\\]"),
                                      "<code>\\1</code>");
    m_TagMap["heading"] =
        std::make_pair(QRegularExpression("\\[heading\\](.*)\\[/heading\\]"),
                       "<h2><strong>\\1</strong></h2>");
    m_TagMap["line"] = std::make_pair(QRegularExpression("\\[line\\]"), "<hr>");

    // lists
    m_TagMap["list"] =
        std::make_pair(QRegularExpression("\\[list\\](.*)\\[/list\\]"), "<ul>\\1</ul>");
    m_TagMap["list="] = std::make_pair(
        QRegularExpression("\\[list.*\\](.*)\\[/list\\]"), "<ol>\\1</ol>");
    m_TagMap["ul"] =
        std::make_pair(QRegularExpression("\\[ul\\](.*)\\[/ul\\]"), "<ul>\\1</ul>");
    m_TagMap["ol"] =
        std::make_pair(QRegularExpression("\\[ol\\](.*)\\[/ol\\]"), "<ol>\\1</ol>");
    m_TagMap["li"] =
        std::make_pair(QRegularExpression("\\[li\\](.*)\\[/li\\]"), "<li>\\1</li>");

    // tables
    m_TagMap["table"] = std::make_pair(
        QRegularExpression("\\[table\\](.*)\\[/table\\]"), "<table>\\1</table>");
    m_TagMap["tr"] =
        std::make_pair(QRegularExpression("\\[tr\\](.*)\\[/tr\\]"), "<tr>\\1</tr>");
    m_TagMap["th"] =
        std::make_pair(QRegularExpression("\\[th\\](.*)\\[/th\\]"), "<th>\\1</th>");
    m_TagMap["td"] =
        std::make_pair(QRegularExpression("\\[td\\](.*)\\[/td\\]"), "<td>\\1</td>");

    // web content
    m_TagMap["url"] = std::make_pair(QRegularExpression("\\[url\\](.*)\\[/url\\]"),
                                     "<a href=\"\\1\">\\1</a>");
    m_TagMap["url="] =
        std::make_pair(QRegularExpression("\\[url=([^\\]]*)\\](.*)\\[/url\\]"),
                       "<a href=\"\\1\">\\2</a>");
    m_TagMap["img"] = std::make_pair(
        QRegularExpression(
            "\\[img(?:\\s*width=\\d+\\s*,?\\s*height=\\d+)?\\](.*)\\[/img\\]"),
        "<img src=\"\\1\">");
    m_TagMap["img="] =
        std::make_pair(QRegularExpression("\\[img=([^\\]]*)\\](.*)\\[/img\\]"),
                       "<img src=\"\\2\" alt=\"\\1\">");
    m_TagMap["email="] = std::make_pair(
        QRegularExpression("\\[email=\"?([^\\]]*)\"?\\](.*)\\[/email\\]"),
        "<a href=\"mailto:\\1\">\\2</a>");
    m_TagMap["youtube"] =
        std::make_pair(QRegularExpression("\\[youtube\\](.*)\\[/youtube\\]"),
                       "<a "
                       "href=\"https://www.youtube.com/watch?v=\\1\">https://"
                       "www.youtube.com/watch?v=\\1</a>");

    // make all patterns non-greedy and case-insensitive
    for (TagMap::iterator iter = m_TagMap.begin(); iter != m_TagMap.end(); ++iter) {
      iter->second.first.setPatternOptions(
          QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::InvertedGreedinessOption);
    }

    // this tag is in fact greedy
    m_TagMap["*"] = std::make_pair(QRegularExpression("\\[\\*\\](.*)"), "<li>\\1</li>");

    m_ColorMap.insert(std::make_pair<QString, QString>("red", "FF0000"));
    m_ColorMap.insert(std::make_pair<QString, QString>("green", "00FF00"));
    m_ColorMap.insert(std::make_pair<QString, QString>("blue", "0000FF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("black", "000000"));
    m_ColorMap.insert(std::make_pair<QString, QString>("gray", "7F7F7F"));
    m_ColorMap.insert(std::make_pair<QString, QString>("white", "FFFFFF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("yellow", "FFFF00"));
    m_ColorMap.insert(std::make_pair<QString, QString>("cyan", "00FFFF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("magenta", "FF00FF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("brown", "A52A2A"));
    m_ColorMap.insert(std::make_pair<QString, QString>("orange", "FFA500"));
    m_ColorMap.insert(std::make_pair<QString, QString>("gold", "FFD700"));
    m_ColorMap.insert(std::make_pair<QString, QString>("deepskyblue", "00BFFF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("salmon", "FA8072"));
    m_ColorMap.insert(std::make_pair<QString, QString>("dodgerblue", "1E90FF"));
    m_ColorMap.insert(std::make_pair<QString, QString>("greenyellow", "ADFF2F"));
    m_ColorMap.insert(std::make_pair<QString, QString>("peru", "CD853F"));
  }

private:
  QRegularExpression m_TagNameExp;
  TagMap m_TagMap;
  std::map<QString, QString> m_ColorMap;
};

QString convertToHTML(const QString& inputParam)
{
  // this code goes over the input string once and replaces all bbtags
  // it encounters. This function is called recursively for every replaced
  // string to convert nested tags.
  //
  // This could be implemented simpler by applying a set of regular expressions
  // for each recognized bb-tag one after the other but that would probably be
  // very inefficient (O(n^2)).

  QString input = inputParam.mid(0).replace("\r\n", "<br/>");
  input.replace("\\\"", "\"").replace("\\'", "'");
  QString result;
  int lastBlock = 0;
  int pos       = 0;

  // iterate over the input buffer
  while ((pos = input.indexOf('[', lastBlock)) != -1) {
    // append everything between the previous tag-block and the current one
    result.append(input.mid(lastBlock, pos - lastBlock));

    if ((pos < (input.size() - 1)) && (input.at(pos + 1) == '/')) {
      // skip invalid end tag
      int tagEnd = input.indexOf(']', pos) + 1;
      if (tagEnd == 0) {
        // no closing tag found
        // move the pos up one so that the opening bracket is ignored next iteration
        pos++;
      } else {
        pos = tagEnd;
      }
    } else {
      // convert the tag and content if necessary
      int length          = -1;
      QString replacement = BBCodeMap::instance().convertTag(input.mid(pos), length);
      if (length != 0) {
        result.append(convertToHTML(replacement));
        // length contains the number of characters in the original tag
        pos += length;
      } else {
        // nothing replaced
        result.append('[');
        ++pos;
      }
    }
    lastBlock = pos;
  }

  // append the remainder (everything after the last tag)
  result.append(input.mid(lastBlock));
  return result;
}

}  // namespace BBCode
