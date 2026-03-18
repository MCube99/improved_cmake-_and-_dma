#pragma once

#include <stdbool.h>

typedef struct stack_type *Stack;

Stack create(void);
int pop(Stack s);
bool is_empty(Stack s);
void make_empty(Stack s);
bool is_full(Stack s);