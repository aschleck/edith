#ifndef _DICTIONARY_LIST_H
#define _DICTIONARY_LIST_H

#include <map>
#include <string>

#include "debug.h"

template<typename K, typename T>
struct GetKey {
  const K &operator()(const T &element);
};

template<typename T,
         typename K = std::string,
         typename KF = GetKey<K, T>
        >
class DictionaryList {
public: 
  DictionaryList();
  DictionaryList(const DictionaryList<T, K, KF> &that);
  DictionaryList(DictionaryList<T, K, KF> &&that);
  ~DictionaryList();

  DictionaryList<T, K, KF> &operator=(DictionaryList<T, K, KF> that);

  size_t size() const;

  T &add(T element);
  bool has(size_t index) const;
  bool has(K key) const;

  const T &operator[](size_t index) const;
  T &operator[](size_t index);

  const T &operator[](K key) const;
  T &operator[](K key);

  class const_iterator;
  const_iterator begin() const;
  const_iterator end() const;

  class iterator;
  iterator begin();
  iterator end();

private:
  std::vector<T> array;
  std::map<K, size_t> dictionary;
  size_t count;

public:
  class const_iterator {
  public:
    const_iterator(const DictionaryList<T, K, KF> *list, size_t index);

    bool operator!=(const const_iterator &that) const;
    const T &operator*() const;
    const_iterator &operator++();

  private:
    const DictionaryList<T, K, KF> *list;
    size_t index;
  };

  class iterator {
  public:
    iterator(DictionaryList<T, K, KF> *list, size_t index);

    bool operator!=(const iterator &that) const;
    T &operator*();
    iterator &operator++();

  private:
    DictionaryList<T, K, KF> *list;
    size_t index;
  };

  friend void swap(DictionaryList<T, K, KF> &first, DictionaryList<T, K, KF> &second) {
    using std::swap;

    swap(first.array, second.array);
    swap(first.dictionary, second.dictionary);
    swap(first.count, second.count);
  }
};

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::DictionaryList() : count(0) {
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::DictionaryList(const DictionaryList<T, K, KF> &that) {
  array = that.array;
  dictionary = that.dictionary;
  count = that.count;
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::DictionaryList(DictionaryList<T, K, KF> &&that) {
  swap(*this, that);
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::~DictionaryList() {
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF> &DictionaryList<T, K, KF>::operator=(DictionaryList<T, K, KF> that) {
  swap(*this, that);

  return *this;
}

template<typename T, typename K, typename KF>
size_t DictionaryList<T, K, KF>::size() const {
  return count;
}

template<typename T, typename K, typename KF>
T &DictionaryList<T, K, KF>::add(T element) {
  size_t index = count;
  array.push_back(element);

  KF get_key;
  dictionary[get_key(element)] = index;

  ++count;

  return array.at(index);
}

template<typename T, typename K, typename KF>
bool DictionaryList<T, K, KF>::has(size_t index) const {
  return index < count;
}

template<typename T, typename K, typename KF>
bool DictionaryList<T, K, KF>::has(K key) const {
  return dictionary.count(key) > 0;
}

template<typename T, typename K, typename KF>
const T &DictionaryList<T, K, KF>::operator[](size_t index) const {
  XASSERT(index < count, "Requested index out of bounds.");
  return array.at(index);
}

template<typename T, typename K, typename KF>
T &DictionaryList<T, K, KF>::operator[](size_t index) {
  XASSERT(index < count, "Requested index out of bounds.");
  return array.at(index);
}

template<typename T, typename K, typename KF>
const T &DictionaryList<T, K, KF>::operator[](K key) const {
  return array[dictionary.at(key)];
}

template<typename T, typename K, typename KF>
T &DictionaryList<T, K, KF>::operator[](K key) {
  return array[dictionary.at(key)];
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::const_iterator DictionaryList<T, K, KF>::begin() const {
  return DictionaryList<T, K, KF>::const_iterator(this, 0);
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::const_iterator DictionaryList<T, K, KF>::end() const {
  return DictionaryList<T, K, KF>::const_iterator(this, count);
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::iterator DictionaryList<T, K, KF>::begin() {
  return DictionaryList<T, K, KF>::iterator(this, 0);
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::iterator DictionaryList<T, K, KF>::end() {
  return DictionaryList<T, K, KF>::iterator(this, count);
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::const_iterator::const_iterator(const DictionaryList<T, K, KF> *_list, size_t _index) :
    list(_list), index(_index) {
}

template<typename T, typename K, typename KF>
bool DictionaryList<T, K, KF>::const_iterator::operator!=(const const_iterator &that) const {
  return index != that.index;
}

template<typename T, typename K, typename KF>
const T &DictionaryList<T, K, KF>::const_iterator::operator*() const {
  return list->operator[](index);
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::const_iterator &DictionaryList<T, K, KF>::const_iterator::operator++() {
  ++index;
  
  return *this;
}

template<typename T, typename K, typename KF>
DictionaryList<T, K, KF>::iterator::iterator(DictionaryList<T, K, KF> *_list, size_t _index) :
    list(_list), index(_index) {
}

template<typename T, typename K, typename KF>
bool DictionaryList<T, K, KF>::iterator::operator!=(const iterator &that) const {
  return index != that.index;
}

template<typename T, typename K, typename KF>
T &DictionaryList<T, K, KF>::iterator::operator*() {
  return list->operator[](index);
}

template<typename T, typename K, typename KF>
typename DictionaryList<T, K, KF>::iterator &DictionaryList<T, K, KF>::iterator::operator++() {
  ++index;
  
  return *this;
}

#endif

