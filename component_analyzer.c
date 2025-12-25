#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <furi_hal.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    uint8_t mode;
    float voltage;
    float resistance;
    bool testing;
} ComponentAnalyzerApp;

static void draw_callback(Canvas* canvas, void* ctx) {
    ComponentAnalyzerApp* app = ctx;
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Component Analyzer");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    canvas_set_font(canvas, FontSecondary);
    
    if(app->testing) {
        canvas_draw_str(canvas, 40, 40, "Testing...");
        canvas_draw_str(canvas, 20, 60, "Connect component to:");
        canvas_draw_str(canvas, 30, 70, "PC0 and PC1");
    } else {
        const char* modes[] = {"Voltage", "Resistance", "Diode Test"};
        canvas_draw_str(canvas, 10, 30, modes[app->mode]);
        
        if(app->mode == 0) {
            char voltage[32];
            snprintf(voltage, sizeof(voltage), "%.3f V", app->voltage);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str(canvas, 20, 60, voltage);
        } else if(app->mode == 1) {
            char resistance[32];
            if(app->resistance < 1000) {
                snprintf(resistance, sizeof(resistance), "%.0f Ohm", app->resistance);
            } else {
                snprintf(resistance, sizeof(resistance), "%.2f kOhm", app->resistance / 1000);
            }
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str(canvas, 10, 60, resistance);
        }
    }
    
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 5, 125, "OK:Test  <->:Mode  <-:Exit");
}

static float measure_voltage() {
    furi_hal_adc_init();
    uint16_t adc_value = furi_hal_adc_read(ADC_CHANNEL_0);
    furi_hal_adc_deinit();
    return (adc_value / 4095.0f) * 3.3f;
}

static float measure_resistance() {
    float voltage = measure_voltage();
    if(voltage < 0.01f) return 0.0f;
    if(voltage > 3.2f) return 10000000.0f;
    return (3.3f - voltage) * 10000.0f / voltage;
}

static void input_callback(InputEvent* input_event, void* ctx) {
    ComponentAnalyzerApp* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

int32_t component_analyzer_app(void* p) {
    UNUSED(p);
    
    ComponentAnalyzerApp* app = malloc(sizeof(ComponentAnalyzerApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mode = 0;
    app->voltage = 0;
    app->resistance = 0;
    app->testing = false;
    
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    InputEvent event;
    bool running = true;
    
    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyBack:
                    running = false;
                    break;
                case InputKeyLeft:
                    app->mode = (app->mode > 0) ? app->mode - 1 : 2;
                    break;
                case InputKeyRight:
                    app->mode = (app->mode < 2) ? app->mode + 1 : 0;
                    break;
                case InputKeyOk:
                    app->testing = true;
                    notification_message(app->notification, &sequence_blink_blue_10);
                    
                    if(app->mode == 0) {
                        app->voltage = measure_voltage();
                    } else if(app->mode == 1) {
                        app->resistance = measure_resistance();
                    }
                    
                    app->testing = false;
                    notification_message(app->notification, &sequence_success);
                    break;
                }
            }
        }
        view_port_update(app->view_port);
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    
    return 0;
}
