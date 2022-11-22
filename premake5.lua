workspace 'chip8-emulator'
  configurations { 'Debug', 'Release' }
  platforms { 'Win64' }
  location 'build'

project 'chip8-emulator'
  kind 'WindowedApp'
  language 'C++'
  files { 'main.cpp' }

  filter 'configurations:Release'
    defines { 'NDEBUG' }
    optimize 'On'

  filter 'configurations:Debug'
    symbols 'Full'
    optimize 'Debug'

  filter 'platforms:Win64'
    system 'Windows'
    architecture 'x86_64'
    
  filter 'system:Windows'
    defines { 'WIN32', 'UNICODE' }
    links { 'user32', 'gdi32', 'Xaudio2', 'ole32', 'comdlg32' }
