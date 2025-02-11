#include "furi/check.h"
#include "irda.h"
#include "irda_common_i.h"
#include <stdbool.h>
#include <furi.h>
#include "irda_i.h"
#include <stdint.h>

static void irda_common_decoder_reset_state(IrdaCommonDecoder* decoder);

static inline size_t consume_samples(uint32_t* array, size_t len, size_t shift) {
    furi_assert(len >= shift);
    len -= shift;
    for (int i = 0; i < len; ++i)
        array[i] = array[i + shift];

    return len;
}

static inline void accumulate_lsb(IrdaCommonDecoder* decoder, bool bit) {
    uint16_t index = decoder->databit_cnt / 8;
    uint8_t shift = decoder->databit_cnt % 8;   // LSB first

    if (!shift)
        decoder->data[index] = 0;

    if (bit) {
        decoder->data[index] |= (0x1 << shift);           // add 1
    } else {
        (void) decoder->data[index];                      // add 0
    }

    ++decoder->databit_cnt;
}

static bool irda_check_preamble(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);

    bool result = false;
    bool start_level = (decoder->level + decoder->timings_cnt + 1) % 2;

    // align to start at Mark timing
    if (!start_level) {
        if (decoder->timings_cnt > 0) {
            decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 1);
        }
    }

    if (decoder->protocol->timings.preamble_mark == 0) {
        return true;
    }

    while ((!result) && (decoder->timings_cnt >= 2)) {
        float preamble_tolerance = decoder->protocol->timings.preamble_tolerance;
        uint16_t preamble_mark = decoder->protocol->timings.preamble_mark;
        uint16_t preamble_space = decoder->protocol->timings.preamble_space;

        if ((MATCH_TIMING(decoder->timings[0], preamble_mark, preamble_tolerance))
            && (MATCH_TIMING(decoder->timings[1], preamble_space, preamble_tolerance))) {
            result = true;
        }

        decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 2);
    }

    return result;
}

/* Pulse Distance Modulation */
IrdaStatus irda_common_decode_pdm(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);

    uint32_t* timings = decoder->timings;
    IrdaStatus status = IrdaStatusError;
    uint32_t bit_tolerance = decoder->protocol->timings.bit_tolerance;
    uint16_t bit1_mark = decoder->protocol->timings.bit1_mark;
    uint16_t bit1_space = decoder->protocol->timings.bit1_space;
    uint16_t bit0_mark = decoder->protocol->timings.bit0_mark;
    uint16_t bit0_space = decoder->protocol->timings.bit0_space;

    while (1) {
        // Stop bit
        if ((decoder->databit_cnt == decoder->protocol->databit_len) && (decoder->timings_cnt == 1)) {
            if (MATCH_TIMING(timings[0], bit1_mark, bit_tolerance)) {
                decoder->timings_cnt = 0;
                status = IrdaStatusReady;
            } else {
                status = IrdaStatusError;
            }
            break;
        }

        if (decoder->timings_cnt >= 2) {
            if (MATCH_TIMING(timings[0], bit1_mark, bit_tolerance)
                && MATCH_TIMING(timings[1], bit1_space, bit_tolerance)) {
                accumulate_lsb(decoder, 1);
            } else if (MATCH_TIMING(timings[0], bit0_mark, bit_tolerance)
                && MATCH_TIMING(timings[1], bit0_space, bit_tolerance)) {
                accumulate_lsb(decoder, 0);
            } else {
                status = IrdaStatusError;
                break;
            }
            decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 2);
        } else {
            status = IrdaStatusOk;
            break;
        }
    }

    return status;
}

/* level switch detection goes in middle of time-quant */
IrdaStatus irda_common_decode_manchester(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);
    IrdaStatus status = IrdaStatusOk;
    uint16_t bit = decoder->protocol->timings.bit1_mark;
    uint16_t tolerance = decoder->protocol->timings.bit_tolerance;

    while (decoder->timings_cnt) {
        uint32_t timing = decoder->timings[0];
        bool* switch_detect = &decoder->switch_detect;
        furi_assert((*switch_detect == true) || (*switch_detect == false));

        bool single_timing = MATCH_TIMING(timing, bit, tolerance);
        bool double_timing = MATCH_TIMING(timing, 2*bit, tolerance);

        if(!single_timing && !double_timing) {
            status = IrdaStatusError;
            break;
        }

        if ((decoder->protocol->manchester_start_from_space) && (decoder->databit_cnt == 0)) {
            *switch_detect = 1; /* fake as we were previously in the middle of time-quant */
            decoder->data[0] = 0;   /* first captured timing should be Mark */
            ++decoder->databit_cnt;
        }

        if (*switch_detect == 0) {
            if (double_timing) {
                status = IrdaStatusError;
                break;
            }
            /* only single timing - level switch required in the middle of time-quant */
            *switch_detect = 1;
        } else {
            /* double timing means we in the middle of time-quant again */
            if (single_timing)
                *switch_detect = 0;
        }

        decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 1);
        status = IrdaStatusOk;
        bool level = (decoder->level + decoder->timings_cnt) % 2;

        if (decoder->databit_cnt < decoder->protocol->databit_len) {
            if (*switch_detect) {
                accumulate_lsb(decoder, level);
            }
            if (decoder->databit_cnt == decoder->protocol->databit_len) {
                if (level) {
                    status = IrdaStatusReady;
                    break;
                }
            }
        } else {
            furi_assert(level);
            /* cover case: sequence should be stopped after last bit was received */
            if (single_timing) {
                status = IrdaStatusReady;
                break;
            } else {
                status = IrdaStatusError;
            }
        }
    }

    return status;
}

