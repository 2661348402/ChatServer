#ifndef THEME_H_
#define THEME_H_

#include <QString>

/// Central color palette for the ChatClient GUI.
/// Call Theme::setDarkMode(true) to switch to dark palette at runtime.
/// All color accessors are static methods returning QString.
class Theme
{
public:
    enum Mode { Light, Dark };

    // ---- Mode control ----
    static Mode mode()             { return s_mode; }
    static void setMode(Mode m)    { s_mode = m; }
    static void setDarkMode(bool on) { s_mode = on ? Dark : Light; }
    static void toggle()           { s_mode = (s_mode == Light) ? Dark : Light; }
    static bool isDark()           { return s_mode == Dark; }

    // ---- Brand / Accent ----
    static QString green()         { return isDark() ? darkGreen         : "#07C160"; }
    static QString greenHover()    { return isDark() ? darkGreenHover    : "#06AD56"; }
    static QString greenPressed()  { return isDark() ? darkGreenPressed  : "#05944A"; }

    // ---- Backgrounds ----
    static QString bgDialog()      { return isDark() ? darkBgDialog      : "#F0F0F0"; }
    static QString bgChat()        { return isDark() ? darkBgChat        : "#F5F5F5"; }
    static QString bgInput()       { return isDark() ? darkBgInput       : "#FAFAFA"; }
    static QString bgCard()        { return isDark() ? darkBgCard        : "#FFFFFF"; }
    static QString bgTopBar()      { return isDark() ? darkBgTopBar      : "#F0F0F0"; }
    static QString bgHeader()      { return isDark() ? darkBgHeader      : "#F5F5F5"; }

    // ---- Borders ----
    static QString borderCard()    { return isDark() ? darkBorderCard    : "#E0E0E0"; }
    static QString borderInput()   { return isDark() ? darkBorderInput   : "#D0D0D0"; }
    static QString borderChat()    { return isDark() ? darkBorderChat    : "#D0D0D0"; }
    static QString borderBubble()  { return isDark() ? darkBorderBubble  : "#E8E8E8"; }
    static QString borderItem()    { return isDark() ? darkBorderItem    : "#EEE"; }

    // ---- Text ----
    static QString textPrimary()   { return isDark() ? darkTextPrimary   : "#333"; }
    static QString textSecondary() { return isDark() ? darkTextSecondary : "#666"; }
    static QString textMuted()     { return isDark() ? darkTextMuted     : "#999"; }
    static QString textLight()     { return isDark() ? darkTextLight     : "#B0B0B0"; }
    static QString textError()     { return isDark() ? darkTextError     : "#E53935"; }
    static QString textLink()      { return isDark() ? darkTextLink      : "#576B95"; }

    // ---- Bubbles ----
    static QString bubbleSelf()     { return isDark() ? darkBubbleSelf     : "#95EC69"; }
    static QString bubbleOther()    { return isDark() ? darkBubbleOther    : "#FFFFFF"; }
    static QString bubbleSelfName() { return isDark() ? darkBubbleSelfName : "#5A8A3C"; }
    static QString bubbleSelfTime() { return isDark() ? darkBubbleSelfTime : "#7CB342"; }
    static QString bubbleOtherTime(){ return isDark() ? darkBubbleOtherTime: "#B0B0B0"; }

    // ---- Buttons ----
    static QString btnDisabled()        { return isDark() ? darkBtnDisabled        : "#BDBDBD"; }
    static QString btnSecondary()       { return isDark() ? darkBtnSecondary       : "#E0E0E0"; }
    static QString btnSecondaryHover()  { return isDark() ? darkBtnSecondaryHover  : "#D0D0D0"; }
    static QString btnSecondaryPressed(){ return isDark() ? darkBtnSecondaryPressed: "#C0C0C0"; }

    // ---- Misc ----
    static QString scrollHandle()       { return isDark() ? darkScrollHandle       : "#C0C0C0"; }
    static QString scrollHandleHover()  { return isDark() ? darkScrollHandleHover  : "#A0A0A0"; }
    static QString groupHeaderBg()      { return isDark() ? darkGroupHeaderBg      : "#E8F5E9"; }
    static QString hoverBg()            { return isDark() ? darkHoverBg            : "#E8F5E9"; }
    static QString selectedBg()         { return isDark() ? darkSelectedBg         : "#C8E6C9"; }
    static QString plusBtnBg()          { return isDark() ? darkPlusBtnBg          : "#E0E0E0"; }
    static QString plusBtnHover()       { return isDark() ? darkPlusBtnHover       : "#C8C8C8"; }
    static QString plusBtnPressed()     { return isDark() ? darkPlusBtnPressed     : "#B0B0B0"; }

private:
    inline static Mode s_mode = Light;

    // ---- Dark palette ----
    static constexpr auto darkGreen         = "#06C160";
    static constexpr auto darkGreenHover    = "#05AA52";
    static constexpr auto darkGreenPressed  = "#048F45";

    static constexpr auto darkBgDialog      = "#1E1E1E";
    static constexpr auto darkBgChat        = "#2B2B2B";
    static constexpr auto darkBgInput       = "#3C3C3C";
    static constexpr auto darkBgCard        = "#333333";
    static constexpr auto darkBgTopBar      = "#2D2D2D";
    static constexpr auto darkBgHeader      = "#252525";

    static constexpr auto darkBorderCard    = "#444";
    static constexpr auto darkBorderInput   = "#555";
    static constexpr auto darkBorderChat    = "#444";
    static constexpr auto darkBorderBubble  = "#444";
    static constexpr auto darkBorderItem    = "#3A3A3A";

    static constexpr auto darkTextPrimary   = "#E0E0E0";
    static constexpr auto darkTextSecondary = "#B0B0B0";
    static constexpr auto darkTextMuted     = "#808080";
    static constexpr auto darkTextLight     = "#606060";
    static constexpr auto darkTextError     = "#FF6B6B";
    static constexpr auto darkTextLink      = "#7BA5D6";

    static constexpr auto darkBubbleSelf     = "#2D8C3C";
    static constexpr auto darkBubbleOther    = "#424242";
    static constexpr auto darkBubbleSelfName = "#6ABF4B";
    static constexpr auto darkBubbleSelfTime = "#5A9E3F";
    static constexpr auto darkBubbleOtherTime= "#808080";

    static constexpr auto darkBtnDisabled        = "#555";
    static constexpr auto darkBtnSecondary       = "#444";
    static constexpr auto darkBtnSecondaryHover  = "#555";
    static constexpr auto darkBtnSecondaryPressed= "#666";

    static constexpr auto darkScrollHandle       = "#555";
    static constexpr auto darkScrollHandleHover  = "#777";
    static constexpr auto darkGroupHeaderBg      = "#1B3D1E";
    static constexpr auto darkHoverBg            = "#3A3A3A";
    static constexpr auto darkSelectedBg         = "#2D4A2F";
    static constexpr auto darkPlusBtnBg          = "#444";
    static constexpr auto darkPlusBtnHover       = "#555";
    static constexpr auto darkPlusBtnPressed     = "#666";
};

#endif // THEME_H_
