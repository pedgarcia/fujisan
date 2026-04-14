/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "macosfullscreen.h"
#include <QWidget>
#include <Cocoa/Cocoa.h>

void fujisanConfigureMacMainWindowNoNativeFullscreen(QWidget *widget)
{
    if (!widget) {
        return;
    }
    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    if (!view) {
        return;
    }
    NSWindow *window = [view window];
    if (!window) {
        return;
    }
    NSWindowCollectionBehavior b = [window collectionBehavior];
    b &= ~(NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorFullScreenAuxiliary);
    b |= NSWindowCollectionBehaviorFullScreenNone;
    [window setCollectionBehavior:b];
}
