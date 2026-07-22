#include "Shortcut.h"

#include <QHash>
#include <QStringList>
#include <stdexcept>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int VK_ESCAPE_KEY = 0x1B;
constexpr int VK_CONTROL_KEY = 0x11;
constexpr int VK_SHIFT_KEY = 0x10;
constexpr int VK_ALT_KEY = 0x12;
constexpr int VK_OEM_3_KEY = 0xC0;

bool keyDown(int keyCode)
{
#ifdef Q_OS_WIN
    return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
#else
    Q_UNUSED(keyCode);
    return false;
#endif
}

bool modifierDown(const QString &name)
{
    if (name == "Ctrl") {
        return keyDown(VK_CONTROL_KEY) || keyDown(0xA2) || keyDown(0xA3);
    }
    if (name == "Shift") {
        return keyDown(VK_SHIFT_KEY) || keyDown(0xA0) || keyDown(0xA1);
    }
    if (name == "Alt") {
        return keyDown(VK_ALT_KEY) || keyDown(0xA4) || keyDown(0xA5);
    }
    return false;
}

QPair<int, QString> parseKey(const QString &lowered, const QString &original)
{
    static const QHash<QString, QPair<int, QString>> aliases = {
        {"`", {VK_OEM_3_KEY, "`"}},
        {"~", {VK_OEM_3_KEY, "`"}},
        {"backtick", {VK_OEM_3_KEY, "`"}},
        {"grave", {VK_OEM_3_KEY, "`"}},
        {"esc", {VK_ESCAPE_KEY, "Esc"}},
        {"escape", {VK_ESCAPE_KEY, "Esc"}},
        {"space", {0x20, "Space"}},
        {"enter", {0x0D, "Enter"}},
        {"return", {0x0D, "Enter"}},
        {"tab", {0x09, "Tab"}},
        {"backspace", {0x08, "Backspace"}},
        {"delete", {0x2E, "Delete"}},
        {"del", {0x2E, "Delete"}},
        {"insert", {0x2D, "Insert"}},
        {"home", {0x24, "Home"}},
        {"end", {0x23, "End"}},
        {"pageup", {0x21, "PageUp"}},
        {"pagedown", {0x22, "PageDown"}},
        {"up", {0x26, "Up"}},
        {"down", {0x28, "Down"}},
        {"left", {0x25, "Left"}},
        {"right", {0x27, "Right"}},
    };
    if (aliases.contains(lowered)) {
        return aliases.value(lowered);
    }
    if (original.size() == 1 && original.at(0).isLetterOrNumber()) {
        const QString upper = original.toUpper();
        return {upper.at(0).unicode(), upper};
    }
    if (lowered.startsWith('f')) {
        bool ok = false;
        const int number = lowered.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 24) {
            return {0x6F + number, QStringLiteral("F%1").arg(number)};
        }
    }
    throw std::invalid_argument(QStringLiteral("unsupported shortcut key: %1").arg(original).toStdString());
}
}

QString KeyboardShortcut::display() const
{
    QStringList parts;
    for (const QString &modifier : {QStringLiteral("Ctrl"), QStringLiteral("Shift"), QStringLiteral("Alt")}) {
        if (modifiers.contains(modifier)) {
            parts.append(modifier);
        }
    }
    parts.append(keyName);
    return parts.join('+');
}

bool KeyboardShortcut::isPressed() const
{
    for (const QString &modifier : modifiers) {
        if (!modifierDown(modifier)) {
            return false;
        }
    }
    return keyDown(keyCode);
}

KeyboardShortcut parseShortcut(const QString &text)
{
    const QStringList rawParts = text.split('+', Qt::SkipEmptyParts);
    if (rawParts.isEmpty()) {
        throw std::invalid_argument("shortcut cannot be empty");
    }

    KeyboardShortcut shortcut;
    bool hasKey = false;
    for (const QString &rawPart : rawParts) {
        const QString part = rawPart.trimmed();
        const QString lowered = part.toLower();
        QString modifier;
        if (lowered == "ctrl" || lowered == "control") {
            modifier = "Ctrl";
        } else if (lowered == "shift") {
            modifier = "Shift";
        } else if (lowered == "alt" || lowered == "option") {
            modifier = "Alt";
        }
        if (!modifier.isEmpty()) {
            shortcut.modifiers.insert(modifier);
            continue;
        }
        if (hasKey) {
            throw std::invalid_argument(QStringLiteral("shortcut has multiple keys: %1").arg(text).toStdString());
        }
        const auto parsed = parseKey(lowered, part);
        shortcut.keyCode = parsed.first;
        shortcut.keyName = parsed.second;
        hasKey = true;
    }
    if (!hasKey) {
        throw std::invalid_argument("shortcut must include a non-modifier key");
    }
    return shortcut;
}

bool exitShortcutPressed(const KeyboardShortcut &shortcut)
{
    KeyboardShortcut emergency;
    emergency.modifiers.insert("Ctrl");
    emergency.keyCode = VK_OEM_3_KEY;
    emergency.keyName = "`";
    return emergency.isPressed() || shortcut.isPressed();
}
