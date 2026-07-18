// Graphs — rolling 1-hour chart of grill temp, target, and enabled meat
// probes, fed by the grill_state graph ring (30s samples). Refreshes every
// 5s. Click or hold the encoder to return to the main menu.
//
// The app is themeless: chart, series, and division lines are all styled
// manually (a stock lv_chart renders unstyled).

#include "ui_graphs.h"
#include "ui_main_menu.h"
#include "ui.h"
#include "ui_styles.h"
#include "grill_state.h"
#include "encoder.h"
#include "esp_log.h"
#include <stdio.h>

#define REFRESH_MS 5000

// Series palette matches the dashboard's language: WHITE = current grill
// temp, ORANGE = target (same as the big CURRENT number / target readout).
#define COL_GRILL  lv_color_hex(0xFFFFFF)
#define COL_TARGET UI_COLOR_ACCENT
static const uint32_t PROBE_COLORS[NUM_MEAT_PROBES] = {
    0xFF4444, 0x44FF44, 0x00D4AA, 0xFFCC00   // red, green, teal, yellow
};

static lv_obj_t *s_screen;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_ser_grill;
static lv_chart_series_t *s_ser_target;
static lv_chart_series_t *s_ser_probe[NUM_MEAT_PROBES];
static lv_obj_t *s_lbl_ymax, *s_lbl_ymin;
static lv_obj_t *s_lbl_probe_leg[NUM_MEAT_PROBES];
static lv_timer_t *s_timer;
static lv_timer_t *s_refresh_timer;

