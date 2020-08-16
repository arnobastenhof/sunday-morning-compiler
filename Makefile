# reset suffix list
.SUFFIXES:
.SUFFIXES: .c .h .o

# use spaces instead of tabs
.RECIPEPREFIX != ps

# paths
PATHS = src/
PATHB = build/
PATHO = build/obj/

# keep object files
.PRECIOUS: $(PATHO)%.o

# commands and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Werror
ALL_CFLAGS = -g -O3 -std=c99 -I$(PATHS) $(CFLAGS)

# file lists

BUILD_PATHS = $(PATHB) $(PATHO) $(PATHH)

.PHONY: all clean

all: $(PATHB)wc

clean:
  -rm -rf $(BUILD_PATHS)

# build directories

$(PATHB):
  -mkdir $(PATHB)

$(PATHO): $(PATHB)
  -mkdir $(PATHO)

# object files

$(PATHO)main.o : $(PATHS)main.c $(PATHO)
  $(CC) $(ALL_CFLAGS) -DNDEBUG -c $< -o $@

# executable

$(PATHB)wc: $(PATHO)main.o
  $(CC) -o $@ $^
