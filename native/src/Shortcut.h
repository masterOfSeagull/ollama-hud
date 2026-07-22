#pragma once

#include <QSet>
#include <QString>

class KeyboardShortcut
{
public:
    QSet<QString> modifiers;
    int keyCode = 0;
    QString keyName;

    QString display() const;
    bool isPressed() const;
};

KeyboardShortcut parseShortcut(const QString &text);
bool exitShortcutPressed(const KeyboardShortcut &shortcut);
