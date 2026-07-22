from __future__ import annotations

import ctypes
import textwrap
import tkinter as tk
from contextlib import suppress
from ctypes import wintypes

from ollama_hud.ui.theme import (
    OVERLAY_BORDER,
    OVERLAY_ERROR,
    OVERLAY_MUTED,
    OVERLAY_PANEL,
    OVERLAY_TEXT,
    OVERLAY_TRANSPARENT,
)

GWL_EXSTYLE = -20
GWLP_WNDPROC = -4
GA_ROOT = 2

WS_EX_LAYERED = 0x00080000
WS_EX_TRANSPARENT = 0x00000020
WS_EX_NOACTIVATE = 0x08000000

LWA_COLORKEY = 0x00000001

WM_NCHITTEST = 0x0084
HTTRANSPARENT = -1

SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010
SWP_FRAMECHANGED = 0x0020
HWND_TOPMOST = -1


class StatusHud:
    def __init__(
        self,
        *,
        left: int = 0,
        top: int = 0,
        width: int | None = None,
        height: int | None = None,
        parent: tk.Misc | None = None,
    ) -> None:
        _enable_dpi_awareness()
        self.closed = False
        self._hwnd: int | None = None
        self._original_wndproc: int | None = None
        self._wndproc_callback: object | None = None
        self._own_root = parent is None

        self.root = tk.Tk() if parent is None else tk.Toplevel(parent)
        self.root.withdraw()
        screen_width = width or self.root.winfo_screenwidth()
        screen_height = height or self.root.winfo_screenheight()
        self.left = left
        self.top = top
        self.width = screen_width
        self.height = screen_height

        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)
        with suppress(tk.TclError):
            self.root.attributes("-transparentcolor", OVERLAY_TRANSPARENT)
        self.root.configure(bg=OVERLAY_TRANSPARENT)
        self.root.geometry(f"{self.width}x{self.height}+{left}+{top}")
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        self.canvas = tk.Canvas(
            self.root,
            width=self.width,
            height=self.height,
            bg=OVERLAY_TRANSPARENT,
            highlightthickness=0,
            bd=0,
        )
        self.canvas.pack(fill="both", expand=True)
        self.root.update_idletasks()
        self._make_input_transparent()
        self.root.deiconify()
        self.root.update_idletasks()
        self._make_input_transparent()

    def display(self, title: str, message: str = "", *, error: bool = False) -> bool:
        if self.closed:
            return False
        self.canvas.delete("all")
        self._draw_panel(title, message, error=error)
        return self.poll()

    def hide(self) -> None:
        with suppress(tk.TclError):
            self.root.withdraw()
            self.root.update_idletasks()
            self.root.update()

    def show(self) -> None:
        if self.closed:
            return
        with suppress(tk.TclError):
            self.root.deiconify()
            self.root.lift()
            self.root.attributes("-topmost", True)
            self._make_input_transparent()
            self.root.update_idletasks()
            self.root.update()

    def poll(self) -> bool:
        if self.closed:
            return False
        try:
            self.root.update_idletasks()
            self.root.update()
        except tk.TclError:
            self.closed = True
            return False
        return not self.closed

    def close(self) -> None:
        self.closed = True
        self._restore_wndproc()
        with suppress(tk.TclError):
            self.root.destroy()

    def _draw_panel(self, title: str, message: str, *, error: bool) -> None:
        wrapped = _wrap(message, 52)
        lines = [title, *wrapped] if wrapped else [title]
        line_height = 18
        panel_width = 440
        panel_height = max(46, 22 + line_height * len(lines))
        x1, y1 = 10, 10
        x2, y2 = x1 + panel_width, y1 + panel_height
        outline = OVERLAY_ERROR if error else OVERLAY_BORDER
        self.canvas.create_rectangle(
            x1,
            y1,
            x2,
            y2,
            fill=OVERLAY_PANEL,
            outline=outline,
            width=1,
            stipple="gray75",
        )
        self.canvas.create_text(
            x1 + 12,
            y1 + 15,
            text=title,
            fill=outline,
            anchor="w",
            font=("Segoe UI", 10, "bold"),
        )
        if message:
            self.canvas.create_text(
                x1 + 12,
                y1 + 34,
                text="\n".join(wrapped),
                fill=OVERLAY_TEXT if not error else OVERLAY_MUTED,
                anchor="nw",
                font=("Segoe UI", 10),
            )

    def _make_input_transparent(self) -> None:
        with suppress(Exception):
            self._hwnd = _get_toplevel_hwnd(self.root.winfo_id())
            _make_click_through_and_color_key(self._hwnd)
            self._subclass_hit_test(self._hwnd)

    def _subclass_hit_test(self, hwnd: int) -> None:
        if self._original_wndproc is not None:
            return

        user32 = ctypes.windll.user32
        user32.CallWindowProcW.argtypes = (
            ctypes.c_void_p,
            wintypes.HWND,
            wintypes.UINT,
            wintypes.WPARAM,
            wintypes.LPARAM,
        )
        user32.CallWindowProcW.restype = wintypes.LPARAM
        wndproc_type = _wndproc_type()

        def wndproc(window: int, message: int, wparam: int, lparam: int) -> int:
            if message == WM_NCHITTEST:
                return HTTRANSPARENT
            return user32.CallWindowProcW(
                self._original_wndproc,
                window,
                message,
                wparam,
                lparam,
            )

        callback = wndproc_type(wndproc)
        callback_ptr = ctypes.cast(callback, ctypes.c_void_p).value
        if callback_ptr is None:
            return

        original = _set_window_long_ptr(user32, hwnd, GWLP_WNDPROC, callback_ptr)
        if original is None:
            return
        self._original_wndproc = original
        self._wndproc_callback = callback

    def _restore_wndproc(self) -> None:
        if self._hwnd is None or self._original_wndproc is None:
            return
        with suppress(Exception):
            _set_window_long_ptr(
                ctypes.windll.user32,
                self._hwnd,
                GWLP_WNDPROC,
                self._original_wndproc,
            )
        self._original_wndproc = None
        self._wndproc_callback = None


