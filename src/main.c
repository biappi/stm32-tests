#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "platform.h"
#include "display.h"


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


char logic_peak_levels[8] = { 0 };
char logic_peak_offset[8] = { 0 };

char logic_control_strip[2][8][7] = { 0 };

uint8_t nibble_char(uint8_t x) {
    return (x < 10) ? ('0' + x) : ('a' + (x - 10));
}

void byte_string(char * string, uint8_t byte) {
    string[0]  = nibble_char((byte & 0xf0) >> 4);
    string[1]  = nibble_char((byte & 0x0f)     );
}

void send_small_dump(const uint8_t * buf, size_t len) {
    //                                   1     1
    //                         01234567890123456
    static uint8_t string[] = "00 00 00 00 00  ";

    for (int i = 0; i < 5; i++) {
        string[(i * 3)    ] = '-';
        string[(i * 3) + 1] = '-';
    }

    for (int i = 0; i < MIN(len, 5); i++) {
        byte_string(string + (i * 3), buf[i]);
    }


    display_send_string(string);
}
enum usb_midi_packet_types {
    usb_midi_sysex_start_or_continue = 0x04,
    usb_midi_sysex_ends_single_byte  = 0x05,
    usb_midi_sysex_ends_two_bytes    = 0x06,
    usb_midi_sysex_ends_three_bytes  = 0x07,
    usb_midi_channel_pressure        = 0x0d,
};


struct buffer {
    size_t  len;
    uint8_t buf[255];
};

struct sysex_parser {
    enum {
        sysex_parser_idle,
        sysex_parser_accumulating,
        sysex_parser_ended,
    } state;

    struct buffer sysex;
};

void sysex_parse(struct sysex_parser * parser, const uint8_t * packet)
{
    const uint8_t
    cable_id    = (packet[0] & 0xf0) >> 0;

    const enum usb_midi_packet_types
    packet_type = (packet[0] & 0x0f);

    struct buffer
    *sysex      = &(parser->sysex);

    switch (packet_type) {
        case usb_midi_sysex_start_or_continue:
            switch (parser->state) {
                case sysex_parser_idle:
                case sysex_parser_ended:
                    sysex->len = 0;
                    break;

                case sysex_parser_accumulating:
                    break;
            }

            sysex->buf[sysex->len++] = packet[1];
            sysex->buf[sysex->len++] = packet[2];
            sysex->buf[sysex->len++] = packet[3];
            parser->state = sysex_parser_accumulating;
            break;

        case usb_midi_sysex_ends_single_byte:
            sysex->buf[sysex->len++] = packet[1];
            parser->state = sysex_parser_ended;
            break;

        case usb_midi_sysex_ends_two_bytes:
            sysex->buf[sysex->len++] = packet[1];
            sysex->buf[sysex->len++] = packet[2];
            parser->state = sysex_parser_ended;
            break;

        case usb_midi_sysex_ends_three_bytes:
            sysex->buf[sysex->len++] = packet[1];
            sysex->buf[sysex->len++] = packet[2];
            sysex->buf[sysex->len++] = packet[3];
            parser->state = sysex_parser_ended;
            break;
    }
}


void usb_midi_received_callback(const uint8_t * buf, size_t len)
{
    static struct sysex_parser parser = { 0 };

    for (int i = 0; i < len / 4; i++) {
        const uint8_t 
        *packet = buf + (i * 4);

        const uint8_t
        cable_id    = (packet[0] & 0xf0) >> 0;

        const enum usb_midi_packet_types
        packet_type = (packet[0] & 0x0f);

        switch (packet_type) {
            case usb_midi_channel_pressure: {
                const uint8_t level   = (packet[2] & 0x0f);
                const uint8_t channel = (packet[2] & 0x70) >> 4;
                logic_peak_levels[channel] = level;
                logic_peak_offset[channel] = 0;
                break;
            }
        }

        sysex_parse(&parser, packet);

        if (parser.state == sysex_parser_ended) {
            const uint8_t * midiData = parser.sysex.buf;

            parser.state = sysex_parser_idle;

            if (
                (midiData[0] == 0xf0) &&
                (midiData[5] == 0x12)
            ) {
                const uint8_t  offset =  midiData[6];
                const uint8_t *src    = &midiData[7];
                      uint8_t *dst    = (((uint8_t *)logic_control_strip) + offset);

                while (*src != 0xf7) {
                    *dst++ = *src++;
                }
            }
        }

    }
}

// - //

void draw_peak_levels(uint8_t t)
{
    #define bar_size 128

    uint8_t data[bar_size] = { 0 };

    int l = logic_peak_levels[t];
    int e = 0;

    if (l) {
        l = MIN(l, 0xc);

        e = (l * 128) / 12 - logic_peak_offset[t];
        logic_peak_offset[t] -= 8;
        if (e < 0) {
            e = 0;
            logic_peak_levels[t] = 0;
        }
    }

    for (int i = 0; i < bar_size; i++) {
        bool    is_on     = (i < e);
        bool    is_odd    = ((i % 2) == 0);
        bool    tick      = ((i % 4) == 0);
        bool    tick_mask = 0x3c;

        uint8_t mark      = 0x00; // tick ? 0x42 : 0x00;
        uint8_t border    = 0x81;
        uint8_t dot       = is_on && is_odd ? 0xff : 0x00;

        data[i] = dot | border;
    }

    display_send_data(data, bar_size);

}

void draw_logic_strip()
{
    for (int t = 0; t < 8; t++) {
        display_select(display_selection_1 + t);

        display_goto_line_column(0, 0);

        for (int i = 0; i < 7; i++) {
            display_send_2x_character_top(logic_control_strip[0][t][i]);
        }

        display_goto_line_column(1, 0);

        for (int i = 0; i < 7; i++) {
            display_send_2x_character_bottom(logic_control_strip[0][t][i]);
        }

        display_goto_line_column(3, 0);

        for (int i = 0; i < 7; i++) {
            display_send_2x_character_top(logic_control_strip[1][t][i]);
        }

        display_goto_line_column(4, 0);

        for (int i = 0; i < 7; i++) {
            display_send_2x_character_bottom(logic_control_strip[1][t][i]);
        }

        display_goto_line_column(6, 0);
        draw_peak_levels(t);
    }
}

void draw_at_startup()
{
    display_select(display_selection_all);
    display_goto_line_column(0, 0);

    for (
        display_selection_t s = display_selection_1;
        s <= display_selection_8;
        s++
    ) {
        //               01234567
        char string[] = "DISP    ";
        string[4] = s - display_selection_1 + '1';
        display_select(s);

        char *s;

        s = string;
        while (*s != 0) {
            display_send_2x_character_top(*s++);
        }

        s = string;
        while (*s != 0) {
            display_send_2x_character_bottom(*s++);
        }
    }
}

int main(void)
{
    platform_init();
    draw_at_startup();

    const uint32_t jiffies_mult = 30; // redraw every N jiffies_mult

    while (1) {
        uint32_t poll_time = platform_jiffies();

        draw_logic_strip();

        uint32_t after_draw = platform_jiffies();
        
        {
            display_select(display_selection_8);
            display_goto_line_column(6, 0);

            char buffer[8];
            snprintf(buffer, sizeof(buffer), "%d", after_draw - poll_time);
            display_send_string(buffer);
        }

        for (int i = 0; i < jiffies_mult; i++) {
            do {
                platform_poll();
                WFI();
            } while(poll_time == platform_jiffies());
        }
    }

}

