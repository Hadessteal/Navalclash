#!/usr/bin/env python3
"""Small Windows GUI for the NavyCraft texture pack converter."""

from __future__ import annotations

import os
import queue
import re
import subprocess
import sys
import threading
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText


APP_DIR = Path(__file__).resolve().parent
REPO_ROOT = APP_DIR.parents[1]
CONVERTER = APP_DIR / "convert_pack.py"
GAME_DIR = REPO_ROOT / "game" / "navycraft"
FALLBACK_TEXTURE_ROOT = REPO_ROOT / "_converted_texture_packs"
DEFAULT_PACK_NAME = "navycraft_converted_pack"


def safe_pack_name(value: str) -> str:
    value = value.replace("\\", "/").strip()
    value = Path(value).stem if value else DEFAULT_PACK_NAME
    value = re.sub(r"[^A-Za-z0-9_.-]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("._-").lower()
    if not value:
        value = "converted_pack"
    if not value.startswith("navycraft_"):
        value = "navycraft_" + value
    return value


def find_current_build_texture_root() -> Path:
    artifact_root = REPO_ROOT / "_github-artifacts"
    candidates: list[tuple[float, str, Path]] = []
    if artifact_root.is_dir():
        for pattern in (
            "*/base-install-full/textures",
            "*/base-install/textures",
            "*/install/textures",
        ):
            for textures in artifact_root.glob(pattern):
                if not textures.is_dir():
                    continue
                install_root = textures.parent
                has_exe = (install_root / "bin" / "luanti.exe").is_file() or (
                    install_root / "bin" / "luantiserver.exe"
                ).is_file()
                if not has_exe:
                    continue
                candidates.append((install_root.stat().st_mtime, install_root.name, textures))
    if candidates:
        candidates.sort()
        return candidates[-1][2]
    return FALLBACK_TEXTURE_ROOT


def default_output_for_pack(pack_name: str, texture_root: Path | None = None) -> Path:
    root = texture_root or find_current_build_texture_root()
    return root / safe_pack_name(pack_name)


class PackConverterApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("NavyCraft Pack Converter")
        self.geometry("760x560")
        self.minsize(680, 500)

        self.texture_root = find_current_build_texture_root()
        self.output_is_auto = True
        self.worker: threading.Thread | None = None
        self.messages: queue.Queue[tuple[str, str | int]] = queue.Queue()
        self._setting_output = False

        self.input_var = tk.StringVar()
        self.output_var = tk.StringVar()
        self.pack_name_var = tk.StringVar(value=DEFAULT_PACK_NAME)
        self.texture_root_var = tk.StringVar(value=str(self.texture_root))
        self.custom_blocks_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Ready")
        self.copy_unmapped_var = tk.BooleanVar(value=True)
        self.overwrite_var = tk.BooleanVar(value=True)
        self.require_license_var = tk.BooleanVar(value=True)
        self.strict_var = tk.BooleanVar(value=False)

        self._set_auto_output()
        self._build_ui()
        self.pack_name_var.trace_add("write", lambda *_: self._pack_name_changed())
        self.output_var.trace_add("write", lambda *_: self._output_changed())
        self.after(100, self._drain_messages)

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(3, weight=1)

        header = ttk.Frame(self, padding=(14, 12, 14, 8))
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="NavyCraft Pack Converter", font=("Segoe UI", 16, "bold")).grid(
            row=0, column=0, sticky="w"
        )
        ttk.Label(
            header,
            text="Convert an open-licensed source texture pack into a NavyCraft texture pack.",
        ).grid(row=1, column=0, sticky="w", pady=(3, 0))

        form = ttk.Frame(self, padding=(14, 0, 14, 8))
        form.grid(row=1, column=0, sticky="ew")
        form.columnconfigure(1, weight=1)

        ttk.Label(form, text="Source pack").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.input_var).grid(row=0, column=1, sticky="ew", padx=8)
        ttk.Button(form, text="Zip...", command=self._choose_input_file).grid(row=0, column=2, padx=(0, 4))
        ttk.Button(form, text="Folder...", command=self._choose_input_folder).grid(row=0, column=3)

        ttk.Label(form, text="Pack name").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.pack_name_var, width=38).grid(row=1, column=1, sticky="w", padx=8)
        ttk.Button(form, text="Use input name", command=self._use_input_name).grid(row=1, column=2, columnspan=2, sticky="ew")

        ttk.Label(form, text="Current build textures").grid(row=2, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.texture_root_var, state="readonly").grid(row=2, column=1, sticky="ew", padx=8)
        ttk.Button(form, text="Refresh", command=self._refresh_texture_root).grid(row=2, column=2, padx=(0, 4))
        ttk.Button(form, text="Open", command=lambda: self._open_path(self.texture_root)).grid(row=2, column=3)

        ttk.Label(form, text="Output folder").grid(row=3, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.output_var).grid(row=3, column=1, sticky="ew", padx=8)
        ttk.Button(form, text="Browse...", command=self._choose_output_folder).grid(row=3, column=2, padx=(0, 4))
        ttk.Button(form, text="Auto", command=self._set_auto_output).grid(row=3, column=3)

        ttk.Label(form, text="Custom blocks JSON").grid(row=4, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.custom_blocks_var).grid(row=4, column=1, sticky="ew", padx=8)
        ttk.Button(form, text="Browse...", command=self._choose_custom_blocks).grid(row=4, column=2, padx=(0, 4))
        ttk.Button(form, text="Clear", command=lambda: self.custom_blocks_var.set("")).grid(row=4, column=3)

        options = ttk.Frame(self, padding=(14, 2, 14, 8))
        options.grid(row=2, column=0, sticky="ew")
        for column in range(5):
            options.columnconfigure(column, weight=1)
        ttk.Checkbutton(options, text="Copy unmapped images", variable=self.copy_unmapped_var).grid(row=0, column=0, sticky="w")
        ttk.Checkbutton(options, text="Overwrite output files", variable=self.overwrite_var).grid(row=0, column=1, sticky="w")
        ttk.Checkbutton(options, text="Require license/readme", variable=self.require_license_var).grid(row=0, column=2, sticky="w")
        ttk.Checkbutton(options, text="Strict", variable=self.strict_var).grid(row=0, column=3, sticky="w")

        actions = ttk.Frame(self, padding=(14, 0, 14, 8))
        actions.grid(row=3, column=0, sticky="nsew")
        actions.columnconfigure(0, weight=1)
        actions.rowconfigure(1, weight=1)
        button_bar = ttk.Frame(actions)
        button_bar.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        self.convert_button = ttk.Button(button_bar, text="Convert", command=lambda: self._start_conversion(False))
        self.convert_button.pack(side="left")
        self.dry_run_button = ttk.Button(button_bar, text="Dry Run", command=lambda: self._start_conversion(True))
        self.dry_run_button.pack(side="left", padx=(8, 0))
        ttk.Button(button_bar, text="Open Output", command=lambda: self._open_path(Path(self.output_var.get()))).pack(
            side="left", padx=(8, 0)
        )
        ttk.Label(button_bar, textvariable=self.status_var).pack(side="right")

        self.log = ScrolledText(actions, height=14, wrap="word", font=("Consolas", 10))
        self.log.grid(row=1, column=0, sticky="nsew")
        self._append_log("Default output:\n" + str(self.output_var.get()) + "\n")

    def _choose_input_file(self) -> None:
        path = filedialog.askopenfilename(
            title="Choose source texture pack zip",
            filetypes=[("Zip files", "*.zip"), ("All files", "*.*")],
        )
        if path:
            self.input_var.set(path)
            self._use_input_name()

    def _choose_input_folder(self) -> None:
        path = filedialog.askdirectory(title="Choose source texture pack folder")
        if path:
            self.input_var.set(path)
            self._use_input_name()

    def _choose_output_folder(self) -> None:
        path = filedialog.askdirectory(title="Choose output texture pack folder")
        if path:
            self.output_is_auto = False
            self.output_var.set(path)

    def _choose_custom_blocks(self) -> None:
        paths = filedialog.askopenfilenames(
            title="Choose custom block mapping JSON",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if paths:
            self.custom_blocks_var.set(";".join(paths))

    def _refresh_texture_root(self) -> None:
        self.texture_root = find_current_build_texture_root()
        self.texture_root_var.set(str(self.texture_root))
        if self.output_is_auto:
            self._set_auto_output()

    def _use_input_name(self) -> None:
        value = self.input_var.get().strip()
        if not value:
            return
        self.pack_name_var.set(safe_pack_name(value))
        if self.output_is_auto:
            self._set_auto_output()

    def _pack_name_changed(self) -> None:
        if self.output_is_auto:
            self._set_auto_output()

    def _output_changed(self) -> None:
        if not self._setting_output:
            self.output_is_auto = False

    def _set_auto_output(self) -> None:
        self.output_is_auto = True
        self._setting_output = True
        try:
            self.output_var.set(str(default_output_for_pack(self.pack_name_var.get(), self.texture_root)))
        finally:
            self._setting_output = False

    def _custom_block_paths(self) -> list[Path]:
        raw = self.custom_blocks_var.get().strip()
        if not raw:
            return []
        return [Path(item.strip()) for item in raw.split(";") if item.strip()]

    def _validate(self) -> tuple[Path, Path, str] | None:
        source_text = self.input_var.get().strip()
        output_text = self.output_var.get().strip()
        source = Path(source_text)
        output = Path(output_text)
        pack_name = safe_pack_name(self.pack_name_var.get())
        if not source_text:
            messagebox.showerror("Missing source pack", "Choose a source pack zip or folder first.")
            return None
        if not source.exists():
            messagebox.showerror("Missing source pack", "Choose a source pack zip or folder first.")
            return None
        if not output_text:
            messagebox.showerror("Missing output folder", "Choose an output folder first.")
            return None
        for path in self._custom_block_paths():
            if not path.is_file():
                messagebox.showerror("Missing custom blocks file", f"Custom block mapping not found:\n{path}")
                return None
        if not CONVERTER.is_file():
            messagebox.showerror("Converter missing", f"Cannot find converter:\n{CONVERTER}")
            return None
        self.pack_name_var.set(pack_name)
        return source, output, pack_name

    def _build_command(self, dry_run: bool) -> list[str] | None:
        validated = self._validate()
        if not validated:
            return None
        source, output, pack_name = validated
        command = [
            sys.executable,
            str(CONVERTER),
            "--input",
            str(source),
            "--output",
            str(output),
            "--pack-name",
            pack_name,
            "--game-dir",
            str(GAME_DIR),
        ]
        if self.copy_unmapped_var.get():
            command.append("--copy-unmapped")
        if self.overwrite_var.get():
            command.append("--overwrite")
        if self.require_license_var.get():
            command.append("--require-license")
        if self.strict_var.get():
            command.append("--strict")
        if dry_run:
            command.append("--dry-run")
        for path in self._custom_block_paths():
            command.extend(["--custom-blocks", str(path)])
        return command

    def _start_conversion(self, dry_run: bool) -> None:
        if self.worker and self.worker.is_alive():
            return
        command = self._build_command(dry_run)
        if not command:
            return
        self.log.delete("1.0", tk.END)
        self._append_log(("Dry run" if dry_run else "Converting") + "...\n")
        self._append_log(" ".join(f'"{part}"' if " " in part else part for part in command) + "\n\n")
        self._set_busy(True, "Running")
        self.worker = threading.Thread(target=self._run_command, args=(command,), daemon=True)
        self.worker.start()

    def _run_command(self, command: list[str]) -> None:
        flags = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0
        try:
            process = subprocess.Popen(
                command,
                cwd=str(REPO_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                creationflags=flags,
            )
            assert process.stdout is not None
            for line in process.stdout:
                self.messages.put(("log", line))
            return_code = process.wait()
            self.messages.put(("done", return_code))
        except Exception as exc:
            self.messages.put(("log", f"Failed to run converter: {exc}\n"))
            self.messages.put(("done", 1))

    def _drain_messages(self) -> None:
        try:
            while True:
                kind, value = self.messages.get_nowait()
                if kind == "log":
                    self._append_log(str(value))
                elif kind == "done":
                    code = int(value)
                    if code == 0:
                        self._append_log("\nFinished successfully.\n")
                        self.status_var.set("Finished")
                    else:
                        self._append_log(f"\nConverter failed with exit code {code}.\n")
                        self.status_var.set("Failed")
                    self._set_busy(False, self.status_var.get())
        except queue.Empty:
            pass
        self.after(100, self._drain_messages)

    def _set_busy(self, busy: bool, status: str) -> None:
        state = "disabled" if busy else "normal"
        self.convert_button.configure(state=state)
        self.dry_run_button.configure(state=state)
        self.status_var.set(status)

    def _append_log(self, text: str) -> None:
        self.log.insert(tk.END, text)
        self.log.see(tk.END)

    def _open_path(self, path: Path) -> None:
        target = path if path.exists() else path.parent
        if not target.exists():
            messagebox.showinfo("Folder not found", f"Folder does not exist yet:\n{path}")
            return
        os.startfile(target)


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--print-defaults" in argv:
        texture_root = find_current_build_texture_root()
        print(f"texture_root={texture_root}")
        print(f"output={default_output_for_pack(DEFAULT_PACK_NAME, texture_root)}")
        return 0
    app = PackConverterApp()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
