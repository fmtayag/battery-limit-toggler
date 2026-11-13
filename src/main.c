#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CONSERVATION_MODE_PATH "/sys/devices/pci0000:00/0000:00:1f.0/PNP0C09:00/VPC2004:00/conservation_mode"
#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 140
#define BUTTON_X 50
#define BUTTON_Y 80
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 40

// X font names (XLFD): -foundry-family-weight-slant-width-style-pixels-...
// "*" means "any".
#define FONT_PREFERRED "-*-dejavu sans-medium-r-*-*-15-*-*-*-*-*-*-*"
#define FONT_FALLBACK  "-*-*-medium-r-*-*-14-*-*-*-*-*-*-*"

// Returns 1 (limit on), 0 (limit off), or -1 if the state can't be read.
int read_state(void) {
    FILE *f = fopen(CONSERVATION_MODE_PATH, "r");

    // File missing
    if (!f) 
        return -1;

    int value = -1;

    // Parse failed
    if (fscanf(f, "%d", &value) != 1) 
        value = -1;

    fclose(f);

    return value;
}

void set_state(int new_state) {
    char buf[512];
    snprintf(buf, sizeof buf, "pkexec sh -c 'echo %d > %s'", new_state, CONSERVATION_MODE_PATH);
    
    if (system(buf) != 0) 
        fprintf(stderr, "failed to set conservation mode\n");
}

void draw(Display *display, Window window, GC graphics_context, int state, bool is_hovering) {
    XClearWindow(display, window);
    
    const char *status;
    const char *detail;
    const char *label;

    if (state == 1) {
        status = "Charging limit: ON";
        detail = "Charges to 80%";
        label = "Turn OFF";
    } else if (state == 0) {
        status = "Charging limit: OFF";
        detail = "Charges to 100%";
        label = "Turn ON";
    } else {
        status = "Charging limit: unavailable";
        detail = "";
        label = "Turn ON";
    }

    XDrawString(display, window, graphics_context, 20, 30, status, strlen(status));
    XDrawString(display, window, graphics_context, 20, 50, detail, strlen(detail));

    int label_width = (int)strlen(label) * 6;
    XFontStruct *fs = XQueryFont(display, XGContextFromGC(graphics_context));
    if (fs) {
        label_width = XTextWidth(fs, label, strlen(label));
        XFreeFontInfo(NULL, fs, 1);
    }

    if (is_hovering) {
        XSetForeground(display, graphics_context, 0xd0d0d0);
    } else {
        XSetForeground(display, graphics_context, 0xe8e8e8);
    }

    XFillRectangle(display, window, graphics_context, BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    XSetForeground(display, graphics_context, BlackPixel(display, DefaultScreen(display)));
    XDrawRectangle(display, window, graphics_context, BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    XDrawString(display, window, graphics_context, BUTTON_X + (BUTTON_WIDTH - label_width) / 2, BUTTON_Y + 25, label, strlen(label));
}

int main(void) {
    // --- Window and display ---
    Display *display = XOpenDisplay(NULL);
        
    if (!display) { 
        fprintf(stderr, "cannot open display\n"); 
        return 1; 
    }

    int default_screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display, 
        RootWindow(display, default_screen), 
        100, 
        100, 
        WINDOW_WIDTH, 
        WINDOW_HEIGHT, 
        1, 
        BlackPixel(display, default_screen), 
        0xf5f5f5
    );

    XStoreName(display, window, "Battery Limit Toggler");

    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = sh->max_width = WINDOW_WIDTH;
    sh->min_height = sh->max_height = WINDOW_HEIGHT;
    XSetWMNormalHints(display, window, sh);

    // --- Events ---
    XSelectInput(display, window, ExposureMask | ButtonPressMask | PointerMotionMask | KeyPressMask);
    Atom atom_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &atom_delete, 1);

    // --- Drawing context and font ---
    GC graphics_context = XCreateGC(display, window, 0, NULL);
    XFontStruct *font = XLoadQueryFont(display, FONT_PREFERRED);
    if (!font) font = XLoadQueryFont(display, FONT_FALLBACK);
    if (font) XSetFont(display, graphics_context, font->fid);
    XSetForeground(display, graphics_context, BlackPixel(display, default_screen));

    // --- Show window ---
    XMapWindow(display, window);

    bool is_hovering = false;
    bool is_running = true;

    // --- Main loop ---
    while(is_running) {
        XEvent e;
        XNextEvent(display, &e);

        switch (e.type) {
        case Expose:
            draw(display, window, graphics_context, read_state(), is_hovering);

            break;

        case MotionNotify: {
            bool touches_button = e.xmotion.x >= BUTTON_X && e.xmotion.x < BUTTON_X + BUTTON_WIDTH &&
                    e.xmotion.y >= BUTTON_Y && e.xmotion.y < BUTTON_Y + BUTTON_HEIGHT;
                    
            if (touches_button != is_hovering) { 
                is_hovering = touches_button; 
                draw(display, window, graphics_context, read_state(), is_hovering); 
            }

            break;
        }

        case ButtonPress:
            if (e.xbutton.x >= BUTTON_X && e.xbutton.x < BUTTON_X + BUTTON_WIDTH &&
                e.xbutton.y >= BUTTON_Y && e.xbutton.y < BUTTON_Y + BUTTON_HEIGHT) {
                int state = read_state();
                if (state == 1) {
                    set_state(0);
                } else if (state == 0) {
                    set_state(1);
                }
                draw(display, window, graphics_context, read_state(), is_hovering);
            }

            break;

        case KeyPress:
            if (XLookupKeysym(&e.xkey, 0) == XK_Escape) 
                is_running = false;

            break;

        case ClientMessage:
            if ((Atom)e.xclient.data.l[0] == atom_delete) 
                is_running = false;

            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
