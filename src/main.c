#include "detect_leak.h"

#include "window.h"
#include "graphics/resources.h"
#include "graphics/sprite_batch.h"

#include <cglm/struct.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

const int32_t grid_width = 80;
const int32_t grid_height = 24;

typedef struct {
	HRESULT result;
	HANDLE output, input;
} PseudoConsole;

typedef struct {
	wchar_t *text;
	UINT32 textLength;
	RECT rc;
} Data;

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX *psi) {
	// Prepare Startup Information structure
	STARTUPINFOEX si;
	ZeroMemory(&si, sizeof(si));
	si.StartupInfo.cb = sizeof(STARTUPINFOEX);

	// Discover the size required for the list
	size_t bytesRequired;
	InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

	// Allocate memory to represent the list
	si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
		GetProcessHeap(), 0, bytesRequired);
	if (!si.lpAttributeList) {
		return E_OUTOFMEMORY;
	}

	// Initialize the list memory location
	if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0,
										   &bytesRequired)) {
		HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Set the pseudoconsole information into the list
	if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
								   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc,
								   sizeof(hpc), NULL, NULL)) {
		HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
		return HRESULT_FROM_WIN32(GetLastError());
	}

	*psi = si;

	return S_OK;
}

PseudoConsole SetUpPseudoConsole(COORD size) {
	HRESULT hr = S_OK;

	// - Close these after CreateProcess of child application with
	// pseudoconsole object.
	HANDLE inputReadSide, outputWriteSide;

	// - Hold onto these and use them for communication with the child
	// through the pseudoconsole.
	HANDLE outputReadSide, inputWriteSide;

	if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
		return (PseudoConsole){
			HRESULT_FROM_WIN32(GetLastError()),
			NULL,
			NULL,
		};
	}

	if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
		return (PseudoConsole){
			HRESULT_FROM_WIN32(GetLastError()),
			NULL,
			NULL,
		};
	}

	HPCON hPC;
	hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &hPC);
	if (FAILED(hr)) {
		return (PseudoConsole){
			hr,
			NULL,
			NULL,
		};
	}

	STARTUPINFOEX siEx;
	PrepareStartupInformation(hPC, &siEx);

	// PCWSTR childApplication = L"C:\\windows\\system32\\cmd.exe";
	// PCWSTR childApplication = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe";
	PCWSTR childApplication = L"python test.py";

	// Create mutable text string for CreateProcessW command line string.
	const size_t charsRequired =
		wcslen(childApplication) + 1; // +1 null terminator
	PWSTR cmdLineMutable =
		(PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

	if (!cmdLineMutable) {
		return (PseudoConsole){
			E_OUTOFMEMORY,
			NULL,
			NULL,
		};
	}

	wcscpy_s(cmdLineMutable, charsRequired, childApplication);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	// Call CreateProcess
	if (!CreateProcessW(NULL, cmdLineMutable, NULL, NULL, FALSE,
						EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
						(LPSTARTUPINFOW)(&siEx.StartupInfo), &pi)) {
		HeapFree(GetProcessHeap(), 0, cmdLineMutable);
		return (PseudoConsole){
			HRESULT_FROM_WIN32(GetLastError()),
			NULL,
			NULL,
		};
	}

	return (PseudoConsole){
		hr,
		outputReadSide,
		inputWriteSide,
	};
}

struct Grid {
    wchar_t *data;
    size_t width;
    size_t height;
    size_t size;
    int32_t cursor_x;
    int32_t cursor_y;
};

struct Grid grid_create(size_t width, size_t height) {
    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(grid_width * grid_height * sizeof(wchar_t)),
        .width = width,
        .height = height,
    };
    assert(grid.data);

    for (size_t i = 0; i < size; i++) {
        grid.data[i] = L' ';
    }

    return grid;
}

void grid_destroy(struct Grid *grid) {
    free(grid->data);
}

void grid_move_cursor(struct Grid *grid, int32_t delta_x, int32_t delta_y) {
    grid->cursor_x += delta_x;

    if (grid->cursor_x < 0) {
        // TODO: Is this right, or should it loop around?
        grid->cursor_x = 0;
    }

    // TODO: This is a hack, we are tying the visible cursor position and the actual cursor position together.
    // That will cause problems when implementing resizing, scrollback, etc. Maybe replace this with proper line wrap later.
    while (grid->cursor_x >= grid->width) {
        grid->cursor_x -= grid->width;
        grid->cursor_y++;
    }

    grid->cursor_y += delta_y;

    // TODO: Handle scrolling.
}

