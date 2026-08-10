import QtQuick
import QtQuick.Window
import QtQuick.Layouts

import OllamaHud.UI

Window {
    id: overlay
    property var appController

    width: Screen.width
    height: Screen.height
    x: Screen.virtualX
    y: Screen.virtualY
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowTransparentForInput

    Rectangle {
        id: panel
        x: 18
        y: 18
        width: Math.min(620, overlay.width - 48)
        height: appController && appController.hudCollapsed
            ? stateText.implicitHeight + 28
            : Math.max(96, stateText.implicitHeight + messageText.implicitHeight
                + (translationText.visible ? translationLabel.implicitHeight + translationText.implicitHeight + 16 : 0) + 40)
        radius: 8
        color: appController && appController.error ? "#d02024" : "#101014"
        opacity: 0.92
        border.width: 1
        border.color: appController && appController.error ? "#ff7777" : "#33333b"

        Text {
            id: stateText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            text: appController ? appController.state : "준비"
            color: "#ffffff"
            font.pixelSize: 12
            font.family: FontSystem.getContentFontSemiBold.name
            elide: Text.ElideRight
        }

        Text {
            id: messageText
            visible: !appController || !appController.hudCollapsed
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: stateText.bottom
            anchors.margins: 14
            anchors.topMargin: 6
            text: appController ? appController.message : ""
            color: "#ffffff"
            font.pixelSize: 18
            font.family: FontSystem.getContentFontBold.name
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Text {
            id: translationLabel
            visible: translationText.visible
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: messageText.bottom
            anchors.margins: 14
            anchors.topMargin: 12
            text: "한국어 번역"
            color: "#c3c6d0"
            font.pixelSize: 12
            font.family: FontSystem.getContentFontSemiBold.name
        }

        Text {
            id: translationText
            visible: !appController || (!appController.hudCollapsed && appController.koreanTranslation.length > 0)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: translationLabel.bottom
            anchors.margins: 14
            anchors.topMargin: 4
            text: appController ? appController.koreanTranslation : ""
            color: "#ffffff"
            font.pixelSize: 18
            font.family: FontSystem.getContentFontBold.name
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }

    Repeater {
        model: appController ? appController.detectorBoxes : []
        delegate: Item {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: Math.max(1, modelData.width)
            height: Math.max(1, modelData.height)
            Rectangle { anchors.fill: parent; color: "transparent"; border.width: 2; border.color: "#55e6a5" }
            Rectangle {
                x: 0; y: -24; height: 22
                width: labelText.implicitWidth + 12
                color: "#153b2d"
                Text { id: labelText; anchors.centerIn: parent; text: modelData.label + " " + Number(modelData.score).toFixed(2); color: "#ffffff"; font.pixelSize: 12 }
            }
        }
    }
}
