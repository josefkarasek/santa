#
# Projekt: santa
# Autor:   xkaras27@stud.fit.vutbr.cz
# Datum:   28.03.2013
# 

CC=gcc                         	            # překladač jazyka C
CFLAGS=-std=gnu99 -lpthread -Wall -Wextra -Werror -pedantic   # parametry překladače

projekt: santa.c
	$(CC) $(CFLAGS) santa.c -o santa
