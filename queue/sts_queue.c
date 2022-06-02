#include "sts_queue.h"
#include <stdlib.h>
#include <threads.h>

typedef struct StsElement {
  void *next;
  void *value;
} StsElement;

struct StsHeader {
  StsElement *head;
  StsElement *tail;
  mtx_t *mutex;
};

static StsHeader* create();
static StsHeader* create() {
  StsHeader *handle = malloc(sizeof(*handle));
  handle->head = NULL;
  handle->tail = NULL;

  mtx_t *mutex = malloc(sizeof(*mutex));
  handle->mutex = mutex;
  
  return handle;
}

static void destroy(StsHeader *header);
static void destroy(StsHeader *header) {
  free(header->mutex);
  free(header);
  header = NULL;
}

static void push(StsHeader *header, void *elem);
static void push(StsHeader *header, void *elem) {
  // Create new element
  StsElement *element = malloc(sizeof(*element));
  element->value = elem;
  element->next = NULL;

  mtx_lock(header->mutex);
  // Is list empty
  if (header->head == NULL) {
	header->head = element;
	header->tail = element;
  } else {
	// Rewire
	StsElement* oldTail = header->tail;
	oldTail->next = element;
	header->tail = element;
  }
  mtx_unlock(header->mutex);
}

static void* pop(StsHeader *header);
static void* pop(StsHeader *header) {
  mtx_lock(header->mutex);
  StsElement *head = header->head;

  // Is empty?
  if (head == NULL) {
	mtx_unlock(header->mutex);
	return NULL;
  } else {
	// Rewire
	header->head = head->next;
	
	// Get head and free element memory
	void *value = head->value;
	free(head);
	
	mtx_unlock(header->mutex);
	return value;
  }
}

static int is_empty(StsHeader *header);
static int is_empty(StsHeader *header)
{
	return (header->head == NULL);
}

_StsQueue const StsQueue = {
  create,
  destroy,
  push,
  pop,
  is_empty
};
