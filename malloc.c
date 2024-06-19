#include <unistd.h>
#include <assert.h>
#include <string.h>

#define BLOCK_META_SIZE sizeof(struct memory_block_meta)
#define ALIGNMENT_SIZE 16

struct memory_block_meta
{
    size_t size;
    struct memory_block_meta *next;
    int free;   
};


struct memory_block_meta *base = NULL, *last = NULL;

size_t 
align_size(size_t size)
{
    if(size % ALIGNMENT_SIZE == 0)
    {
        return size;
    }
    return ALIGNMENT_SIZE * (size / ALIGNMENT_SIZE + 1);
}

struct memory_block_meta*
find_free_block(size_t size)
{
    struct memory_block_meta* current_block = base;
    while(current_block && !((current_block->free != 0) && (current_block->size >= size)))
    {
        current_block = current_block->next;
    }
    
    return current_block;
}

struct memory_block_meta*
request_new_block(size_t size)
{
    void* p = sbrk(size + BLOCK_META_SIZE);
    if(p == (void*)-1) 
    {
        return NULL;
    } 

    struct memory_block_meta *new_block = p;
    new_block->free = 0;
    new_block->size = size;
    new_block->next = NULL;
    
    if(last)
    {
        last->next = new_block;
    }
    last = new_block;

    return new_block;
    
}

int
split_block(struct memory_block_meta* block, size_t size)
{
    if(size + BLOCK_META_SIZE >= block->size)
    {
        return 0;
    }

    struct memory_block_meta* new_block = (struct memory_block_meta*)((void*)(block + 1) + size);
    new_block->free = 1;
    new_block->next = block->next;
    new_block->size = block->size - BLOCK_META_SIZE - size;
    block->next = new_block;
    block->size = size; 

    return 1;
}

void* 
malloc(size_t size)
{
    struct memory_block_meta* block = NULL;  
    size = align_size(size);
    if(!base)
    {
        block = request_new_block(size);
        if(!block)
        {
            return NULL;
        }
        base = block;
    } else {
        block = find_free_block(size);
        if(block)
        {
            if(size + BLOCK_META_SIZE < block->size)
            {
                split_block(block, size);
            }
            block->free = 0;
        } else {
            block = request_new_block(size);
            if(!block) {
                return NULL;
            }
        }
    }

    return block + 1;
}

struct memory_block_meta*
merge_blocks(struct memory_block_meta* first_block, struct memory_block_meta* second_block)
{
    if(!first_block || !second_block)
    {
        if(first_block)
        {
            return first_block;
        }

        if(second_block)
        {
            return second_block;
        }

        return NULL;
    }
    
    struct memory_block_meta* new_block = first_block < second_block ? first_block : second_block;
    new_block->free = 1;
    new_block->size = first_block->size + second_block->size + BLOCK_META_SIZE;
    new_block->next = first_block < second_block ? second_block->next : first_block->next;

    return new_block;
}

void
free(void* ptr)
{
    if(!ptr)
    {
        return;
    }
    
    struct memory_block_meta* block = (struct memory_block_meta*)ptr - 1;
    block->free = 1;

    // TODO: merge with previous block
    if(block->next && (block->next->free != 0))
    {
        merge_blocks(block, block->next);
    }
    
}

void*
realloc(void* ptr, size_t size)
{
    if(size == 0)
    {
        free(ptr);
        return NULL;
    }

    if(!ptr)
    {
        return malloc(size);
    }
    
    struct memory_block_meta* block = (struct memory_block_meta*)ptr - 1;
    if(block->size >= size)
    {
        return ptr;
    }
    
    void* new_ptr = malloc(size);

    if(new_ptr)
    {
        memcpy(new_ptr, ptr, block->size);
        if(block->free == 0)
        {
            free(ptr);
        }
    }    

    return new_ptr;
}

void*
calloc(size_t nelem, size_t elemsize)
{
    size_t size = nelem * elemsize;
    void* ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

