from __future__ import annotations

import tkinter as tk
from tkinter import ttk

APP_BG = "#101116"
APP_CHROME = "#202130"
APP_SURFACE = "#181922"
APP_PANEL = "#22232d"
APP_PANEL_ALT = "#2b2c36"
APP_FIELD = "#14151d"
APP_BORDER = "#343644"
APP_BORDER_SOFT = "#292b36"
APP_TEXT = "#f3f4fb"
APP_MUTED = "#a7aab8"
APP_DIM = "#747887"
APP_ACCENT = "#8b5cf6"
APP_ACCENT_2 = "#23d18b"
APP_WARN = "#f2b84b"
APP_ERROR = "#ff6b6b"

OVERLAY_TRANSPARENT = "#010203"
OVERLAY_PANEL = "#171821"
OVERLAY_BORDER = APP_ACCENT
OVERLAY_TEXT = APP_TEXT
OVERLAY_MUTED = APP_MUTED
OVERLAY_ERROR = APP_ERROR


def configure_ttk_theme(root: tk.Tk) -> None:
    root.configure(bg=APP_BG)
    root.option_add("*Font", "{Segoe UI} 10")
    root.option_add("*TCombobox*Listbox.background", APP_FIELD)
    root.option_add("*TCombobox*Listbox.foreground", APP_TEXT)
    root.option_add("*TCombobox*Listbox.selectBackground", "#133d38")
    root.option_add("*TCombobox*Listbox.selectForeground", APP_TEXT)
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure(".", background=APP_BG, foreground=APP_TEXT)
    style.configure("TFrame", background=APP_BG)
    style.configure("Panel.TFrame", background=APP_PANEL)
    style.configure("Surface.TFrame", background=APP_SURFACE)
    style.configure("Chrome.TFrame", background=APP_CHROME)
    style.configure("Alt.TFrame", background=APP_PANEL_ALT)
    style.configure(
        "Panel.TLabelframe",
        background=APP_BG,
        foreground=APP_TEXT,
        bordercolor=APP_BORDER,
    )
    style.configure(
        "Panel.TLabelframe.Label",
        background=APP_BG,
        foreground=APP_TEXT,
    )
    style.configure("TLabel", background=APP_BG, foreground=APP_TEXT)
    style.configure("Panel.TLabel", background=APP_PANEL, foreground=APP_TEXT)
    style.configure("Muted.TLabel", background=APP_BG, foreground=APP_MUTED)
    style.configure("PanelMuted.TLabel", background=APP_PANEL, foreground=APP_MUTED)
    style.configure("Surface.TLabel", background=APP_SURFACE, foreground=APP_TEXT)
    style.configure("SurfaceMuted.TLabel", background=APP_SURFACE, foreground=APP_MUTED)
    style.configure("Chrome.TLabel", background=APP_CHROME, foreground=APP_TEXT)
    style.configure("ChromeMuted.TLabel", background=APP_CHROME, foreground=APP_MUTED)
    style.configure(
        "Status.TLabel",
        background=APP_CHROME,
        foreground=APP_ACCENT,
        font=("Segoe UI", 10, "bold"),
    )
    style.configure(
        "TEntry",
        fieldbackground=APP_FIELD,
        foreground=APP_TEXT,
        bordercolor=APP_BORDER,
        insertcolor=APP_TEXT,
        padding=(8, 6),
    )
    style.configure(
        "TCombobox",
        background=APP_FIELD,
        fieldbackground=APP_FIELD,
        foreground=APP_TEXT,
        bordercolor=APP_BORDER,
        arrowcolor=APP_TEXT,
        insertcolor=APP_TEXT,
        padding=(8, 6),
    )
    style.map(
        "TCombobox",
        background=[("readonly", APP_FIELD), ("active", APP_FIELD)],
        fieldbackground=[("readonly", APP_FIELD), ("active", APP_FIELD)],
        foreground=[("readonly", APP_TEXT), ("disabled", APP_MUTED)],
        selectbackground=[("readonly", "#133d38")],
        selectforeground=[("readonly", APP_TEXT)],
    )
    style.configure(
        "TButton",
        background=APP_PANEL_ALT,
        foreground=APP_TEXT,
        bordercolor=APP_BORDER,
        padding=(14, 8),
    )
    style.map("TButton", background=[("active", "#383a49")])
    style.configure(
        "TCheckbutton",
        background=APP_PANEL,
        foreground=APP_TEXT,
        focuscolor=APP_PANEL,
    )
    style.map(
        "TCheckbutton",
        background=[("active", APP_PANEL)],
        foreground=[("disabled", APP_MUTED)],
    )
    style.configure(
        "Accent.TButton",
        background="#3f2c76",
        foreground=APP_TEXT,
        bordercolor=APP_ACCENT,
    )
    style.map("Accent.TButton", background=[("active", "#4f3794")])
    style.configure(
        "Ghost.TButton",
        background=APP_SURFACE,
        foreground=APP_MUTED,
        bordercolor=APP_BORDER_SOFT,
        padding=(12, 7),
    )
    style.map(
        "Ghost.TButton",
        background=[("active", APP_PANEL)],
        foreground=[("active", APP_TEXT)],
    )
