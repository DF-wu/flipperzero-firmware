#include "subghz_protocol_nero_radio.h"

struct SubGhzProtocolNeroRadio {
    SubGhzProtocolCommon common;
};

SubGhzProtocolNeroRadio* subghz_protocol_nero_radio_alloc(void) {
    SubGhzProtocolNeroRadio* instance = furi_alloc(sizeof(SubGhzProtocolNeroRadio));

    instance->common.name = "Nero Radio";
    instance->common.code_min_count_bit_for_found = 55;
    instance->common.te_short = 200;
    instance->common.te_long = 400;
    instance->common.te_delta = 80;
    instance->common.type_protocol = TYPE_PROTOCOL_STATIC;
    instance->common.to_string = (SubGhzProtocolCommonToStr)subghz_protocol_nero_radio_to_str;
    instance->common.to_save_string =
        (SubGhzProtocolCommonGetStrSave)subghz_protocol_nero_radio_to_save_str;
    instance->common.to_load_protocol_from_file =
        (SubGhzProtocolCommonLoadFromFile)subghz_protocol_nero_radio_to_load_protocol_from_file;
    instance->common.to_load_protocol =
        (SubGhzProtocolCommonLoadFromRAW)subghz_decoder_nero_radio_to_load_protocol;
    instance->common.get_upload_protocol =
        (SubGhzProtocolCommonEncoderGetUpLoad)subghz_protocol_nero_radio_send_key;

    return instance;
}

void subghz_protocol_nero_radio_free(SubGhzProtocolNeroRadio* instance) {
    furi_assert(instance);
    free(instance);
}

bool subghz_protocol_nero_radio_send_key(
    SubGhzProtocolNeroRadio* instance,
    SubGhzProtocolCommonEncoder* encoder) {
    furi_assert(instance);
    furi_assert(encoder);
    size_t index = 0;
    encoder->size_upload = 2 + 47 * 2 + 2 + (instance->common.code_last_count_bit * 2);
    if(encoder->size_upload > SUBGHZ_ENCODER_UPLOAD_MAX_SIZE) return false;

    //Send header
    encoder->upload[index++] = level_duration_make(true, (uint32_t)instance->common.te_short);
    encoder->upload[index++] =
        level_duration_make(false, (uint32_t)instance->common.te_short * 37);
    for(uint8_t i = 0; i < 47; i++) {
        encoder->upload[index++] = level_duration_make(true, (uint32_t)instance->common.te_short);
        encoder->upload[index++] = level_duration_make(false, (uint32_t)instance->common.te_short);
    }

    //Send start bit
    encoder->upload[index++] = level_duration_make(true, (uint32_t)instance->common.te_short * 4);
    encoder->upload[index++] = level_duration_make(false, (uint32_t)instance->common.te_short);

    //Send key data
    for(uint8_t i = instance->common.code_last_count_bit; i > 0; i--) {
        if(bit_read(instance->common.code_last_found, i - 1)) {
            //send bit 1
            encoder->upload[index++] =
                level_duration_make(true, (uint32_t)instance->common.te_long);
            encoder->upload[index++] =
                level_duration_make(false, (uint32_t)instance->common.te_short);
        } else {
            //send bit 0
            encoder->upload[index++] =
                level_duration_make(true, (uint32_t)instance->common.te_short);
            encoder->upload[index++] =
                level_duration_make(false, (uint32_t)instance->common.te_long);
        }
    }

    return true;
}

void subghz_protocol_nero_radio_reset(SubGhzProtocolNeroRadio* instance) {
    instance->common.parser_step = 0;
}

/** Analysis of received data
 * 
 * @param instance SubGhzProtocolNeroRadio instance
 */
// void subghz_protocol_nero_radio_check_remote_controller(SubGhzProtocolNeroRadio* instance) {
//     //пока не понятно с серийником, но код статический
//     // uint64_t code_found_reverse = subghz_protocol_common_reverse_key(instance->common.code_found, instance->common.code_count_bit);
//     // uint32_t code_fix = code_found_reverse & 0xFFFFFFFF;
//     // //uint32_t code_hop = (code_found_reverse >> 24) & 0xFFFFF;

//     // instance->common.serial = code_fix & 0xFFFFFFF;
//     // instance->common.btn = (code_fix >> 28) & 0x0F;

//     //if (instance->common.callback) instance->common.callback((SubGhzProtocolCommon*)instance, instance->common.context);

// }

