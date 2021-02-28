#ifndef DS_H
#define DS_H

#include "common.h"
#include <initializer_list>

extern bool map_equals(const char* lhs, const char* rhs);
extern u32 map_hash(const char* key);

extern bool map_equals(void* lhs, void* rhs);
extern u32 map_hash(void* key);

extern bool map_equals(u64 lhs, u64 rhs);
extern u32 map_hash(u64 key);

template <typename K, typename V>
struct map {
    struct kvp {
        K key;
        V value;
    };

    enum BinState {
        Free,
        Taken,
        Deleted
    };

    struct bin : kvp {
        u32 hash;
        BinState binstate;
    };

    bin* bins;
    u32 numbins; 
    u32 numfreebins;

    map() : map(8) {
    }

    map(u32 numbins) { 
        alloc_bins(numbins); 
    }
    ~map() { 
        free(bins); 
    }

    map(map& other)  = delete;
    map& operator= (const map& other)  = delete;

    map(map&& other) {
        bins = other.bins;
        numbins = other.numbins;
        numfreebins = other.numfreebins;
        other.bins = nullptr;
    }

    map& operator= (const map&& other) {
        bins = other.bins;
        numbins = other.numbins;
        numfreebins = other.numfreebins;
        other.bins = nullptr;
        return *this;
    }

    void alloc_bins(u32 numbins) {
        bins = (bin*)malloc(sizeof(bin) * numbins);
        this->numbins = numbins;
        this->numfreebins = (u32)(numbins * 0.65f);
        for (u32 i = 0; i < numbins; i++)
            bins[i].binstate = Free;
    }

    void realloc() {
        bin* old_bins = bins;
        u32 old_numbins = numbins;
        alloc_bins(numbins * 2);

        for (u32 i = 0; i < old_numbins; i++) {
            if (old_bins[i].binstate == Taken) {
                // TODO we can avoid recalculating the hash here as its already stored inside the old bin
                // but for now I'd like to reuse the insert function here so fuck it
                insert(old_bins[i].key, std::move(old_bins[i].value));
            }
        }
        free(old_bins);
    }

    bool insert(K key, V value) {
        if (numfreebins-- == 0)
            realloc();

        u32 hash = map_hash(key);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].binstate != Free) {
            if (bins[bin_index].hash == hash)
                if (map_equals(bins[bin_index].key, key))
                    return false;

            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        bins[bin_index].hash = hash;
        bins[bin_index].key = key;
        bins[bin_index].binstate = Taken;
        new (&bins[bin_index].value) V(std::move(value));
        return true;
    }

    i64 indexof(K key) {
        u32 hash = map_hash(key);
        u32 bin_index = hash % numbins;

        while (bins[bin_index].binstate != Free) {
            if (bins[bin_index].hash == hash) {
                if (map_equals(bins[bin_index].key, key)) {
                    return bin_index;
                }
            }
            bin_index++;
            if (bin_index >= numbins)
                bin_index = 0;
        }
        return -1;
    }
    
    // If the key is in the map true is returned and out's value is set accordingly
    // If the key is not in the map, out's value is unchanged
    bool find(K key, V* out) {
        i64 index = indexof(key);
        if (index < 0)
            return false;

        *out = bins[index].value;
        return true;
    }

    V* find2(K key) {
        i64 index = indexof(key);
        return index < 0 ? nullptr : &bins[index].value;
    }

    void remove(K key) {
        i64 index = indexof(key);
        if (index < 0)
            return;

        bins[index].~V();
        bins[index].binstate = Free;
        assert(0);
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
            } while (i < a->numbins && a->bins[i].binstate != Taken);
            return *this;
        }
        kvp& operator*() const { return a->bins[i]; }
        bool operator==(const iterator& other) { return a == other.a && i == other.i; };
        bool operator!=(const iterator& other) { return a != other.a || i != other.i; };
    };

    iterator begin() { 
        u32 i;
        for (i = 0; i < numbins; i++)
            if (bins[i].binstate == Taken)
                break;
        return { .a = this, .i = i };
    }
    iterator end()   { return { this, numbins }; }
};


template <typename T>
struct arr_ref {
    T* buffer;
    u32 size;

