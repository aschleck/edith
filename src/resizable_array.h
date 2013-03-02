#ifndef _RESIZABLE_ARRAY_H
#define _RESIZABLE_ARRAY_H

#include <algorithm>

#include "debug.h"

template<typename T, typename I = unsigned int>
class ResizableArray {
public:
  ResizableArray(I growth = 64, I length = 32);
  ResizableArray(const ResizableArray<T, I> &that);
  ResizableArray(ResizableArray<T, I> &&that);
  ~ResizableArray();

  ResizableArray<T, I> &operator=(ResizableArray<T, I> other);

  const T &operator[](I i) const;
  T &operator[](I i);

  void ensure_space(I amount);

protected:
  T *data;
  I growth;
  I length;

  void grow(I min_size); 

public:
  friend void swap(ResizableArray<T, I> &first, ResizableArray<T, I> &second) {
    using std::swap;

    swap(first.growth, second.growth);
    swap(first.data, second.data);
    swap(first.length, second.length);
  }
};

template<typename T, typename I>
ResizableArray<T, I>::ResizableArray(I _growth, I _length) : growth(_growth) {
  XASSERT(growth > 0, "Growth must be greater than 0.");

  if (_length > 0) {
    data = new T[_length];
  } else {
    data = NULL;
  }

  length = _length;
}

template<typename T, typename I>
ResizableArray<T, I>::ResizableArray(ResizableArray<T, I> &&that) {
  growth = that.growth;
  data = that.data;
  length = that.length;

  that.length = 0;
  that.data = NULL;
}

template<typename T, typename I>
ResizableArray<T, I>::ResizableArray(const ResizableArray<T, I> &that) {
  using std::copy;

  growth = that.growth;

  if (that.length > 0) {
    data = new T[that.length];
    copy(that.data, that.data + that.length, data);
  } else {
    data = NULL;
  }

  length = that.length;
}

template<typename T, typename I>
ResizableArray<T, I>::~ResizableArray() {
  if (length > 0) {
    delete[] data;
  }
}

template<typename T, typename I>
ResizableArray<T, I> &ResizableArray<T, I>::operator=(ResizableArray<T, I> other) {
  swap(*this, other);

  return *this;
}

template<typename T, typename I>
const T &ResizableArray<T, I>::operator[](I i) const {
  XASSERT(i < length, "Index out of bounds");

  return data[i];
}

template<typename T, typename I>
T &ResizableArray<T, I>::operator[](I i) {
  XASSERT(i < length, "Index out of bounds");

  return data[i];
}

template<typename T, typename I>
void ResizableArray<T, I>::ensure_space(I amount) {
  if (amount >= length) {
    grow(amount);
  }
}

template<typename T, typename I>
void ResizableArray<T, I>::grow(I min_size) {
  T *old = data;
  I old_length = length;

  while (length < min_size) {
    XASSERT((I)(growth + length) > length, "Cannot grow vector any further.");

    length += growth;
  }

  data = new T[length];

  if (old_length > 0) {
    std::copy(old, old + old_length, data);
    delete[] old;
  }
}

#endif