void draw_character(struct SpriteBatch *sprite_batch, wchar_t character, int32_t x, int32_t y, int32_t origin_y) {
    if (character < 33 || character > 126) {
        return;
    }

    sprite_batch_add(sprite_batch, (struct Sprite){
                                        .x = x * 6,
                                        .y = origin_y - (y + 1) * 14,
                                        .width = 6,
                                        .height = 14,
                                        .texture_x = 8 * (character - 32),
                                        .texture_width = 6,
                                        .texture_height = 14,
                                    });
}

int main() {
    struct Window window = window_create("CBlock", 640, 480);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    uint32_t program_2d = program_create("assets/shader_2d.vert", "assets/shader_2d.frag");

    // TODO: Add texture_bind and texture_destroy
    struct Texture texture_atlas_2d = texture_create("assets/texture_atlas.png");

    struct SpriteBatch sprite_batch = sprite_batch_create(16);

    mat4s projection_matrix_2d;

    int32_t projection_matrix_location_2d = glGetUniformLocation(program_2d, "projection_matrix");

    Data data = {0};
    data.text = NULL;
    data.textLength = 0;
    PseudoConsole console = SetUpPseudoConsole((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", console.result);
    struct Grid grid = grid_create(grid_width, grid_height);

    DWORD bytesAvailable;
    CHAR chBuf[1024];

    double last_frame_time = glfwGetTime();
    float fps_print_timer = 0.0f;

    float elapsed_time = 0.0f;

    while (!glfwWindowShouldClose(window.glfw_window)) {
        // Update:
        if (window.was_resized) {
            glViewport(0, 0, window.width, window.height);
            projection_matrix_2d = glms_ortho(0.0f, (float)window.width, 0.0f, (float)window.height, -100.0, 100.0);

            window.was_resized = false;
        }

        double current_frame_time = glfwGetTime();
        float delta_time = (float)(current_frame_time - last_frame_time);
        last_frame_time = current_frame_time;

        fps_print_timer += delta_time;

        if (fps_print_timer > 1.0f) {
            fps_print_timer = 0.0f;
            printf("fps: %f\n", 1.0f / delta_time);
        }

        elapsed_time += delta_time;
        float time_of_day = 0.5f * (sin(elapsed_time * 0.1f) + 1.0f);

        sprite_batch_begin(&sprite_batch);

        // START HANDLE TERMINAL

        PeekNamedPipe(console.output, NULL, 0, NULL, &bytesAvailable, NULL);
        if (bytesAvailable > 0) {
            int32_t bytesToRead = 1024;
            DWORD dwRead;
            BOOL success = ReadFile(console.output, chBuf, bytesToRead, &dwRead, NULL);
            if (success && dwRead != 0) {
                data.text = (wchar_t *)malloc((dwRead + 1) * sizeof(wchar_t));
                assert(data.text);
                size_t out;
                mbstowcs_s(&out, data.text, dwRead + 1, chBuf, dwRead);
                data.textLength = dwRead + 1;

                for (size_t i = 0; i < data.textLength;) {
                    // Check for escape sequences. https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
                    // If <n> is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they are assumed to be 1.
                    size_t start_i = i;
                    if (data.text[i] == L'\x1b') {
                        i++;

                        if (data.text[i] == L'[') {
                            i++;
                            if (data.text[i] == L'm') {
                                i++;
                                // TODO: Reset colors.
                                continue;
                            }

                            while (iswdigit(data.text[i])) {
                                i++;
                            }

                            if (data.text[i] == L'm') {
                                i++;
                                // TODO: Change colors.
                                continue;
                            }

                            if (data.text[i] == L'C') {
                                i++;
                                // TODO: Move by the parsed number.
                                grid_move_cursor(&grid, 1, 0);
                                continue;
                            }
                        }

                        // This sequence is invalid, ignore it.
                        i = start_i;
                    }

                    if (data.text[i] == L'\n') {
                        grid.cursor_x = 0;
                        grid.cursor_y++;
                    }

                    grid.data[grid.cursor_x + grid.cursor_y * grid_width] = data.text[i];
                    grid_move_cursor(&grid, 1, 0);

                    i++;
                }
            }
        }

        for (size_t y = 0; y < grid_height; y++) {
            for (size_t x = 0; x < grid_width; x++) {
                size_t i = x + y * grid_width;
                draw_character(&sprite_batch, grid.data[i], x, y, window.height);
            }
        }

        // END HANDLE TERMINAL

        sprite_batch_end(&sprite_batch, texture_atlas_2d.width, texture_atlas_2d.height);

        // Draw:
        glClearColor(sky_color_r * time_of_day, sky_color_g * time_of_day, sky_color_b * time_of_day, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_2d);
        glUniformMatrix4fv(projection_matrix_location_2d, 1, GL_FALSE, (const float *)&projection_matrix_2d);
        glBindTexture(GL_TEXTURE_2D, texture_atlas_2d.id);
        sprite_batch_draw(&sprite_batch);

        window_update(&window);

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();
    }

    grid_destroy(&grid);

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
