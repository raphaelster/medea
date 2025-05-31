#pragma once

#include "inputstate.h"
#include "resourceloader.h"

class GLFWwindow;

namespace Medea {
    struct Window {
        public:
        Coord span;
        GLFWwindow* const window;
        InputState& inputState;

        Window(const Window&) = delete;
        Window& operator=(const Window) = delete;

        Window(Coord _span, GLFWwindow* glWindow, InputState& in);

        static Window& convertGlfwWindow(GLFWwindow* w);

        //cameraPlace, projectionMatrix are to convert mouse coordinates to world space
        void poll(glm::mat4 projectViewMatrix);

        //since width and height can change by dragging screen boundaries, this will always be up to date
        glm::mat4 getProjectMatrix(float FOVRadians);

        protected:

        private:
    };
}
