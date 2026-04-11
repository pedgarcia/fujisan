# FastBasic debugging in VS Code with Fujisan

This guide walks through using the **[FastBasic Debugger](https://marketplace.visualstudio.com/items?itemName=ericcarr.fastbasic-debugger)** VS Code extension with **Fujisan** as the emulator. Fujisan is not downloaded automatically by the extension (unlike Altirra / Atari800MacX), so you install it yourself and connect over Fujisan’s TCP server.

**Requirements**

- **Fujisan 1.1.4 or newer** — [Releases](https://github.com/pedgarcia/fujisan/releases)
- **FastBasic cross-compiler** — [Releases](https://github.com/dmsc/fastbasic/releases/latest) (pick the archive for your OS)
- **Visual Studio Code** (or a VS Code–compatible editor)
- Extension: **FastBasic Debugger** (publisher: search the Marketplace for “FastBasic Debugger”)

For TCP details used by the extension, see [TCP_SERVER_API.md](TCP_SERVER_API.md).

---

## Step 1 — Install Fujisan

Install a build for your platform from the [Fujisan releases](https://github.com/pedgarcia/fujisan/releases) page.

| Platform | Typical install |
|----------|-----------------|
| **macOS** | Download the **arm64** DMG for Apple Silicon or **x86_64** for Intel Macs. Open the `.dmg`, drag **Fujisan** to **Applications**, or run it from the mounted volume. Gatekeeper: if macOS blocks the app, use **System Settings → Privacy & Security** to allow it, or right‑click → **Open** the first time. |
| **Windows** | Unzip **Fujisan-…-windows.zip**. Run **Fujisan.exe** from the extracted folder (or a shortcut you create). No special steps beyond normal Windows executables. |
| **Linux** | Install the `.deb` (Debian/Ubuntu) with your package manager, or unpack the `.tar.gz` and run the **Fujisan** binary from a terminal. Ensure Qt dependencies match your distro (the release notes usually list what is needed). **ARM64** vs **x86_64**: download the asset that matches your CPU. |

**Before debugging:** start Fujisan, then enable the TCP server: **Tools → TCP Server**. The default listen address is **localhost** and the default port is **6502** (shown in the status bar when the server is on). The debugger expects Fujisan to already be running with the server enabled; it does not launch Fujisan for you.

---

## Step 2 — Install the FastBasic cross-compiler

1. Open [FastBasic releases (latest)](https://github.com/dmsc/fastbasic/releases/latest).
2. Download the **cross-compiler** archive for your OS (names vary by release; look for Windows, Linux, or macOS/darwin builds).
3. Extract the archive to a folder you will keep (for example `C:\atari\fastbasic-4.7HF` on Windows, or `~/atari/fastbasic-4.7HF` on Mac/Linux).

You need the directory that contains the compiler executable:

- **Windows:** `fastbasic.exe`
- **macOS / Linux:** `fastbasic` (executable bit set)

The extension accepts **`compilerPath` as that folder**; it resolves `fastbasic` / `fastbasic.exe` inside it.

**Permissions (macOS / Linux):** if the binary is not executable, run `chmod +x fastbasic` in that folder.

---

## Step 3 — Install the VS Code extension

1. In VS Code, open **Extensions** (`Ctrl+Shift+X` / `Cmd+Shift+X`).
2. Search for **FastBasic Debugger** and install it.

Alternatively install a `.vsix` from the extension’s project/releases if you use a custom build.

---

## Step 4 — Create `launch.json`

1. Open your FastBasic project folder in VS Code.
2. Open **Run and Debug** (`Ctrl+Shift+D` / `Cmd+Shift+D`).
3. Click **create a launch.json file** (or **Add Configuration**) and choose **FastBasic** / **FastBasic Debug: Fujisan** if offered, or paste the configuration below into `.vscode/launch.json`.

Minimal **Fujisan** configuration:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "fastbasic",
      "request": "launch",
      "name": "Debug FastBASIC (Fujisan)",
      "sourceFile": "${file}",
      "compilerPath": "/ABSOLUTE/PATH/TO/fastbasic-folder",
      "emulatorType": "fujisan",
      "fujisanHost": "localhost",
      "fujisanPort": 6502
    }
  ]
}
```

Replace `compilerPath` with the **absolute** path to the extracted FastBasic folder (the one that contains `fastbasic` or `fastbasic.exe`).

### Platform notes for `compilerPath`

| Platform | Example |
|----------|---------|
| **Windows** | `C:\\atari\\fastbasic-4.7HF` — use doubled backslashes in JSON, or forward slashes: `C:/atari/fastbasic-4.7HF` |
| **macOS** | `/Users/you/atari/fastbasic-4.7HF` |
| **Linux** | `/home/you/atari/fastbasic-4.7HF` |

### Optional Fujisan-related fields

| Property | Purpose |
|----------|---------|
| `autoConfigureH4` | Default `true`. When true, the extension sets Fujisan’s **H4:** host mapping to your project’s `bin` folder over TCP so the debug runtime can exchange files. Set to `false` if you map **H4:** yourself in Fujisan. |
| `fujisanBootMode` | `"none"` (default), `"warm"`, or `"cold"`. Controls reset before loading the XEX: no extra boot; warm boot; or FujiNet-oriented cold restart sequence. Use `"warm"` or `"cold"` when you need a cleaner machine or FujiNet state. |
| `fujisanHost` / `fujisanPort` | Override if the TCP server is not on `localhost:6502`. |


### Optional VS Code settings

In **Settings**, search for **FastBasic**:

- **`fastbasic.defaultEmulator`** — set to `fujisan` so **Debug File** / **Run File** use Fujisan when no launch name is fixed.
- **`fastbasic.defaultLaunchConfiguration`** — set to the `"name"` of your Fujisan config (e.g. `Debug FastBASIC (Fujisan)`) so editor toolbar actions match the Run view.

---

## Step 5 — Debug

1. Start **Fujisan** and turn on **Tools → TCP Server**. (This is enabled by default, so you don't need to do anything unless you turned that off earlier)
2. Open a `.bas` file.
3. Press **F5** (debug) or **Ctrl+F5** / **Cmd+F5** (run without debugging), or use the configuration from the Run view.

The extension compiles to an XEX under a **`bin`** folder next to your source, connects to Fujisan, and loads the program. If compile fails, check the **Output** pane for FastBasic messages.

---

## Troubleshooting (short)

- **Connection errors** — Confirm Fujisan is running and **TCP Server** is enabled; firewall must allow **localhost** (rarely an issue on the same machine).
- **Wrong compiler** — `compilerPath` must be the folder containing `fastbasic` / `fastbasic.exe`, not a parent directory.
- **H4: issues** — If you disabled `autoConfigureH4`, map **H4:** manually to the project’s `bin` directory as you would for Altirra/Atari800.
- **Extension limitations** — The debugger uses IOCB channels **#4** and **#5**; your program should use other channels for normal I/O. See the extension’s readme for breakpoint and stepping notes.

For Fujisan-specific behavior and API commands, see [TCP_SERVER_API.md](TCP_SERVER_API.md) and [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
