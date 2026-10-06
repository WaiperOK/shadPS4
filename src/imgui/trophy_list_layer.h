// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace ImGui::Trophies {

// Add/remove the trophy list overlay layer. Call Register() once after ImGui is initialized.
void Register();
void Unregister();

// Show/hide the trophy list window. Toggle() is bound to a hotkey, Open() is used when the
// title asks for its trophy list (sceNpTrophyShowTrophyList).
void Toggle();
void Open();
bool IsOpen();

} // namespace ImGui::Trophies
