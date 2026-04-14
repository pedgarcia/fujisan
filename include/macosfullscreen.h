/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef MACOSFULLSCREEN_H
#define MACOSFULLSCREEN_H

class QWidget;

// On macOS, disable the system "Enter Full Screen" Window menu item and zoom-titlebar
// full-screen for the main window. Fujisan uses View > Fullscreen (Cmd+Return) with a
// custom fullscreen window instead. No-op on other platforms (see stub .cpp).
void fujisanConfigureMacMainWindowNoNativeFullscreen(QWidget *mainWindow);

#endif
