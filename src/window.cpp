#include "window.h"
#include "vec2.h"
#include "3dmath.h"

//glfwSetCursorPosCallback(window, cursor_position_callback);

//

namespace Medea {
    std::unordered_map<GLFWwindow*, Medea::Window&> glWindowMapping;

    Window& Window::convertGlfwWindow(GLFWwindow* w) {
        return glWindowMapping.at(w);
    }

    /*
    void framebuffer_size_callback(GLFWwindow* glWindow, int width, int height) {
        glViewport(0, 0, width, height);
        Window& window = Window::convertGlfwWindow(glWindow);

        window.width = width;
        window.height = height;
    }
    */

    Window::Window(Coord res, GLFWwindow* glWindow, InputState& is)
        : span(res), window(glWindow), inputState(is) {
        glWindowMapping.insert(std::pair<GLFWwindow*, Window&>(glWindow, *this));

        //glfwSetFramebufferSizeCallback(glWindow, framebuffer_size_callback);

        is.hook(*this);
    }

    void Window::poll(glm::mat4 projectViewMatrix) {
        inputState.poll(projectViewMatrix, Vec2(span));
    }

    glm::mat4 Window::getProjectMatrix(float FOVRadians) {
        return glm::perspective(FOVRadians, ((float)span.x) / span.y, 0.5f, 600.0f);
    }

}