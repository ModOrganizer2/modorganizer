import QtQuick 2.7


Rectangle {
  color: "#EE888888"
  width: childrenRect.width + 15
  height: childrenRect.height + 15
  z: 20000
  clip: false
  border.color: "black"
  border.width: 1
  property alias text: tooltipText.text
  Text {
    id: tooltipText
    font.pointSize: 11
    wrapMode: Text.WordWrap
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.leftMargin: 7
    anchors.rightMargin: 7
    z: parent.z
    color: "black"
    onTextChanged: {
      if (width > 200)
        width = 200
    }
  }
  visible: false
}
