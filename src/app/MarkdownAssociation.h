#pragma once

// Called ONLY by Velopack lifecycle hooks (pre-QApplication, 30 s critical window).
// Raw WinAPI, no Qt, no UI, best-effort (never throws, never blocks).
// Signature must stay `void(void*, const char*)` — see vpkc_hook_callback_t.
namespace YuzNote {

void RegisterMarkdownAssociation(void *, const char *);
void UnregisterMarkdownAssociation(void *, const char *);

} // namespace YuzNote