void subghz_protocol_nero_radio_parse(
    SubGhzProtocolNeroRadio* instance,
    bool level,
    uint32_t duration) {
    switch(instance->common.parser_step) {
    case 0:
        if((level) &&
           (DURATION_DIFF(duration, instance->common.te_short) < instance->common.te_delta)) {
            instance->common.parser_step = 1;
            instance->common.te_last = duration;
            instance->common.header_count = 0;
        } else {
            instance->common.parser_step = 0;
        }
        break;
    case 1:
        if(level) {
            if((DURATION_DIFF(duration, instance->common.te_short) < instance->common.te_delta) ||
               (DURATION_DIFF(duration, instance->common.te_short * 4) <
                instance->common.te_delta)) {
                instance->common.te_last = duration;
            } else {
                instance->common.parser_step = 0;
            }
        } else if(DURATION_DIFF(duration, instance->common.te_short) < instance->common.te_delta) {
            if(DURATION_DIFF(instance->common.te_last, instance->common.te_short) <
               instance->common.te_delta) {
                // Found header
                instance->common.header_count++;
                break;
            } else if(
                DURATION_DIFF(instance->common.te_last, instance->common.te_short * 4) <
                instance->common.te_delta) {
                // Found start bit
                if(instance->common.header_count > 40) {
                    instance->common.parser_step = 2;
                    instance->common.code_found = 0;
                    instance->common.code_count_bit = 0;
                } else {
                    instance->common.parser_step = 0;
                }
            } else {
                instance->common.parser_step = 0;
            }
        } else {
            instance->common.parser_step = 0;
        }
        break;
    case 2:
        if(level) {
            instance->common.te_last = duration;
            instance->common.parser_step = 3;
        } else {
            instance->common.parser_step = 0;
        }
        break;
    case 3:
        if(!level) {
            if(duration >= (instance->common.te_short * 10 + instance->common.te_delta * 2)) {
                //Found stop bit
                instance->common.parser_step = 0;
                if(instance->common.code_count_bit >=
                   instance->common.code_min_count_bit_for_found) {
                    instance->common.code_last_found = instance->common.code_found;
                    instance->common.code_last_count_bit = instance->common.code_count_bit;
                    if(instance->common.callback)
                        instance->common.callback(
                            (SubGhzProtocolCommon*)instance, instance->common.context);
                }
                instance->common.code_found = 0;
                instance->common.code_count_bit = 0;
                instance->common.parser_step = 0;
                break;
            } else if(
                (DURATION_DIFF(instance->common.te_last, instance->common.te_short) <
                 instance->common.te_delta) &&
                (DURATION_DIFF(duration, instance->common.te_long) < instance->common.te_delta)) {
                subghz_protocol_common_add_bit(&instance->common, 0);
                instance->common.parser_step = 2;
            } else if(
                (DURATION_DIFF(instance->common.te_last, instance->common.te_long) <
                 instance->common.te_delta) &&
                (DURATION_DIFF(duration, instance->common.te_short) < instance->common.te_delta)) {
                subghz_protocol_common_add_bit(&instance->common, 1);
                instance->common.parser_step = 2;
            } else {
                instance->common.parser_step = 0;
            }
        } else {
            instance->common.parser_step = 0;
        }
        break;
    }
}

void subghz_protocol_nero_radio_to_str(SubGhzProtocolNeroRadio* instance, string_t output) {
    uint32_t code_found_hi = instance->common.code_last_found >> 32;
    uint32_t code_found_lo = instance->common.code_last_found & 0x00000000ffffffff;

    uint64_t code_found_reverse = subghz_protocol_common_reverse_key(
        instance->common.code_last_found, instance->common.code_last_count_bit);

    uint32_t code_found_reverse_hi = code_found_reverse >> 32;
    uint32_t code_found_reverse_lo = code_found_reverse & 0x00000000ffffffff;

    string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "Yek:0x%lX%08lX\r\n",
        instance->common.name,
        instance->common.code_last_count_bit,
        code_found_hi,
        code_found_lo,
        code_found_reverse_hi,
        code_found_reverse_lo);
}

void subghz_protocol_nero_radio_to_save_str(SubGhzProtocolNeroRadio* instance, string_t output) {
    uint32_t code_found_hi = instance->common.code_last_found >> 32;
    uint32_t code_found_lo = instance->common.code_last_found & 0x00000000ffffffff;

    string_printf(
        output,
        "Protocol: %s\n"
        "Bit: %d\n"
        "Key: %08lX%08lX\n",
        instance->common.name,
        instance->common.code_last_count_bit,
        code_found_hi,
        code_found_lo);
}

bool subghz_protocol_nero_radio_to_load_protocol_from_file(
    FileWorker* file_worker,
    SubGhzProtocolNeroRadio* instance) {
    bool loaded = false;
    string_t temp_str;
    string_init(temp_str);
    int res = 0;
    int data = 0;

    do {
        // Read and parse bit data from 2nd line
        if(!file_worker_read_until(file_worker, temp_str, '\n')) {
            break;
        }
        res = sscanf(string_get_cstr(temp_str), "Bit: %d\n", &data);
        if(res != 1) {
            break;
        }
        instance->common.code_last_count_bit = (uint8_t)data;

        // Read and parse key data from 3nd line
        if(!file_worker_read_until(file_worker, temp_str, '\n')) {
            break;
        }
        // strlen("Key: ") = 5
        string_right(temp_str, 5);

        uint8_t buf_key[8] = {0};
        if(!subghz_protocol_common_read_hex(temp_str, buf_key, 8)) {
            break;
        }

        for(uint8_t i = 0; i < 8; i++) {
            instance->common.code_last_found = instance->common.code_last_found << 8 | buf_key[i];
        }

        loaded = true;
    } while(0);

    string_clear(temp_str);

    return loaded;
}

void subghz_decoder_nero_radio_to_load_protocol(SubGhzProtocolNeroRadio* instance, void* context) {
    furi_assert(context);
    furi_assert(instance);
    SubGhzProtocolCommonLoad* data = context;
    instance->common.code_last_found = data->code_found;
    instance->common.code_last_count_bit = data->code_count_bit;
}