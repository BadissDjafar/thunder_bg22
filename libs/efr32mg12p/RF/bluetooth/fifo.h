#ifndef FIFO_H
#define FIFO_H
#include <stdint.h>

enum fifo_flag{
  fifo_flag_full      = 0x01,
  fifo_flag_dma_full  = 0x02,
};

typedef uint16_t fifo_size_t;

struct fifo {
  uint8_t *buffer;
  uint8_t *head;
  uint8_t *tail;
  uint8_t *dma_tail;
  uint8_t flags;
  fifo_size_t buffer_size;
};

#define fifo_define(name, size)    \
  uint8_t name##__buffer[size];    \
  struct fifo name =               \
  {                                \
    .buffer      = name##__buffer, \
    .buffer_size = size,           \
  }

void fifo_init(struct fifo *fifo, uint8_t config_flags);
static inline uint8_t *fifo_buffer(struct fifo *fifo)
{
  return fifo->buffer;
}
static inline fifo_size_t fifo_size(struct fifo *fifo)
{
  return fifo->buffer_size;
}
fifo_size_t fifo_length(struct fifo *fifo);
fifo_size_t fifo_space(struct fifo *fifo);
fifo_size_t fifo_read(struct fifo *fifo, uint8_t *buffer, fifo_size_t count);
fifo_size_t fifo_write(struct fifo *fifo, const uint8_t *buffer, fifo_size_t count);
fifo_size_t fifo_dma_space(struct fifo *fifo);
fifo_size_t fifo_dma_reserve(struct fifo *fifo, uint8_t **buffer, fifo_size_t count);
void fifo_dma_set_tail(struct fifo *fifo, uint8_t *tail);
#endif /* FIFO_H */
