CC 	   = gcc

CFLAGS = -Wall -Wextra -ggdb

TARGET = mysh
SRCS   = main.c shell.c gettoken.c history.c
OBJS   = main.o shell.o gettoken.o history.o

RM 	   = rm -f

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

main.o: shell.h history.h

shell.o: shell.h gettoken.h history.h

gettoken.o: shell.h gettoken.h history.h

history.o: history.h

clean:
	$(RM) $(TARGET) $(OBJS)

clean_target:
	$(RM) $(TARGET)

clean_obj:
	$(RM) $(OBJS)