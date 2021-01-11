#ifndef DS_H
#define DS_H

#include "common.h"
#include <initializer_list>

extern bool map_equals(const char* lhs, const char* rhs);
extern u32 map_hash(const char* key);

extern bool map_equals(void* lhs, void* rhs);
extern u32 map_hash(void* key);

template <typename K, typename V>
struct map {

    struct kvp {
        K key;
        V value;
    };

    struct bin : kvp {
        u32 hash;
        bool haskey;
    };

    bin* bins;
    u32 numbins; 
    u32 numfreebins;

    map(u32 numbins = 8) { 
        alloc_bins(numbins); 
    }
    ~map() { 
        free(bins); 
    }

    map(map& other)  = delete;
    map(map&& other) = delete;
    map& operator= (const map& other)  = delete;
    map& operator= (const map&& other) = delete;

    void alloc_bins(u32 numbins) {
        bins = (bin*)malloc(sizeof(bin) * numbins);
        this->numbins = numbins;
        this->numfreebins = (u32)(numbins * 0.65f);
        for (u32 i = 0; i < numbins; i++)
            bins[i].haskey = false;
    }

    void realloc() {
        bin* old_bins = bins;
        u32 old_numbins = numbins;
        alloc_bins(numbins * 2);

        for (u32 i = 0; i < old_numbins; i++) {
            if (old_bins[i].haskey) {
                // TODO we can avoid recalculating the hash here as its already stored inside the old bin
                // but for now I'd like to reuse the insert function here so fuck it
                insert(old_bins[i].key, old_bins[i].value);
            }
        }
        free(old_bins);
    }

    bool insert(K key, V value) {
        if (numfreebins-- == 0)
            realloc();

        u32 hash = map_hash(key);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].haskey) {
            if (bins[bin_index].hash == hash)
                if (map_equals(bins[bin_index].key, key))
                    return false;

            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        bins[bin_index].hash = hash;
        bins[bin_index].key = key;
        bins[bin_index].haskey = true;
        new (&bins[bin_index].value) V(std::move(value));
        return true;
    }
    
    // If the key is in the map true is returned and out's value is set accordingly
    // If the key is not in the map, out's value is unchanged
    bool find(K key, V* out) {
        u32 hash = map_hash(key);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].haskey) {
            if (bins[bin_index].hash == hash) {
                if (map_equals(bins[bin_index].key, key)) {
                    if (out)
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

    V* find2(K key) {
        u32 hash = map_hash(key);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].haskey) {
            if (bins[bin_index].hash == hash) {
                if (map_equals(bins[bin_index].key, key)) {
                    return &bins[bin_index].value;
                }
            }
            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        return nullptr;
    }

    V operator[](K key) const {
        return *find2(key);
    }

    // TODO this  functionality should be baked in into insert
    V& operator[](K key) {
        V* v = find2(key);
        if (v)
            return *v;
        insert(key, {});
        return *find2(key);
    }

    struct iterator {
        map* a;
        u32 i;
        
        iterator& operator++() {
            do {
                i++;
            } while (i < a->numbins && !a->bins[i].haskey);
            return *this;
        }
        kvp& operator*() const { return a->bins[i]; }
        bool operator==(const iterator& other) { return a == other.a && i == other.i; };
        bool operator!=(const iterator& other) { return a != other.a || i != other.i; };
    };

    iterator begin() { 
        u32 i;
        for (i = 0; i < numbins; i++)
            if (bins[i].haskey)
                break;
        return { .a = this, .i = i };
    }
    iterator end()   { return { this, numbins }; }
};


template <typename T>
struct arr {
    T* buffer;
    u32 size;
    u32 capacity;

    arr(u32 capacity = 8) 
        : capacity(capacity), size(0), buffer((T*)malloc(sizeof(T) * capacity)) { }

    arr(std::initializer_list<T> init) : arr(init.size()) {
        u32 i = 0;
        for (const auto& v : init) {
            buffer[i] = v;
            i++;
        }
        size = i;
    }

    arr(const arr& other) { 
        copy(other); 
    }

    arr(arr&& other) {
        buffer = other.buffer;
        size = other.size;
        capacity = other.capacity;
        other.buffer = nullptr;
    }

    ~arr() { 
        if (buffer) 
            free(buffer); 
    }

    arr& operator= (const arr& other) {
        if (buffer) free(buffer);
        copy(other);
        return *this;
    }

    arr& operator= (arr&& other) {
        buffer = other.buffer;
        size = other.size;
        capacity = other.capacity;
        other.buffer = nullptr;
        return *this;
    }

    void copy(const arr& other) {
        size = other.size;
        capacity = other.capacity;
        buffer = (T*)malloc(sizeof(T) * capacity);
        memcpy(buffer, other.buffer, sizeof(T) * size);
    }

    void realloc() {
        capacity *= 2;
        T* new_buffer = (T*)malloc(sizeof(T) * capacity);
        memcpy(new_buffer, buffer, sizeof(T) * size);
        free(buffer);
        buffer = new_buffer;
    }

    T& push(T value) {
        if (size >= capacity)
            realloc();
        buffer[size++] = std::move(value);
        return buffer[size - 1];
    }

    T pop() {
        return buffer[--size];
    }

    T  operator[](u32 i) const { return buffer[i]; }
    T& operator[](u32 i)       { return buffer[i]; }

    T last() const  { return buffer[size - 1]; }
    T& last()       { return buffer[size - 1]; }

    T* begin() { return buffer; }
    T* end()   { return buffer + size; }

};

#define BUCKET_SIZE 16

template<typename T>
struct bucketed_arr {
    arr<T*> buckets;
    u32 last_bucket_size = 0;

    bucketed_arr() {
        new_bucket();
    }

    void new_bucket() {
        T* bucket = (T*)malloc(sizeof(T) * BUCKET_SIZE);
        buckets.push(bucket);
        last_bucket_size = 0;
    }

    T operator[](u32 i) const {
        u32 bucket = i % BUCKET_SIZE;
        u32 index = i / BUCKET_SIZE;

        return buckets[bucket][index];
    }

    T& operator[](u32 i) {
        u32 bucket = i % BUCKET_SIZE;
        u32 index = i / BUCKET_SIZE;

        return buckets[bucket][index];
    }

    T& push(T value) {
        if (last_bucket_size >= BUCKET_SIZE)
            new_bucket();

        T& ref = buckets.last()[last_bucket_size++];
        ref = value;
        return ref;
    }
};

#endif // guard
