#include <cassert>
#include <cstdio>
#include <errno.h>

#include <Windows.h>
#include <xaudio2.h>
#include <comdef.h>

#include "src/cpu.hpp"
#include "src/instructions.hpp"
#include "src/types.hpp"

#define TIMER_INTERVAL 10000000
#define SIMULATIONS_PER_FRAME 20
#define SPEED 60

#define LOG(x, ...) {                  \
  WCHAR _buffer[500];                  \
  swprintf_s(_buffer, x, __VA_ARGS__); \
  OutputDebugString(_buffer);          \
}

#define ASSERT_HRESULT(result) {                                \
  if (!SUCCEEDED(result)) {                                     \
    _com_error _error(result);                                  \
    LOG(L"HRESULT Error Message: %s\n", _error.ErrorMessage()); \
    assert(SUCCEEDED(result));                                  \
  }                                                             \
}

const WCHAR input_map[] = {
  L'x', // 0
  L'1', // 1
  L'2', // 2
  L'3', // 3
  L'Q', // 4
  L'W', // 5
  L'E', // 6
  L'A', // 7
  L'S', // 8
  L'D', // 9
  L'Z', // A
  L'X', // B
  L'4', // C
  L'R', // D
  L'F', // E
  L'v', // F
};

LRESULT CALLBACK event_handler(
  HWND window_handle,
  UINT message,
  WPARAM w_param,
  LPARAM l_param
) {
  INT result = 0;
  switch (message) {
    case WM_CREATE: {
      CREATESTRUCT *create_struct = (CREATESTRUCT *)l_param;
      SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR)create_struct->lpCreateParams);
    } break;

    case WM_CLOSE: {
      Cpu *cpu = (Cpu*)GetWindowLongPtr(window_handle, GWLP_USERDATA);
      cpu->running = false;

      DestroyWindow(window_handle);
    } break;

    case WM_DESTROY: {
      PostQuitMessage(0);
    } break;

    case WM_KEYDOWN: {
      if (w_param == VK_ESCAPE) {
        PostMessage(window_handle, WM_CLOSE, NULL, NULL);
      }

      Cpu *cpu = (Cpu*)GetWindowLongPtr(window_handle, GWLP_USERDATA);
      const WCHAR key = (WCHAR)w_param;

      for (u8 i = 0; i < 0x10; i++) {
        if (input_map[i] == key) {
          cpu->input[i] = true;
        }
      }
    } break;

    case WM_KEYUP: {
      Cpu *cpu = (Cpu*)GetWindowLongPtr(window_handle, GWLP_USERDATA);
      const WCHAR key = (WCHAR)w_param;

      for (u8 i = 0; i < 0x10; i++) {
        if (input_map[i] == key) {
          cpu->input[i] = false;
        }
      }
    } break;

    default: {
      result = DefWindowProc(window_handle, message, w_param, l_param);
    }
  }
  return result;
}

