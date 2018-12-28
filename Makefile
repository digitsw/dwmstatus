dwmstatus: status.c
	cc -o dwmstatus status.c -lX11 `pkg-config --cflags --libs alsa`
clean: rm dwmstatus
