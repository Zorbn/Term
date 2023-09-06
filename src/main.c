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

/*
 * TODO:
 * Support setting fg and bg colors, Micro uses this to draw the cursor.
 * Support arrows, ctrl, alt, and repeating keys like backspace by holding them down.
 * Unicode characters and box drawing characters aren't handled well,
 * Dynamic resizing.
 * (ie: Helix breaks when it shows a unicode animation after opening a file, or when it does box drawing while typing a
 * command).
 */

/*
 * Missing features:
 * Mouse input,
 * Copy/paste,
 * Scrollback,
 */

// TODO: Is this too big for the buffer? Is there a way to make it smaller while not splitting escape codes and failing
// to process them?
#define READ_BUFFER_SIZE 8192

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

typedef struct {
    HRESULT result;
    HPCON hpc;
    HANDLE h_process;
    HANDLE output, input;
} PseudoConsole;

typedef struct {
    wchar_t *text;
    UINT32 textLength;
    RECT rc;
} Data;

bool data_match_char(Data *data, wchar_t character, size_t *i) {
    if (*i >= data->textLength) {
        return false;
    }

    if (data->text[*i] != character) {
        return false;
    }

    *i += 1;
    return true;
}

bool data_peek_digit(Data *data, size_t i) {
    if (i >= data->textLength) {
        return false;
    }

    return iswdigit(data->text[i]);
}

