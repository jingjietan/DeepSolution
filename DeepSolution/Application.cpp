#include "Application.h"

#include "Core/Common.h"
#include "Core/Transition.h"
#include "Core/Image.h"

#include <VkBootstrap.h>
#include <volk.h>
#include <spdlog/spdlog.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera/FreeCamera.h"
#include "Camera/ArcballCamera.h"

namespace {
	static ImGuiKey GLFWKeyToImGuiKey(int key)
	{
		switch (key)
		{
		case GLFW_KEY_TAB: return ImGuiKey_Tab;
		case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
		case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
		case GLFW_KEY_UP: return ImGuiKey_UpArrow;
		case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
		case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
		case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
		case GLFW_KEY_HOME: return ImGuiKey_Home;
		case GLFW_KEY_END: return ImGuiKey_End;
		case GLFW_KEY_INSERT: return ImGuiKey_Insert;
		case GLFW_KEY_DELETE: return ImGuiKey_Delete;
		case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
		case GLFW_KEY_SPACE: return ImGuiKey_Space;
		case GLFW_KEY_ENTER: return ImGuiKey_Enter;
		case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
		case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
		case GLFW_KEY_COMMA: return ImGuiKey_Comma;
		case GLFW_KEY_MINUS: return ImGuiKey_Minus;
		case GLFW_KEY_PERIOD: return ImGuiKey_Period;
		case GLFW_KEY_SLASH: return ImGuiKey_Slash;
		case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
		case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
		case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
		case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
		case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
		case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
		case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
		case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
		case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
		case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
		case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
		case GLFW_KEY_KP_0: return ImGuiKey_Keypad0;
		case GLFW_KEY_KP_1: return ImGuiKey_Keypad1;
		case GLFW_KEY_KP_2: return ImGuiKey_Keypad2;
		case GLFW_KEY_KP_3: return ImGuiKey_Keypad3;
		case GLFW_KEY_KP_4: return ImGuiKey_Keypad4;
		case GLFW_KEY_KP_5: return ImGuiKey_Keypad5;
		case GLFW_KEY_KP_6: return ImGuiKey_Keypad6;
		case GLFW_KEY_KP_7: return ImGuiKey_Keypad7;
		case GLFW_KEY_KP_8: return ImGuiKey_Keypad8;
		case GLFW_KEY_KP_9: return ImGuiKey_Keypad9;
		case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
		case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
		case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
		case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
		case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
		case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
		case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
		case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
		case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
		case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
		case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
		case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
		case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
		case GLFW_KEY_MENU: return ImGuiKey_Menu;
		case GLFW_KEY_0: return ImGuiKey_0;
		case GLFW_KEY_1: return ImGuiKey_1;
		case GLFW_KEY_2: return ImGuiKey_2;
		case GLFW_KEY_3: return ImGuiKey_3;
		case GLFW_KEY_4: return ImGuiKey_4;
		case GLFW_KEY_5: return ImGuiKey_5;
		case GLFW_KEY_6: return ImGuiKey_6;
		case GLFW_KEY_7: return ImGuiKey_7;
		case GLFW_KEY_8: return ImGuiKey_8;
		case GLFW_KEY_9: return ImGuiKey_9;
		case GLFW_KEY_A: return ImGuiKey_A;
		case GLFW_KEY_B: return ImGuiKey_B;
		case GLFW_KEY_C: return ImGuiKey_C;
		case GLFW_KEY_D: return ImGuiKey_D;
		case GLFW_KEY_E: return ImGuiKey_E;
		case GLFW_KEY_F: return ImGuiKey_F;
		case GLFW_KEY_G: return ImGuiKey_G;
		case GLFW_KEY_H: return ImGuiKey_H;
		case GLFW_KEY_I: return ImGuiKey_I;
		case GLFW_KEY_J: return ImGuiKey_J;
		case GLFW_KEY_K: return ImGuiKey_K;
		case GLFW_KEY_L: return ImGuiKey_L;
		case GLFW_KEY_M: return ImGuiKey_M;
		case GLFW_KEY_N: return ImGuiKey_N;
		case GLFW_KEY_O: return ImGuiKey_O;
		case GLFW_KEY_P: return ImGuiKey_P;
		case GLFW_KEY_Q: return ImGuiKey_Q;
		case GLFW_KEY_R: return ImGuiKey_R;
		case GLFW_KEY_S: return ImGuiKey_S;
		case GLFW_KEY_T: return ImGuiKey_T;
		case GLFW_KEY_U: return ImGuiKey_U;
		case GLFW_KEY_V: return ImGuiKey_V;
		case GLFW_KEY_W: return ImGuiKey_W;
		case GLFW_KEY_X: return ImGuiKey_X;
		case GLFW_KEY_Y: return ImGuiKey_Y;
		case GLFW_KEY_Z: return ImGuiKey_Z;
		case GLFW_KEY_F1: return ImGuiKey_F1;
		case GLFW_KEY_F2: return ImGuiKey_F2;
		case GLFW_KEY_F3: return ImGuiKey_F3;
		case GLFW_KEY_F4: return ImGuiKey_F4;
		case GLFW_KEY_F5: return ImGuiKey_F5;
		case GLFW_KEY_F6: return ImGuiKey_F6;
		case GLFW_KEY_F7: return ImGuiKey_F7;
		case GLFW_KEY_F8: return ImGuiKey_F8;
		case GLFW_KEY_F9: return ImGuiKey_F9;
		case GLFW_KEY_F10: return ImGuiKey_F10;
		case GLFW_KEY_F11: return ImGuiKey_F11;
		case GLFW_KEY_F12: return ImGuiKey_F12;
		case GLFW_KEY_F13: return ImGuiKey_F13;
		case GLFW_KEY_F14: return ImGuiKey_F14;
		case GLFW_KEY_F15: return ImGuiKey_F15;
		case GLFW_KEY_F16: return ImGuiKey_F16;
		case GLFW_KEY_F17: return ImGuiKey_F17;
		case GLFW_KEY_F18: return ImGuiKey_F18;
		case GLFW_KEY_F19: return ImGuiKey_F19;
		case GLFW_KEY_F20: return ImGuiKey_F20;
		case GLFW_KEY_F21: return ImGuiKey_F21;
		case GLFW_KEY_F22: return ImGuiKey_F22;
		case GLFW_KEY_F23: return ImGuiKey_F23;
		case GLFW_KEY_F24: return ImGuiKey_F24;
		default: return ImGuiKey_None;
		}
	}

}

