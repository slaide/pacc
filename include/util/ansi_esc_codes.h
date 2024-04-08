// ANSI escape codes, see https://en.wikipedia.org/wiki/ANSI_escape_code
/// ANSI escape code - ESC - escape
#define ESC "\033"
/// ANSI escape code - CSI - control sequence introducer
#define CSI ESC "["
/// ANSI escape code - OSC - operating system command
#define OSC ESC "]"
/// ANSI escape code - ST - string terminator
#define ST ESC "\\"

// CSI n 'm' - Select graphic rendition (SGR) parameters
// n=0 - reset
// n=1 - bold
// n=30-37 - foreground color
// n=40-47 - background color
// and tons more, see https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_parameters
// can be combined, e.g. CSI "1;31m" - bold red text
#define SGR_RESET "0"

#define SGR_BOLD "1"
/// opposite of bold
#define SGR_FAINT "2"
#define SGR_NORMAL_WEIGHT "22"

#define SGR_ITALIC "3"

#define SGR_UNDERLINED "4"
#define SGR_DOUBLE_UNDERLINED "21"
#define SGR_NOT_UNDERLINED "24"

#define SGR_OVERLINED "53"
#define SGR_NOT_OVERLINED "53"

#define SGR_SLOW_BLINK "5"
#define SGR_FAST_BLINK "6"

/// swap foreground and background colors
#define SGR_REVERSE "7"

#define SGR_CONCEAL "8"

#define SGR_CROSSED_OUT "9"

#define SGR_FOREGROUND_BLACK "30"
#define SGR_FOREGROUND_RED "31"
#define SGR_FOREGROUND_GREEN "32"
#define SGR_FOREGROUND_YELLOW "33"
#define SGR_FOREGROUND_BLUE "34"
#define SGR_FOREGROUND_MAGENTA "35"
#define SGR_FOREGROUND_CYAN "36"
#define SGR_FOREGROUND_WHITE "37"

#define TEXT_COLOR_BLACK   CSI SGR_BOLD ";" SGR_FOREGROUND_BLACK "m"
#define TEXT_COLOR_RED     CSI SGR_BOLD ";" SGR_FOREGROUND_RED "m"
#define TEXT_COLOR_GREEN   CSI SGR_BOLD ";" SGR_FOREGROUND_GREEN "m"
#define TEXT_COLOR_YELLOW  CSI SGR_BOLD ";" SGR_FOREGROUND_YELLOW "m"
#define TEXT_COLOR_BLUE    CSI SGR_BOLD ";" SGR_FOREGROUND_BLUE "m"
#define TEXT_COLOR_MAGENTA CSI SGR_BOLD ";" SGR_FOREGROUND_MAGENTA "m"
#define TEXT_COLOR_CYAN    CSI SGR_BOLD ";" SGR_FOREGROUND_CYAN "m"
#define TEXT_COLOR_WHITE   CSI SGR_BOLD ";" SGR_FOREGROUND_WHITE "m"
#define TEXT_COLOR_RESET   CSI SGR_RESET "m"
