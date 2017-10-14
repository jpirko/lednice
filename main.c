/*
 * Copyright (c) 2017 Jiri Pirko <jiri@resnulli.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "usbdrv.h"

enum lednice_usb_cmd {
	LEDNICE_USB_CMD_GET_INFO,
	LEDNICE_USB_CMD_GET_LED_INFO,
	LEDNICE_USB_CMD_SET_LED_BRIGHTNESS,
	LEDNICE_USB_CMD_GET_LED_BRIGHTNESS,
};

#define LEDNICE_USB_MSG_NAME_SIZE 16

struct lednice_info_msg {
	usbWord_t led_count;
	uchar dev_name[LEDNICE_USB_MSG_NAME_SIZE];
};

struct lednice_led_info_msg {
	uchar led_name[LEDNICE_USB_MSG_NAME_SIZE];
	uchar led_subname[LEDNICE_USB_MSG_NAME_SIZE];
	uchar max_brightness;
};

struct lednice_led_brightness_msg {
	uchar brightness;
};

/* USB communication with the device is done only by sending and receiving
 * control urb messages.
 *
 * LEDNICE_USB_CMD_GET_INFO - get information about led device
 *   IN:
 *     value 0
 *     index 0
 *   OUT:
 *     struct lednice_info_msg
 *
 * LEDNICE_USB_CMD_GET_LED_INFO - get information about specific led
 *   IN:
 *     value 0
 *     index LED_ID (0 .. lednice_info_msg.led_count - 1)
 *   OUT:
 *     struct lednice_led_info_msg
 *
 * LEDNICE_USB_CMD_SET_LED_BRIGHTNESS - set specific led brightness
 *   IN:
 *     value BRIGHTNESS (0 .. 255)
 *     index LED_ID (0 .. lednice_info_msg.led_count - 1)
 *   OUT:
 *     NULL
 *
 * LEDNICE_USB_CMD_GET_LED_BRIGHTNESS - get specific led brightness
 *   IN:
 *     value 0
 *     index LED_ID (0 .. lednice_info_msg.led_count - 1)
 *   OUT: struct lednice_led_info_msg
 *     struct lednice_led_brightness_msg
 */

union lednice_msg {
	struct lednice_info_msg info;
	struct lednice_led_info_msg led_info;
	struct lednice_led_brightness_msg brightness;
};

static uchar cur_brightness;

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (usbRequest_t *) data;
	static union lednice_msg msg;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_VENDOR)
		return 0;

	memset(&msg, 0, sizeof(msg));

	switch (rq->bRequest) {
	case LEDNICE_USB_CMD_GET_INFO:
		msg.info.led_count.bytes[0] = 1;
		sprintf((char *) msg.info.dev_name, "ds_simple");
		usbMsgPtr = (void *) &msg.info;
		return sizeof(msg.info);
	case LEDNICE_USB_CMD_GET_LED_INFO:
		if (rq->wIndex.bytes[0] != 0)
			return 0;
		msg.led_info.max_brightness = 255;
		sprintf((char *) msg.led_info.led_name, "led_1");
		usbMsgPtr = (void *) &msg.led_info;
		return sizeof(msg.led_info);
	case LEDNICE_USB_CMD_SET_LED_BRIGHTNESS:
		if (rq->wIndex.bytes[0] != 0)
			return 0;
		cur_brightness = rq->wValue.bytes[0];
		OCR0B = 255 - cur_brightness;
		return 0;
	case LEDNICE_USB_CMD_GET_LED_BRIGHTNESS:
		if (rq->wIndex.bytes[0] != 0)
			return 0;
		msg.brightness.brightness = cur_brightness;
		usbMsgPtr = (void *) &msg.brightness;
		return sizeof(msg.brightness);
	}
	return 0;
}

int main(void)
{
	uchar i;

	usbInit();

	usbDeviceDisconnect();
	for (i = 0; i < 20; i++) /* 300 ms disconnect */
		_delay_ms(15);
	usbDeviceConnect();

	DDRB = 1 << DDB1 | 1 << DDB0;
	TCCR0A = 3 << COM0A0 | 3 << COM0B0 | 3 << WGM00;
	TCCR0B = 0 << WGM02 | 1 << CS00;

	OCR0B = 255;

	wdt_enable(WDTO_1S);
	sei();

	for (;;) {
		wdt_reset();
		usbPoll();
	}
	return 0;
}
