#include "window.h"
#include <stdio.h>

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    Window *win = (Window*)glfwGetWindowUserPointer(window);
    if (win) {
        win->fb_width = width;
        win->fb_height = height;
        
        glfwGetWindowSize(window, &win->win_width, &win->win_height);
        
        win->aspect_ratio = (height > 0) ? ((float)width / (float)height) : 1.0f;
        win->resized = true;
        glViewport(0, 0, width, height);
    }
}

bool window_init(Window *win, const char *title, int width, int height) {
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return false;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    win->handle = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!win->handle) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(win->handle);
    glfwSetWindowUserPointer(win->handle, win);
    glfwSetFramebufferSizeCallback(win->handle, framebuffer_size_callback);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return false;
    }
    
    glfwGetFramebufferSize(win->handle, &win->fb_width, &win->fb_height);
    glfwGetWindowSize(win->handle, &win->win_width, &win->win_height);
    
    win->aspect_ratio = (win->fb_height > 0) ? ((float)win->fb_width / (float)win->fb_height) : 1.0f;
    win->resized = false;
    
    glViewport(0, 0, win->fb_width, win->fb_height);
    glfwSwapInterval(1); // Enable VSync
    
    return true;
}

void window_cleanup(Window *win) {
    if (win->handle) {
        glfwDestroyWindow(win->handle);
        win->handle = NULL;
    }
    glfwTerminate();
}

bool window_should_close(const Window *win) {
    return glfwWindowShouldClose(win->handle);
}

void window_poll_events(void) {
    glfwPollEvents();
}

void window_swap_buffers(const Window *win) {
    glfwSwapBuffers(win->handle);
}
