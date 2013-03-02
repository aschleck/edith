#ifndef _DICTIONARY_LIST_H
#define _DICTIONARY_LIST_H

#include <map>
#include <string>

#include "resizable_array.h"

template<typename K, typename T>
struct GetKey {
  const K &operator()(const T &element);
};

template<typename T,
         typename I = unsigned int,
         typename K = std::string,
         typename KF = GetKey<K, T>
        >
class DictionaryList {
public: 
  DictionaryList();
  DictionaryList(const DictionaryList<T, I, K, KF> &that);
  DictionaryList(DictionaryList<T, I, K, KF> &&that);
  ~DictionaryList();

  DictionaryList<T, I, K, KF> &operator=(DictionaryList<T, I, K, KF> that);

  I size() const;

  T &add(T element);
  bool has(I index) const;
  bool has(K key) const;

  const T &operator[](I index) const;
  T &operator[](I index);

  const T &operator[](K key) const;
  T &operator[](K key);

  class const_iterator;
  const_iterator begin() const;
  const_iterator end() const;

  class iterator;
  iterator begin();
  iterator end();

private:
  ResizableArray<T, I> array;
  std::map<K, I> dictionary;
  I count;

public:
  class const_iterator {
  public:
    const_iterator(const DictionaryList<T, I, K, KF> *list, I index);

    bool operator!=(const const_iterator &that) const;
    const T &operator*() const;
    const_iterator &operator++();

  private:
    const DictionaryList<T, I, K, KF> *list;
    I index;
  };

  class iterator {
  public:
    iterator(DictionaryList<T, I, K, KF> *list, I index);

    bool operator!=(const iterator &that) const;
    T &operator*();
    iterator &operator++();

  private:
    DictionaryList<T, I, K, KF> *list;
    I index;
  };

  friend void swap(DictionaryList<T, I, K, KF> &first, DictionaryList<T, I, K, KF> &second) {
    using std::swap;

    swap(first.array, second.array);
    swap(first.dictionary, second.dictionary);
    swap(first.count, second.count);
  }
};

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::DictionaryList() : count(0) {
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::DictionaryList(const DictionaryList<T, I, K, KF> &that) {
  array = that.array;
  dictionary = that.dictionary;
  count = that.count;
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::DictionaryList(DictionaryList<T, I, K, KF> &&that) {
  swap(*this, that);
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::~DictionaryList() {
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF> &DictionaryList<T, I, K, KF>::operator=(DictionaryList<T, I, K, KF> that) {
  swap(*this, that);

  return *this;
}

template<typename T, typename I, typename K, typename KF>
I DictionaryList<T, I, K, KF>::size() const {
  return count;
}

template<typename T, typename I, typename K, typename KF>
T &DictionaryList<T, I, K, KF>::add(T element) {
  array.ensure_space(count + 1);

  I index = count;
  array[index] = element;

  KF get_key;
  dictionary[get_key(element)] = index;

  ++count;

  return array[index];
}

template<typename T, typename I, typename K, typename KF>
bool DictionaryList<T, I, K, KF>::has(I index) const {
  return index < count;
}

template<typename T, typename I, typename K, typename KF>
bool DictionaryList<T, I, K, KF>::has(K key) const {
  return dictionary.count(key) > 0;
}

template<typename T, typename I, typename K, typename KF>
const T &DictionaryList<T, I, K, KF>::operator[](I index) const {
  XASSERT(index < count, "Requested index out of bounds.");
  return array[index];
}

template<typename T, typename I, typename K, typename KF>
T &DictionaryList<T, I, K, KF>::operator[](I index) {
  XASSERT(index < count, "Requested index out of bounds.");
  return array[index];
}

template<typename T, typename I, typename K, typename KF>
const T &DictionaryList<T, I, K, KF>::operator[](K key) const {
  return array[dictionary.at(key)];
}

template<typename T, typename I, typename K, typename KF>
T &DictionaryList<T, I, K, KF>::operator[](K key) {
  return array[dictionary.at(key)];
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::const_iterator DictionaryList<T, I, K, KF>::begin() const {
  return DictionaryList<T, I, K, KF>::const_iterator(this, 0);
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::const_iterator DictionaryList<T, I, K, KF>::end() const {
  return DictionaryList<T, I, K, KF>::const_iterator(this, count);
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::iterator DictionaryList<T, I, K, KF>::begin() {
  return DictionaryList<T, I, K, KF>::iterator(this, 0);
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::iterator DictionaryList<T, I, K, KF>::end() {
  return DictionaryList<T, I, K, KF>::iterator(this, count);
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::const_iterator::const_iterator(const DictionaryList<T, I, K, KF> *_list, I _index) :
    list(_list), index(_index) {
}

template<typename T, typename I, typename K, typename KF>
bool DictionaryList<T, I, K, KF>::const_iterator::operator!=(const const_iterator &that) const {
  return index != that.index;
}

template<typename T, typename I, typename K, typename KF>
const T &DictionaryList<T, I, K, KF>::const_iterator::operator*() const {
  return list->operator[](index);
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::const_iterator &DictionaryList<T, I, K, KF>::const_iterator::operator++() {
  ++index;
  
  return *this;
}

template<typename T, typename I, typename K, typename KF>
DictionaryList<T, I, K, KF>::iterator::iterator(DictionaryList<T, I, K, KF> *_list, I _index) :
    list(_list), index(_index) {
}

template<typename T, typename I, typename K, typename KF>
bool DictionaryList<T, I, K, KF>::iterator::operator!=(const iterator &that) const {
  return index != that.index;
}

template<typename T, typename I, typename K, typename KF>
T &DictionaryList<T, I, K, KF>::iterator::operator*() {
  return list->operator[](index);
}

template<typename T, typename I, typename K, typename KF>
typename DictionaryList<T, I, K, KF>::iterator &DictionaryList<T, I, K, KF>::iterator::operator++() {
  ++index;
  
  return *this;
}

#endif

