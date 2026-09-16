#pragma once

#include <QString>

namespace YuzNote {
namespace Theme {

// Dark, Telegram-flavored approximations (exact Telegram hexes unknowable
// offline; tuned at the P3 visual gate). Page background goes through the
// widget palette; text colors through DefaultStyleSheet().
inline constexpr char kPageBackground[] = "#17212b";
inline constexpr char kText[] = "#e8e8e8";
inline constexpr char kHeading[] = "#ffffff";
inline constexpr char kLink[] = "#6ab2f2";
inline constexpr char kMuted[] = "#7d8590";
inline constexpr char kCodeForeground[] = "#d6deeb";
inline constexpr char kCodeBackground[] = "#0e1621";
inline constexpr char kQuote[] = "#8fa3bf";
inline constexpr char kTableHeader[] = "#242f3d";

QString DefaultStyleSheet();

} // namespace Theme
} // namespace YuzNote
