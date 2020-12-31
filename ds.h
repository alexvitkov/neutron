#ifndef DS_H
#define DS_H

#include "common.h"
#include <initializer_list>

// https://github.com/explosion/murmurhash/blob/master/murmurhash/MurmurHash2.cpp
u32 MurmurHash2 (const u8* data, int len, u32 seed) {
  const u32 m = 0x5bd1e995;
  const int r = 24;

  u32 h = seed ^ len;

  while(len >= 4) {
    u32 k = *(u32*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  switch(len) {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
} 

template <typename V>
struct strtable {

    struct bin {
        u32 hash;
        const char* key;
        V value;
    };

    bin* bins;
    u32 numbins; 
    u32 numfreebins;

    strtable(u32 numbins = 8) { alloc_bins(numbins); }
    ~strtable()               { free(bins); }

    strtable(strtable& other)  = delete;
    strtable(strtable&& other) = delete;
    strtable& operator= (const strtable& other)  = delete;
    strtable& operator= (const strtable&& other) = delete;

    void alloc_bins(u32 numbins) {
        bins = (bin*)malloc(sizeof(bin) * numbins);
        this->numbins = numbins;
        this->numfreebins = (u32)(numbins * 0.65f);
        for (u32 i = 0; i < numbins; i++)
            bins[i].key = nullptr;
    }

    void realloc() {
        bin* old_bins = bins;
        u32 old_numbins = numbins;
        alloc_bins(numbins * 2);

        for (u32 i = 0; i < old_numbins; i++) {
            if (old_bins[i].key) {
                // TODO we can avoid recalculating the hash here as its already stored inside the old bin
                // but for now I'd like to reuse the insert function here so fuck it
                insert(old_bins[i].key, old_bins[i].value);
            }
        }
        free(old_bins);
    }

    bool insert(const char* key, V value) {
        u32 len = strlen(key);
        return insert(key, len, value);
    }

    bool insert(const char* key, u32 len, V value) {
        if (numfreebins-- == 0)
            realloc();

        u32 hash = MurmurHash2((u8*)key, strlen(key), 32);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].key) {
            if (bins[bin_index].hash == hash)
                if (strlen(bins[bin_index].key) == len && !strncmp(bins[bin_index].key, key, len))
                    return false;

            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }

        bins[bin_index] = { .hash = hash, .key = key, .value = value };
        return true;
    }
    
    V find(const char* key) {
        u32 len = strlen(key);
        return find(key, len);
    }

    bool try_find(const char* key, V* out) {
        u32 len = strlen(key);
        u32 hash = MurmurHash2((u8*)key, len, 32);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].key) {
            if (bins[bin_index].hash == hash) {
                if (strlen(bins[bin_index].key) == len && !strncmp(bins[bin_index].key, key, len)) {
                    *out = bins[bin_index].value;
                    return true;
                }
            }
            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        return false;
    }

    V find(const char* key, u32 len) {
        u32 hash = MurmurHash2((u8*)key, len, 32);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].key) {
            if (bins[bin_index].hash == hash)
                if (strlen(bins[bin_index].key) == len && !strncmp(bins[bin_index].key, key, len))
                    return bins[bin_index].value;
            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        return {};
    }

};


template <typename T>
struct arr {
    T* buffer;
    u32 size;
    u32 capacity;

    arr(u32 capacity = 8) : capacity(capacity), size(0), buffer((T*)malloc(sizeof(T) * capacity)) { }

    arr(std::initializer_list<T> init) : arr(init.size()) {
        u32 i = 0;
        for (const auto& v : init) {
            buffer[i] = v;
            i++;
        }
        size = i;
    }
    
    ~arr() { 
        free(buffer); 
    }

    arr& operator= (const arr& other) {
        if (buffer)
            free(buffer);
        copy(other);
        return *this;
    }

    arr(const arr& other) {
        copy(other);
    }

    void copy(const arr& other) {
        size = other.size;
        capacity = other.capacity;
        buffer = (T*)malloc(sizeof(T) * capacity);
        memcpy(buffer, other.buffer, sizeof(T) * size);
    }

    arr(arr&& other) = delete;
    arr& operator= (const arr&& other) = delete;

    void realloc() {
        capacity *= 2;
        T* new_buffer = (T*)malloc(sizeof(T) * capacity);
        memcpy(new_buffer, buffer, sizeof(T) * size);
        free(buffer);
        buffer = new_buffer;
    }

    void push(T value) {
        if (size >= capacity)
            realloc();
        buffer[size++] = value;
    }

    template <typename ... Ts>
    void emplace(Ts && ... args) {
        if (size >= capacity)
            realloc();
        new (&buffer[size++]) T (args...);
    }

    T pop() {
        return buffer[--size];
    }

    T  operator[](u32 i) const { return buffer[i]; }
    T& operator[](u32 i)       { return buffer[i]; }

    T last() const  { return buffer[size - 1]; }
    T& last()       { return buffer[size - 1]; }

    struct iterator {
        arr* a;
        u32 i;
        
        iterator& operator++() {
            i++;
            return *this;
        }
        T& operator*() const { return a->buffer[i]; }
        bool operator==(const iterator& other) { return a == other.a && i == other.i; };
        bool operator!=(const iterator& other) { return a != other.a || i != other.i; };
    };

    iterator begin() { return { this, 0 }; }
    iterator end()   { return { this, size }; }

};

#endif // guard