Application::Application(int width, int height)
{
	check(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_ = glfwCreateWindow(width, height, "DeepSolution", nullptr, nullptr);
	check(window_);

	glfwSetWindowUserPointer(window_, this);
	glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddMousePosEvent(xpos, ypos);

		if (io.WantCaptureMouse)
		{
			return;
		}

		auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
		static float xPos = xpos;
		static float yPos = ypos;
		float dX = xpos - xPos;
		float dY = yPos - ypos;

		xPos = xpos;
		yPos = ypos;
		if (!app->isFocused || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) != GLFW_PRESS)
		{
			return;
		}

		app->state_.camera_->moveDirection(dX, dY);
	});
	glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int mods) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseButtonEvent(button, action == GLFW_PRESS);

		if (io.WantCaptureMouse)
		{
			return;
		}
	});
	glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddKeyEvent(GLFWKeyToImGuiKey(key), action == GLFW_PRESS);
	});
	glfwSetScrollCallback(window_, [](GLFWwindow* window, double xoffset, double yoffset) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseWheelEvent(xoffset, yoffset);

		if (io.WantCaptureMouse)
		{
			return;
		}

		auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
		if (!app->isFocused)
		{
			return;
		}
		app->state_.camera_->scrollWheel(xoffset, yoffset);
	});

	state_.camera_ = std::make_unique<ArcballCamera>(glm::vec3(2, 0, 0), glm::vec3(0, 0, 0), float(width), float(height), 0.01f, 50.0f);
	//state_.camera_ = std::make_unique<FreeCamera>(glm::vec3(0, 2, 0), 0.f, -177.f, width, height, 0.01f, 50.0f);

	device_.init(window_);
	
	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	imgui_ = std::make_unique<ImGuiAdapter>(window_, device_);

	//
	swapchain_ = std::make_unique<Swapchain>(device_, device_.getDepthFormat());

	scene_ = std::make_unique<Scene>(device_);
	renderer_ = std::make_unique<Renderer>(device_, *scene_);

	// scene_->loadCubeMap("assets/rostock_laage_airport_4k.hdr");
	scene_->loadCubeMap("assets/metro_noord_4k.hdr", renderer_->getIBRSet());

