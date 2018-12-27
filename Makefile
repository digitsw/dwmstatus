dwmstatus: status.c
	cc -o dwmstatus status.c -lX11
clean: rm dwmstatus