    T  operator[](u32 i) const { return buffer[i]; }
    T& operator[](u32 i)       { return buffer[i]; }

    T* begin() { return buffer; }
    T* end()   { return buffer + size; }
};


template <typename T>
struct arr {
    T* buffer;
    u32 size;
    u32 capacity;

    arr(u32 capacity = 8) 
        : capacity(capacity), size(0), buffer((T*)malloc(sizeof(T) * capacity)) { }

    arr(std::initializer_list<T> init) : arr((u32)init.size()) {
        u32 i = 0;
        for (const auto& v : init) {
            new (&buffer[i]) T(v);
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
        if (buffer) free(buffer);
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

        for (u32 i = 0; i < size; i++)
            new (&buffer[i]) T (other.buffer[i]);
    }

    void realloc(u32 new_capacity) {
        capacity = new_capacity;
        T* new_buffer = (T*)malloc(sizeof(T) * capacity);
        for (u32 i = 0; i < size; i++)
            new (&new_buffer[i]) T (std::move(buffer[i]));
        free(buffer);
        buffer = new_buffer;
    }

    arr_ref<T> release() {
        arr_ref<T> r { .buffer = buffer, .size = size };
        this->buffer = nullptr;
        return r;
    }

    T& push_unique(T value) {
        for (u32 i = 0; i < size; i++) {
            if (buffer[i] == value)
                return buffer[i];
        }
        return push(value);
    }

    T& push(T value) {
        if (size >= capacity) {
            realloc(capacity * 2);
        }

        T* ptr = &buffer[size++];
        new (ptr) T(value);

        return buffer[size - 1];
    }

    // Move the last element to tht index-th position,
    // overriding the index-th element
    void delete_unordered(u32 index) {
        assert(size && "trying to delete_unordered an empty arr");
        new (&buffer[index]) T (std::move(buffer[size - 1]));
        size--;
    }

    T pop() {
        assert(size && "trying to pop an empty arr");
        return buffer[--size];
    }

    bool contains(T value) {
        for (auto& v : *this)
            if (v == value)
                return true;
        return false;
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
    u32 size;

    bucketed_arr() {
        size = 0;
        new_bucket();
    }

    void new_bucket() {
        T* bucket = (T*)malloc(sizeof(T) * BUCKET_SIZE);
        buckets.push(bucket);
        last_bucket_size = 0;
    }

    T operator[](u32 i) const {
        u32 bucket = i / BUCKET_SIZE;
        u32 index = i % BUCKET_SIZE;

        return buckets[bucket][index];
    }

    T& operator[](u32 i) {
        u32 bucket = i / BUCKET_SIZE;
        u32 index = i % BUCKET_SIZE;

        return buckets[bucket][index];
    }

    T& push(T value) {
        if (last_bucket_size >= BUCKET_SIZE)
            new_bucket();

        T& ref = buckets.last()[last_bucket_size++];
        ref = value;
        size ++;
        return ref;
    }

    struct iterator {
        bucketed_arr* a;
        u32 bucket, i;
        
        iterator& operator++() {
            i++;
            if (i == BUCKET_SIZE) {
                i = 0;
                bucket++;
            }
            return *this;
        }

        T& operator*() const { return a->buckets[bucket][i]; }
        bool operator==(const iterator& other) { return a == other.a && bucket == other.bucket && i == other.i; }
        bool operator!=(const iterator& other) { return a != other.a || bucket != other.bucket || i != other.i; }
    };

    iterator begin() { return { .a = this, .bucket = 0,                .i = 0 }; }
    iterator end()   { return { .a = this, .bucket = buckets.size - 1, .i = last_bucket_size }; }
};



struct linear_alloc {
    arr<char*> blocks;
    char* current;
    u64 remaining;

    char* alloc(u64 bytes);
    void free_all();

    inline linear_alloc() : blocks(), current(nullptr), remaining(0) {}

    linear_alloc(linear_alloc& other) = delete;
    linear_alloc(linear_alloc&& other) = delete;
    linear_alloc& operator=(const linear_alloc& other) = delete;
    linear_alloc& operator=(linear_alloc&& other) = delete;
};


#endif // guard