static void refresh_chart(void)
{
    // Snapshot the ring in chronological order under the lock
    float grill[GRAPH_HISTORY_SIZE], probe[NUM_MEAT_PROBES][GRAPH_HISTORY_SIZE];
    int16_t target[GRAPH_HISTORY_SIZE];
    bool enabled[NUM_MEAT_PROBES];
    char food[NUM_MEAT_PROBES][16];
    int goal[NUM_MEAT_PROBES];
    int count;

    grill_state_lock();
    grill_state_t *gs = grill_state_get();
    count = gs->graph_count;
    int start = (count >= GRAPH_HISTORY_SIZE) ? gs->graph_index : 0;
    for (int n = 0; n < count; n++) {
        int idx = (start + n) % GRAPH_HISTORY_SIZE;
        grill[n] = gs->graph_grill[idx];
        target[n] = gs->graph_target[idx];
        for (int p = 0; p < NUM_MEAT_PROBES; p++) {
            probe[p][n] = gs->graph_probe[p][idx];
        }
    }
    for (int p = 0; p < NUM_MEAT_PROBES; p++) {
        enabled[p] = gs->probes[p].enabled;
        goal[p] = (int)gs->probes[p].target_temp;
        snprintf(food[p], sizeof(food[p]), "%s", gs->probes[p].food_type);
    }
    grill_state_unlock();

    // Autoscale Y over everything visible; keep a sane floor
    float ymin = 1e9f, ymax = -1e9f;
    for (int n = 0; n < count; n++) {
        if (grill[n] > 0) {
            if (grill[n] < ymin) ymin = grill[n];
            if (grill[n] > ymax) ymax = grill[n];
        }
        if (target[n] > 0) {
            if (target[n] < ymin) ymin = target[n];
            if (target[n] > ymax) ymax = target[n];
        }
        for (int p = 0; p < NUM_MEAT_PROBES; p++) {
            if (enabled[p] && probe[p][n] > 0) {
                if (probe[p][n] < ymin) ymin = probe[p][n];
                if (probe[p][n] > ymax) ymax = probe[p][n];
            }
        }
    }
    if (ymin > ymax) { ymin = 50; ymax = 300; }        // no data yet
    ymin = (float)(((int)ymin / 25) * 25);             // round out to 25s
    ymax = (float)(((int)ymax / 25 + 1) * 25);
    if (ymax - ymin < 50) ymax = ymin + 50;
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y,
                       (int32_t)ymin, (int32_t)ymax);

    lv_label_set_text_fmt(s_lbl_ymax, "%d\xC2\xB0", (int)ymax);
    lv_label_set_text_fmt(s_lbl_ymin, "%d\xC2\xB0", (int)ymin);

    // Fill series right-aligned so "now" is always the right edge
    int pad = GRAPH_HISTORY_SIZE - count;
    for (int n = 0; n < GRAPH_HISTORY_SIZE; n++) {
        int i = n - pad;
        bool have = (i >= 0);
        lv_chart_set_value_by_id(s_chart, s_ser_grill, n,
            (have && grill[i] > 0) ? (int32_t)grill[i] : LV_CHART_POINT_NONE);
        lv_chart_set_value_by_id(s_chart, s_ser_target, n,
            (have && target[i] > 0) ? (int32_t)target[i] : LV_CHART_POINT_NONE);
        for (int p = 0; p < NUM_MEAT_PROBES; p++) {
            lv_chart_set_value_by_id(s_chart, s_ser_probe[p], n,
                (have && enabled[p] && probe[p][i] > 0)
                    ? (int32_t)probe[p][i] : LV_CHART_POINT_NONE);
        }
    }
    lv_chart_refresh(s_chart);

    // Probe legend entries: "P1 Brisket 203°" — goal lives in the legend
    // on the LCD (dashed goal lines would clutter a 320px chart)
    for (int p = 0; p < NUM_MEAT_PROBES; p++) {
        if (enabled[p]) {
            if (goal[p] > 0) {
                lv_label_set_text_fmt(s_lbl_probe_leg[p], "P%d %s %d\xC2\xB0",
                                      p + 1, food[p][0] ? food[p] : "", goal[p]);
            } else {
                lv_label_set_text_fmt(s_lbl_probe_leg[p], "P%d %s", p + 1,
                                      food[p][0] ? food[p] : "");
            }
            lv_obj_clear_flag(s_lbl_probe_leg[p], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_lbl_probe_leg[p], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void timer_cb(lv_timer_t *t)
{
    if (!s_screen) return;
    if (ui_encoder_swallowed()) {   // alarm-ack gesture owns the encoder
        encoder_get_diff(); encoder_get_button_event();
        return;
    }

    // Any encoder activity exits (single-purpose viewing screen)
    encoder_btn_event_t btn = encoder_get_button_event();
    if (btn == ENCODER_BTN_SHORT || btn == ENCODER_BTN_LONG) {
        if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
        if (s_refresh_timer) { lv_timer_delete(s_refresh_timer); s_refresh_timer = NULL; }
        ui_encoder_set_direct(false);
        s_screen = NULL;
        ui_load_screen(ui_main_menu_create());
        return;
    }
}

static void refresh_timer_cb(lv_timer_t *t)
{
    if (s_screen) refresh_chart();
}

lv_obj_t *ui_graphs_create(void)
{
    // Direct encoder mode: no focusable widgets here, any click exits
    ui_encoder_set_direct(true);
    encoder_get_button_event();
    encoder_get_diff();

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 320, 480);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_remove_all_objs(g);

    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, "GRAPHS - LAST HOUR");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl, 8, 4);

    s_chart = lv_chart_create(s_screen);
    lv_obj_set_size(s_chart, 276, 330);
    lv_obj_set_pos(s_chart, 36, 40);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, GRAPH_HISTORY_SIZE);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(s_chart, 5, 7);
    // Manual styling (themeless app)
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_chart, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(s_chart, 1, 0);
    lv_obj_set_style_radius(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 2, 0);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x222222), 0);  // div lines
    lv_obj_set_style_line_width(s_chart, 1, 0);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);           // series
    lv_obj_set_style_width(s_chart, 0, LV_PART_INDICATOR);            // no dots
    lv_obj_set_style_height(s_chart, 0, LV_PART_INDICATOR);

    s_ser_grill = lv_chart_add_series(s_chart, COL_GRILL, LV_CHART_AXIS_PRIMARY_Y);
    s_ser_target = lv_chart_add_series(s_chart, COL_TARGET, LV_CHART_AXIS_PRIMARY_Y);
    for (int p = 0; p < NUM_MEAT_PROBES; p++) {
        s_ser_probe[p] = lv_chart_add_series(
            s_chart, lv_color_hex(PROBE_COLORS[p]), LV_CHART_AXIS_PRIMARY_Y);
    }

    // Y-axis extremes
    s_lbl_ymax = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_ymax, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_ymax, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_ymax, 2, 40);

    s_lbl_ymin = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_lbl_ymin, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_ymin, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(s_lbl_ymin, 2, 358);

    // X-axis: -60m .. now
    lv_obj_t *lx0 = lv_label_create(s_screen);
    lv_label_set_text(lx0, "-60 min");
    lv_obj_set_style_text_font(lx0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lx0, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lx0, 36, 374);

    lv_obj_t *lx1 = lv_label_create(s_screen);
    lv_label_set_text(lx1, "now");
    lv_obj_set_style_text_font(lx1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lx1, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lx1, 282, 374);

    // Legend: grill/target fixed, probes when enabled
    lv_obj_t *lg = lv_label_create(s_screen);
    lv_label_set_text(lg, "GRILL");
    lv_obj_set_style_text_font(lg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lg, COL_GRILL, 0);
    lv_obj_set_pos(lg, 8, 396);

    lv_obj_t *lt = lv_label_create(s_screen);
    lv_label_set_text(lt, "TARGET");
    lv_obj_set_style_text_font(lt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lt, COL_TARGET, 0);
    lv_obj_set_pos(lt, 62, 396);

    for (int p = 0; p < NUM_MEAT_PROBES; p++) {
        s_lbl_probe_leg[p] = lv_label_create(s_screen);
        lv_obj_set_style_text_font(s_lbl_probe_leg[p], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_lbl_probe_leg[p],
                                    lv_color_hex(PROBE_COLORS[p]), 0);
        lv_obj_set_pos(s_lbl_probe_leg[p], 8 + (p % 2) * 156, 416 + (p / 2) * 20);
        lv_obj_add_flag(s_lbl_probe_leg[p], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *hint = lv_label_create(s_screen);
    lv_label_set_text(hint, "Click = back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -6);

    refresh_chart();
    s_timer = lv_timer_create(timer_cb, 100, NULL);                  // encoder exit poll
    s_refresh_timer = lv_timer_create(refresh_timer_cb, REFRESH_MS, NULL);

    return s_screen;
}
