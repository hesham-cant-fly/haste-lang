/**
 * @file my_termcolor.h
 * @brief ANSI terminal escape code macros for colored output.
 *
 * Each macro expands to a string literal that you can embed directly
 * in `printf` / `print` format strings. Always pair with
 * `ANSI_CODE_RESET` to avoid leaking styles.
 */

#ifndef MY_TERMCOLOR_H_
#define MY_TERMCOLOR_H_

/** @def ANSI_CODE_RESET
 *  @brief Reset all attributes back to default. */
#define ANSI_CODE_RESET "\033[00m"
/** @def ANSI_CODE_BOLD
 *  @brief Bold / bright foreground. */
#define ANSI_CODE_BOLD "\033[1m"
/** @def ANSI_CODE_DARK
 *  @brief Dim / dark foreground. */
#define ANSI_CODE_DARK "\033[2m"
/** @def ANSI_CODE_UNDERLINE
 *  @brief Underline text. */
#define ANSI_CODE_UNDERLINE "\033[4m"
/** @def ANSI_CODE_BLINK
 *  @brief Blinking text. */
#define ANSI_CODE_BLINK "\033[5m"
/** @def ANSI_CODE_REVERSE
 *  @brief Reverse video (swap fg/bg). */
#define ANSI_CODE_REVERSE "\033[7m"
/** @def ANSI_CODE_CONCEALED
 *  @brief Concealed / hidden text. */
#define ANSI_CODE_CONCEALED "\033[8m"
/** @def ANSI_CODE_GRAY
 *  @brief Gray foreground. */
#define ANSI_CODE_GRAY "\033[30m"
/** @def ANSI_CODE_GREY
 *  @brief Grey foreground (alias for GRAY). */
#define ANSI_CODE_GREY "\033[30m"
/** @def ANSI_CODE_RED
 *  @brief Red foreground. */
#define ANSI_CODE_RED "\033[31m"
/** @def ANSI_CODE_GREEN
 *  @brief Green foreground. */
#define ANSI_CODE_GREEN "\033[32m"
/** @def ANSI_CODE_YELLOW
 *  @brief Yellow foreground. */
#define ANSI_CODE_YELLOW "\033[33m"
/** @def ANSI_CODE_BLUE
 *  @brief Blue foreground. */
#define ANSI_CODE_BLUE "\033[34m"
/** @def ANSI_CODE_MAGENTA
 *  @brief Magenta foreground. */
#define ANSI_CODE_MAGENTA "\033[35m"
/** @def ANSI_CODE_CYAN
 *  @brief Cyan foreground. */
#define ANSI_CODE_CYAN "\033[36m"
/** @def ANSI_CODE_WHITE
 *  @brief White foreground. */
#define ANSI_CODE_WHITE "\033[37m"
/** @def ANSI_CODE_BG_GRAY
 *  @brief Gray background. */
#define ANSI_CODE_BG_GRAY "\033[40m"
/** @def ANSI_CODE_BG_GREY
 *  @brief Grey background (alias for BG_GRAY). */
#define ANSI_CODE_BG_GREY "\033[40m"
/** @def ANSI_CODE_BG_RED
 *  @brief Red background. */
#define ANSI_CODE_BG_RED "\033[41m"
/** @def ANSI_CODE_BG_GREEN
 *  @brief Green background. */
#define ANSI_CODE_BG_GREEN "\033[42m"
/** @def ANSI_CODE_BG_YELLOW
 *  @brief Yellow background. */
#define ANSI_CODE_BG_YELLOW "\033[43m"
/** @def ANSI_CODE_BG_BLUE
 *  @brief Blue background. */
#define ANSI_CODE_BG_BLUE "\033[44m"
/** @def ANSI_CODE_BG_MAGENTA
 *  @brief Magenta background. */
#define ANSI_CODE_BG_MAGENTA "\033[45m"
/** @def ANSI_CODE_BG_CYAN
 *  @brief Cyan background. */
#define ANSI_CODE_BG_CYAN "\033[46m"
/** @def ANSI_CODE_BG_WHITE
 *  @brief White background. */
#define ANSI_CODE_BG_WHITE "\033[47m"

#endif // MY_TERMCOLOR_H_
