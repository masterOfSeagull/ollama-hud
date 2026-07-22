import QtQuick
import QtQuick.Window
import QtQuick.Layouts

import GenyDL

Window {
    id: overlay
    property var appController

    width: Math.min(620, Screen.width - 48)
    height: Math.max(96, overlayContent.implicitHeight + 28)
    x: 18
    y: 18
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowTransparentForInput

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 8
        color: appController && appController.error ? "#d02024" : "#101014"
        opacity: 0.92
        border.width: 1
        border.color: appController && appController.error ? "#ff7777" : "#33333b"

        ColumnLayout {
            id: overlayContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: appController ? appController.state : "READY"
                color: "#ffffff"
                font.pixelSize: 12
                font.family: FontSystem.getContentFontSemiBold.name
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: appController ? appController.message : ""
                color: "#ffffff"
                font.pixelSize: 18
                font.family: FontSystem.getContentFontBold.name
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }
        }
    }
}
