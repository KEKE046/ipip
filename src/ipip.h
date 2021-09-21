#pragma once

#include<GLFW/glfw3.h>

namespace ipip {
    void updateWindow(GLFWwindow * window);
    void initServer(int port);
    void stopServer();
} // namespace ipip
