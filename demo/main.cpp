
#include "inputstate.h"

#include "vec2.h"
#include <vector>
#include "resourceloader.h"

#include "game.h"

#include <fenv.h>

#include <chrono>
#include <thread>

#include "medea/core.h"

#include "util/stablevector.h"

const Coord SCREEN_RES(1280, 720);

bool lockMouse=false;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

void APIENTRY GLDebugCallback(GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *message,
            const void *userParam) {
    std::cout<<"[GL Debug]: "<<message<<std::endl;
}

int main() {

    //feenableexcept(FE_ALL_EXCEPT & ~FE_INEXACT);

    InputState input;


    glfwInitVulkanLoader(vkGetInstanceProcAddr);

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* glWindow = glfwCreateWindow(SCREEN_RES.x, SCREEN_RES.y, "tntts-vk", NULL /* <- fullscreen! */, NULL);

    Medea::Window window(SCREEN_RES, glWindow, input);

    //VulkanEngine engine(window, input);

    vk::raii::Context vkContext;

    Medea::Core core = Medea::Core::make(vkContext, window);

    IndexStableVector<std::unique_ptr<double>> iv;

    size_t e0 = iv.add(std::make_unique<double>(1.0));
    /*size_t e1 = iv.add(2.0);
    size_t e2 = iv.add(3.0);
    size_t e3 = iv.add(4.0);

    iv.erase(e0);
    iv.erase(e2);

    for (auto [idx, d] : iv) std::cout<<d<<std::endl;

    iv.add(10.0);
    iv.add(11.0);
    iv.add(12.0);

    for (auto [idx, d] : iv) std::cout<<d<<std::endl;*/


    Game game;
    game.start(core, window);


    glfwTerminate();

    return 0;
}
