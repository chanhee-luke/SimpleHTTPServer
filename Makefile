CC=		gcc
CFLAGS=		-g -gdwarf-2 -Wall -Werror -std=gnu99
LD=		gcc
LDFLAGS=	-L.
AR=		ar
ARFLAGS=	rcs
TARGETS=	forking.o handler.o request.o single.o socket.o spidey.o utils.o spidey

all:		$(TARGETS)

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) *.o *.log *.input

.SUFFIXES:
.PHONY:		all test benchmark clean

%.o : %.c
	@echo Compiling $@...
	@$(CC) $(CFLAGS) -o $@ -c $<

spidey : forking.o handler.o request.o single.o socket.o spidey.o utils.o 
	@echo Linking $@...
	@$(LD) $(LDFLAGS) -o $@ $^
