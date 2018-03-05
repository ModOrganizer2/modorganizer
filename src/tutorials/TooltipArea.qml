import QtQuick 2.7

Rectangle {
  radius: 2
  color: clickable ? "#AA6666AA" : "#AA882222"
  height: 100
  border.color: "black"
  border.width: 2
  z: 1
  smooth: true
  property bool wasLocked: false
  property bool clickable: false
  property string tooltipText: ""

  MouseArea {
    id: clickArea
    anchors.fill: parent
    hoverEnabled: true

    onPositionChanged: {
      if (parent.parent.width - (parent.x + mouseX) < tooltip.width + 50) {
        tooltip.x = parent.x +  mouseX - 15 - tooltip.width
      } else {
        tooltip.x = parent.x + mouseX + 15
      }

      if (parent.parent.height - (parent.y + mouseY) < tooltip.height + 50) {
        tooltip.y = parent.y + mouseY - 15 - tooltip.height
      } else {
        tooltip.y = parent.y + mouseY + 15
      }
    }

    onEntered: {
      tooltip.visible = true
      tooltip.text = tooltipText
    }

    onPressed: {
      wasLocked = tutToplevel.backgroundEnabled()
      if (wasLocked && clickable) {
        tutorialControl.simulateClick(mouseX + parent.x, mouseY + parent.y)
      }
      mouse.accepted = false
    }

    onCanceled: {
      tooltip.visible = false
    }

    onExited: {
      tooltip.visible = false
    }
  }
}
