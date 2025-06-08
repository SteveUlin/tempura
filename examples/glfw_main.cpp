// glfw_main.cpp

// An example of using GLFW with C++ to create a window.

#include <GLFW/glfw3.h>

#include <print>
#include <cmath>

void drawCircle(float cx, float cy, float radius, int num_segments) {
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);  // Center of circle
  for (int i = 0; i <= num_segments; ++i) {
    float theta = 2.0 * M_PI * float(i) / float(num_segments);  // Angle
    float x = radius * cosf(theta);  // X coordinate
    float y = radius * sinf(theta);  // Y coordinate
    glVertex2f(x + cx, y + cy);      // Vertex of circle
  }
  glEnd();
}

auto main() -> int {
  std::println("GLFW Example: Creating a Window");

  // Initialize GLFW
  if (glfwInit() == 0) {
    std::println("Failed to initialize GLFW");
    return -1;
  }
  std::atexit(glfwTerminate);

  // Create a windowed mode window and its OpenGL context
  GLFWwindow* window =
      glfwCreateWindow(640, 480, "Hello World", nullptr, nullptr);
  if (window == nullptr) {
    std::println("Failed to create GLFW window");
    return -1;
  }

  // Make the window's context current
  glfwMakeContextCurrent(window);

  // Main loop
  while (glfwWindowShouldClose(window) == 0) {
    //   // Render here (clear the screen)
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the color to light cyan
    glColor3f(0.678, 0.847, 0.902);
    drawCircle(0.0F, 0.0F, 0.1F, 32);  // Draw a circle at the center

    //   // Swap front and back buffers
    glfwSwapBuffers(window);

    //   // Poll for and process events
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  return 0;
}
