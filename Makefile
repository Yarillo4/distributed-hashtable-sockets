# We define variables for directories
DIROBJ  = obj
DIRINC  = include
DIRSRC  = src
DIRBIN  = .
# Compiler
CC      = clang -g
CFLAGS  = -W -Werror -I$(DIRINC) $(DEBUG_FLAG)
CLIBS   = -lpthread
# Dependencies, objects, ...
DEPS    = $(wildcard include/*.h)
OBJETS  = $(DEPS:$(DIRINC)/%.h=obj/%.o)
.SUFFIXES:
# We create targets 
all : 
	@make server --no-print-directory
	@make client --no-print-directory
server : src/server.c $(OBJETS)
	$(CC) $(CFLAGS) -o $@.out $^ $(CLIBS)
debug : 
	@make DEBUG_FLAG=-DDEBUG_LEVEL=999 --no-print-directory
warnings : 
	@make DEBUG_FLAG=-DDEBUG_LEVEL=1 --no-print-directory
client : src/client.c $(OBJETS)
	$(CC) $(CFLAGS) -o $@.out $^ $(CLIBS)
$(DIROBJ)/%.o : $(DIRSRC)/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

# Targets to call manually
.PHONY: archive
archive:
	tar -f archive.tar.gz -cvz $(DIRSRC)/*.c $(DIRINC)/*.h Makefile
.PHONY: clean
clean:
	rm -r $(DIROBJ)
	rm -r *.out