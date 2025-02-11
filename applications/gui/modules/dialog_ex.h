#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Dialog anonymous structure */
typedef struct DialogEx DialogEx;

/** DialogEx result */
typedef enum {
    DialogExResultLeft,
    DialogExResultCenter,
    DialogExResultRight,
} DialogExResult;

/** DialogEx result callback type
 * @warning comes from GUI thread
 */
typedef void (*DialogExResultCallback)(DialogExResult result, void* context);

/** Allocate and initialize dialog
 * This dialog used to ask simple questions
 * @return DialogEx instance
 */
DialogEx* dialog_ex_alloc();

/** Deinitialize and free dialog
 * @param dialog - DialogEx instance
 */
void dialog_ex_free(DialogEx* dialog_ex);

/** Get dialog view
 * @param dialog - DialogEx instance
 * @return View instance that can be used for embedding
 */
View* dialog_ex_get_view(DialogEx* dialog_ex);

/** Set dialog result callback
 * @param dialog_ex - DialogEx instance
 * @param callback - result callback function
 */
void dialog_ex_set_result_callback(DialogEx* dialog_ex, DialogExResultCallback callback);

/** Set dialog context
 * @param dialog_ex - DialogEx instance
 * @param context - context pointer, will be passed to result callback
 */
void dialog_ex_set_context(DialogEx* dialog_ex, void* context);

/** Set dialog header text
 * If text is null, dialog header will not be rendered
 * @param dialog - DialogEx instance
 * @param text - text to be shown, can be multiline
 * @param x, y - text position
 * @param horizontal, vertical - text aligment
 */
void dialog_ex_set_header(
    DialogEx* dialog_ex,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);

/** Set dialog text
 * If text is null, dialog text will not be rendered
 * @param dialog - DialogEx instance
 * @param text - text to be shown, can be multiline
 * @param x, y - text position
 * @param horizontal, vertical - text aligment
 */
void dialog_ex_set_text(
    DialogEx* dialog_ex,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);

/** Set dialog icon
 * If x or y is negative, dialog icon will not be rendered
 * @param dialog - DialogEx instance
 * @param x, y - icon position
 * @param name - icon to be shown
 */
void dialog_ex_set_icon(DialogEx* dialog_ex, uint8_t x, uint8_t y, const Icon* icon);

/** Set left button text
 * If text is null, left button will not be rendered and processed
 * @param dialog - DialogEx instance
 * @param text - text to be shown
 */
void dialog_ex_set_left_button_text(DialogEx* dialog_ex, const char* text);

/** Set center button text
 * If text is null, center button will not be rendered and processed
 * @param dialog - DialogEx instance
 * @param text - text to be shown
 */
void dialog_ex_set_center_button_text(DialogEx* dialog_ex, const char* text);

/** Set right button text
 * If text is null, right button will not be rendered and processed
 * @param dialog - DialogEx instance
 * @param text - text to be shown
 */
void dialog_ex_set_right_button_text(DialogEx* dialog_ex, const char* text);

/** Clean dialog
 * @param dialog_ex DialogEx instance
 */
void dialog_ex_clean(DialogEx* dialog_ex);

#ifdef __cplusplus
}
#endif