/* Pulse Width Modulation */
IrdaStatus irda_common_decode_pwm(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);

    uint32_t* timings = decoder->timings;
    IrdaStatus status = IrdaStatusOk;
    uint32_t bit_tolerance = decoder->protocol->timings.bit_tolerance;
    uint16_t bit1_mark = decoder->protocol->timings.bit1_mark;
    uint16_t bit1_space = decoder->protocol->timings.bit1_space;
    uint16_t bit0_mark = decoder->protocol->timings.bit0_mark;

    while (decoder->timings_cnt) {
        bool level = (decoder->level + decoder->timings_cnt + 1) % 2;

        if (level) {
            if (MATCH_TIMING(timings[0], bit1_mark, bit_tolerance)) {
                accumulate_lsb(decoder, 1);
            } else if (MATCH_TIMING(timings[0], bit0_mark, bit_tolerance)) {
                accumulate_lsb(decoder, 0);
            } else {
                status = IrdaStatusError;
                break;
            }
        } else {
            if (!MATCH_TIMING(timings[0], bit1_space, bit_tolerance)) {
                status = IrdaStatusError;
                break;
            }
        }
        decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 1);

        if (decoder->databit_cnt == decoder->protocol->databit_len) {
            status = IrdaStatusReady;
            break;
        }
    }

    return status;
}

IrdaMessage* irda_common_decode(IrdaCommonDecoder* decoder, bool level, uint32_t duration) {
    furi_assert(decoder);

    IrdaMessage* message = 0;
    IrdaStatus status = IrdaStatusError;

    if (decoder->level == level) {
        irda_common_decoder_reset(decoder);
    }
    decoder->level = level;   // start with low level (Space timing)

    decoder->timings[decoder->timings_cnt] = duration;
    decoder->timings_cnt++;
    furi_check(decoder->timings_cnt <= sizeof(decoder->timings));

    while(1) {
        switch (decoder->state) {
        case IrdaCommonDecoderStateWaitPreamble:
            if (irda_check_preamble(decoder)) {
                decoder->state = IrdaCommonDecoderStateDecode;
                decoder->databit_cnt = 0;
                decoder->switch_detect = false;
                continue;
            }
            break;
        case IrdaCommonDecoderStateDecode:
            status = decoder->protocol->decode(decoder);
            if (status == IrdaStatusReady) {
                if (decoder->protocol->interpret(decoder)) {
                    message = &decoder->message;
                    decoder->state = IrdaCommonDecoderStateProcessRepeat;
                } else {
                    decoder->state = IrdaCommonDecoderStateWaitPreamble;
                }
            } else if (status == IrdaStatusError) {
                irda_common_decoder_reset_state(decoder);
                continue;
            }
            break;
        case IrdaCommonDecoderStateProcessRepeat:
            if (!decoder->protocol->decode_repeat) {
                decoder->state = IrdaCommonDecoderStateWaitPreamble;
                continue;
            }
            status = decoder->protocol->decode_repeat(decoder);
            if (status == IrdaStatusError) {
                irda_common_decoder_reset_state(decoder);
                continue;
            } else if (status == IrdaStatusReady) {
                decoder->message.repeat = true;
                message = &decoder->message;
            }
            break;
        }
        break;
    }

    return message;
}

void* irda_common_decoder_alloc(const IrdaCommonProtocolSpec* protocol) {
    furi_assert(protocol);

    uint32_t alloc_size = sizeof(IrdaCommonDecoder)
                          + protocol->databit_len / 8
                          + !!(protocol->databit_len % 8);
    IrdaCommonDecoder* decoder = furi_alloc(alloc_size);
    memset(decoder, 0, alloc_size);
    decoder->protocol = protocol;
    decoder->level = true;
    return decoder;
}

void irda_common_decoder_free(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);
    free(decoder);
}

void irda_common_decoder_reset_state(IrdaCommonDecoder* decoder) {
    decoder->state = IrdaCommonDecoderStateWaitPreamble;
    decoder->databit_cnt = 0;
    decoder->switch_detect = false;
    decoder->message.protocol = IrdaProtocolUnknown;
    if (decoder->protocol->timings.preamble_mark == 0) {
        if (decoder->timings_cnt > 0) {
            decoder->timings_cnt = consume_samples(decoder->timings, decoder->timings_cnt, 1);
        }
    }
}

void irda_common_decoder_reset(IrdaCommonDecoder* decoder) {
    furi_assert(decoder);

    irda_common_decoder_reset_state(decoder);
    decoder->timings_cnt = 0;
}

