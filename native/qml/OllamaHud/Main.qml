import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import OllamaHud.UI
import OllamaHud.UI.Controls as Controls

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

    Loader {
        active: hotReloadEnabled
        sourceComponent: Component {
            Shortcut {
                sequence: "F5"
                onActivated: HotReload.reload()
            }
        }
    }

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
                    anchors.margins: 20
                    anchors.topMargin: 10
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: 10

                        text: "Ollama HUD"
                        color: Colors.textPrimary
                        font.family: FontSystem.getContentFontBold.name
                        font.pixelSize: 25
                        horizontalAlignment: Text.AlignLeft
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: 13
                        Layout.topMargin: 10

                        text: appController.hudRunning ? "Overlay running" : "Control panel"
                        color: appController.hudRunning ? Colors.success : Colors.textSecondary
                        font.family: FontSystem.getContentFontBold.name
                        font.pixelSize: Typography.t2
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                    }

                    Rectangle {
                        Layout.topMargin: 10
                        Layout.fillWidth: true
                        height: 1
                        color: Colors.lineBorderActivated
                    }

                    Item {
                        width: 1
                        height: 5
                    }

                    Repeater {
                        model: ["Runtime", "Prompt", "Settings", "Detector", "Log"]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 5
                            height: 42
                            radius: 8
                            color: nav.currentIndex === index
                                ? Colors.backgroundActivated
                                : hover.hovered
                                    ? Colors.backgroundHovered2
                                    : "transparent"
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

                            HoverHandler {
                                id: hover
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        id: themeToggle
                        Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                        Layout.leftMargin: 10
                        implicitWidth: 66
                        implicitHeight: 32
                        radius: height / 2
                        color: Colors.backgroundItemActivated
                        border.width: 1
                        border.color: Colors.borderActivated

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 9
                            anchors.verticalCenter: parent.verticalCenter
                            text: "☀"
                            color: Colors.lightMode ? Colors.warning : Colors.textMuted
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 9
                            anchors.verticalCenter: parent.verticalCenter
                            text: "☾"
                            color: Colors.lightMode ? Colors.textMuted : Colors.textPrimary
                            font.pixelSize: 17
                        }
                        Rectangle {
                            width: 26
                            height: 26
                            radius: width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            x: Colors.lightMode ? 3 : parent.width - width - 3
                            color: Colors.lightMode ? "#fff6d6" : Colors.backgroundFocused
                            border.width: 1
                            border.color: Colors.borderActivated
                            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Colors.mode = Colors.lightMode ? Colors.modeDark : Colors.modeLight
                        }
                        HoverHandler { id: themeToggleHover }
                        ToolTip.visible: themeToggleHover.hovered
                        ToolTip.text: Colors.lightMode ? "Switch to dark theme" : "Switch to light theme"
                        ToolTip.delay: 500
                    }

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
                                    foot: "Trigger " + appController.settingsStore.triggerShortcut + "  |  Sim " + appController.settingsStore.simulationTriggerShortcut + " / stop " + appController.settingsStore.simulationStopShortcut + "  |  Exit " + appController.settingsStore.exitShortcut
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
                                        title: "Input simulation"
                                        body: appController.simulationRunning ? "Running" : appController.simulationStatus
                                        foot: "Start " + appController.settingsStore.simulationTriggerShortcut + "  |  Stop " + appController.settingsStore.simulationStopShortcut
                                        stateColor: appController.simulationRunning ? Colors.warning : Colors.success
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

                    DetectorPage { }

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

                    FieldLabel { text: "Simulation trigger"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.simulationTriggerShortcut
                        onEditingFinished: appController.settingsStore.simulationTriggerShortcut = text
                    }

                    FieldLabel { text: "Simulation emergency stop"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.simulationStopShortcut
                        onEditingFinished: appController.settingsStore.simulationStopShortcut = text
                    }

                    FieldLabel { text: "Detector on / off"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.detectorToggleShortcut
                        onEditingFinished: appController.settingsStore.detectorToggleShortcut = text
                    }

                    FieldLabel { text: "Live detection on / off"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.liveDetectionToggleShortcut
                        onEditingFinished: appController.settingsStore.liveDetectionToggleShortcut = text
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

    component DetectorPage: ScrollView {
        id: detectorScroll
        clip: true
        contentWidth: availableWidth
        contentHeight: detectorContent.implicitHeight + 44
        property int guideTargetIndex: -1

        FolderDialog {
            id: guideFolderDialog
            property bool positiveFolder: true
            title: (positiveFolder ? "Choose positive guide folder" : "Choose negative guide folder") + (guideTargetIndex >= 0 ? " for image target" : "")
            onAccepted: {
                if (guideTargetIndex >= 0)
                    appController.detectorSettingsStore.setImageTargetGuideFolder(guideTargetIndex, positiveFolder, selectedFolder.toLocalFile())
                else if (positiveFolder)
                    appController.detectorSettingsStore.positiveGuideFolder = selectedFolder.toLocalFile()
                else
                    appController.detectorSettingsStore.negativeGuideFolder = selectedFolder.toLocalFile()
            }
        }

        FileDialog {
            id: detectorSettingsSaveDialog
            title: "Save detector settings"
            fileMode: FileDialog.SaveFile
            currentFolder: appController.detectorSettingsStore.settingsFolder
            defaultSuffix: "json"
            nameFilters: ["Detector settings (*.json)"]
            onAccepted: appController.detectorSettingsStore.saveToFile(selectedFile.toLocalFile())
        }

        FileDialog {
            id: detectorSettingsLoadDialog
            title: "Load detector settings"
            fileMode: FileDialog.OpenFile
            currentFolder: appController.detectorSettingsStore.settingsFolder
            nameFilters: ["Detector settings (*.json)"]
            onAccepted: appController.detectorSettingsStore.loadFromFile(selectedFile.toLocalFile())
        }

        Item {
            width: detectorScroll.availableWidth
            height: detectorContent.implicitHeight + 44

            ColumnLayout {
                id: detectorContent
                x: 22
                y: 22
                width: Math.max(520, parent.width - 44)
                spacing: 14

                RuntimeCard {
                    Layout.fillWidth: true
                    title: "OWLv2 detector"
                    body: appController.detectorStatus
                    foot: "Guide scores are raw OWLv2 logits; text scores are confidence values. Detector " + appController.settingsStore.detectorToggleShortcut + " | Live " + appController.settingsStore.liveDetectionToggleShortcut + "."
                    stateColor: appController.liveDetection ? Colors.success : (appController.detectorSettingsStore.enabled ? Colors.warning : Colors.textMuted)
                    expanded: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 8
                    color: Colors.backgroundActivated
                    border.width: 1
                    border.color: Colors.borderActivated
                    implicitHeight: detectorGrid.implicitHeight + 36

                    GridLayout {
                        id: detectorGrid
                        anchors.fill: parent
                        anchors.margins: 18
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 14

                        FieldLabel { text: "Enable ground detector" }
                        Controls.Switch {
                            checked: appController.detectorSettingsStore.enabled
                            onToggled: appController.detectorSettingsStore.enabled = checked
                        }
                        FieldLabel { text: "Model" }
                        Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.model; onEditingFinished: appController.detectorSettingsStore.model = text }
                        FieldLabel { text: "Device / dtype" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.ComboBox { Layout.preferredWidth: 130; model: ["auto", "cpu", "cuda"]; currentIndex: Math.max(0, model.indexOf(appController.detectorSettingsStore.device)); onActivated: appController.detectorSettingsStore.device = modelTextAt(currentIndex) }
                            Controls.ComboBox { Layout.preferredWidth: 130; model: ["auto", "float32", "float16", "bfloat16"]; currentIndex: Math.max(0, model.indexOf(appController.detectorSettingsStore.dtype)); onActivated: appController.detectorSettingsStore.dtype = modelTextAt(currentIndex) }
                        }
                        FieldLabel { text: "Text threshold" }
                        OptionField { text: appController.detectorSettingsStore.textThreshold; onEditingFinished: appController.detectorSettingsStore.textThreshold = parseFloat(text) }
                        FieldLabel { text: "Guide threshold (raw logit)" }
                        OptionField { text: appController.detectorSettingsStore.guideThreshold; onEditingFinished: appController.detectorSettingsStore.guideThreshold = parseFloat(text) }
                        FieldLabel { text: "Positive guide folder" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.positiveGuideFolder; onEditingFinished: appController.detectorSettingsStore.positiveGuideFolder = text }
                            Controls.Button { text: "Browse"; onClicked: { detectorScroll.guideTargetIndex = -1; guideFolderDialog.positiveFolder = true; guideFolderDialog.open() } }
                        }
                        FieldLabel { text: "Negative guide folder" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.negativeGuideFolder; onEditingFinished: appController.detectorSettingsStore.negativeGuideFolder = text }
                            Controls.Button { text: "Browse"; onClicked: { detectorScroll.guideTargetIndex = -1; guideFolderDialog.positiveFolder = false; guideFolderDialog.open() } }
                        }
                        FieldLabel { text: "Live detection rate (Hz)" }
                        OptionField { text: appController.detectorSettingsStore.liveRate; onEditingFinished: appController.detectorSettingsStore.liveRate = parseFloat(text) }
                        FieldLabel { text: "Targets"; Layout.alignment: Qt.AlignTop; Layout.topMargin: 10 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: "Text prompts may have their own threshold, for example entrance:0.3, portal:0.6; prompts without :threshold use the global text threshold. Image targets use positive and optional negative image sets; default folders are searched recursively at positive guide folder/Target name/ and negative guide folder/Target name/."
                                wrapMode: Text.WordWrap
                                color: Colors.textMuted
                                font.pixelSize: Typography.t3
                            }
                            Repeater {
                                model: appController.detectorSettingsStore.targets
                                delegate: Rectangle {
                                    required property int index
                                    required property var modelData
                                    Layout.fillWidth: true
                                    radius: 8
                                    color: Colors.backgroundItemActivated
                                    border.width: 1
                                    border.color: Colors.borderActivated
                                    implicitHeight: targetFields.implicitHeight + 22
                                    ColumnLayout {
                                        id: targetFields
                                        anchors.fill: parent
                                        anchors.margins: 11
                                        spacing: 7
                                        Text { text: modelData.type === "image" ? "Image target" : "Text target"; color: Colors.textSecondary; font.pixelSize: Typography.t3; font.bold: true }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            FieldLabel { text: modelData.type === "image" && useDefaultGuideDirectories.checked ? "Search for target" : "Target name" }
                                            Controls.TextField {
                                                id: textNameField
                                                Layout.fillWidth: true
                                                visible: modelData.type !== "image"
                                                text: modelData.name
                                                onEditingFinished: appController.detectorSettingsStore.updateTextTarget(index, text, textPromptsField.text)
                                            }
                                            Controls.TextField {
                                                id: imageNameField
                                                Layout.fillWidth: true
                                                visible: modelData.type === "image" && !useDefaultGuideDirectories.checked
                                                text: modelData.name
                                                onEditingFinished: appController.detectorSettingsStore.updateImageTarget(index, text, useDefaultGuideDirectories.checked, positiveGuideDir.text, negativeGuideDir.text)
                                            }
                                            Controls.ComboBox {
                                                id: imageTargetSearch
                                                Layout.fillWidth: true
                                                visible: modelData.type === "image" && useDefaultGuideDirectories.checked
                                                model: appController.detectorSettingsStore.defaultGuideTargetNames
                                                currentIndex: appController.detectorSettingsStore.defaultGuideTargetIndex(modelData.name)
                                                onActivated: appController.detectorSettingsStore.updateImageTarget(index, modelTextAt(currentIndex), true, positiveGuideDir.text, negativeGuideDir.text)
                                            }
                                            Controls.Button { text: "Remove"; onClicked: appController.detectorSettingsStore.removeTarget(index) }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type !== "image"
                                            FieldLabel { text: "Prompts (optional :threshold)" }
                                            Controls.TextField {
                                                id: textPromptsField
                                                Layout.fillWidth: true
                                                text: modelData.prompts
                                                placeholderText: "entrance:0.3, portal:0.6, cave door:0.1"
                                                onEditingFinished: appController.detectorSettingsStore.updateTextTarget(index, textNameField.text, text)
                                            }
                                        }
                                        Controls.CheckBox {
                                            id: useDefaultGuideDirectories
                                            visible: modelData.type === "image"
                                            text: "Use default guide image-set directories"
                                            checked: modelData.useDefaultGuideDirectories
                                            onToggled: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, checked, positiveGuideDir.text, negativeGuideDir.text)
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && useDefaultGuideDirectories.checked
                                            text: "Searches recursively in:\nPositive: " + appController.detectorSettingsStore.positiveGuideFolder + "/" + modelData.name + "/\nNegative: " + appController.detectorSettingsStore.negativeGuideFolder + "/" + modelData.name + "/"
                                            wrapMode: Text.WordWrap
                                            color: Colors.textMuted
                                            font.pixelSize: Typography.t3
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && !useDefaultGuideDirectories.checked
                                            FieldLabel { text: "Positive images folder" }
                                            Controls.TextField {
                                                id: positiveGuideDir
                                                Layout.fillWidth: true
                                                text: modelData.positiveGuideDir
                                                placeholderText: "Required; subfolders are included"
                                                onEditingFinished: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, false, text, negativeGuideDir.text)
                                            }
                                            Controls.Button { text: "Choose"; onClicked: { detectorScroll.guideTargetIndex = index; guideFolderDialog.positiveFolder = true; guideFolderDialog.open() } }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && !useDefaultGuideDirectories.checked
                                            FieldLabel { text: "Negative images folder" }
                                            Controls.TextField {
                                                id: negativeGuideDir
                                                Layout.fillWidth: true
                                                text: modelData.negativeGuideDir
                                                placeholderText: "Optional; subfolders are included"
                                                onEditingFinished: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, false, positiveGuideDir.text, text)
                                            }
                                            Controls.Button { text: "Choose"; onClicked: { detectorScroll.guideTargetIndex = index; guideFolderDialog.positiveFolder = false; guideFolderDialog.open() } }
                                        }
                                    }
                                }
                            }
                            RowLayout {
                                Controls.Button { text: "+ Add text target"; onClicked: appController.detectorSettingsStore.addTextTarget() }
                                Controls.Button { text: "+ Add image target"; onClicked: appController.detectorSettingsStore.addImageTarget() }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Button { text: "Save detector settings"; isDefault: true; onClicked: appController.detectorSettingsStore.save() }
                    Controls.Button { text: "Save As..."; onClicked: detectorSettingsSaveDialog.open() }
                    Controls.Button { text: "Load..."; onClicked: detectorSettingsLoadDialog.open() }
                    Controls.Button { text: appController.liveDetection ? "Stop Live Detection" : "Start Live Detection"; style: appController.liveDetection ? "danger" : "success"; onClicked: appController.liveDetection ? appController.stopLiveDetection() : appController.startLiveDetection() }
                    Controls.Button { text: "Detector defaults"; onClicked: appController.detectorSettingsStore.resetToDefaults() }
                    Item { Layout.fillWidth: true }
                    Text { text: appController.detectorSettingsStore.lastError; color: Colors.error; visible: text.length > 0; font.pixelSize: Typography.t3 }
                }
            }
        }
    }
}
