#include <stdexcept>
#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <stdint.h>

enum AllocErrorType {
    InvalidFree,
    NoMemory,
};

class AllocError: std::runtime_error {
private:
    AllocErrorType type;

public:
    AllocError(AllocErrorType _type, std::string message):
            runtime_error(message),
            type(_type)
    { }

    AllocErrorType getType() const { return type; }
};

class Allocator;

class Memblock {
public:

    Memblock(int8_t *_start, size_t _size) : start(_start), size(_size)
    {
        end = start + size - 1;
    }

    Memblock() : start(NULL), end(NULL), size(0) { } 

    int8_t *get_start() { return start; }
    int8_t *get_end() { return end; }
    size_t get_size() { return size; }

    void shift_start(int8_t *_start)
    {
        start = _start;
        size = end - start + 1;
    }

    void shift_end(int8_t *_end)
    {
        end = _end;
        size = end - start + 1;
    }

    void set_null()
    {
        start = NULL;
        end = NULL;
        size = 0;
    }

    void show() 
    {
        printf("[ %p | %lu | %p ] ", start, size, end);
    }
    
private:
    int8_t *start, *end;
    size_t size;
};

class Pointer {
public:
    
    Pointer() : block(NULL), id(-1) { } 
    Pointer(Memblock **_block, int _id) : block(_block), id(_id) { } 
    void *get() const 
    {
        if (block != NULL) 
            return reinterpret_cast<void*> ((*block)->get_start()); 
        else 
            return NULL;
    } 

    void set_null()
    {
        block = NULL;
        id = -1;
    }
    
    void show()
    {
        if (block != NULL)
        {
            (*block)->show();
        }
        else
        {
            printf("NULL");
        }
    }
    int get_id() { return id; }


private: 
    Memblock **block;
    int id;
};


const int max_ptrs = 2048;
class Allocator {
public:

    Allocator(void *_base, size_t _size) :
        base(reinterpret_cast<int8_t*>(_base)), size(_size)        
    { 
        for (int i = 0; i < max_ptrs; i++)
        {
            blocks_used[i] = NULL;
            blocks_free[i] = NULL;
        }
    
        Memblock *block = new Memblock(base, size);
        add_block_free(block);
    }

    ~Allocator()
    {
        for (int i = 0; i < max_ptrs; i++)
        {
            if (blocks_used[i] != NULL)
            {
                delete blocks_used[i];
                blocks_used[i] = NULL;
            }
            if (blocks_free[i] != NULL)
            {
                blocks_free[i] = NULL;
                delete blocks_free[i];
            }
        }
    }
    
    Pointer alloc(size_t N);
    void realloc(Pointer &p, size_t N);
    void free(Pointer &p);

    void defrag(); 
    std::string dump() { return ""; }

    void show() 
    {
        printf("Free:\n");
        for (int i = 0; i < max_ptrs; i++)
        {
            if (blocks_free[i] != NULL)
                blocks_free[i]->show();
        }
        printf("\nUsed:\n");
        for (int i = 0; i < max_ptrs; i++)
        {
            if (blocks_used[i] != NULL)
                blocks_used[i]->show();
        }
        printf("\n");
    }

private:

    int8_t *base;
    size_t size;
    
    Memblock *blocks_used[max_ptrs];
    Memblock *blocks_free[max_ptrs];

    int add_block_used(Memblock *block);
    void del_block_used(int id);
    int add_block_free(Memblock *block);
    void del_block_free(int id);

    int find_free_block(int8_t *b_start);
    void shift_block_used(int id, int8_t *new_start);
    void shift_block_free(int id, int8_t *new_start);
};

