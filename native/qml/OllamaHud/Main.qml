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

                        text: appController.hudRunning ? "오버레이 실행 중" : "제어판"
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
                        model: ["실행 상태", "프롬프트", "설정", "감지기", "로그"]
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
                        ToolTip.text: Colors.lightMode ? "어두운 테마로 전환" : "밝은 테마로 전환"
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
                            text: appController.hudRunning ? "HUD 중지" : "HUD 시작"
                            isDefault: true
                            style: appController.hudRunning ? "danger" : "success"
                            onClicked: appController.hudRunning ? appController.stopHud() : appController.startHud()
                        }
                        Controls.Button {
                            text: appController.active ? "처리 중" : "지금 질문"
                            enabled: !appController.active
                            onClicked: appController.captureOnce()
                        }
                        Controls.Button {
                            text: "연결 테스트"
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
                                    title: "실행 상태"
                                    body: appController.visualAnswer.length > 0 ? appController.visualAnswer : "다음 실행 트리거를 기다리고 있습니다."
                                    foot: "실행 " + appController.settingsStore.triggerShortcut + "  |  시뮬레이션 " + appController.settingsStore.simulationTriggerShortcut + " / 중지 " + appController.settingsStore.simulationStopShortcut + "  |  HUD 중지 " + appController.settingsStore.exitShortcut
                                    stateColor: appController.error ? Colors.error : (appController.active ? Colors.warning : Colors.success)
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "모델"
                                        body: appController.settingsStore.model
                                        foot: appController.settingsStore.host
                                    }
                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "입력 시뮬레이션"
                                        body: appController.simulationRunning ? "실행 중" : appController.simulationStatus
                                        foot: "시작 " + appController.settingsStore.simulationTriggerShortcut + "  |  중지 " + appController.settingsStore.simulationStopShortcut
                                        stateColor: appController.simulationRunning ? Colors.warning : Colors.success
                                    }
                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "캡처"
                                        body: appController.captureId.length > 0 ? appController.captureId : "아직 캡처가 없습니다"
                                        foot: "최대 변 길이 " + appController.settingsStore.screenshotMaxEdge + " px"
                                    }
                                    RuntimeCard {
                                        Layout.fillWidth: true
                                        implicitHeight: 126
                                        title: "메모리"
                                        body: appController.settingsStore.memoryQaPairs + "개 질문/답변 쌍"
                                        foot: appController.settingsStore.think ? "사고 모드 사용" : "사고 모드 사용 안 함"
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
                                    title: "대화 로그"
                                    body: appController.sessionLogEntries.length === 1
                                        ? "이 세션에서 요청 1개를 캡처했습니다."
                                        : "이 세션에서 요청 " + appController.sessionLogEntries.length + "개를 캡처했습니다."
                                    foot: "영구 기록은 logs/chat.log에 텍스트만 저장됩니다. 세션 스크린샷은 Ollama HUD를 종료하면 삭제됩니다."
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Controls.Button {
                                        text: "로그 폴더 열기"
                                        isDefault: true
                                        onClicked: appController.openLogFolder()
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: appController.sessionLogEntries.length === 0
                                    radius: 8
                                    color: Colors.backgroundActivated
                                    border.width: 1
                                    border.color: Colors.borderActivated
                                    implicitHeight: 96

                                    Text {
                                        anchors.centerIn: parent
                                        width: parent.width - 36
                                        text: "이 세션에는 아직 질문/답변 기록이 없습니다. 새 캡처는 임시 스크린샷 미리보기와 함께 여기에 표시됩니다."
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WordWrap
                                        color: Colors.textMuted
                                        font.pixelSize: Typography.t2
                                    }
                                }

                                Repeater {
                                    model: appController.sessionLogEntries

                                    delegate: Rectangle {
                                        id: sessionEntryCard
                                        required property int index
                                        required property var modelData
                                        property bool expanded: false

                                        Layout.fillWidth: true
                                        radius: 8
                                        color: Colors.backgroundActivated
                                        border.width: 1
                                        border.color: modelData.isError ? Colors.error : Colors.borderActivated
                                        implicitHeight: sessionEntryContent.implicitHeight + 32

                                        ColumnLayout {
                                            id: sessionEntryContent
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 10

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.timestamp + "  |  " + (modelData.captureId.length > 0 ? modelData.captureId : "캡처 없음")
                                                    color: Colors.textSecondary
                                                    font.pixelSize: Typography.t3
                                                    font.family: FontSystem.getContentFontSemiBold.name
                                                }
                                                Text {
                                                    text: modelData.isError
                                                        ? "오류"
                                                        : "완료: " + (modelData.doneReason.length > 0 ? modelData.doneReason : "알 수 없음")
                                                    color: modelData.isError ? Colors.error : Colors.success
                                                    font.pixelSize: Typography.t3
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignTop
                                                spacing: 14

                                                Rectangle {
                                                    Layout.preferredWidth: 280
                                                    Layout.preferredHeight: 158
                                                    Layout.alignment: Qt.AlignTop
                                                    radius: 7
                                                    color: Colors.backgroundItemActivated
                                                    border.width: 1
                                                    border.color: Colors.borderActivated
                                                    clip: true

                                                    Image {
                                                        anchors.fill: parent
                                                        source: modelData.screenshotUrl
                                                        fillMode: Image.PreserveAspectFit
                                                        asynchronous: true
                                                        cache: false
                                                        visible: modelData.screenshotUrl.length > 0
                                                    }

                                                    Text {
                                                        anchors.centerIn: parent
                                                        width: parent.width - 24
                                                        visible: modelData.screenshotUrl.length === 0
                                                        text: "스크린샷 없음"
                                                        horizontalAlignment: Text.AlignHCenter
                                                        color: Colors.textMuted
                                                        font.pixelSize: Typography.t3
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        enabled: modelData.screenshotPath.length > 0
                                                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                        onClicked: appController.openSessionScreenshot(modelData.screenshotPath)
                                                    }
                                                }

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    Layout.alignment: Qt.AlignTop
                                                    spacing: 7

                                                    FieldLabel { text: "질문" }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.question
                                                        wrapMode: Text.WordWrap
                                                        maximumLineCount: sessionEntryCard.expanded ? 1000 : 4
                                                        elide: Text.ElideRight
                                                        color: Colors.textPrimary
                                                        font.pixelSize: Typography.t2
                                                    }

                                                    FieldLabel { text: modelData.isError ? "오류" : "답변" }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.isError ? modelData.error : modelData.answer
                                                        wrapMode: Text.WordWrap
                                                        maximumLineCount: sessionEntryCard.expanded ? 1000 : 7
                                                        elide: Text.ElideRight
                                                        color: modelData.isError ? Colors.error : Colors.textPrimary
                                                        font.pixelSize: Typography.t2
                                                    }

                                                    Text {
                                                        Layout.fillWidth: true
                                                        visible: !modelData.isError
                                                        text: (modelData.generatedTokens >= 0 ? "생성 토큰 " + modelData.generatedTokens + "개" : "토큰 수 없음")
                                                            + (modelData.totalDurationSeconds >= 0 ? "  |  " + Number(modelData.totalDurationSeconds).toFixed(2) + " s" : "")
                                                        color: Colors.textMuted
                                                        font.pixelSize: Typography.t3
                                                    }
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Controls.Button {
                                                    text: sessionEntryCard.expanded ? "접기" : "전체 질문/답변 보기"
                                                    sizeType: "small"
                                                    onClicked: sessionEntryCard.expanded = !sessionEntryCard.expanded
                                                }
                                                Controls.Button {
                                                    text: "스크린샷 열기"
                                                    sizeType: "small"
                                                    enabled: modelData.screenshotPath.length > 0
                                                    onClicked: appController.openSessionScreenshot(modelData.screenshotPath)
                                                }
                                                Item { Layout.fillWidth: true }
                                            }
                                        }
                                    }
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
        property string persistenceMessage: ""
        property bool persistenceFailed: false
        clip: true
        contentWidth: availableWidth
        contentHeight: settingsContent.implicitHeight + 44

        function showPersistenceResult(success, successMessage) {
            persistenceFailed = !success
            persistenceMessage = success
                ? successMessage
                : appController.settingsStore.lastError
        }

        function afterPendingEditCommitted(action) {
            settingsScroll.forceActiveFocus()
            Qt.callLater(action)
        }

        function saveDefaultSettings() {
            afterPendingEditCommitted(function() {
                const success = appController.saveSettings()
                showPersistenceResult(success, promptOnly ? "프롬프트 설정을 저장했습니다." : "설정을 저장했습니다.")
            })
        }

        Connections {
            target: appController.settingsStore
            function onSettingsChanged() {
                settingsScroll.persistenceMessage = ""
            }
        }

        FileDialog {
            id: promptSettingsSaveDialog
            title: "프롬프트 설정 저장"
            fileMode: FileDialog.SaveFile
            currentFolder: appController.settingsStore.settingsFolder
            defaultSuffix: "yaml"
            nameFilters: ["프롬프트 설정 (*.yaml *.yml)"]
            onAccepted: {
                const selectedPath = appController.settingsStore.localFilePath(selectedFile)
                const success = appController.settingsStore.savePromptToFile(selectedPath)
                settingsScroll.showPersistenceResult(success, "프롬프트 설정 스냅샷을 저장했습니다.")
            }
        }

        FileDialog {
            id: promptSettingsLoadDialog
            title: "프롬프트 설정 불러오기"
            fileMode: FileDialog.OpenFile
            currentFolder: appController.settingsStore.settingsFolder
            nameFilters: ["프롬프트 설정 (*.yaml *.yml)"]
            onAccepted: {
                const selectedPath = appController.settingsStore.localFilePath(selectedFile)
                const success = appController.settingsStore.loadPromptFromFile(selectedPath)
                settingsScroll.showPersistenceResult(success, "프롬프트 설정을 불러왔습니다.")
            }
        }

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
                            text: "지시문"
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
                            text: "스크린샷 컨텍스트"
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
                            text: "질문"
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

                    FieldLabel { text: "호스트"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 360
                        visible: !promptOnly
                        text: appController.settingsStore.host
                        onEditingFinished: appController.settingsStore.host = text
                    }

                    FieldLabel { text: "모델"; visible: !promptOnly }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        visible: !promptOnly
                        editable: false
                        model: ["huihui_ai/qwen3-vl-abliterated:8b-instruct", "gemma4:12b", "qwen3.6:latest"]
                        currentIndex: Math.max(0, model.indexOf(appController.settingsStore.model))
                        onActivated: appController.settingsStore.model = modelTextAt(currentIndex)
                    }

                    FieldLabel { text: "실행 단축키"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.triggerShortcut
                        onEditingFinished: appController.settingsStore.triggerShortcut = text
                    }

                    FieldLabel { text: "접기 / 펼치기"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.clearShortcut
                        onEditingFinished: appController.settingsStore.clearShortcut = text
                    }

                    FieldLabel { text: "HUD 중지"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.exitShortcut
                        onEditingFinished: appController.settingsStore.exitShortcut = text
                    }

                    FieldLabel { text: "시뮬레이션 실행"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.simulationTriggerShortcut
                        onEditingFinished: appController.settingsStore.simulationTriggerShortcut = text
                    }

                    FieldLabel { text: "시뮬레이션 긴급 중지"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.simulationStopShortcut
                        onEditingFinished: appController.settingsStore.simulationStopShortcut = text
                    }

                    FieldLabel { text: "입력 장치"; visible: !promptOnly }
                    Controls.ComboBox {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        model: ["ricochet", "sendinput"]
                        currentIndex: Math.max(0, model.indexOf(appController.settingsStore.simulationInputBackend))
                        onActivated: appController.settingsStore.simulationInputBackend = modelTextAt(currentIndex)
                    }

                    FieldLabel { text: "Ricochet COM 포트"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.simulationRicochetPort
                        placeholderText: "COM3"
                        onEditingFinished: appController.settingsStore.simulationRicochetPort = text
                    }

                    FieldLabel { text: "감지기 켜기 / 끄기"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.detectorToggleShortcut
                        onEditingFinished: appController.settingsStore.detectorToggleShortcut = text
                    }

                    FieldLabel { text: "실시간 감지 켜기 / 끄기"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 220
                        visible: !promptOnly
                        text: appController.settingsStore.liveDetectionToggleShortcut
                        onEditingFinished: appController.settingsStore.liveDetectionToggleShortcut = text
                    }

                    FieldLabel { text: "스크린샷 최대 변 길이"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 140
                        visible: !promptOnly
                        text: appController.settingsStore.screenshotMaxEdge
                        validator: IntValidator { bottom: 64; top: 4096 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: appController.settingsStore.screenshotMaxEdge = parseInt(text)
                    }

                    FieldLabel { text: "메모리 질문/답변 쌍"; visible: !promptOnly }
                    Controls.TextField {
                        Layout.preferredWidth: 140
                        visible: !promptOnly
                        text: appController.settingsStore.memoryQaPairs
                        validator: IntValidator { bottom: 0; top: 20 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: appController.settingsStore.memoryQaPairs = parseInt(text)
                    }

                    FieldLabel { text: "유지 시간"; visible: !promptOnly }
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
                            text: "분"
                            color: Colors.textMuted
                            font.pixelSize: Typography.t3
                        }
                    }

                    FieldLabel { text: "사고 모드"; visible: !promptOnly }
                    Controls.Switch {
                        visible: !promptOnly
                        checked: appController.settingsStore.think
                        onToggled: appController.settingsStore.think = checked
                    }

                    FieldLabel { text: "컨텍스트 토큰"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.numCtx
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.numCtx = text
                    }

                    FieldLabel { text: "최대 출력 토큰"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.numPredict
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.numPredict = text
                    }

                    FieldLabel { text: "최근 N개 반복"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.repeatLastN
                        integerOnly: true
                        onEditingFinished: appController.settingsStore.repeatLastN = text
                    }

                    FieldLabel { text: "반복 패널티"; visible: !promptOnly }
                    OptionField {
                        visible: !promptOnly
                        text: appController.settingsStore.repeatPenalty
                        onEditingFinished: appController.settingsStore.repeatPenalty = text
                    }

                    FieldLabel { text: "온도"; visible: !promptOnly }
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
                        text: "저장"
                        isDefault: true
                        onClicked: settingsScroll.saveDefaultSettings()
                    }
                    Controls.Button {
                        text: "다른 이름으로 저장..."
                        visible: settingsScroll.promptOnly
                        onClicked: settingsScroll.afterPendingEditCommitted(function() {
                            promptSettingsSaveDialog.open()
                        })
                    }
                    Controls.Button {
                        text: "불러오기..."
                        visible: settingsScroll.promptOnly
                        onClicked: promptSettingsLoadDialog.open()
                    }
                    Controls.Button {
                        text: "기본값"
                        onClicked: settingsScroll.afterPendingEditCommitted(function() {
                            const success = appController.settingsStore.resetToDefaults()
                            settingsScroll.showPersistenceResult(success, "기본값을 복원했습니다.")
                        })
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    text: settingsScroll.persistenceMessage
                    color: settingsScroll.persistenceFailed ? Colors.error : Colors.success
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: Typography.t3
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
        property string persistenceMessage: ""
        property bool persistenceFailed: false

        function showPersistenceResult(success, successMessage) {
            persistenceFailed = !success
            persistenceMessage = success
                ? successMessage
                : appController.detectorSettingsStore.lastError
        }

        function afterPendingEditCommitted(action) {
            // Detector editors update the C++ store from onEditingFinished. Move
            // focus first, then wait one event turn so the requested action sees
            // the value that is still being edited when its button is clicked.
            detectorScroll.forceActiveFocus()
            Qt.callLater(action)
        }

        function saveDefaultSettings() {
            afterPendingEditCommitted(function() {
                const success = appController.detectorSettingsStore.save()
                showPersistenceResult(success, "감지기 설정을 저장했습니다.")
            })
        }

        Connections {
            target: appController.detectorSettingsStore
            function onChanged() {
                detectorScroll.persistenceMessage = ""
            }
        }

        FolderDialog {
            id: guideFolderDialog
            property bool positiveFolder: true
            title: (positiveFolder ? "양성 가이드 폴더 선택" : "음성 가이드 폴더 선택") + (guideTargetIndex >= 0 ? " (이미지 대상)" : "")
            onAccepted: {
                const selectedPath = appController.detectorSettingsStore.localFilePath(selectedFolder)
                if (guideTargetIndex >= 0)
                    appController.detectorSettingsStore.setImageTargetGuideFolder(guideTargetIndex, positiveFolder, selectedPath)
                else if (positiveFolder)
                    appController.detectorSettingsStore.positiveGuideFolder = selectedPath
                else
                    appController.detectorSettingsStore.negativeGuideFolder = selectedPath
            }
        }

        FileDialog {
            id: detectorSettingsSaveDialog
            title: "감지기 설정 저장"
            fileMode: FileDialog.SaveFile
            currentFolder: appController.detectorSettingsStore.settingsFolder
            defaultSuffix: "json"
            nameFilters: ["감지기 설정 (*.json)"]
            onAccepted: {
                const selectedPath = appController.detectorSettingsStore.localFilePath(selectedFile)
                const success = appController.detectorSettingsStore.saveToFile(selectedPath)
                detectorScroll.showPersistenceResult(success, "감지기 설정 스냅샷을 저장했습니다.")
            }
        }

        FileDialog {
            id: detectorSettingsLoadDialog
            title: "감지기 설정 불러오기"
            fileMode: FileDialog.OpenFile
            currentFolder: appController.detectorSettingsStore.settingsFolder
            nameFilters: ["감지기 설정 (*.json)"]
            onAccepted: {
                const selectedPath = appController.detectorSettingsStore.localFilePath(selectedFile)
                const success = appController.detectorSettingsStore.loadFromFile(selectedPath)
                detectorScroll.showPersistenceResult(success, "감지기 설정을 불러왔습니다.")
            }
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
                    title: "OWLv2 감지기"
                    body: appController.detectorStatus
                    foot: "가이드 점수는 원시 OWLv2 로짓이고, 텍스트 점수는 신뢰도 값입니다. 감지기 " + appController.settingsStore.detectorToggleShortcut + " | 실시간 " + appController.settingsStore.liveDetectionToggleShortcut + "."
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

                        FieldLabel { text: "화면 감지기 사용" }
                        Controls.Switch {
                            checked: appController.detectorSettingsStore.enabled
                            onToggled: appController.detectorSettingsStore.enabled = checked
                        }
                        FieldLabel { text: "모델" }
                        Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.model; onEditingFinished: appController.detectorSettingsStore.model = text }
                        FieldLabel { text: "장치 / 데이터 형식" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.ComboBox { Layout.preferredWidth: 130; model: ["auto", "cpu", "cuda"]; currentIndex: Math.max(0, model.indexOf(appController.detectorSettingsStore.device)); onActivated: appController.detectorSettingsStore.device = modelTextAt(currentIndex) }
                            Controls.ComboBox { Layout.preferredWidth: 130; model: ["auto", "float32", "float16", "bfloat16"]; currentIndex: Math.max(0, model.indexOf(appController.detectorSettingsStore.dtype)); onActivated: appController.detectorSettingsStore.dtype = modelTextAt(currentIndex) }
                        }
                        FieldLabel { text: "텍스트 임계값" }
                        OptionField { text: appController.detectorSettingsStore.textThreshold; onEditingFinished: appController.detectorSettingsStore.textThreshold = parseFloat(text) }
                        FieldLabel { text: "가이드 임계값 (원시 로짓)" }
                        OptionField { text: appController.detectorSettingsStore.guideThreshold; onEditingFinished: appController.detectorSettingsStore.guideThreshold = parseFloat(text) }
                        FieldLabel { text: "양성 가이드 폴더" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.positiveGuideFolder; onEditingFinished: appController.detectorSettingsStore.positiveGuideFolder = text }
                            Controls.Button { text: "찾아보기"; onClicked: { detectorScroll.guideTargetIndex = -1; guideFolderDialog.positiveFolder = true; guideFolderDialog.open() } }
                        }
                        FieldLabel { text: "음성 가이드 폴더" }
                        RowLayout {
                            Layout.fillWidth: true
                            Controls.TextField { Layout.fillWidth: true; text: appController.detectorSettingsStore.negativeGuideFolder; onEditingFinished: appController.detectorSettingsStore.negativeGuideFolder = text }
                            Controls.Button { text: "찾아보기"; onClicked: { detectorScroll.guideTargetIndex = -1; guideFolderDialog.positiveFolder = false; guideFolderDialog.open() } }
                        }
                        FieldLabel { text: "실시간 감지 빈도 (Hz)" }
                        OptionField { text: appController.detectorSettingsStore.liveRate; onEditingFinished: appController.detectorSettingsStore.liveRate = parseFloat(text) }
                        FieldLabel { text: "대상"; Layout.alignment: Qt.AlignTop; Layout.topMargin: 10 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: "텍스트 프롬프트에는 자체 임계값을 지정할 수 있습니다. 예: entrance:0.3, portal:0.6. :임계값이 없는 프롬프트에는 전역 텍스트 임계값이 적용됩니다. 이미지 대상은 양성 이미지 세트와 선택적 음성 이미지 세트를 사용하며, 기본 폴더에서는 양성 가이드 폴더/대상 이름/ 및 음성 가이드 폴더/대상 이름/을 재귀적으로 검색합니다."
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
                                        Text { text: modelData.type === "image" ? "이미지 대상" : "텍스트 대상"; color: Colors.textSecondary; font.pixelSize: Typography.t3; font.bold: true }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            FieldLabel { text: modelData.type === "image" && useDefaultGuideDirectories.checked ? "대상 검색" : "대상 이름" }
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
                                            Controls.Button { text: "삭제"; onClicked: appController.detectorSettingsStore.removeTarget(index) }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type !== "image"
                                            FieldLabel { text: "프롬프트 (:임계값 선택)" }
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
                                            text: "기본 가이드 이미지 세트 폴더 사용"
                                            checked: modelData.useDefaultGuideDirectories
                                            onToggled: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, checked, positiveGuideDir.text, negativeGuideDir.text)
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && useDefaultGuideDirectories.checked
                                            text: "재귀 검색 위치:\n양성: " + appController.detectorSettingsStore.positiveGuideFolder + "/" + modelData.name + "/\n음성: " + appController.detectorSettingsStore.negativeGuideFolder + "/" + modelData.name + "/"
                                            wrapMode: Text.WordWrap
                                            color: Colors.textMuted
                                            font.pixelSize: Typography.t3
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && !useDefaultGuideDirectories.checked
                                            FieldLabel { text: "양성 이미지 폴더" }
                                            Controls.TextField {
                                                id: positiveGuideDir
                                                Layout.fillWidth: true
                                                text: modelData.positiveGuideDir
                                                placeholderText: "필수: 하위 폴더 포함"
                                                onEditingFinished: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, false, text, negativeGuideDir.text)
                                            }
                                            Controls.Button { text: "선택"; onClicked: { detectorScroll.guideTargetIndex = index; guideFolderDialog.positiveFolder = true; guideFolderDialog.open() } }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: modelData.type === "image" && !useDefaultGuideDirectories.checked
                                            FieldLabel { text: "음성 이미지 폴더" }
                                            Controls.TextField {
                                                id: negativeGuideDir
                                                Layout.fillWidth: true
                                                text: modelData.negativeGuideDir
                                                placeholderText: "선택 사항: 하위 폴더 포함"
                                                onEditingFinished: appController.detectorSettingsStore.updateImageTarget(index, modelData.name, false, positiveGuideDir.text, text)
                                            }
                                            Controls.Button { text: "선택"; onClicked: { detectorScroll.guideTargetIndex = index; guideFolderDialog.positiveFolder = false; guideFolderDialog.open() } }
                                        }
                                    }
                                }
                            }
                            RowLayout {
                                Controls.Button { text: "+ 텍스트 대상 추가"; onClicked: appController.detectorSettingsStore.addTextTarget() }
                                Controls.Button { text: "+ 이미지 대상 추가"; onClicked: appController.detectorSettingsStore.addImageTarget() }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Button { text: "감지기 설정 저장"; isDefault: true; onClicked: detectorScroll.saveDefaultSettings() }
                    Controls.Button {
                        text: "다른 이름으로 저장..."
                        onClicked: detectorScroll.afterPendingEditCommitted(function() {
                            detectorSettingsSaveDialog.open()
                        })
                    }
                    Controls.Button { text: "불러오기..."; onClicked: detectorSettingsLoadDialog.open() }
                    Controls.Button {
                        text: appController.liveDetection ? "실시간 감지 중지" : "실시간 감지 시작"
                        style: appController.liveDetection ? "danger" : "success"
                        onClicked: {
                            if (appController.liveDetection) {
                                appController.stopLiveDetection()
                            } else {
                                detectorScroll.afterPendingEditCommitted(function() {
                                    appController.startLiveDetection()
                                })
                            }
                        }
                    }
                    Controls.Button {
                        text: "감지기 기본값"
                        onClicked: detectorScroll.afterPendingEditCommitted(function() {
                            const success = appController.detectorSettingsStore.resetToDefaults()
                            detectorScroll.showPersistenceResult(success, "감지기 기본값을 복원하고 저장했습니다.")
                        })
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    text: detectorScroll.persistenceMessage
                    color: detectorScroll.persistenceFailed ? Colors.error : Colors.success
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: Typography.t3
                }
            }
        }
    }
}
