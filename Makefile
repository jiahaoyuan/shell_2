CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror
CFLAGS += -D_GNU_SOURCE -std=gnu99  # added
EXECS = 33sh cs0330_noprompt

PROMPT = -DPROMPT

.PHONY: all clean
all: $(EXECS)
33sh: sh.c jobs.c
	gcc $(CFLAGS) $(PROMPT) $^ -o 33sh
	#TODO: compile your program, including the -DPROMPT macro
cs0330_noprompt: sh.c jobs.c 
	gcc $(CFLAGS) $^ -o cs0330_noprompt
	#TODO: compile your program without the prompt macro
clean:
	rm -f 33sh cs0330_noprompt
	#TODO: clean up any executable files that this Makefile has produced
