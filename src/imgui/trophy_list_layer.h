// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace ImGui::Trophies {

void Register();
void Unregister();

void Toggle();
void Open();
bool IsOpen();

} // namespace ImGui::Trophies
