/*
 * Vst3Protocol.h - constants shared by the scanner and Vst3List
 *
 * Copyright (c) 2026 LMMS developers
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_VST3_PROTOCOL_H
#define LMMS_VST3_PROTOCOL_H

namespace lmms::vst3
{

//! Bump when the scanner JSON schema or scan policy changes.
inline constexpr int scannerProtocolVersion = 1;

//! Bump when the on-disk cache layout changes.
inline constexpr int cacheVersion = 1;

} // namespace lmms::vst3

#endif // LMMS_VST3_PROTOCOL_H