#define RE(r) \
r->getVertexModule(),\
r->getFragmentModule(),\
r->getImageSet(),\
r->getPipelineLayout()

	// scene_->loadGLTF("assets/subway/scene.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/Waterbottle/glTF/Waterbottle.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/BoomBoxWithAxes/glTF/BoomBoxWithAxes.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/SciFiHelmet/glTF/SciFiHelmet.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
	scene_->loadGLTF("assets/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf", RE(renderer_));
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/Suzanne/glTF/Suzanne.gltf");
}

void Application::run()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();

		// Update Camera
		int width, height;
		glfwGetFramebufferSize(window_, &width, &height);
		state_.camera_->updateViewport(width, height);

		// 
		if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window_, true);
			break;
		}

		static float timeout = 0.f;
		if (timeout <= 0 && glfwGetKey(window_, GLFW_KEY_J) == GLFW_PRESS)
		{
			if (isFocused)
			{
				glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				isFocused = false;
			}
			else
			{
				glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				isFocused = true;
			}
			timeout = 1.0f;
		}


		auto deltaTime = state_.timer_.getDeltaTime();
		timeout -= deltaTime;

		if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
		{
			state_.camera_->movePosition(Direction::Forward, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
		{
			state_.camera_->movePosition(Direction::Backward, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
		{
			state_.camera_->movePosition(Direction::Left, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
		{
			state_.camera_->movePosition(Direction::Right, deltaTime);
		}

		imgui_->BeginFrame();

		ImGui::ShowDemoWindow();

		ImGui::Begin("Lights", &showLightMenu_);
		ImGui::SeparatorText("Lights");
		for (size_t i = 0; i < state_.lights_.size(); i++)
		{
			ImGui::Text("Light [%d]: %s", i, glm::to_string(state_.lights_[i].position).c_str());
		}

		ImGui::SeparatorText("Add light");
		ImGui::SliderFloat3("Position", glm::value_ptr(lightPosition_), -10.0f, 10.0f);
		if (ImGui::Button("Add light"))
		{
			state_.lights_.push_back(Light{ lightPosition_ });
			lightPosition_ = glm::vec3();
		}
		
		ImGui::End();

		imgui_->EndFrame();

		Draw();
	}
}

Application::~Application()
{
	vkDeviceWaitIdle(device_.device);
	
	renderer_.reset();
	scene_.reset();
	
	imgui_.reset();
	swapchain_.reset();

	device_.deinit();
}

void Application::Draw()
{
	// Guard.
	int width, height;
	glfwGetFramebufferSize(window_, &width, &height);
	if (width == 0 || height == 0)
	{
		return;
	}

	// Scene Rendering
	auto commandBuffer = swapchain_->Acquire(width , height);

	Transition::UndefinedToColorAttachment(swapchain_->GetCurrentImage(), commandBuffer, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

	renderer_->draw(commandBuffer, swapchain_->GetCurrentImageView(), swapchain_->GetDepthImageView(), scene_->getDrawables(), state_);

	// ImGui Rendering
	imgui_->Draw(swapchain_->GetCurrentImageView(), swapchain_->GetExtent(), commandBuffer);
	
	// Transition Then Present.
	auto image = swapchain_->GetCurrentImage();
	Transition::ColorAttachmentToPresentable(image, commandBuffer);
	swapchain_->Present();
}