HWND create_window(HINSTANCE instance_handle, INT show_flag, Cpu *cpu) {
  const LPCWSTR class_name = L"CHIP8EmulatorWindowClass";

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
    L"CHIP-8 Emulator",
    WS_POPUP | WS_VISIBLE,
    0,
    0,
    // TODO(steven): Get actual monitor size
    1920,
    1080,
    NULL,
    NULL,
    instance_handle,
    cpu
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
  HRESULT result = CoInitialize(NULL);
  assert(SUCCEEDED(result));

  IXAudio2 *xaudio = nullptr;
  result = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
  ASSERT_HRESULT(result)

  IXAudio2MasteringVoice *master_voice = nullptr;
  result = xaudio->CreateMasteringVoice(&master_voice);
  ASSERT_HRESULT(result)

  WAVEFORMATEX wave_format = {};
  wave_format.wFormatTag = WAVE_FORMAT_PCM;
  wave_format.nChannels = 1;
  wave_format.nSamplesPerSec = 48100;
  wave_format.nAvgBytesPerSec = 48100;
  wave_format.nBlockAlign = 1;
  wave_format.wBitsPerSample = 8;
  wave_format.cbSize = 0;

  IXAudio2SourceVoice *voice = nullptr;
  result = xaudio->CreateSourceVoice(&voice, (WAVEFORMATEX*)&wave_format);
  ASSERT_HRESULT(result)

  u8 audio_data[0x100];

  XAUDIO2_BUFFER audio_buffer = {};
  audio_buffer.AudioBytes = 0x100;
  audio_buffer.pAudioData = audio_data;
  audio_buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

  result = voice->SubmitSourceBuffer(&audio_buffer);
  ASSERT_HRESULT(result)

  bool audio_playing = false;

  Instruction instructions[0x10] = {};
  instructions[0x00] = &x0;
  instructions[0x01] = &x1NNN;
  instructions[0x02] = &x2NNN;
  instructions[0x03] = &x3XNN;
  instructions[0x04] = &x4XNN;
  instructions[0x05] = &x5XY0;
  instructions[0x06] = &x6XNN;
  instructions[0x07] = &x7XNN;
  instructions[0x08] = &x8;
  instructions[0x09] = &x9XY0;
  instructions[0x0a] = &xANNN;
  instructions[0x0b] = &xBNNN;
  instructions[0x0c] = &xCXNN;
  instructions[0x0d] = &xDXYN;
  instructions[0x0e] = &xE;
  instructions[0x0f] = &xF;

  Cpu *cpu = new Cpu(instructions);

  HWND window_handle = create_window(instance_handle, show_flags, cpu);
  HDC device_context = GetDC(window_handle);

  FILE *file;

  // errno = fopen_s(&file, "./rom/IBM Logo.ch8", "rb");
  // errno = fopen_s(&file, "./rom/Space Invaders.ch8", "rb");
  errno = fopen_s(&file, "./rom/pong.ch8", "rb");
  // errno = fopen_s(&file, "./rom/test_opcode.ch8", "rb");
  // errno = fopen_s(&file, "./rom/c8_test.ch8", "rb");
  assert(errno == 0);

  const u16 read_buffer_size = 0xfff - 0x200;
  fread_s(&cpu->game_memory[0x200], read_buffer_size, 1, read_buffer_size, file);
  assert(errno == 0);

  fclose(file);
  assert(errno == 0);

  HANDLE waitable_timer_handle = CreateWaitableTimer(NULL, TRUE, NULL);
  assert(waitable_timer_handle != NULL);

  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);

  const LONGLONG frame_time = TIMER_INTERVAL / SPEED;

  MSG message = {};
  while (cpu->running) {
    LARGE_INTEGER begin;
    QueryPerformanceCounter(&begin);

    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }

    for (int i = 0; i < SIMULATIONS_PER_FRAME; i++) {
      cpu->simulate();
    }

    cpu->update_timers();

    if (!audio_playing && cpu->sound_timer > 0) {
      voice->Start(0);
      audio_playing = true;
    } else if (audio_playing && cpu->sound_timer == 0) {
      voice->Stop();
      audio_playing = false;
    }

    if (cpu->requires_repaint) {
      render(cpu, window_handle, device_context);
      cpu->requires_repaint = false;
    }

    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);

    LONGLONG difference = end.QuadPart - begin.QuadPart;
    difference = (f64)difference / frequency.QuadPart * TIMER_INTERVAL;

    LARGE_INTEGER wait_time;
    wait_time.QuadPart = frame_time - difference;

    if (wait_time.QuadPart > 0) {
      // Inverse value to wait in relative time
      wait_time.QuadPart = -wait_time.QuadPart; 

      bool success = SetWaitableTimer(waitable_timer_handle, &wait_time, 0, NULL, NULL, FALSE);
      assert(success);

      success = WaitForSingleObject(waitable_timer_handle, INFINITE) == WAIT_OBJECT_0;
      assert(success);  
    }
  }

  xaudio->Release();

  ReleaseDC(window_handle, device_context);

  return 0;
}