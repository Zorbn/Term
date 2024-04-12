#include "reader.h"

#include "font.h"

static DWORD WINAPI read_thread_start(void *start_info) {
    struct ReadThreadData *data = start_info;

    struct Grid *grid = data->grid;
    struct Renderer *renderer = data->renderer;
    struct PseudoConsole *pseudo_console = data->pseudo_console;
    struct TextBuffer *text_buffer = &data->text_buffer;

    while (true) {
        if (!ReadFile(pseudo_console->output, text_buffer->data, TEXT_BUFFER_CAPACITY, &text_buffer->length, NULL)) {
            break;
        }

        WaitForSingleObject(data->mutex, INFINITE);

        text_buffer->length += text_buffer->kept_length;
        text_buffer->kept_length = 0;

        data->needs_redraw = true;
        SetEvent(data->event);

        for (size_t i = 0; i < text_buffer->length;) {
            // Skip multi-byte text. Replace it with a box character.
            if (text_buffer->data[i] & 0x80) {
                size_t j = 0;
                while (j < 4 && ((text_buffer->data[i] << j) & 0x80)) {
                    j++;
                }

                size_t end_i = i + j - 1;
                if (end_i >= text_buffer->length) {
                    // The multi-byte utf8 character was split across multiple reads.
                    text_buffer_keep_from_i(text_buffer, i);
                    break;
                }

                i = end_i;
                text_buffer->data[i] = FONT_LENGTH + 32;
            }

            // Check for escape sequences.
            // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
            // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
            // are assumed to be 1.
            size_t furthest_i = 0;
            if (grid_parse_escape_sequence(grid, text_buffer, &data->title_buffer, &i, &furthest_i)) {
                continue;
            } else if (furthest_i >= text_buffer->length && i < text_buffer->length) {
                // The parse failed due to reaching the end of the buffer, the sequence may have been split
                // across multiple reads.
                text_buffer_keep_from_i(text_buffer, i);
                break;
            }

            // Parse escape characters:
            if (text_buffer_match_char(text_buffer, '\r', &i)) {
                grid_cursor_move_to(grid, 0, grid->cursor_y);
                continue;
            }

            if (text_buffer_match_char(text_buffer, '\n', &i)) {
                if (grid->cursor_y == grid->height - 1) {
                    renderer_scroll_down(renderer, true);
                    grid_scroll_down(grid);
                } else {
                    grid_cursor_move(grid, 0, 1);
                }
                continue;
            }

            if (text_buffer_match_char(text_buffer, '\b', &i)) {
                grid_cursor_move(grid, -1, 0);
                continue;
            }

            // \a is the alert/bell escape sequence, ignore it.
            if (text_buffer_match_char(text_buffer, '\a', &i)) {
                continue;
            }

            if (grid->cursor_x >= grid->width) {
                grid_cursor_move_to(grid, 0, grid->cursor_y + 1);
            }
            grid_set_char(grid, grid->cursor_x, grid->cursor_y, text_buffer->data[i]);
            grid->cursor_x++;

            i++;
        }

        ReleaseMutex(data->mutex);
    }

    return 0;
}

struct ReadThreadData read_thread_data_create(struct Grid *grid, struct PseudoConsole *pseudo_console, struct Renderer *renderer) {
    struct ReadThreadData read_thread_data = (struct ReadThreadData){
        .grid = grid,
        .pseudo_console = pseudo_console,
        .renderer = renderer,
        .text_buffer = text_buffer_create(),
        .mutex = CreateMutex(NULL, false, NULL),
        .event = CreateEvent(NULL, false, false, NULL),
    };

    assert(read_thread_data.mutex);
    assert(read_thread_data.event);

    return read_thread_data;
}

void read_thread_data_destroy(struct ReadThreadData *read_thread_data) {
    CloseHandle(read_thread_data->mutex);
    CloseHandle(read_thread_data->event);

    text_buffer_destroy(&read_thread_data->text_buffer);
}

void read_thread_data_lock(struct ReadThreadData *read_thread_data) {
    WaitForSingleObject(read_thread_data->mutex, INFINITE);
}

void read_thread_data_unlock(struct ReadThreadData *read_thread_data) {
    ReleaseMutex(read_thread_data->mutex);
}

struct Reader reader_create(struct ReadThreadData *read_thread_data) {
    HANDLE read_thread = CreateThread(NULL, 0, read_thread_start, read_thread_data, 0, NULL);
    assert(read_thread);

    return (struct Reader){
        .read_thread = read_thread,
    };
}

void reader_destroy(struct Reader *reader) {
    TerminateThread(reader->read_thread, 0);
    CloseHandle(reader->read_thread);
}