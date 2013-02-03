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

#include <QRegExp>
#include <map>
#include <algorithm>


namespace BBCode {


class BBCodeMap {

  typedef std::map<QString, std::pair<QRegExp, QString> > TagMap;

public:

  static BBCodeMap &instance() {
    static BBCodeMap s_Instance;
    return s_Instance;
  }

  QString convertTag(const QString &input, int &length)
  {
    // extract the tag name
    m_TagNameExp.indexIn(input, 1, QRegExp::CaretAtOffset);
    QString tagName = m_TagNameExp.cap(0).toLower();
    //qDebug("tag name %s", tagName.toUtf8().constData());
    TagMap::iterator tagIter = m_TagMap.find(tagName);
    if (tagIter != m_TagMap.end()) {
      // recognized tag
      if (tagName.endsWith('=')) {
        tagName.chop(1);
      }
      QString closeTag = tagName == "*" ? "<br/>"
                                         : QString("[/%1]").arg(tagName);

      int closeTagPos = input.indexOf(closeTag, 0, Qt::CaseInsensitive);
      //qDebug("close tag %s at %d", closeTag.toUtf8().constData(), closeTagPos);

      if (closeTagPos > -1) {
        length = closeTagPos + closeTag.length();
        QString temp = input.mid(0, length);
        if (tagIter->second.first.indexIn(temp) == 0) {
          return temp.replace(tagIter->second.first, tagIter->second.second);
        } else {
          // expression doesn't match. either the input string is invalid
          // or the expression is
          qWarning("%s doesn't match the expression for %s",
                   temp.toUtf8().constData(), tagName.toUtf8().constData());
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
  BBCodeMap()
    : m_TagNameExp("^[a-zA-Z*]*=?")
  {
    m_TagMap["b"]      = std::make_pair(QRegExp("\\[b\\](.*)\\[/b\\]"),
                                        "<b>\\1</b>");
    m_TagMap["i"]      = std::make_pair(QRegExp("\\[i\\](.*)\\[/i\\]"),
                                        "<i>\\1</i>");
    m_TagMap["u"]      = std::make_pair(QRegExp("\\[u\\](.*)\\[/u\\]"),
                                        "<u>\\1</u>");
    m_TagMap["s"]      = std::make_pair(QRegExp("\\[s\\](.*)\\[/s\\]"),
                                        "<s>\\1</s>");
    m_TagMap["sub"]    = std::make_pair(QRegExp("\\[sub\\](.*)\\[/sub\\]"),
                                        "<sub>\\1</sub>");
    m_TagMap["sup"]    = std::make_pair(QRegExp("\\[sup\\](.*)\\[/sup\\]"),
                                        "<sup>\\1</sup>");
    m_TagMap["size="]  = std::make_pair(QRegExp("\\[size=([^\\]]*)\\](.*)\\[/size\\]"),
                                        "<font size=\"\\1\">\\2</font>");
    m_TagMap["color="] = std::make_pair(QRegExp("\\[color=([^\\]]*)\\](.*)\\[/color\\]"),
                                        "<font style=\"color: #\\1;\">\\2</font>");
    m_TagMap["font="]  = std::make_pair(QRegExp("\\[font=([^\\]]*)\\](.*)\\[/font\\]"),
                                        "<font face=\\1>\\2</font>");
    m_TagMap["center"] = std::make_pair(QRegExp("\\[center\\](.*)\\[/center\\]"),
                                        "<div align=\"center\">\\1</div>");
    m_TagMap["quote"]  = std::make_pair(QRegExp("\\[quote\\](.*)\\[/quote\\]"),
                                        "<blockquote>\"\\1\"</blockquote>");
    m_TagMap["quote="] = std::make_pair(QRegExp("\\[quote=([^\\]]*)\\](.*)\\[/quote\\]"),
                                        "<blockquote>\"\\2\"<br/><span>--\\1</span></blockquote></p>");
    m_TagMap["code"]   = std::make_pair(QRegExp("\\[code\\](.*)\\[/code\\]"),
                                        "<pre>\\1</pre>");
    m_TagMap["heading"]= std::make_pair(QRegExp("\\[heading\\](.*)\\[/heading\\]"),
                                        "<h2><strong>\\1</strong></h2>");

    // lists
    m_TagMap["list"]  = std::make_pair(QRegExp("\\[list\\](.*)\\[/list\\]"),
                                       "<ul>\\1</ul>");
    m_TagMap["list="] = std::make_pair(QRegExp("\\[list.*\\](.*)\\[/list\\]"),
                                       "<ol>\\1</ol>");
    m_TagMap["ul"]    = std::make_pair(QRegExp("\\[ul\\](.*)\\[/ul\\]"),
                                       "<ul>\\1</ul>");
    m_TagMap["ol"]    = std::make_pair(QRegExp("\\[ol\\](.*)\\[/ol\\]"),
                                       "<ol>\\1</ol>");
    m_TagMap["li"]    = std::make_pair(QRegExp("\\[li\\](.*)\\[/li\\]"),
                                       "<li>\\1</li>");
    m_TagMap["*"]     = std::make_pair(QRegExp("\\[\\*\\](.*)<br/>"),
                                       "<li>\\1</li>");

    // tables
    m_TagMap["table"] = std::make_pair(QRegExp("\\[table\\](.*)\\[/table\\]"),
                                       "<table>\\1</table>");
    m_TagMap["tr"]    = std::make_pair(QRegExp("\\[tr\\](.*)\\[/tr\\]"),
                                       "<tr>\\1</tr>");
    m_TagMap["th"]    = std::make_pair(QRegExp("\\[th\\](.*)\\[/th\\]"),
                                       "<th>\\1</th>");
    m_TagMap["td"]    = std::make_pair(QRegExp("\\[td\\](.*)\\[/td\\]"),
                                       "<td>\\1</td>");

    // web content
    m_TagMap["url"]     = std::make_pair(QRegExp("\\[url\\](.*)\\[/url\\]"),
                                         "<a href=\"\\1\">\\1</a>");
    m_TagMap["url="]    = std::make_pair(QRegExp("\\[url=([^\\]]*)\\](.*)\\[/url\\]"),
                                         "<a href=\"\\1\">\\2</a>");
    m_TagMap["img"]     = std::make_pair(QRegExp("\\[img\\](.*)\\[/img\\]"),
                                         "<img src=\"\\1\"/>");
    m_TagMap["img="]     = std::make_pair(QRegExp("\\[img=([^\\]]*)\\](.*)\\[/img\\]"),
                                         "<img src=\"\\2\" align=\"\\1\" />");
    m_TagMap["email="]  = std::make_pair(QRegExp("\\[email=\"?([^\\]]*)\"?\\](.*)\\[/email\\]"),
                                         "<a href=\"mailto:\\1\">\\2</a>");
    m_TagMap["youtube"] = std::make_pair(QRegExp("\\[youtube\\](.*)\\[/youtube\\]"),
                                         "<a href=\"http://www.youtube.com/v/\\1\">http://www.youtube.com/v/\\1</a>");

    // make all patterns non-greedy and case-insensitive
    for (TagMap::iterator iter = m_TagMap.begin(); iter != m_TagMap.end(); ++iter) {
      iter->second.first.setCaseSensitivity(Qt::CaseInsensitive);
      iter->second.first.setMinimal(true);
    }
  }


private:
  QRegExp m_TagNameExp;
  TagMap m_TagMap;
};


QString convertToHTML(const QString &inputParam)
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
  int pos = 0;

  // iterate over the input buffer
  while ((pos = input.indexOf('[', lastBlock)) != -1) {
    // append everything between the previous tag-block and the current one
    result.append(input.midRef(lastBlock, pos - lastBlock));

    // convert the tag and content if necessary
    int length = -1;
    QString replacement = BBCodeMap::instance().convertTag(input.mid(pos), length);
    if (length != 0) {
      QString temp = convertToHTML(replacement);
      result.append(temp);
      // length contains the number of characters in the original tag
      pos += length;
    } else {
      // nothing replaced
      result.append('[');
      ++pos;
    }
    lastBlock = pos;
  }

  // append the remainder (everything after the last tag)
  result.append(input.midRef(lastBlock));
  return result;
}

} // namespace BBCode

