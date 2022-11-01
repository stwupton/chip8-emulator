#include <cassert>
#include <cstdio>
#include <cstring>
#include <errno.h>

#include <Windows.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

const u8 font_data[] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

template <typename T, size_t Size>
struct Stack {
  size_t head = 0;
  T data[Size];

  T &push(const T &value) {
    assert(this->head < Size);
    return this->data[this->head++] = value;
  }

  T &pop() {
    assert(this->head > 0);
    return this->data[--this->head];
  }
};

struct Cpu {
  u8 game_memory[0xfff];
  u32 screen_data[32][64];
  u8 registers[16];
  u16 address_i = 0;
  u16 program_counter = 0x200;
  Stack<u16, 32> stack; // NOTE(steven): Not sure how big the stack should be
  u8 delay_timer;
  u8 sound_timer;

  Cpu() {
    memcpy(&this->game_memory[0x50], font_data, sizeof(font_data));
  }
};

u16 get_op(u8 *from) {
  u16 op = *from;
  op <<= 8;
  op |= *(++from);
  return op;
}

typedef void (*Op_Instruction)(Cpu *cpu, u16 op);

void x0(Cpu *cpu, u16 op) {
  switch (op & 0xff) {
    // CLS
    case 0xe0: {
      memset(cpu->screen_data, 0, sizeof(cpu->screen_data));
    } break;

    // RET
    case 0xee: {
      cpu->program_counter = cpu->stack.pop();
    } break;
  }
}

// JP addr
void x1NNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->program_counter = nnn;
}

// LD Vx, nn
void x6XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  cpu->registers[x] = nn;
}

// ADD Vx, nn
void x7XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  cpu->registers[x] += nn;
}

// LD I, addr
void xANNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->address_i = nnn;
}

// DRW Vx, Vy, n
void xDXYN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 xn = cpu->registers[x];

  const u8 y = (u8)(op >> 4) & 0x0f;
  const u8 yn = cpu->registers[y];

  const u8 n = (u8)op & 0x0f;

  for (u8 row = 0; row < n; row++) {
    const u8 row_data = cpu->game_memory[cpu->address_i + row];
    for (u8 column = 0; column < 8; column++) {
      // TODO(steven): XOR pixels and set collision flag
      u8 pixel = row_data & (0x80 >> column);
      cpu->screen_data[yn + row][xn + column] = pixel > 0 ? 0xffffff : 0;
    }
  }
}

static bool should_close = false;

LRESULT CALLBACK event_handler(
  HWND window_handle,
	UINT message,
	WPARAM w_param,
	LPARAM l_param
) {
	INT result = 0;
	switch (message) {
		case WM_CLOSE: {
			should_close = true;
			DestroyWindow(window_handle);
		} break;

		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;

		case WM_KEYDOWN: {
			if (w_param == VK_ESCAPE) {
				PostMessage(window_handle, WM_CLOSE, NULL, NULL);
			}
		} break;

		default: {
			result = DefWindowProc(window_handle, message, w_param, l_param);
		}
	}
	return result;
}

HWND create_window(HINSTANCE instance_handle, INT show_flag) {
	const LPCWSTR class_name = L"8CHIP";

	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.lpfnWndProc = event_handler;
	window_class.hInstance = instance_handle;
	window_class.lpszClassName = class_name;
	window_class.style = CS_OWNDC;
	RegisterClassEx(&window_class);

	const HWND window_handle = CreateWindowEx(
		0,
		class_name,
		L"8Chip Emulator",
		WS_POPUP | WS_VISIBLE,
		0,
		0,
		// TODO(steven): Get actual monitor size
		1920,
		1080,
		NULL,
		NULL,
		instance_handle,
		nullptr
	);
	assert(window_handle != NULL);

	ShowWindow(window_handle, show_flag);
  return window_handle;
}

void render(Cpu *cpu, HWND window_handle, HDC device_context) {
  RECT client;
  GetClientRect(window_handle, &client);

  const LONG client_x = client.left;
  const LONG client_y = client.top;
  const LONG client_width = client.right - client.left;
  const LONG client_height = client.bottom - client.top;

  const LONG source_x = 0;
  const LONG source_y = 0;
  const LONG source_width = 64;
  const LONG source_height = 32;

  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = source_width;
  bitmap_info.bmiHeader.biHeight = -source_height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  bitmap_info.bmiHeader.biSizeImage = sizeof(cpu->screen_data);

  StretchDIBits(
    device_context, 
    client_x, 
    client_y, 
    client_width, 
    client_height, 
    source_x, 
    source_y, 
    source_width, 
    source_height,
    cpu->screen_data,
    &bitmap_info,
    DIB_RGB_COLORS,
    SRCCOPY
  );
}

INT WINAPI wWinMain(
  HINSTANCE instance_handle,
	HINSTANCE prev_instance_handle,
	PWSTR cmd_args,
	INT show_flags
) {
  HWND window_handle = create_window(instance_handle, show_flags);
  HDC device_context = GetDC(window_handle);

  Cpu *cpu = new Cpu();

  Op_Instruction instructions[0x10] = {};
  instructions[0x00] = &x0;
  instructions[0x01] = &x1NNN;
  instructions[0x06] = &x6XNN;
  instructions[0x07] = &x7XNN;
  instructions[0x0a] = &xANNN;
  instructions[0x0d] = &xDXYN;

  FILE *file;

  errno = fopen_s(&file, "./rom/IBM Logo.ch8", "rb");
  assert(errno == 0);

  const u16 read_buffer_size = 0xfff - 0x200;
  fread_s(&cpu->game_memory[0x200], read_buffer_size, 1, read_buffer_size, file);
  assert(errno == 0);

  fclose(file);
  assert(errno == 0);

  MSG message = {};
  while (!should_close) {
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }

    u16 op = get_op(&cpu->game_memory[cpu->program_counter]);
    cpu->program_counter += 2;

    Op_Instruction instruction = instructions[(op & 0xf000) >> 12];    
    assert(instruction != nullptr);

    if (instruction != nullptr) {
      instruction(cpu, op);
    }

    const bool screen_data_changed = (op & 0xf000) == 0xd000 || op == 0x00e0;
    if (screen_data_changed) {
      render(cpu, window_handle, device_context);
    }

    if (cpu->program_counter >= 0xfff) {
      break;
    }
  }

  ReleaseDC(window_handle, device_context);

  return 0;
}