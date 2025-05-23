EXE=htproxy

$(EXE): main.c
	cc -Wall -o $@ $<

format:
	clang-format -style=file -i *.c
