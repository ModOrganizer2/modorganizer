import QtQuick 2.7

Rectangle {
    radius: 10
    color: "transparent"
    height: 100
    border.color: "black"
    border.width: 3
    opacity: 0.9
    smooth: true

    SequentialAnimation on opacity {
        loops: Animation.Infinite

        PropertyAnimation { easing.type: Easing.InOutSine; duration: 300; to: 0.1 }
        PropertyAnimation { easing.type: Easing.OutInSine; duration: 300; to: 0.7 }
    }
}
