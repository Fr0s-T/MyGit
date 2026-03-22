NAME = out/MyGit.out

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude
LDFLAGS = -lcrypto

SRC = $(wildcard src/*.c) \
	$(wildcard src/commands/*.c) \
	$(wildcard src/add/*.c) \
	$(wildcard src/checkout/*.c) \
	$(wildcard src/merge/*.c) \
	$(wildcard src/core/*.c) \
	$(wildcard src/helpers/*.c) \
	$(wildcard src/data_structures/*.c)
OBJ = $(patsubst %.c,out/obj/%.o,$(SRC))

all: $(NAME)

$(NAME): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) -o $(NAME) $(LDFLAGS)

out/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf out/obj

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
