// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SIDEWALK_UI_DISPLAY_H
#define SIDEWALK_UI_DISPLAY_H

#include <stdio.h>
#include <lvgl.h>

void sidewalk_init_ui_display();
void sidewalk_update_ui_display(float temperature, float humidity, int16_t light);
void sidewalk_cleanup_ui_display();

extern lv_obj_t *sidewalk_label_last;

extern int64_t sidewalk_last_updated_age;

#endif // SIDEWALK_UI_DISPLAY_H
