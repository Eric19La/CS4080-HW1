#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node
{
    char *value;
    struct Node *prev;
    struct Node *next;
} Node;

typedef struct
{
    Node *head;
    Node *tail;
} List;

Node *makeNode(const char *str)
{
    Node *node = malloc(sizeof(Node));
    node->value = malloc(strlen(str) + 1);
    strcpy(node->value, str);
    node->prev = NULL;
    node->next = NULL;
    return node;
}

void insert(List *list, const char *str)
{
    Node *node = makeNode(str);
    if (list->tail == NULL)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
}

Node *find(List *list, const char *str)
{
    Node *cur = list->head;
    while (cur != NULL)
    {
        if (strcmp(cur->value, str) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

void deleteNode(List *list, Node *node)
{
    if (node->prev != NULL)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    free(node->value);
    free(node);
}

void printList(List *list)
{
    Node *cur = list->head;
    while (cur != NULL)
    {
        printf("%s -> ", cur->value);
        cur = cur->next;
    }
    printf("NULL\n");
}

int main()
{
    List list = {NULL, NULL};

    insert(&list, "LeBron");
    insert(&list, "Jordan");
    insert(&list, "Kobe");
    printList(&list);

    Node *found = find(&list, "Jordan");
    printf("Found: %s\n", found ? found->value : "(not found)");

    deleteNode(&list, found);
    printList(&list);

    Node *cur = list.head;
    while (cur != NULL)
    {
        Node *next = cur->next;
        deleteNode(&list, cur);
        cur = next;
    }

    return 0;
}