#include "allocator.h"

void Allocator::del_block_used(int id)
{
    delete blocks_used[id];
    blocks_used[id] = NULL;
}

void Allocator::del_block_free(int id)
{
    delete blocks_free[id];
    blocks_free[id] = NULL;
} 

int Allocator::add_block_used(Memblock *block)
{
    int i = 0;
    bool found = false;
    while (!found && i < max_ptrs)
    {
        if (blocks_used[i] == NULL)
        {
            found = true;
        }
        else
        {
            i++;
        }
    }

    if (found) 
    {
        blocks_used[i] = block;
        return i;
    }
    else
        throw AllocError(NoMemory, "Maximum pointers reached");
    return -1;
}

int Allocator::add_block_free(Memblock *block)
{

    int j = 0;
    while (j < max_ptrs)
    {
        if (blocks_free[j] != NULL && 
            blocks_free[j]->get_end() == block->get_start() - 1)
        {
            block->shift_start(blocks_free[j]->get_start());
            del_block_free(j);
            return add_block_free(block);
        }
        
        else if (blocks_free[j] != NULL &&
                 blocks_free[j]->get_start() == block->get_end() + 1)
        {
            block->shift_end(blocks_free[j]->get_end());
            del_block_free(j);
            return add_block_free(block);
        }

        j++;
    }
    
    int i = 0;
    bool found = false;
    while (!found && i < max_ptrs)
    {
        if (blocks_free[i] == NULL)
        {
            found = true;
        }
        else
        {
            i++;
        }
    }

    if (found) 
    {
        blocks_free[i] = block;
        return i;
    }
    else
        throw AllocError(NoMemory, "Maximum pointers reached");
    return -1;
}

Pointer Allocator::alloc(size_t N)
{
    Pointer pointer;

    int i = 0;
    bool found = false;
    while (!found && i < max_ptrs)
    {
        if (blocks_free[i] == NULL)
        {
            i++;
            continue;
        }
            
        if (blocks_free[i]->get_size() >= N)
        {
            found = true;
        }
        else
        {
            i++;
        }
    }
    
    if (found)
    {
        size_t size_old = blocks_free[i]->get_size();
        int8_t *start_old = blocks_free[i]->get_start();
        
        if (size_old == N)
        {
            del_block_free(i);
        }
        else
        {
            int8_t *start_new = start_old + N;
            blocks_free[i]->shift_start(start_new);
        }
 
        Memblock *block = new Memblock(start_old, N);
        int i = add_block_used(block);
        pointer = Pointer(&blocks_used[i], i);
    }
    else
    {
        throw AllocError(NoMemory, "alloc()");
    }

    return pointer;
}
        
void Allocator::free(Pointer &p) 
{
    int i = p.get_id();

    int8_t *p_start = blocks_used[i]->get_start();
    size_t p_size = blocks_used[i]->get_size();
    int8_t *p_end = blocks_used[i]->get_end();
    
    del_block_used(i);

    Memblock *block = new Memblock(p_start, p_size);
    add_block_free(block);

    p.set_null();
} 

void Allocator::realloc(Pointer &p, size_t N)
{
    int i = p.get_id();
    
    if (i == -1)
    {
        p = alloc(N);
        return;
    }

    int8_t *p_start = blocks_used[i]->get_start();
    size_t p_size = blocks_used[i]->get_size();
    int8_t *p_end = blocks_used[i]->get_end();
    
    if (p_size == N)
        return;

    if (p_size > N)
    {
        size_t size_delta = p_size - N;
        blocks_used[i]->shift_end(p_end - size_delta);

        Memblock *block = new Memblock(p_end-size_delta+1, size_delta);
        add_block_free(block);
        return;
    }

    size_t size_delta = N - p_size;

    int j = 0;
    bool found_next = false;
    while (!found_next && j < max_ptrs)
    {
        if (blocks_free[j] != NULL &&
            blocks_free[j]->get_start() == p_end + 1)
        {
            found_next = true;
        }
        else
        {
            j++;
        }
    }
    
    if (found_next)
    {
        if (blocks_free[j]->get_size() > size_delta)
        {
            blocks_used[i]->shift_end(p_end + size_delta);
            blocks_free[j]->shift_start(p_end + size_delta + 1);
            return;
        }
        else if (blocks_free[j]->get_size() == size_delta)    
        {
            blocks_used[i]->shift_end(p_end + size_delta);
            del_block_free(j);
            return;
        } 
    }
        
    j = 0;
    bool found = false;
    while (!found && j < max_ptrs)
    {
        if (blocks_free[j] != NULL &&
            blocks_free[j]->get_size() >= N)
        {
            found = true;
        }
        else
        {
            j++;
        }
    }
    
    if (!found)
        throw AllocError(NoMemory, "realloc()");
    
    int8_t *start_new = blocks_free[j]->get_start();

    for (int k = 0; k < p_size; k++)
    {
        start_new[k] = p_start[k];
    }

    Memblock *block = new Memblock(start_new, N);
    
    if (blocks_free[j]->get_size() == N)
    {
       del_block_free(j);
    }
    else
    {
        blocks_free[j]->shift_start(start_new + N);
    }

    del_block_used(i);
    
    Memblock *free_block = new Memblock(p_start, p_size);
    add_block_free(free_block);
    
    blocks_used[i] = block;
}

int Allocator::find_free_block(int8_t *b_start)
{
    int i = 0;
    while (i < max_ptrs)
    {
        if (blocks_free[i] != NULL &&
            blocks_free[i]->get_end() == b_start-1)
        {
            return i;
        }
        i++;
    }

    return -1;
}

void Allocator::shift_block_used(int id, int8_t *new_start)
{
    int8_t *b_start = blocks_used[id]->get_start();
    size_t b_size = blocks_used[id]->get_size();
    int8_t *b_end = blocks_used[id]->get_end();

    for (int i = 0; i < b_size; i++)
    {
        new_start[i] = b_start[i];
    }
    
    blocks_used[id]->shift_start(new_start);
    blocks_used[id]->shift_end(b_end - b_start + new_start);
}

void Allocator::shift_block_free(int id, int8_t *new_start)
{   
    blocks_free[id]->shift_start(new_start);
}
       
void Allocator::defrag()
{
    int closest_id = -1, free_id = -1;
    int8_t *free_start = NULL;
    size_t free_size = 0;
    int8_t *min_start = base + size + 1;
    for (int i = 0; i < max_ptrs; i++)
    {
        if (blocks_used[i] == NULL)
            continue;

        int8_t *b_start = blocks_used[i]->get_start();
        int j = find_free_block(b_start);
        if (j != -1 && b_start < min_start)
        {
            closest_id = i;
            free_id = j;
            free_start = blocks_free[j]->get_start();
            free_size = blocks_free[j]->get_size();
            min_start = b_start;
        }
    }            
    
    if (closest_id == -1)
        return;

    shift_block_used(closest_id, free_start);
    
    del_block_free(free_id);

//    Memblock *block = new Memblock(min_start, free_size);
    Memblock *block = new Memblock(blocks_used[closest_id]->get_end() + 1, free_size); 
    add_block_free(block);        

    defrag();
}
 

/*                                      
char buff[65536];
        
int main()
{
   
    Allocator a(buff, sizeof(buff));    
   
    int size = 135;
    
    Pointer p1 = a.alloc(size);
    Pointer p2 = a.alloc(size);

    a.realloc(p1, 270);
    a.free(p1);
    a.free(p2);

    a.show();

}        
*/   
    
    
    
    
    
    
                                   
