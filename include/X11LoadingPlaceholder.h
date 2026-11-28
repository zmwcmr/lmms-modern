/*
 * X11LoadingPlaceholder.h - placeholder for plugin X11 windows
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_X11_LOADING_PLACEHOLDER_H
#define LMMS_X11_LOADING_PLACEHOLDER_H

namespace lmms
{

//! Paint a dark LMMS logo and a loading hint into an unmapped X11 window.
//! The plugin UI paints over it when it is ready.
void paintLoadingPlaceholder(void* display, unsigned long window);

} // namespace lmms

#endif // LMMS_X11_LOADING_PLACEHOLDER_H
