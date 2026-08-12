#ifndef WINDOW_H
#define WINDOW_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct Window {
    GLFWwindow *handle;
    int fb_width;       // Framebuffer width in pixels (for glViewport)
    int fb_height;      // Framebuffer height in pixels
    int win_width;      // Window width in points (for UI layout & mouse coords)
    int win_height;     // Window height in points
    float aspect_ratio;
    bool resized;
} Window;

bool window_init(Window *win, const char *title, int width, int height);
void window_cleanup(Window *win);
bool window_should_close(const Window *win);
void window_poll_events(void);
void window_swap_buffers(const Window *win);

#endif // WINDOW_H
