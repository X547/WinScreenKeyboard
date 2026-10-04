project(
	'WinScreenKeyboard',
	'cpp',
	version: '0.1.0',
	meson_version: '>=1.0',
	default_options: [
		'cpp_std=c++20',
		'warning_level=3',
	]
)

cpp = meson.get_compiler('cpp')

add_project_arguments(
	'-DUNICODE',
	'-D_UNICODE',
	'-DWIN32_LEAN_AND_MEAN',
	'-DNOMINMAX',
	'-D_WIN32_WINNT=0x0A00',
	'-DWINVER=0x0A00',
	language: 'cpp'
)

executable(
	'ScreenKeyboard',
	'src/Main.cpp',
	'src/KeyboardLayout.cpp',
	'src/KeyboardHandler.cpp',
	'src/KeyboardWindow.cpp',
	'src/TrayIcon.cpp',
	win_subsystem: 'windows',
	link_args: ['-municode', '-static'],
	dependencies: [
		cpp.find_library('user32'),
		cpp.find_library('gdi32'),
		cpp.find_library('shell32'),
	]
)
