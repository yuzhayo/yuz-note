#include "viewer/ViewerTheme.h"

namespace YuzNote {
namespace Theme {

QString DefaultStyleSheet()
{
    return QStringLiteral(
        "h1 { color: #ffffff; font-size: x-large; font-weight: bold; }\n"
        "h2 { color: #ffffff; font-size: large; font-weight: bold; }\n"
        "h3 { color: #ffffff; font-size: medium; font-weight: bold; }\n"
        "p, li, td, th { color: #e8e8e8; }\n"
        "a { color: #6ab2f2; }\n"
        "pre { font-family: Consolas, monospace; background-color: #0e1621; "
        "color: #d6deeb; }\n"
        "code { font-family: Consolas, monospace; }\n"
        "blockquote { color: #8fa3bf; }\n"
        "th { background-color: #242f3d; }\n");
}

} // namespace Theme
} // namespace YuzNote
