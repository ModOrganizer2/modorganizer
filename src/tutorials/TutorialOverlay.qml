import QtQuick 2.7
import "tutorials.js" as Logic

Rectangle  {
  color: "transparent"

  id: tutToplevel

  property int step : 0
  property alias description: tutDescription
  property alias highlight: tutHighlight
  property alias text: tutDescription.text
  property alias boxOpacity: tutDescription.opacity
  property int offsetBottom: 50
  property int maxWidth: 400

  signal tabChanged(int index)

  function init() {
    Logic.init()
    Logic.nextStep()
  }

  function enableBackground(enabled) {
    disabledBackground.visible = enabled
  }

  function backgroundEnabled() {
    return disabledBackground.visible
  }

  function nextStep() {
    if (step == 0) {
      Logic.init()
    }

    Logic.nextStep()
  }

  TutorialDescription {
    id: tutDescription
    innerWidth: maxWidth
    anchors.bottomMargin: offsetBottom
    onClicked: {
      Logic.clickNext()
    }
  }

  Rectangle {
    id: disabledBackground
    anchors.fill: parent
    opacity: 0.2
    color: "#808080"
  }

  Connections {
    target: manager
    onTabChanged: tabChanged(index)
  }

  Tooltip {
    id: tooltip
  }

  Highlight {
    id: tutHighlight
  }
}
