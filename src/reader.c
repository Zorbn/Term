#include "reader.h"

#include "font.h"

static DWORD WINAPI read_thread_start(void *start_info) {
    struct ReadThreadData *data = start_info;

    struct Grid *grid = data->grid;
    struct Renderer *renderer = data->renderer;
    struct Window *window = data->window;
    struct PseudoConsole *pseudo_console = &window->pseudo_console;

    struct TextBuffer text_buffer = text_buffer_create();

    while (true) {
        if (!ReadFile(
                pseudo_console->output, text_buffer.data, TEXT_BUFFER_CAPACITY, &text_buffer.length, NULL)) {
            break;
        }

        puts("read");

        WaitForSingleObject(data->mutex, INFINITE);

        text_buffer.length += text_buffer.kept_length;
        text_buffer.kept_length = 0;

        window->needs_redraw = true;

        for (size_t i = 0; i < text_buffer.length;) {
            // Skip multi-byte text. Replace it with a box character.
            if (text_buffer.data[i] & 0x80) {
                size_t j = 0;
                while (j < 4 && ((text_buffer.data[i] << j) & 0x80)) {
                    j++;
                }

                size_t end_i = i + j - 1;
                if (end_i >= text_buffer.length) {
                    // The multi-byte utf8 character was split across multiple reads.
                    text_buffer_keep_from_i(&text_buffer, i);
                    break;
                }

                i = end_i;
                text_buffer.data[i] = FONT_LENGTH + 32;
            }

            // Check for escape sequences.
            // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
            // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
            // are assumed to be 1.
            size_t furthest_i = 0;
            if (grid_parse_escape_sequence(grid, &text_buffer, &i, &furthest_i, window)) {
                continue;
            } else if (furthest_i >= text_buffer.length && i < text_buffer.length) {
                // The parse failed due to reaching the end of the buffer, the sequence may have been split
                // across multiple reads.
                text_buffer_keep_from_i(&text_buffer, i);
                break;
            }

            // Parse escape characters:
            if (text_buffer_match_char(&text_buffer, '\r', &i)) {
                grid_cursor_move_to(grid, 0, grid->cursor_y);
                continue;
            }

            if (text_buffer_match_char(&text_buffer, '\n', &i)) {
                if (grid->cursor_y == grid->height - 1) {
                    renderer_scroll_down(renderer, true);
                    grid_scroll_down(grid);
                } else {
                    grid_cursor_move(grid, 0, 1);
                }
                continue;
            }

            if (text_buffer_match_char(&text_buffer, '\b', &i)) {
                grid_cursor_move(grid, -1, 0);
                continue;
            }

            // \a is the alert/bell escape sequence, ignore it.
            if (text_buffer_match_char(&text_buffer, '\a', &i)) {
                continue;
            }

            if (grid->cursor_x >= grid->width) {
                grid_cursor_move_to(grid, 0, grid->cursor_y + 1);
            }
            grid_set_char(grid, grid->cursor_x, grid->cursor_y, text_buffer.data[i]);
            grid->cursor_x++;

            i++;
        }

        ReleaseMutex(data->mutex);
    }

    text_buffer_destroy(&text_buffer);

    return 0;
}

struct Reader reader_create(struct ReadThreadData *read_thread_data) {
    HANDLE read_thread = CreateThread(NULL, 0, read_thread_start, read_thread_data, 0, NULL);
    assert(read_thread);

    return (struct Reader){
        .read_thread = read_thread,
    };
}

void reader_destroy(struct Reader *reader, struct ReadThreadData *read_thread_data) {
    DWORD bytes_available;

    while (PeekNamedPipe(read_thread_data->window->pseudo_console.output, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
    }

    puts("read thread destroyed");

    TerminateThread(reader->read_thread, 0);
    CloseHandle(reader->read_thread);
}