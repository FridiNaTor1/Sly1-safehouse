// SafeHouse builds only (CANE_SAFEHOUSE): Sly1-Release --gl-selftest, the renderer's start without a disc.
//
// The window, its context (OpenGL, Vulkan through SafeHouse's GL layer, or Metal on macOS / iOS) and every program
// GL::InitGL builds, then 30 presented frames of a known colour with the present quad drawn over half the window, the
// window's centre read back where the renderer allows it. Exit 0 when nothing failed: a program that did not build
// throws in InitGL, and a GL error left by the start or the frames fails the test. For platforms whose renderer the
// development machine cannot run (Metal: CI on a Mac), as Sly 2's --gl-selftest.
#include "../gl.h"
#include "../glshaders.h"
#include "../font.h"
#include <cstdio>

int ShGlSelfTest()
{
	g_gl.InitGL();
	int failures = 0;
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		std::printf("gl-selftest: GL error 0x%x after the renderer's start\n", error);
		++failures;
	}
	std::printf("gl-selftest: renderer %s, %s\n", SzRendererName(),
		reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	for (int frame = 0; frame < 30; ++frame)
	{
		glfwPollEvents();
		glfwGetFramebufferSize(g_gl.window, &framebufferWidth, &framebufferHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, framebufferWidth, framebufferHeight);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		// (the present program and quad over the left half: white)
		glViewport(0, 0, framebufferWidth / 2, framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glScreenShader.Use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, whiteTex);
		glBindVertexArray(g_gl.sao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		cane::gl::SwapBuffers(g_gl.window);
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::printf("gl-selftest: GL error 0x%x in frame %d\n", error, frame);
			++failures;
			break;
		}
	}
	std::printf("gl-selftest: %dx%d, %s\n", framebufferWidth, framebufferHeight, failures == 0 ? "passed" : "FAILED");
	g_gl.TerminateGL();
	return failures == 0 ? 0 : 1;
}
