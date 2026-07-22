from __future__ import annotations

import queue
import threading
import tkinter as tk
from contextlib import suppress
from dataclasses import replace
from tkinter import messagebox, ttk

from ollama_hud.core.controller import HudController
from ollama_hud.core.state import RuntimeSnapshot
from ollama_hud.services.hotkey_service import parse_shortcut
from ollama_hud.services.ollama_service import OllamaClient
from ollama_hud.services.settings_service import (
    AVAILABLE_MODELS,
    DEFAULT_CONFIG_PATH,
    USER_CONFIG_PATH,
    HudSettings,
    load_settings,
    save_settings,
    validate_settings,
)
from ollama_hud.ui.status_overlay import StatusHud
from ollama_hud.ui.theme import (
    APP_ACCENT,
    APP_ACCENT_2,
    APP_BG,
    APP_BORDER,
    APP_BORDER_SOFT,
    APP_CHROME,
    APP_DIM,
    APP_FIELD,
    APP_MUTED,
    APP_PANEL,
    APP_PANEL_ALT,
    APP_SURFACE,
    APP_TEXT,
    configure_ttk_theme,
)

AUTOSAVE_DELAY_MS = 800


class OllamaHudGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Ollama HUD - Vision Runtime Manager")
        self.root.geometry("1180x760")
        self.root.minsize(980, 640)
        self.settings = load_settings()
        self.hud: StatusHud | None = None
        self.runtime: HudController | None = None
        self.log_queue: queue.Queue[str] = queue.Queue()

        self.host_var = tk.StringVar(value=self.settings.host)
        self.model_var = tk.StringVar(value=self.settings.model)
        self.trigger_var = tk.StringVar(value=self.settings.trigger_shortcut)
        self.exit_var = tk.StringVar(value=self.settings.exit_shortcut)
        self.clear_var = tk.StringVar(value=self.settings.clear_shortcut)
        self.max_edge_var = tk.StringVar(value=str(self.settings.screenshot_max_edge))
        self.timeout_var = tk.StringVar(value=str(int(self.settings.timeout_seconds)))
        self.memory_pairs_var = tk.StringVar(value=str(self.settings.memory_qa_pairs))
        self.keep_alive_minutes_var = tk.StringVar(
            value=_keep_alive_minutes(self.settings.keep_alive)
        )
        self.think_var = tk.BooleanVar(value=self.settings.think)
        self.temperature_var = tk.StringVar(value=str(self.settings.options.get("temperature", "")))
        self.top_p_var = tk.StringVar(value=str(self.settings.options.get("top_p", "")))
        self.num_predict_var = tk.StringVar(value=str(self.settings.options.get("num_predict", "")))
        self.num_ctx_var = tk.StringVar(value=str(self.settings.options.get("num_ctx", "")))
        self.repeat_penalty_var = tk.StringVar(
            value=str(self.settings.options.get("repeat_penalty", ""))
        )
        self.repeat_last_n_var = tk.StringVar(
            value=str(self.settings.options.get("repeat_last_n", ""))
        )

        self.instruction_text: tk.Text
        self.query_text: tk.Text
        self.log_text: tk.Text
        self.model_combo: ttk.Combobox
        self.status_var = tk.StringVar(value="Idle")
        self.autosave_after_id: str | None = None
        self.last_logged_capture_id = ""

        self._configure_theme()
        self._build_layout()
        self._bind_autosave()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._poll_log()

    def _configure_theme(self) -> None:
        configure_ttk_theme(self.root)

    def _build_layout(self) -> None:
        shell = tk.Frame(self.root, bg=APP_BG)
        shell.pack(fill="both", expand=True)
        shell.columnconfigure(1, weight=1)
        shell.rowconfigure(1, weight=1)

        self._build_chrome(shell)
        self._build_sidebar(shell)
        self._build_workspace(shell)
        self._build_status_bar(shell)

    def _build_chrome(self, parent: tk.Frame) -> None:
        chrome = tk.Frame(parent, bg=APP_CHROME, height=58)
        chrome.grid(row=0, column=0, columnspan=2, sticky="ew")
        chrome.grid_propagate(False)
        chrome.columnconfigure(2, weight=1)

        dots = tk.Frame(chrome, bg=APP_CHROME)
        dots.grid(row=0, column=0, sticky="w", padx=(18, 12))
        for color in ("#ff5f57", "#ffbd2e", "#28c840"):
            dot = tk.Canvas(dots, width=14, height=14, bg=APP_CHROME, highlightthickness=0)
            dot.create_oval(2, 2, 12, 12, fill=color, outline=color)
            dot.pack(side="left", padx=4)

        tk.Label(
            chrome,
            text="Ollama HUD - Vision Runtime Manager",
            bg=APP_CHROME,
            fg=APP_MUTED,
            font=("Segoe UI", 11, "bold"),
        ).grid(row=0, column=1, sticky="w")

        nav = tk.Frame(chrome, bg=APP_CHROME)
        nav.grid(row=0, column=2, sticky="w", padx=36)
        for index, item in enumerate(("Home", "Capture", "Prompt", "Configuration", "Logs")):
            active = index == 0
            label = tk.Label(
                nav,
                text=item,
                bg="#31264e" if active else APP_CHROME,
                fg=APP_TEXT if active else APP_MUTED,
                font=("Segoe UI", 10, "bold" if active else "normal"),
                padx=14,
                pady=8,
            )
            label.pack(side="left", padx=(0, 8))
            if active:
                label.configure(highlightthickness=1, highlightbackground="#4f3794")

        token = tk.Frame(
            chrome,
            bg=APP_SURFACE,
            highlightbackground=APP_BORDER,
            highlightthickness=1,
        )
        token.grid(row=0, column=3, sticky="e", padx=(0, 18), pady=8)
        tk.Label(
            token,
            text="OLLAMA",
            bg=APP_FIELD,
            fg=APP_ACCENT_2,
            font=("Segoe UI", 9, "bold"),
            padx=12,
            pady=9,
        ).pack(side="left", padx=(8, 10), pady=6)
        tk.Label(
            token,
            textvariable=self.model_var,
            bg=APP_SURFACE,
            fg=APP_TEXT,
            font=("Segoe UI", 9, "bold"),
        ).pack(side="top", anchor="w", padx=(0, 16), pady=(8, 0))
        tk.Label(
            token,
            textvariable=self.host_var,
            bg=APP_SURFACE,
            fg=APP_MUTED,
            font=("Segoe UI", 8),
        ).pack(side="top", anchor="w", padx=(0, 16), pady=(0, 7))

    def _build_sidebar(self, parent: tk.Frame) -> None:
        sidebar = tk.Frame(parent, bg=APP_SURFACE, width=255)
        sidebar.grid(row=1, column=0, sticky="nsew", padx=(16, 8), pady=16)
        sidebar.grid_propagate(False)
        sidebar.rowconfigure(2, weight=1)

        brand = tk.Label(
            sidebar,
            text="OLLAMA HUD",
            bg="#08090d",
            fg=APP_TEXT,
            font=("Segoe UI", 18),
            padx=24,
            pady=14,
        )
        brand.grid(row=0, column=0, sticky="ew", padx=18, pady=(18, 24))

        tk.Label(
            sidebar,
            text="Vision Runtime",
            bg=APP_SURFACE,
            fg=APP_TEXT,
            font=("Segoe UI", 13, "bold"),
        ).grid(row=1, column=0, sticky="w", padx=24)

        nav = tk.Frame(sidebar, bg=APP_SURFACE)
        nav.grid(row=2, column=0, sticky="nsew", padx=24, pady=(12, 0))
        for label, value in (
            ("Status", "Control center"),
            ("Model", self.settings.model),
            ("Capture", f"{self.settings.screenshot_max_edge}px edge"),
            ("Memory", f"{self.settings.memory_qa_pairs} Q/A pairs"),
            ("Hotkeys", self.settings.trigger_shortcut),
            ("Logs", "Text-only history"),
        ):
            row = tk.Frame(nav, bg=APP_SURFACE)
            row.pack(fill="x", pady=8)
            tk.Label(
                row,
                text=label,
                bg=APP_SURFACE,
                fg=APP_TEXT,
                font=("Segoe UI", 10, "bold"),
                width=9,
                anchor="w",
            ).pack(side="left")
            tk.Label(
                row,
                text=value,
                bg=APP_SURFACE,
                fg=APP_MUTED,
                font=("Segoe UI", 9),
                anchor="w",
            ).pack(side="left", fill="x", expand=True)

        launch_card = tk.Frame(
            sidebar,
            bg=APP_PANEL_ALT,
            highlightbackground=APP_BORDER_SOFT,
            highlightthickness=1,
        )
        launch_card.grid(row=3, column=0, sticky="ew", padx=18, pady=18)
        tk.Label(
            launch_card,
            text="Live HUD",
            bg=APP_PANEL_ALT,
            fg=APP_TEXT,
            font=("Segoe UI", 12, "bold"),
        ).pack(anchor="w", padx=18, pady=(16, 2))
        tk.Label(
            launch_card,
            text="Start the click-through overlay, then trigger capture over the target screen.",
            bg=APP_PANEL_ALT,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
            wraplength=190,
            justify="left",
        ).pack(anchor="w", padx=18, pady=(0, 14))
        ttk.Button(
            launch_card,
            text="Start HUD",
            style="Accent.TButton",
            command=self.start_hud,
        ).pack(fill="x", padx=18, pady=(0, 8))
        ttk.Button(
            launch_card,
            text="Stop HUD",
            style="Ghost.TButton",
            command=self.stop_hud,
        ).pack(fill="x", padx=18, pady=(0, 18))

    def _build_workspace(self, parent: tk.Frame) -> None:
        workspace = tk.Frame(parent, bg=APP_BG)
        workspace.grid(row=1, column=1, sticky="nsew", padx=(8, 16), pady=16)
        workspace.columnconfigure(0, weight=1)
        workspace.rowconfigure(2, weight=1)

        self._build_command_bar(workspace)
        self._build_metric_strip(workspace)
        self._build_main_panel(workspace)

    def _build_command_bar(self, parent: tk.Frame) -> None:
        command = tk.Frame(
            parent,
            bg=APP_PANEL,
            highlightbackground=APP_BORDER_SOFT,
            highlightthickness=1,
        )
        command.grid(row=0, column=0, sticky="ew")
        command.columnconfigure(0, weight=1)

        add_url = tk.Frame(
            command,
            bg=APP_FIELD,
            highlightbackground=APP_BORDER,
            highlightthickness=1,
        )
        add_url.grid(row=0, column=0, sticky="ew", padx=18, pady=14)
        add_url.columnconfigure(1, weight=1)
        tk.Label(
            add_url,
            text="Capture request",
            bg=APP_FIELD,
            fg=APP_TEXT,
            font=("Segoe UI", 12, "bold"),
            padx=16,
            pady=13,
        ).grid(row=0, column=0, sticky="w")
        tk.Label(
            add_url,
            text="Current screen -> Ollama answer",
            bg=APP_FIELD,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
        ).grid(row=0, column=1, sticky="w", padx=(0, 12))

        actions = tk.Frame(command, bg=APP_PANEL)
        actions.grid(row=0, column=1, sticky="e", padx=(0, 18))
        for text, callback, style in (
            ("Start HUD", self.start_hud, "Accent.TButton"),
            ("Test Ollama", self.test_ollama, "TButton"),
            ("Defaults", self.return_to_default, "TButton"),
        ):
            ttk.Button(actions, text=text, command=callback, style=style).pack(
                side="left", padx=(8, 0)
            )

    def _build_metric_strip(self, parent: tk.Frame) -> None:
        metrics = tk.Frame(parent, bg=APP_BG)
        metrics.grid(row=1, column=0, sticky="ew", pady=(14, 14))
        metrics.columnconfigure((0, 1, 2, 3), weight=1)

        for index, (label, var, accent) in enumerate(
            (
                ("Trigger", self.trigger_var, APP_ACCENT),
                ("Clear", self.clear_var, APP_ACCENT_2),
                ("Exit", self.exit_var, "#ffbd2e"),
                ("Memory", self.memory_pairs_var, APP_MUTED),
            )
        ):
            card = tk.Frame(
                metrics,
                bg=APP_SURFACE,
                highlightbackground=APP_BORDER_SOFT,
                highlightthickness=1,
            )
            card.grid(row=0, column=index, sticky="ew", padx=(0 if index == 0 else 10, 0))
            tk.Label(
                card,
                text=label,
                bg=APP_SURFACE,
                fg=APP_MUTED,
                font=("Segoe UI", 9),
            ).pack(anchor="w", padx=16, pady=(12, 2))
            tk.Label(
                card,
                textvariable=var,
                bg=APP_SURFACE,
                fg=accent,
                font=("Segoe UI", 15, "bold"),
            ).pack(anchor="w", padx=16, pady=(0, 13))

    def _build_main_panel(self, parent: tk.Frame) -> None:
        panel = tk.Frame(
            parent,
            bg=APP_SURFACE,
            highlightbackground=APP_BORDER_SOFT,
            highlightthickness=1,
        )
        panel.grid(row=2, column=0, sticky="nsew")
        panel.columnconfigure(0, weight=1)
        panel.columnconfigure(1, weight=1)
        panel.rowconfigure(2, weight=1, minsize=360)

        header = tk.Frame(panel, bg=APP_SURFACE)
        header.grid(row=0, column=0, columnspan=2, sticky="ew", padx=18, pady=(18, 10))
        header.columnconfigure(0, weight=1)
        tk.Label(
            header,
            text="HUD Session",
            bg=APP_SURFACE,
            fg=APP_TEXT,
            font=("Segoe UI", 17),
        ).grid(row=0, column=0, sticky="w")
        tk.Label(
            header,
            textvariable=self.status_var,
            bg=APP_SURFACE,
            fg=APP_ACCENT_2,
            font=("Segoe UI", 10, "bold"),
        ).grid(row=0, column=1, sticky="e")

        filters = tk.Frame(panel, bg=APP_SURFACE)
        filters.grid(row=1, column=0, columnspan=2, sticky="ew", padx=18, pady=(0, 14))
        filters.columnconfigure(0, weight=1)
        self._model_picker(filters, 0, "Model", self.model_var)
        ttk.Entry(filters, textvariable=self.host_var).grid(
            row=0,
            column=1,
            sticky="ew",
            padx=(12, 0),
        )
        ttk.Button(filters, text="Save", command=self._autosave_current_settings).grid(
            row=0, column=2, padx=(12, 0)
        )

        left_shell = self._panel_section(panel, row=2, column=0, title="Runtime Configuration")
        right = self._panel_section(panel, row=2, column=1, title="Prompt & Activity")
        left = self._scrollable_form(left_shell)

        self._entry(left, 0, "Trigger shortcut", self.trigger_var)
        self._entry(left, 1, "Clear shortcut", self.clear_var)
        self._entry(left, 2, "Exit shortcut", self.exit_var)
        self._entry(left, 3, "Screenshot max edge", self.max_edge_var)
        self._entry(left, 4, "Timeout seconds", self.timeout_var)
        self._entry(left, 5, "Q/A memory pairs", self.memory_pairs_var)
        self._keep_alive_entry(left, 6)
        self._checkbutton(left, 7, "Think", self.think_var)
        self._entry(left, 8, "Temperature", self.temperature_var)
        self._entry(left, 9, "Top P", self.top_p_var)
        self._entry(left, 10, "Max output tokens", self.num_predict_var)
        self._entry(left, 11, "Context tokens", self.num_ctx_var)
        self._entry(left, 12, "Repeat penalty", self.repeat_penalty_var)
        self._entry(left, 13, "Repeat last N", self.repeat_last_n_var)

        right.columnconfigure(0, weight=1)
        right.rowconfigure(1, weight=1, minsize=50)
        right.rowconfigure(3, weight=2, minsize=72)
        right.rowconfigure(5, weight=1, minsize=62)
        self._text_block(right, 0, "Instruction")
        self.instruction_text = self._text_widget(right, 1, height=3)
        self.instruction_text.insert("1.0", self.settings.instruction)
        self.instruction_text.edit_modified(False)
        self._text_block(right, 2, "Query")
        self.query_text = self._text_widget(right, 3, height=4)
        self.query_text.insert("1.0", self.settings.query)
        self.query_text.edit_modified(False)
        self._text_block(right, 4, "Session Log")
        self.log_text = self._text_widget(right, 5, height=4, muted=True)
        self._append_log(
            "Ready. Start the HUD, then press the trigger shortcut over the target screen.\n"
        )

    def _panel_section(self, parent: tk.Frame, *, row: int, column: int, title: str) -> tk.Frame:
        outer = tk.Frame(parent, bg=APP_SURFACE)
        outer.grid(
            row=row,
            column=column,
            sticky="nsew",
            padx=(18, 8) if column == 0 else (8, 18),
            pady=(0, 18),
        )
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)
        tk.Label(
            outer,
            text=title,
            bg=APP_SURFACE,
            fg=APP_MUTED,
            font=("Segoe UI", 10, "bold"),
        ).grid(row=0, column=0, sticky="w", pady=(0, 8))
        frame = tk.Frame(
            outer,
            bg=APP_PANEL,
            highlightbackground=APP_BORDER_SOFT,
            highlightthickness=1,
        )
        frame.grid(row=1, column=0, sticky="nsew")
        frame.columnconfigure(1, weight=1)
        return frame

    def _scrollable_form(self, parent: tk.Frame) -> tk.Frame:
        parent.rowconfigure(0, weight=1)
        parent.columnconfigure(0, weight=1)
        canvas = tk.Canvas(
            parent,
            bg=APP_PANEL,
            highlightthickness=0,
            bd=0,
        )
        scrollbar = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
        inner = tk.Frame(canvas, bg=APP_PANEL)
        inner.columnconfigure(1, weight=1)
        window_id = canvas.create_window((0, 0), window=inner, anchor="nw")

        def sync_scroll(_event: tk.Event | None = None) -> None:
            canvas.configure(scrollregion=canvas.bbox("all"))

        def resize_inner(event: tk.Event) -> None:
            canvas.itemconfigure(window_id, width=event.width)

        inner.bind("<Configure>", sync_scroll)
        canvas.bind("<Configure>", resize_inner)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")
        return inner

    def _build_status_bar(self, parent: tk.Frame) -> None:
        bar = tk.Frame(
            parent,
            bg=APP_CHROME,
            height=42,
            highlightbackground=APP_BORDER_SOFT,
            highlightthickness=1,
        )
        bar.grid(row=2, column=0, columnspan=2, sticky="ew", padx=16, pady=(0, 14))
        bar.grid_propagate(False)
        bar.columnconfigure(3, weight=1)
        for index, text in enumerate(
            (
                "Visible: 0",
                f"Capture: {self.settings.screenshot_max_edge}px",
                f"Memory: {self.settings.memory_qa_pairs}",
            )
        ):
            tk.Label(
                bar,
                text=text,
                bg=APP_CHROME,
                fg=APP_MUTED,
                font=("Segoe UI", 9),
            ).grid(row=0, column=index, sticky="w", padx=(18 if index == 0 else 10, 0))
        tk.Label(
            bar,
            text="Online",
            bg=APP_CHROME,
            fg=APP_ACCENT_2,
            font=("Segoe UI", 9, "bold"),
        ).grid(row=0, column=4, sticky="e", padx=(0, 8))
        tk.Label(
            bar,
            text="AC",
            bg=APP_CHROME,
            fg=APP_TEXT,
            font=("Segoe UI", 9),
        ).grid(row=0, column=5, sticky="e", padx=(0, 18))

    def _entry(
        self,
        parent: tk.Frame,
        row: int,
        label: str,
        variable: tk.StringVar,
    ) -> None:
        tk.Label(
            parent,
            text=label,
            bg=APP_PANEL,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
        ).grid(row=row, column=0, sticky="w", pady=6, padx=(16, 12))
        ttk.Entry(parent, textvariable=variable).grid(
            row=row,
            column=1,
            sticky="ew",
            pady=6,
            padx=(0, 16),
        )

    def _keep_alive_entry(self, parent: tk.Frame, row: int) -> None:
        tk.Label(
            parent,
            text="Keep alive",
            bg=APP_PANEL,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
        ).grid(row=row, column=0, sticky="w", pady=6, padx=(16, 12))
        field = tk.Frame(parent, bg=APP_PANEL)
        field.grid(row=row, column=1, sticky="ew", pady=6, padx=(0, 16))
        field.columnconfigure(0, weight=1)
        ttk.Entry(field, textvariable=self.keep_alive_minutes_var).grid(
            row=0,
            column=0,
            sticky="ew",
        )
        tk.Label(
            field,
            text="min",
            bg=APP_PANEL,
            fg=APP_DIM,
            font=("Segoe UI", 9),
        ).grid(row=0, column=1, sticky="w", padx=(8, 0))

    def _checkbutton(
        self,
        parent: tk.Frame,
        row: int,
        label: str,
        variable: tk.BooleanVar,
    ) -> None:
        tk.Label(
            parent,
            text=label,
            bg=APP_PANEL,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
        ).grid(row=row, column=0, sticky="w", pady=6, padx=(16, 12))
        ttk.Checkbutton(parent, text="Enabled", variable=variable).grid(
            row=row,
            column=1,
            sticky="w",
            pady=6,
            padx=(0, 16),
        )

    def _model_picker(
        self,
        parent: tk.Frame,
        row: int,
        label: str,
        variable: tk.StringVar,
    ) -> None:
        values = _model_choices(self.settings.model)
        model_shell = tk.Frame(parent, bg=APP_SURFACE)
        model_shell.grid(row=row, column=0, sticky="ew")
        model_shell.columnconfigure(1, weight=1)
        tk.Label(
            model_shell,
            text=label,
            bg=APP_SURFACE,
            fg=APP_MUTED,
            font=("Segoe UI", 9),
        ).grid(row=0, column=0, sticky="w", padx=(0, 10))
        self.model_combo = ttk.Combobox(model_shell, textvariable=variable, values=values)
        self.model_combo.grid(row=0, column=1, sticky="ew")

    def _text_block(self, parent: tk.Frame, row: int, title: str) -> None:
        tk.Label(
            parent,
            text=title,
            bg=APP_PANEL,
            fg=APP_MUTED,
            font=("Segoe UI", 9, "bold"),
        ).grid(row=row, column=0, sticky="w", padx=16, pady=(10 if row else 12, 4))

    def _text_widget(
        self,
        parent: tk.Frame,
        row: int,
        *,
        height: int,
        muted: bool = False,
    ) -> tk.Text:
        widget = tk.Text(
            parent,
            height=height,
            wrap="word",
            bg=APP_FIELD,
            fg=APP_MUTED if muted else APP_TEXT,
            insertbackground=APP_TEXT,
            relief="flat",
            bd=0,
            padx=12,
            pady=10,
        )
        widget.grid(row=row, column=0, sticky="nsew", padx=16, pady=(0, 4))
        return widget

    def _bind_autosave(self) -> None:
        for variable in (
            self.host_var,
            self.trigger_var,
            self.exit_var,
            self.clear_var,
            self.max_edge_var,
            self.timeout_var,
            self.memory_pairs_var,
            self.keep_alive_minutes_var,
            self.think_var,
            self.temperature_var,
            self.top_p_var,
            self.num_predict_var,
            self.num_ctx_var,
            self.repeat_penalty_var,
            self.repeat_last_n_var,
        ):
            variable.trace_add("write", lambda *_args: self._schedule_autosave())
        self.model_var.trace_add("write", lambda *_args: self._schedule_autosave())
        self.model_combo.bind("<<ComboboxSelected>>", self._on_model_committed)
        self.model_combo.bind("<FocusOut>", self._on_model_committed)
        self.instruction_text.bind("<<Modified>>", self._on_instruction_modified)
        self.query_text.bind("<<Modified>>", self._on_query_modified)

    def _on_model_committed(self, _event: tk.Event) -> None:
        model = self.model_var.get().strip()
        if not model:
            self.status_var.set("Settings pending")
            return
        try:
            settings = self._settings_from_fields()
        except ValueError:
            settings = replace(load_settings(USER_CONFIG_PATH), model=model)
        self.settings = settings
        self._persist_settings(settings, reason="autosaved model", log=False)

    def _on_instruction_modified(self, _event: tk.Event) -> None:
        if not self.instruction_text.edit_modified():
            return
        self.instruction_text.edit_modified(False)
        self._schedule_autosave()

    def _on_query_modified(self, _event: tk.Event) -> None:
        if not self.query_text.edit_modified():
            return
        self.query_text.edit_modified(False)
        self._schedule_autosave()

    def _schedule_autosave(self) -> None:
        if self.autosave_after_id is not None:
            with suppress(tk.TclError):
                self.root.after_cancel(self.autosave_after_id)
        self.autosave_after_id = self.root.after(AUTOSAVE_DELAY_MS, self._autosave_after_edit)

    def _autosave_after_edit(self) -> None:
        self.autosave_after_id = None
        try:
            settings = self._settings_from_fields()
        except ValueError:
            self.status_var.set("Settings pending")
            return
        self.settings = settings
        self._persist_settings(settings, reason="autosaved", log=False)

    def start_hud(self) -> None:
        if self.runtime is not None and self.hud is not None and not self.hud.closed:
            self._append_log("HUD is already running.\n")
            return
        try:
            settings = self._settings_from_fields()
        except ValueError as exc:
            messagebox.showerror("Invalid settings", str(exc))
            return
        self.settings = settings
        self._persist_settings(settings, reason="autosaved before starting HUD")
        self.hud = StatusHud(parent=self.root)
        self.runtime = HudController(settings)
        snapshot = self.runtime.snapshot
        self.hud.display(snapshot.state.value, snapshot.message)
        self.status_var.set("HUD running")
        self._append_log(
            f"HUD started. Trigger: {self.runtime.trigger_shortcut.display}; "
            f"clear: {self.runtime.clear_shortcut.display}; "
            f"exit: {self.runtime.exit_shortcut.display}; emergency: Ctrl+`.\n"
        )
        self.root.after(30, self._poll_hud)

    def stop_hud(self) -> None:
        if self.runtime is not None:
            self.runtime.stop()
        if self.hud is not None:
            self.hud.close()
        self.hud = None
        self.runtime = None
        self.status_var.set("Idle")
        self._append_log("HUD stopped.\n")

    def test_ollama(self) -> None:
        try:
            settings = self._settings_from_fields()
        except ValueError as exc:
            messagebox.showerror("Invalid settings", str(exc))
            return
        self.settings = settings
        self._persist_settings(settings, reason="autosaved before testing Ollama")
        self.status_var.set("Testing Ollama...")
        self._append_log("Testing Ollama with the configured model.\n")

        def worker() -> None:
            try:
                response = OllamaClient(settings).test_model()
            except Exception as exc:
                self.log_queue.put(f"Test failed: {exc}\n")
                self.log_queue.put("__STATUS__:Test failed")
            else:
                self.log_queue.put(f"Test response: {response}\n")
                self.log_queue.put("__STATUS__:Ollama OK")

        threading.Thread(target=worker, daemon=True).start()

    def return_to_default(self) -> None:
        confirmed = messagebox.askyesno(
            "Return to default settings",
            "Reset all settings to config/default.yaml?",
            parent=self.root,
        )
        if not confirmed:
            return
        settings = load_settings(DEFAULT_CONFIG_PATH)
        self._apply_settings_to_fields(settings)
        self.settings = settings
        self._persist_settings(settings, reason="reset to default")

    def _autosave_current_settings(self) -> None:
        try:
            settings = self._settings_from_fields()
        except ValueError as exc:
            self._append_log(f"Settings not saved: {exc}\n")
            return
        self.settings = settings
        self._persist_settings(settings, reason="saved")

    def _persist_settings(self, settings: HudSettings, *, reason: str, log: bool = True) -> None:
        try:
            save_settings(settings, USER_CONFIG_PATH)
        except OSError as exc:
            self._append_log(f"Could not save settings: {exc}\n")
            return
        self.status_var.set("Settings autosaved" if "autosaved" in reason else "Settings saved")
        if log:
            self._append_log(f"Settings {reason}: {USER_CONFIG_PATH}.\n")

    def _poll_hud(self) -> None:
        if self.runtime is None or self.hud is None:
            return
        if self.hud.closed:
            self.stop_hud()
            return
        snapshot = self.runtime.poll_updates()
        self._display_snapshot(snapshot)
        if self.runtime.should_exit():
            self.stop_hud()
            return
        if self.runtime.clear_pressed_once() and not self.runtime.active:
            self._display_snapshot(self.runtime.clear_visual_answer())
            self._append_log("HUD answer cleared.\n")
        if self.runtime.trigger_pressed_once() and not self.runtime.active:
            self.runtime.start_request(self.hud)
            self._display_snapshot(self.runtime.snapshot)
        self.root.after(30, self._poll_hud)

    def _display_snapshot(self, snapshot: RuntimeSnapshot) -> None:
        if self.hud is not None:
            self.hud.display(snapshot.state.value, snapshot.message, error=snapshot.is_error)
        self.status_var.set(snapshot.state.value if not snapshot.message else snapshot.message)
        if snapshot.capture_id and snapshot.capture_id != self.last_logged_capture_id:
            self.last_logged_capture_id = snapshot.capture_id
            self._append_log(f"Capture {snapshot.capture_id} sent to Ollama.\n")

    def _settings_from_fields(self) -> HudSettings:
        trigger = parse_shortcut(self.trigger_var.get()).display
        exit_shortcut = parse_shortcut(self.exit_var.get()).display
        clear_shortcut = parse_shortcut(self.clear_var.get()).display
        max_edge = _int_field(self.max_edge_var.get(), "Screenshot max edge")
        timeout = _float_field(self.timeout_var.get(), "Timeout seconds")
        memory_pairs = _int_field(self.memory_pairs_var.get(), "Q/A memory pairs")
        query = self.query_text.get("1.0", "end").strip()
        instruction = self.instruction_text.get("1.0", "end").strip()
        keep_alive_minutes = _int_field(self.keep_alive_minutes_var.get(), "Keep alive")
        options = dict(self.settings.options)
        options["temperature"] = _float_field(self.temperature_var.get(), "Temperature")
        options["top_p"] = _float_field(self.top_p_var.get(), "Top P")
        options["num_predict"] = _int_field(self.num_predict_var.get(), "Max output tokens")
        options["num_ctx"] = _int_field(self.num_ctx_var.get(), "Context tokens")
        options["repeat_penalty"] = _float_field(self.repeat_penalty_var.get(), "Repeat penalty")
        options["repeat_last_n"] = _int_field(self.repeat_last_n_var.get(), "Repeat last N")
        settings = HudSettings(
            host=self.host_var.get().strip(),
            model=self.model_var.get().strip(),
            trigger_shortcut=trigger,
            exit_shortcut=exit_shortcut,
            clear_shortcut=clear_shortcut,
            screenshot_max_edge=max_edge,
            timeout_seconds=timeout,
            memory_qa_pairs=memory_pairs,
            instruction=instruction,
            query=query,
            keep_alive=f"{keep_alive_minutes}m",
            think=self.think_var.get(),
            options=options,
        )
        validate_settings(settings)
        return settings

    def _apply_settings_to_fields(self, settings: HudSettings) -> None:
        self.host_var.set(settings.host)
        self.model_var.set(settings.model)
        self.trigger_var.set(settings.trigger_shortcut)
        self.exit_var.set(settings.exit_shortcut)
        self.clear_var.set(settings.clear_shortcut)
        self.max_edge_var.set(str(settings.screenshot_max_edge))
        self.timeout_var.set(str(int(settings.timeout_seconds)))
        self.memory_pairs_var.set(str(settings.memory_qa_pairs))
        self.keep_alive_minutes_var.set(_keep_alive_minutes(settings.keep_alive))
        self.think_var.set(settings.think)
        self.temperature_var.set(str(settings.options.get("temperature", "")))
        self.top_p_var.set(str(settings.options.get("top_p", "")))
        self.num_predict_var.set(str(settings.options.get("num_predict", "")))
        self.num_ctx_var.set(str(settings.options.get("num_ctx", "")))
        self.repeat_penalty_var.set(str(settings.options.get("repeat_penalty", "")))
        self.repeat_last_n_var.set(str(settings.options.get("repeat_last_n", "")))
        self.instruction_text.delete("1.0", "end")
        self.instruction_text.insert("1.0", settings.instruction)
        self.instruction_text.edit_modified(False)
        self.query_text.delete("1.0", "end")
        self.query_text.insert("1.0", settings.query)
        self.query_text.edit_modified(False)

    def _poll_log(self) -> None:
        try:
            while True:
                item = self.log_queue.get_nowait()
                if item.startswith("__STATUS__:"):
                    self.status_var.set(item.removeprefix("__STATUS__:"))
                else:
                    self._append_log(item)
        except queue.Empty:
            pass
        self.root.after(100, self._poll_log)

    def _append_log(self, text: str) -> None:
        self.log_text.insert("end", text)
        self.log_text.see("end")

    def _on_close(self) -> None:
        if self.autosave_after_id is not None:
            with suppress(tk.TclError):
                self.root.after_cancel(self.autosave_after_id)
            self.autosave_after_id = None
        self._autosave_current_settings()
        with suppress(Exception):
            self.stop_hud()
        self.root.destroy()


def main() -> int:
    root = tk.Tk()
    OllamaHudGui(root)
    root.mainloop()
    return 0


def _model_choices(current_model: str) -> tuple[str, ...]:
    choices = list(AVAILABLE_MODELS)
    if current_model and current_model not in choices:
        choices.insert(0, current_model)
    return tuple(choices)


def _int_field(value: str, label: str) -> int:
    try:
        return int(value)
    except ValueError as exc:
        raise ValueError(f"{label} must be an integer.") from exc


def _float_field(value: str, label: str) -> float:
    try:
        return float(value)
    except ValueError as exc:
        raise ValueError(f"{label} must be a number.") from exc


def _keep_alive_minutes(value: str) -> str:
    text = value.strip().lower()
    if text.endswith("m"):
        candidate = text[:-1]
    elif text.endswith("h"):
        try:
            return str(round(float(text[:-1]) * 60))
        except ValueError:
            return "30"
    else:
        candidate = text
    try:
        return str(int(candidate))
    except ValueError:
        return "30"


if __name__ == "__main__":
    raise SystemExit(main())
