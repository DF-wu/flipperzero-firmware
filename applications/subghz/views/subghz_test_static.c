#include "subghz_test_static.h"
#include "../subghz_i.h"

#include <math.h>
#include <furi.h>
#include <furi-hal.h>
#include <input/input.h>
#include <notification/notification-messages.h>
#include <lib/subghz/protocols/subghz_protocol_princeton.h>

static const uint32_t subghz_test_static_keys[] = {
    0x0074BADE,
    0x0074BADD,
    0x0074BADB,
    0x00E34A4E,
};

struct SubghzTestStatic {
    View* view;
    SubGhzEncoderPrinceton* encoder;
};

typedef enum {
    SubghzTestStaticStatusRx,
    SubghzTestStaticStatusTx,
} SubghzTestStaticStatus;

typedef struct {
    uint8_t frequency;
    uint32_t real_frequency;
    uint8_t button;
} SubghzTestStaticModel;

void subghz_test_static_draw(Canvas* canvas, SubghzTestStaticModel* model) {
    char buffer[64];

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "CC1101 Static");

    canvas_set_font(canvas, FontSecondary);
    // Frequency
    snprintf(
        buffer,
        sizeof(buffer),
        "Freq: %03ld.%03ld.%03ld Hz",
        model->real_frequency / 1000000 % 1000,
        model->real_frequency / 1000 % 1000,
        model->real_frequency % 1000);
    canvas_draw_str(canvas, 0, 20, buffer);
    snprintf(buffer, sizeof(buffer), "Key: %d", model->button);
    canvas_draw_str(canvas, 0, 31, buffer);
}

bool subghz_test_static_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubghzTestStatic* instance = context;

    if(event->key == InputKeyBack) {
        return false;
    }

    with_view_model(
        instance->view, (SubghzTestStaticModel * model) {
            if(event->type == InputTypeShort) {
                if(event->key == InputKeyLeft) {
                    if(model->frequency > 0) model->frequency--;
                } else if(event->key == InputKeyRight) {
                    if(model->frequency < subghz_frequencies_count - 1) model->frequency++;
                } else if(event->key == InputKeyDown) {
                    if(model->button > 0) model->button--;
                } else if(event->key == InputKeyUp) {
                    if(model->button < 3) model->button++;
                }
            }

            model->real_frequency = subghz_frequencies[model->frequency];

            if(event->key == InputKeyOk) {
                NotificationApp* notification = furi_record_open("notification");
                if(event->type == InputTypePress) {
                    notification_message_block(notification, &sequence_set_red_255);

                    FURI_LOG_I("SubghzTestStatic", "TX Start");
                    furi_hal_subghz_idle();
                    furi_hal_subghz_set_frequency_and_path(subghz_frequencies[model->frequency]);

                    subghz_encoder_princeton_set(
                        instance->encoder, subghz_test_static_keys[model->button], 10000);

                    furi_hal_subghz_start_async_tx(
                        subghz_encoder_princeton_yield, instance->encoder);
                } else if(event->type == InputTypeRelease) {
                    FURI_LOG_I("SubghzTestStatic", "TX Stop");
                    furi_hal_subghz_stop_async_tx();

                    notification_message(notification, &sequence_reset_red);
                }
                furi_record_close("notification");
            }

            return true;
        });

    return true;
}

void subghz_test_static_enter(void* context) {
    furi_assert(context);
    SubghzTestStatic* instance = context;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);

    hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    hal_gpio_write(&gpio_cc1101_g0, false);

    with_view_model(
        instance->view, (SubghzTestStaticModel * model) {
            model->frequency = subghz_frequencies_433_92;
            model->real_frequency = subghz_frequencies[model->frequency];
            model->button = 0;
            return true;
        });
}

void subghz_test_static_exit(void* context) {
    furi_assert(context);
    furi_hal_subghz_sleep();
}

SubghzTestStatic* subghz_test_static_alloc() {
    SubghzTestStatic* instance = furi_alloc(sizeof(SubghzTestStatic));

    // View allocation and configuration
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubghzTestStaticModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)subghz_test_static_draw);
    view_set_input_callback(instance->view, subghz_test_static_input);
    view_set_enter_callback(instance->view, subghz_test_static_enter);
    view_set_exit_callback(instance->view, subghz_test_static_exit);

    instance->encoder = subghz_encoder_princeton_alloc();

    return instance;
}

void subghz_test_static_free(SubghzTestStatic* instance) {
    furi_assert(instance);
    subghz_encoder_princeton_free(instance->encoder);
    view_free(instance->view);
    free(instance);
}

View* subghz_test_static_get_view(SubghzTestStatic* instance) {
    furi_assert(instance);
    return instance->view;
}