def _wrap(text: str, width: int) -> list[str]:
    if not text:
        return []
    return textwrap.wrap(text, width=width)


def _enable_dpi_awareness() -> None:
    with suppress(Exception):
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
        return
    with suppress(Exception):
        ctypes.windll.user32.SetProcessDPIAware()


def _make_click_through_and_color_key(hwnd: int) -> None:
    user32 = ctypes.windll.user32
    user32.GetWindowLongW.argtypes = (wintypes.HWND, ctypes.c_int)
    user32.GetWindowLongW.restype = wintypes.LONG
    user32.SetWindowLongW.argtypes = (wintypes.HWND, ctypes.c_int, wintypes.LONG)
    user32.SetWindowLongW.restype = wintypes.LONG
    user32.SetLayeredWindowAttributes.argtypes = (
        wintypes.HWND,
        wintypes.COLORREF,
        wintypes.BYTE,
        wintypes.DWORD,
    )
    user32.SetLayeredWindowAttributes.restype = wintypes.BOOL
    user32.SetWindowPos.argtypes = (
        wintypes.HWND,
        wintypes.HWND,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        wintypes.UINT,
    )
    user32.SetWindowPos.restype = wintypes.BOOL
    current = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
    user32.SetWindowLongW(hwnd, GWL_EXSTYLE, _transparent_window_styles(current))
    user32.SetLayeredWindowAttributes(hwnd, _colorref(OVERLAY_TRANSPARENT), 0, LWA_COLORKEY)
    user32.SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE,
    )


def _transparent_window_styles(current: int) -> int:
    return current | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE


def _get_toplevel_hwnd(hwnd: int) -> int:
    user32 = ctypes.windll.user32
    user32.GetAncestor.argtypes = (wintypes.HWND, wintypes.UINT)
    user32.GetAncestor.restype = wintypes.HWND
    root = user32.GetAncestor(hwnd, GA_ROOT)
    return root or hwnd


def _wndproc_type() -> type:
    return ctypes.WINFUNCTYPE(
        wintypes.LPARAM,
        wintypes.HWND,
        wintypes.UINT,
        wintypes.WPARAM,
        wintypes.LPARAM,
    )


def _set_window_long_ptr(user32: object, hwnd: int, index: int, value: int) -> int | None:
    set_window_long_ptr = getattr(user32, "SetWindowLongPtrW", user32.SetWindowLongW)
    set_window_long_ptr.argtypes = (wintypes.HWND, ctypes.c_int, ctypes.c_void_p)
    set_window_long_ptr.restype = ctypes.c_void_p
    result = set_window_long_ptr(hwnd, index, value)
    if result is None:
        return None
    return int(result)


def _colorref(hex_color: str) -> int:
    value = hex_color.lstrip("#")
    red = int(value[0:2], 16)
    green = int(value[2:4], 16)
    blue = int(value[4:6], 16)
    return red | (green << 8) | (blue << 16)
