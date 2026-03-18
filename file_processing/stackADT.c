
#include <stdio.h>
#include <stdlib.h>
#include "stackADT.h"
#include <string.h>

#define STACK_SIZE 100

typedef int Item;

struct stack_type
{
    Item contents[STACK_SIZE];
    int top;
};

static struct stack_type myStack;


Stack create(void) //only allowed one stack instance
{
    memset(myStack.contents, 0, STACK_SIZE);
    myStack.top = 0;
    Stack s = &myStack;
    return(s);
}

int pop(Stack s) // deleting a value
{
    if (is_empty(s))
    {
        return(0);
    }
    return s->contents[--s->top];
}

bool is_empty(Stack s)
{
    return(s->top==0);
}

void make_empty(Stack s)
{
    s->top = 0;
}

void push(Stack s, Item i) //adding a value
{
    if(is_full(s))
    {
        make_empty(s);
    } 
    s->contents[s->top++] = i;
}

bool is_full(Stack s)
{
    return(s->top == STACK_SIZE);
}