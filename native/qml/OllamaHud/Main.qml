import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

import GenyDL
import GenyDL.Controls as Controls

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "Ollama HUD"
    color: Colors.pageground

    QtObject {
        id: appRootObjects
        property bool isLeftToRight: true
    }

    Component.onCompleted: AppGlobals.appWindow = root

    Rectangle {
        anchors.fill: parent
        color: Colors.pageground

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 248
                Layout.fillHeight: true
                color: Colors.sideBarContainer

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 14

                    Text {
                        text: "Ollama HUD"
                        color: Colors.textPrimary
                        font.family: FontSystem.getContentFontBold.name
                        font.pixelSize: 24
                    }

                    Text {
                        Layout.fillWidth: true
                        text: appController.hudRunning ? "Overlay running" : "Control panel"
                        color: appController.hudRunning ? Colors.success : Colors.textSecondary
                        font.pixelSize: Typography.t2
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Colors.lineBorderActivated
                    }

                    Repeater {
                        model: ["Runtime", "Prompt", "Settings", "Log"]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 42
                            radius: 8
                            color: nav.currentIndex === index ? Colors.backgroundActivated : "transparent"
                            border.width: nav.currentIndex === index ? 1 : 0
                            border.color: Colors.borderActivated

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                text: modelData
                                color: nav.currentIndex === index ? Colors.textPrimary : Colors.textSecondary
                                font.pixelSize: Typography.t2
                                font.family: nav.currentIndex === index ? FontSystem.getContentFontSemiBold.name : FontSystem.getContentFontRegular.name
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: nav.currentIndex = index
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 76
                    color: Colors.background
                    border.width: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 22
                        anchors.rightMargin: 22
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                text: appController.state
                                color: appController.error ? Colors.error : Colors.textPrimary
                                font.pixelSize: Typography.h4
                                font.family: FontSystem.getContentFontBold.name
                            }
                            Text {
                                Layout.fillWidth: true
                                text: appController.message
                                color: Colors.textSecondary
                                font.pixelSize: Typography.t2
                                elide: Text.ElideRight
                            }
                        }

                        Controls.Button {
                            text: appController.hudRunning ? "Stop HUD" : "Start HUD"
                            isDefault: true
                            style: appController.hudRunning ? "danger" : "success"
                            onClicked: appController.hudRunning ? appController.stopHud() : appController.startHud()
                        }
                        Controls.Button {
                            text: appController.active ? "Working" : "Ask Now"
                            enabled: !appController.active
                            onClicked: appController.captureOnce()
                        }
                        Controls.Button {
                            text: "Test"
                            enabled: !appController.active
                            onClicked: appController.testOllama()
                        }
                    }
                }

                StackLayout {
                    id: nav
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0

                    ScrollView {
                        id: runtimeScroll
                        clip: true
                        contentWidth: availableWidth
                        contentHeight: runtimeContent.implicitHeight + 44

                        Item {
                            width: runtimeScroll.availableWidth
                            height: runtimeContent.implicitHeight + 44

                            ColumnLayout {
                                id: runtimeContent
                                x: 22
                                y: 22
                                width: Math.max(320, parent.width - 44)
                                spacing: 16

                                RuntimeCard {
                                    Layout.fillWidth: true
                                    expanded: true
                                    title: "Runtime"
                                    body: appController.visualAnswer.length > 0 ? appController.visualAnswer : "Ready for the next trigger."
                                    foot: "Trigger " + appController.settingsStore.triggerShortcut + "  |  Collapse " + appController.settingsStore.clearShortcut + "  |  Exit " + appController.settingsStore.exitShortcut
                                    stateColor: appController.error ? Colors.error : (appController.active ? Colors.warning : Colors.success)
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "Model"
                                        body: appController.settingsStore.model
                                        foot: appController.settingsStore.host
                                    }
                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "Capture"
                                        body: appController.captureId.length > 0 ? appController.captureId : "No capture yet"
                                        foot: "Max edge " + appController.settingsStore.screenshotMaxEdge + " px"
                                    }
                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "Memory"
                                        body: appController.settingsStore.memoryQaPairs + " Q/A pairs"
                                        foot: appController.settingsStore.think ? "Thinking enabled" : "Thinking disabled"
                                    }
                                }
                            }
                        }
                    }

                    SettingsPage {
                        promptOnly: true
                    }

                    SettingsPage {
                        promptOnly: false
                    }

                    ScrollView {
                        id: logScroll
                        clip: true
                        contentWidth: availableWidth
                        contentHeight: logContent.implicitHeight + 44

                        Item {
                            width: logScroll.availableWidth
                            height: logContent.implicitHeight + 44

                            ColumnLayout {
                                id: logContent
                                x: 22
                                y: 22
                                width: Math.max(320, parent.width - 44)
                                spacing: 12
                                RuntimeCard {
                                    Layout.fillWidth: true
                                    title: "Chat Log"
                                    body: "Text-only request history is appended to logs/chat.log."
                                    foot: "Screenshot payloads are omitted from the log."
                                }
                                Controls.Button {
                                    text: "Toggle HUD Collapse"
                                    onClicked: appController.toggleHudCollapsed()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component RuntimeCard: Rectangle {
        property string title
        property string body
        property string foot
        property color stateColor: Colors.secondry
        property bool expanded: false

        radius: 8
        color: Colors.backgroundActivated
        border.width: 1
        border.color: Colors.borderActivated
        implicitHeight: expanded ? cardContent.implicitHeight + 36 : 156
        Layout.preferredHeight: implicitHeight
        Layout.minimumHeight: implicitHeight

        ColumnLayout {
            id: cardContent
            anchors.fill: parent
            anchors.margins: 18
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: stateColor
                }
                Text {
                    text: title
                    color: Colors.textSecondary
                    font.pixelSize: Typography.t3
                    font.family: FontSystem.getContentFontSemiBold.name
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.fillHeight: !expanded
                text: body
                color: Colors.textPrimary
                font.pixelSize: Typography.h4
                font.family: FontSystem.getContentFontBold.name
                wrapMode: Text.WordWrap
                elide: expanded ? Text.ElideNone : Text.ElideRight
                maximumLineCount: expanded ? 0 : 3
            }
            Text {
                Layout.fillWidth: true
                text: foot
                color: Colors.textMuted
                font.pixelSize: Typography.t3
                elide: Text.ElideRight
            }
        }
    }

    component FieldLabel: Text {
        color: Colors.textSecondary
        font.pixelSize: Typography.t3
        font.family: FontSystem.getContentFontSemiBold.name
    }

    component SettingsPage: ScrollView {
        id: settingsScroll
        property bool promptOnly: false
        clip: true
        contentWidth: availableWidth
        contentHeight: settingsContent.implicitHeight + 44

        Item {
            width: settingsScroll.availableWidth
            height: settingsContent.implicitHeight + 44

            ColumnLayout {
                id: settingsContent
                x: 22
                y: 22
                width: Math.max(520, parent.width - 44)
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    radius: 8
                    color: Colors.backgroundActivated
                    border.width: 1
                    border.color: Colors.borderActivated
                    implicitHeight: content.implicitHeight + 36

                    GridLayout {
                        id: content
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 18
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 14

                        FieldLabel {
                            text: "Instruction"
                            visible: promptOnly
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 10
                        }
                        Basic.ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 104
                            visible: promptOnly
                            clip: true

                            background: Rectangle {
                                radius: 8
                                color: Colors.backgroundItemActivated
                                border.width: 1
                                border.color: instructionArea.activeFocus ? Colors.borderFocused : Colors.borderActivated
                            }

                            Basic.TextArea {
                                id: instructionArea
                                text: appController.settingsStore.instruction
                                wrapMode: TextArea.Wrap
                                color: Colors.textPrimary
                                selectedTextColor: Colors.textPrimary
                                selectionColor: Colors.secondryBack
                                padding: 12
                                font.pixelSize: Typography.t2
                                background: null
                                onActiveFocusChanged: if (!activeFocus) appController.settingsStore.instruction = text
                            }
                        }

                        FieldLabel {
                            text: "Screenshot context"
                            visible: promptOnly
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 10
                        }
                        Basic.ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 104
                            visible: promptOnly
                            clip: true

                            background: Rectangle {
                                radius: 8
                                color: Colors.backgroundItemActivated
                                border.width: 1
                                border.color: screenshotContextArea.activeFocus ? Colors.borderFocused : Colors.borderActivated
                            }

                            Basic.TextArea {
                                id: screenshotContextArea
                                text: appController.settingsStore.screenshotContext
                                wrapMode: TextArea.Wrap
                                color: Colors.textPrimary
                                selectedTextColor: Colors.textPrimary
                                selectionColor: Colors.secondryBack
                                padding: 12
                                font.pixelSize: Typography.t2
                                background: null
                                onActiveFocusChanged: if (!activeFocus) appController.settingsStore.screenshotContext = text
                            }
                        }

                        FieldLabel {
                            text: "Query"
                            visible: promptOnly
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 10
                        }
                        Basic.ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 156
                            visible: promptOnly
                            clip: true

                            background: Rectangle {
                                radius: 8
                                color: Colors.backgroundItemActivated
                                border.width: 1
                                border.color: queryArea.activeFocus ? Colors.borderFocused : Colors.borderActivated
                            }

                            Basic.TextArea {
                                id: queryArea
                                text: appController.settingsStore.query
                                wrapMode: TextArea.Wrap
                                color: Colors.textPrimary
                                selectedTextColor: Colors.textPrimary
                                selectionColor: Colors.secondryBack
                                padding: 12
                                font.pixelSize: Typography.t2
                                background: null
                                onActiveFocusChanged: if (!activeFocus) appController.settingsStore.query = text
                            }
                        }

                    FieldLabel { text: "Host"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 360
                        visible: !promptOnly
                        text: appController.settingsStore.host
                        onEditingFinished: appController.settingsStore.host = text
                    }

                    FieldLabel { text: "Model"; visible: !promptOnly }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        visible: !promptOnly
                        editable: false
                        model: ["huihui_ai/qwen3-vl-abliterated:8b-instruct", "gemma4:12b", "qwen3.6:latest"]
                        currentIndex: Math.max(0, model.indexOf(appController.settingsStore.model))
                        onActivated: appController.settingsStore.model = modelTextAt(currentIndex)
                    }

                    FieldLabel { text: "Trigger"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.triggerShortcut
                        onEditingFinished: appController.settingsStore.triggerShortcut = text
                    }

                    FieldLabel { text: "Collapse / expand"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.clearShortcut
                        onEditingFinished: appController.settingsStore.clearShortcut = text
                    }

                    FieldLabel { text: "Exit"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.exitShortcut
                        onEditingFinished: appController.settingsStore.exitShortcut = text
                    }

                    FieldLabel { text: "Screenshot max edge"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 140
                        visible: !promptOnly
                        text: appController.settingsStore.screenshotMaxEdge
                        validator: IntValidator { bottom: 64; top: 4096 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: appController.settingsStore.screenshotMaxEdge = parseInt(text)
                    }

                    FieldLabel { text: "Memory pairs"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 140
                        visible: !promptOnly
                        text: appController.settingsStore.memoryQaPairs
                        validator: IntValidator { bottom: 0; top: 20 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: appController.settingsStore.memoryQaPairs = parseInt(text)
                    }

                    FieldLabel { text: "Keep alive"; visible: !promptOnly }
                    RowLayout {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        spacing: 8

                        Controls.TextField {
                            Layout.preferredWidth: 100
                            text: appController.settingsStore.keepAliveMinutes
                            validator: IntValidator { bottom: 0; top: 1000000 }
                            inputMethodHints: Qt.ImhDigitsOnly
                            onEditingFinished: appController.settingsStore.keepAliveMinutes = text
                        }
                        Text {
                            text: "min"
                            color: Colors.textMuted
                            font.pixelSize: Typography.t3
                        }
                    }

                    FieldLabel { text: "Think"; visible: !promptOnly }
                    Controls.Switch {
                        visible: !promptOnly
                        checked: appController.settingsStore.think
                        onToggled: appController.settingsStore.think = checked
                    }

                    FieldLabel { text: "Context tokens"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.numCtx
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.numCtx = text
                    }

                    FieldLabel { text: "Max output tokens"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.numPredict
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.numPredict = text
                    }

                    FieldLabel { text: "Repeat last N"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.repeatLastN
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.repeatLastN = text
                    }

                    FieldLabel { text: "Repeat penalty"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.repeatPenalty
                        onEditingFinished: appController.settingsStore.repeatPenalty = text
                    }

                    FieldLabel { text: "Temperature"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.temperature
                        onEditingFinished: appController.settingsStore.temperature = text
                    }

                    FieldLabel { text: "Top P"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.topP
                        onEditingFinished: appController.settingsStore.topP = text
                    }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Button {
                        text: "Save"
                        isDefault: true
                        onClicked: appController.saveSettings()
                    }
                    Controls.Button {
                        text: "Defaults"
                        onClicked: appController.settingsStore.resetToDefaults()
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: appController.settingsStore.lastError
                        color: Colors.error
                        visible: text.length > 0
                        font.pixelSize: Typography.t3
                    }
                }
            }
        }
    }

    component OptionField: Controls.TextField {
        property bool integerOnly: false

        Layout.preferredWidth: 140
        validator: integerOnly ? optionIntValidator : optionDoubleValidator
        inputMethodHints: Qt.ImhFormattedNumbersOnly

        IntValidator {
            id: optionIntValidator
            bottom: 0
            top: 1000000
        }
        DoubleValidator {
            id: optionDoubleValidator
            bottom: 0
            top: 1000000
            decimals: 4
            notation: DoubleValidator.StandardNotation
        }
    }
}
