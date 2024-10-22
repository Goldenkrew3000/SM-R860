# Temp build script
gcc entry.c utils.c framebuffer.c charging_screen.c -o watch `pkg-config --cflags --libs cairo pango pangoft2 freetype2 pangocairo`
