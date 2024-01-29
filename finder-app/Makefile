ifdef CROSS_COMPILE
	compiler = $(CROSS_COMPILE)gcc
else
	compiler = gcc
endif

all : writer

writer : writer.c
	$(compiler) -o writer writer.c
.PHONY : clean
clean:
	-rm writer