void data_destroy(Data *data) {
    free(data->text);
}

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX *psi) {
    // Prepare Startup Information structure
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    // Discover the size required for the list
    size_t bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(
            si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL)) {
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
            NULL,
            NULL,
        };
    }

    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    HPCON hpc;
    hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &hpc);
    if (FAILED(hr)) {
        return (PseudoConsole){
            hr,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    STARTUPINFOEX siEx;
    PrepareStartupInformation(hpc, &siEx);

    // PCWSTR childApplication = L"C:\\windows\\system32\\cmd.exe";
    PCWSTR childApplication = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe";
    // PCWSTR childApplication = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe -c \"nvim --clean\"";
    // PCWSTR childApplication = L"python test.py";

    // Create mutable text string for CreateProcessW command line string.
    const size_t charsRequired = wcslen(childApplication) + 1; // +1 null terminator
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

    if (!cmdLineMutable) {
        return (PseudoConsole){
            E_OUTOFMEMORY,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Call CreateProcess
    if (!CreateProcessW(NULL, cmdLineMutable, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
            (LPSTARTUPINFOW)(&siEx.StartupInfo), &pi)) {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    return (PseudoConsole){
        hr,
        hpc,
        pi.hProcess,
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

    int32_t saved_cursor_x;
    int32_t saved_cursor_y;

    bool show_cursor;
};

struct Grid grid_create(size_t width, size_t height) {
    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(width * height * sizeof(wchar_t)),
        .width = width,
        .height = height,
        .size = size,
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

void grid_scroll_down(struct Grid *grid) {
    memmove(&grid->data[0], &grid->data[grid->width], (grid->size - grid->width) * sizeof(wchar_t));

    size_t new_row_start_i = (grid->height - 1) * grid->width;
    for (size_t i = 0; i < grid->width; i++) {
        grid->data[i + new_row_start_i] = L' ';
    }
}

void grid_char_set(struct Grid *grid, int32_t x, int32_t y, wchar_t character) {
    if (x < 0 || y < 0 || x >= grid->width || y >= grid->height) {
        return;
    }

    grid->data[x + y * grid->width] = character;
}

void grid_cursor_save(struct Grid *grid) {
    grid->saved_cursor_x = grid->cursor_x;
    grid->saved_cursor_y = grid->cursor_y;
}

void grid_cursor_restore(struct Grid *grid) {
    grid->cursor_x = grid->saved_cursor_x;
    grid->cursor_y = grid->saved_cursor_y;
}

void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y) {
    grid->cursor_x = x;
    grid->cursor_y = y;

    if (grid->cursor_x < 0) {
        grid->cursor_x = 0;
    }

    if (grid->cursor_x >= grid->width) {
        grid->cursor_x = grid->width - 1;
    }

    if (grid->cursor_y >= grid->height) {
        grid->cursor_y = grid->height - 1;
    }
}

void grid_cursor_move(struct Grid *grid, int32_t delta_x, int32_t delta_y) {
    grid_cursor_move_to(grid, grid->cursor_x + delta_x, grid->cursor_y + delta_y);
}

// Returns true if an escape sequence was parsed.
bool grid_parse_escape_sequence(struct Grid *grid, Data *data, size_t *i, struct Window *window) {
    size_t start_i = *i;
    if (!data_match_char(data, L'\x1b', i)) {
        *i = start_i;
        return false;
    }

    // Simple cursor positioning:
    if (data_match_char(data, L'7', i)) {
        grid_cursor_save(grid);
        return true;
    }

    if (data_match_char(data, L'8', i)) {
        grid_cursor_restore(grid);
        return true;
    }

    if (data_match_char(data, L'H', i)) {
        // TODO:
        puts("set tab stop");
        return true;
    }

    // Operating system commands:
    if (data_match_char(data, L']', i)) {
        bool has_zero_or_two = data_match_char(data, L'0', i) || data_match_char(data, L'2', i);
        if (!has_zero_or_two || !data_match_char(data, L';', i)) {
            *i = start_i;
            return false;
        }

        // Window titles can be at most 255 characters.
        for (size_t title_length = 0; title_length < 255; title_length++) {
            size_t peek_i = *i + title_length;
            bool has_bel = data_match_char(data, L'\x7', &peek_i);
            bool has_terminator =
                has_bel || (data_match_char(data, L'\x1b', &peek_i) && data_match_char(data, L'\x5c', &peek_i));
            if (!has_terminator) {
                continue;
            }

            size_t title_string_length = title_length + 1;
            char *title = malloc(title_string_length);
            wcstombs_s(NULL, title, title_string_length, data->text + *i, title_length);

            glfwSetWindowTitle(window->glfw_window, (char *)title);

            free(title);

            *i = peek_i;
            return true;
        }
    }

    // Control sequence introducers:
    if (data_match_char(data, L'[', i)) {
        bool starts_with_question_mark = data_match_char(data, L'?', i);

        // TODO: Make sure when parsing sequences that they don't have more numbers supplied then they allow, ie: no
        // ESC[10;5A because A should only accept one number.
        // The maximum amount of numbers supported is 16, for the "m" commands (text formatting).
        int32_t parsed_numbers[16] = {0};
        size_t parsed_number_count = 0;

        for (size_t parsed_number_i = 0; parsed_number_i < 16; parsed_number_i++) {
            bool did_parse_number = false;
            while (data_peek_digit(data, *i)) {
                did_parse_number = true;
                int32_t digit = data->text[*i] - L'0';
                parsed_numbers[parsed_number_i] = parsed_numbers[parsed_number_i] * 10 + digit;
                *i += 1;
            }

            if (did_parse_number) {
                parsed_number_count++;
            } else {
                break;
            }

            if (!data_match_char(data, L';', i)) {
                break;
            }
        }

        // Cursor visibility:
        if (starts_with_question_mark) {
            // Unrecognized numbers here are just ignored, since they are sometimes
            // sent by programs trying to change the mouse mode or other things that we don't support.
            bool should_show_hide_cursor = parsed_number_count == 1 && parsed_numbers[0] == 25;

            if (data_match_char(data, L'h', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = true;
                }
                return true;
            }

            if (data_match_char(data, L'l', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = false;
                }
                return true;
            }

            if (data_match_char(data, L'u', i)) {
                return true;
            }

            *i = start_i;
            return false;
        }

        // Cursor shape:
        if (data_match_char(data, L' ', i)) {
            if (data_match_char(data, L'q', i)) {
                // TODO: Change cursor shape.
                return true;
            }

            *i = start_i;
            return false;
        }

        // Text formatting:
        {
            if (data_match_char(data, L'm', i)) {
                // TODO: Change format.
                return true;
            }
        }

        // Cursor positioning:
        {
            if (parsed_number_count == 0) {
                if (data_match_char(data, L's', i)) {
                    grid_cursor_save(grid);
                    return true;
                }

                if (data_match_char(data, L'u', i)) {
                    grid_cursor_restore(grid);
                    return true;
                }
            }

            if (parsed_number_count < 2) {
                int32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 1;

                // Up:
                if (data_match_char(data, L'A', i)) {
                    grid_cursor_move(grid, 0, -n);
                    return true;
                }

                // Down:
                if (data_match_char(data, L'B', i)) {
                    grid_cursor_move(grid, 0, n);
                    return true;
                }

                // Backward:
                if (data_match_char(data, L'D', i)) {
                    grid_cursor_move(grid, -n, 0);
                    return true;
                }

                // Forward:
                if (data_match_char(data, L'C', i)) {
                    grid_cursor_move(grid, n, 0);
                    return true;
                }

                // Horizontal absolute:
                if (data_match_char(data, L'G', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, n - 1, grid->cursor_y);
                    }
                    return true;
                }

                // Vertical absolute:
                if (data_match_char(data, L'd', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, grid->cursor_x, n - 1);
                    }
                    return true;
                }
            }

            int32_t y = parsed_number_count > 0 ? parsed_numbers[0] : 1;
            int32_t x = parsed_number_count > 1 ? parsed_numbers[1] : 1;

            // Cursor position or horizontal vertical position:
            if (data_match_char(data, L'H', i) || data_match_char(data, L'f', i)) {
                if (x > 0 && y > 0) {
                    grid_cursor_move_to(grid, x - 1, y - 1);
                }
                return true;
            }
        }

        // Text modification:
        {
            int32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 0;

            if (data_match_char(data, L'X', i)) {
                int32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
                int32_t erase_count = grid->size - erase_start;
                if (n < erase_count) {
                    erase_count = n;
                }

                for (size_t erase_i = 0; erase_i < erase_count; erase_i++) {
                    grid->data[erase_start + erase_i] = L' ';
                }

                return true;
            }

            switch (n) {
                case 0: {
                    // Erase line after cursor.
                    if (data_match_char(data, L'K', i)) {
                        for (size_t x = grid->cursor_x; x < grid->width; x++) {
                            grid_char_set(grid, x, grid->cursor_y, L' ');
                        }

                        return true;
                    }

                    break;
                }
                case 2: {
                    if (data_match_char(data, L'J', i)) {
                        // Erase entire display.
                        for (size_t y = 0; y < grid->height; y++) {
                            for (size_t x = 0; x < grid->width; x++) {
                                grid_char_set(grid, x, y, L' ');
                            }
                        }

                        return true;
                    }

                    break;
                }
            }
        }
    }

    // This sequence is invalid, ignore it.
    *i = start_i;
    return false;
}

void grid_draw_character(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z,
    int32_t origin_y, float r, float g, float b) {

    wchar_t character = grid->data[x + y * grid->width];
    if (character < 33 || character > 126) {
        return;
    }

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 8 * (character - 32),
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, int32_t origin_y) {
    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 0,
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = 1.0f,
                                       .g = 1.0f,
                                       .b = 1.0f,
                                   });

    grid_draw_character(grid, sprite_batch, x, y, z + 1, origin_y, 0.0f, 0.0f, 0.0f);
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
    const size_t data_text_capacity = READ_BUFFER_SIZE + 1;
    data.text = malloc(data_text_capacity * sizeof(wchar_t));
    assert(data.text);
    data.textLength = 0;

    const int32_t grid_width = window.width / 6;
    const int32_t grid_height = window.height / 14;
    PseudoConsole console = SetUpPseudoConsole((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", console.result);
    struct Grid grid = grid_create(grid_width, grid_height);

    CHAR read_buffer[READ_BUFFER_SIZE];

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

        // START HANDLE TERMINAL
        for (size_t i = 0; i < window.typed_chars.length; i++) {
            CHAR write_buffer[1];
            write_buffer[0] = (CHAR)window.typed_chars.data[i];
            WriteFile(console.input, write_buffer, 1, NULL, NULL);
        }

        // TODO: Simplify this logic and support more keys, i'm just doing this for testing.
        if (input_is_button_pressed(&window.input, GLFW_KEY_ENTER)) {
            CHAR write_buffer[1];
            write_buffer[0] = '\r';
            WriteFile(console.input, write_buffer, 1, NULL, NULL);
        }

        if (input_is_button_pressed(&window.input, GLFW_KEY_ESCAPE)) {
            CHAR write_buffer[1];
            write_buffer[0] = '\x1b';
            WriteFile(console.input, write_buffer, 1, NULL, NULL);
        }

        if (input_is_button_pressed(&window.input, GLFW_KEY_BACKSPACE)) {
            CHAR write_buffer[1];
            write_buffer[0] = '\x7f';
            WriteFile(console.input, write_buffer, 1, NULL, NULL);
        }

        if (input_is_button_pressed(&window.input, GLFW_KEY_TAB)) {
            CHAR write_buffer[1];
            write_buffer[0] = '\t';
            WriteFile(console.input, write_buffer, 1, NULL, NULL);
        }

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        DWORD bytes_available;
        PeekNamedPipe(console.output, NULL, 0, NULL, &bytes_available, NULL);

        if (bytes_available > 0) {
            DWORD dwRead;
            BOOL did_read = ReadFile(console.output, read_buffer, READ_BUFFER_SIZE, &dwRead, NULL);
            if (did_read && dwRead != 0) {
                size_t out;
                mbstowcs_s(&out, data.text, data_text_capacity, read_buffer, dwRead);
                data.textLength = dwRead;

                for (size_t i = 0; i < data.textLength;) {
                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    if (grid_parse_escape_sequence(&grid, &data, &i, &window)) {
                        continue;
                    }

                    // Parse escape characters:
                    if (data_match_char(&data, L'\r', &i)) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y);
                        continue;
                    }

                    if (data_match_char(&data, L'\n', &i)) {
                        if (grid.cursor_y == grid.height - 1) {
                            grid_scroll_down(&grid);
                        } else {
                            grid.cursor_y++;
                        }
                        continue;
                    }

                    if (data_match_char(&data, L'\b', &i)) {
                        grid_cursor_move(&grid, -1, 0);
                        continue;
                    }

                    if (grid.cursor_x >= grid.width) {
                        grid.cursor_x = 0;
                        grid.cursor_y++;
                    }
                    grid_char_set(&grid, grid.cursor_x, grid.cursor_y, data.text[i]);
                    grid.cursor_x++;

                    i++;
                }
            }
        }

        sprite_batch_begin(&sprite_batch);

        for (size_t y = 0; y < grid.height; y++) {
            for (size_t x = 0; x < grid.width; x++) {
                grid_draw_character(&grid, &sprite_batch, x, y, 0, window.height, 1.0f, 1.0f, 1.0f);
            }
        }

        if (grid.show_cursor) {
            grid_draw_cursor(&grid, &sprite_batch, grid.cursor_x, grid.cursor_y, 1, window.height);
        }

        sprite_batch_end(&sprite_batch, texture_atlas_2d.width, texture_atlas_2d.height);

        // END HANDLE TERMINAL

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

    ClosePseudoConsole(console.hpc);

    grid_destroy(&grid);
    data_destroy(&data);

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
