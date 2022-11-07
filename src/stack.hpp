#pragma once

#include <cassert>

template <typename T, size_t Size>
struct Stack {
  size_t head = 0;
  T data[Size];

  T &push(const T &value) {
    assert(this->head < Size);
    return this->data[this->head++] = value;
  }

  T &pop() {
    assert(this->head > 0);
    return this->data[--this->head];
  }
};