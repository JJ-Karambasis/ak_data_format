#ifndef AK_DF_H
#define AK_DF_H

#ifdef AK_DF_STATIC
#define AK_DF_DEF static
#else
#ifdef __cplusplus
#define AK_DF_DEF extern "C"
#else
#define AK_DF_DEF extern
#endif // __cplusplus
#endif //AK_DF_STATIC

#ifndef __cplusplus
#include <stdalign.h>
#endif

#include <stdarg.h>
#include <stddef.h>

typedef unsigned char      akdf_u8;
typedef   signed char      akdf_i8;
typedef unsigned short     akdf_u16;
typedef   signed short     akdf_i16;
typedef unsigned int       akdf_u32;
typedef   signed int       akdf_i32;
typedef unsigned long long akdf_u64;
typedef   signed long long akdf_i64;

typedef enum akdf_parser_node_type 
{
    AKDF_PARSER_NODE_TYPE_INTEGER,
    AKDF_PARSER_NODE_TYPE_DOUBLE,
    AKDF_PARSER_NODE_TYPE_STRING,
    AKDF_PARSER_NODE_TYPE_OBJECT,
    AKDF_PARSER_NODE_TYPE_ARRAY
} akdf_parser_node_type;

typedef struct akdf_parser_node 
{ 
    akdf_parser_node_type Type; 
} akdf_parser_node;

typedef struct akdf_context
{
    akdf_parser_node* RootNode;
} akdf_context;

typedef struct akdf_parsing_error
{
    akdf_u64    Length;
    const char* Error;
} akdf_parsing_error;

typedef struct akdf_parsing_errors
{
    akdf_u64           ErrorCount;
    akdf_parsing_error Errors;
} akdf_parsing_errors;

AK_DF_DEF akdf_context*  AKDF_Parse(const char* Buffer, akdf_u64 BufferLength);
AK_DF_DEF akdf_context*  AKDF_Stream();
AK_DF_DEF void           AKDF_Release(akdf_context* Context);

//NOTE(EVERYONE): Parsing api
AK_DF_DEF const akdf_parser_node*  AKDF_Get_Entry(const akdf_parser_node* Node, const char* Str);
AK_DF_DEF akdf_i64                 AKDF_Get_Integer(const akdf_parser_node* Node);
AK_DF_DEF double                   AKDF_Get_Double(const akdf_parser_node* Node);
AK_DF_DEF const char*              AKDF_Get_String(const akdf_parser_node* Node);
AK_DF_DEF akdf_u64                 AKDF_Get_Array_Count(const akdf_parser_node* Node);
AK_DF_DEF const akdf_parser_node*  AKDF_Get_Array(const akdf_parser_node* Node);
AK_DF_DEF akdf_parsing_errors      AKDF_Get_Parsing_Errors();

//NOTE(EVERYONE): Streaming api
AK_DF_DEF akdf_parser_node* AKDF_Create_Integer(akdf_context* Context, akdf_i64 Value);
AK_DF_DEF akdf_parser_node* AKDF_Create_Double(akdf_context* Context, double Value);
AK_DF_DEF akdf_parser_node* AKDF_Create_String(akdf_context* Context, const char* Str);
AK_DF_DEF akdf_parser_node* AKDF_Create_Array(akdf_context* Context);
AK_DF_DEF akdf_parser_node* AKDF_Create_Object(akdf_context* Context);
AK_DF_DEF void AKDF_Array_Insert(akdf_parser_node* ArrayNode,  akdf_parser_node* ValueNode);
AK_DF_DEF void AKDF_Object_Insert(akdf_parser_node* ParentNode, akdf_parser_node* ChildNode, const char* ChildName);
AK_DF_DEF const char* AKDF_Serialize(akdf_context* Context, akdf_u64* StreamLength);

#endif //AK_DATA_FORMAT_H

#ifndef AK_DF_MALLOC
#include <stdlib.h>
#define AK_DF_MALLOC(size) malloc(size)
#define AK_DF_FREE(memory) free(memory)
#endif //AK_DF_MALLOC

#ifndef AK_DF_MEMCPY
#include <string.h>
#define AK_DF_MEMCPY(dest, src, n) memcpy(dest, src, n)
#define AK_DF_MEMSET(ptr, value, n) memset(ptr, value, n)
#endif //AK_DF_MEMCPY

#ifndef AK_DF_ASSERT
#include <assert.h>
#define AK_DF_ASSERT(condition, message) assert(condition)
#endif //AK_DF_ASSERT

#ifdef AK_DF_IMPLEMENTATION

static void AKDF__Memory_Copy(void* Dest, const void* Src, size_t Size)
{
    AK_DF_MEMCPY(Dest, Src, Size);
}

static void AKDF__Memory_Set(void* Dest, akdf_u8 Value, size_t Size)
{
    AK_DF_MEMSET(Dest, Value, Size);
}

static void AKDF__Memory_Clear(void* Dest, size_t Size)
{
    AK_DF_MEMSET(Dest, 0, Size);
}

static void* AKDF__Malloc(size_t Size)
{
    return AK_DF_MALLOC(Size);
}

static void AKDF__Free(void* Memory)
{
    if(Memory) AK_DF_FREE(Memory);
}

static akdf_u64 AKDF__Memory_Align(akdf_u64 Value, akdf_u64 Alignment)
{
    Alignment = Alignment ? Alignment : 1;
    akdf_u64 Mod = Value & (Alignment-1);
    return Mod ? Value + (Alignment-Mod) : Value;
}

//~Internal arena implementation
typedef struct akdf__arena akdf__arena;

typedef struct akdf__arena_block
{
    akdf_u8*           Memory;
    akdf_u64           Used;
    akdf_u64           Size;
    struct akdf__arena_block* Next;
} akdf__arena_block;

typedef struct akdf__arena_marker
{
    akdf__arena*       Arena;
    akdf__arena_block* Block;
    akdf_u64           Marker;
} akdf__arena_marker;

struct akdf__arena
{
    akdf__arena_block* FirstBlock;
    akdf__arena_block* CurrentBlock;
    akdf__arena_block* LastBlock;
    akdf_u64           InitialBlockSize;
};

static akdf__arena AKDF__Create_Arena(akdf_u64 InitialBlockSize)
{
    akdf__arena Result;
    AKDF__Memory_Clear(&Result, sizeof(akdf__arena));
    Result.InitialBlockSize = InitialBlockSize;
    return Result;
}

static void AKDF__Delete_Arena(akdf__arena* Arena)
{
    if(Arena->FirstBlock)
    {
        akdf__arena_block* Block = Arena->FirstBlock;
        while(Block)
        {
            akdf__arena_block* BlockToDelete = Block;
            Block = Block->Next;
            AKDF__Free(BlockToDelete);
        }
    }
    AKDF__Memory_Clear(Arena, sizeof(akdf__arena));
}

static akdf__arena_block* AKDF__Arena_Allocate_Block(akdf_u64 BlockSize)
{
    akdf__arena_block* Block = (akdf__arena_block*)AKDF__Malloc(sizeof(akdf__arena_block)+BlockSize);
    if(!Block)
    {
        //TODO(JJ): Diagnostic and error logging
        return NULL;
    }
    
    AKDF__Memory_Clear(Block, sizeof(akdf__arena_block));
    Block->Memory = (akdf_u8*)(Block+1);
    Block->Size = BlockSize;
    
    return Block;
}

static void AKDF__Arena_Add_Block(akdf__arena* Arena, akdf__arena_block* Block)
{
    (Arena->FirstBlock == 0) ? 
    (Arena->FirstBlock = Arena->LastBlock = Block) : 
    (Arena->LastBlock->Next = Block, Arena->LastBlock = Block);
}

static akdf__arena_block* AKDF__Arena_Get_Block(akdf__arena* Arena, akdf_u64 Size, akdf_u64 Alignment)
{
    akdf__arena_block* Block = Arena->CurrentBlock;
    if(!Block) return NULL;
    
    akdf_u64 Used = AKDF__Memory_Align(Block->Used, Alignment);
    while(Used+Size > Block->Size)
    {
        Block = Block->Next;
        if(!Block) return NULL;
        Used = AKDF__Memory_Align(Block->Used, Alignment);
    }
    
    return Block;
}

static void* AKDF__Arena_Allocate(akdf__arena* Arena, akdf_u64 Size, akdf_u64 Alignment)
{
    if(!Size) return NULL;
    
    akdf__arena_block* Block = AKDF__Arena_Get_Block(Arena, Size, Alignment);
    if(!Block)
    {
        akdf_u64 BlockSize = Arena->InitialBlockSize;
        if(Size > BlockSize)
            BlockSize = (Alignment) ? AKDF__Memory_Align(Size, Alignment) : Size;
        
        Block = AKDF__Arena_Allocate_Block(BlockSize);
        if(!Block)
        {
            //TODO(JJ): Diagnostic and error logging
            return NULL;
        }
        
        AKDF__Arena_Add_Block(Arena, Block);
    }
    
    Arena->CurrentBlock = Block;
    if(Alignment) Arena->CurrentBlock->Used = AKDF__Memory_Align(Arena->CurrentBlock->Used, Alignment);
    
    akdf_u8* Ptr = Arena->CurrentBlock->Memory + Arena->CurrentBlock->Used;
    Arena->CurrentBlock->Used += Size;
    
    return Ptr;
}

static akdf__arena_marker AKDF__Arena_Get_Marker(akdf__arena* Arena)
{
    akdf__arena_marker Marker;
    Marker.Arena = Arena;
    Marker.Block = Arena->CurrentBlock;
    if(Arena->CurrentBlock) Marker.Marker = Arena->CurrentBlock->Used;
    return Marker;
}

static void AKDF__Arena_Set_Marker(akdf__arena* Arena, akdf__arena_marker Marker)
{
    AK_DF_ASSERT(Arena == Marker.Arena, "Invalid arena for marker!");
    
    //NOTE(EVERYONE): If the block is null it always signalizes the beginning of the arena
    if(!Marker.Block)
    {
        Arena->CurrentBlock = Arena->FirstBlock;
        if(Arena->CurrentBlock) Arena->CurrentBlock->Used = 0;
    }
    else
    {
        Arena->CurrentBlock = Marker.Block;
        Arena->CurrentBlock->Used = Marker.Marker;
        
        akdf__arena_block* ArenaBlock;
        for(ArenaBlock = Arena->CurrentBlock->Next; ArenaBlock; ArenaBlock = ArenaBlock->Next)
            ArenaBlock->Used = 0;
    }
}

#define AKDF__Push_Struct(arena, type) (type*)AKDF__Arena_Allocate(arena, sizeof(type), alignof(type))

//~Shared internal implementation

typedef struct akdf__parsing_error_stream
{
    akdf__arena         ErrorStream;
    akdf_parsing_errors Errors;
} akdf__parsing_error_stream;

static akdf__parsing_error_stream Global_AKDF__ErrorStream;
static void AKDF__Reset_Parsing_Errors()
{
    if(Global_AKDF__ErrorStream.ErrorStream.InitialBlockSize)
        AKDF__Delete_Arena(&Global_AKDF__ErrorStream.ErrorStream);
    Global_AKDF__ErrorStream.ErrorStream = AKDF__Create_Arena(1024*32);
    AKDF__Memory_Clear(&Global_AKDF__ErrorStream.Errors, sizeof(akdf_parsing_errors));
}

typedef enum akdf__context_type
{
    AKDF__CONTEXT_TYPE_PARSING,
    AKDF__CONTEXT_TYPE_STREAMING
} akdf__context_type;

typedef struct akdf__parse_header
{
    akdf__context_type Type;
} akdf__parse_header;

//~Parse internal implementation
typedef struct akdf__parse_context
{
    akdf_context       Context;
    akdf__parse_header Header;
} akdf__parse_context;

AK_DF_DEF akdf_context* AKDF_Parse(const char* Buffer, akdf_u64 BufferLength)
{
    AKDF__Reset_Parsing_Errors();
    akdf__arena Arena = AKDF__Create_Arena(1024*1024);
    akdf__parse_context* Context = AKDF__Push_Struct(&Arena, akdf__parse_context);
    return (akdf_context*)Context;
}

AK_DF_DEF const akdf_parser_node*  AKDF_Get_Entry(const akdf_parser_node* Node, const char* Str);
AK_DF_DEF akdf_i64                 AKDF_Get_Integer(const akdf_parser_node* Node);
AK_DF_DEF double                   AKDF_Get_Double(const akdf_parser_node* Node);
AK_DF_DEF const char*              AKDF_Get_String(const akdf_parser_node* Node);
AK_DF_DEF akdf_u64                 AKDF_Get_Array_Count(const akdf_parser_node* Node);
AK_DF_DEF const akdf_parser_node*  AKDF_Get_Array(const akdf_parser_node* Node);

AK_DF_DEF akdf_parsing_errors AKDF_Get_Parsing_Errors()
{
    return Global_AKDF__ErrorStream.Errors;
}

static void AKDF__Release_Parser(akdf__parse_context* Context)
{
}

//~Stream internal implementation
typedef struct akdf__stream_context
{
    akdf_context       Context;
    akdf__parse_header Header;
} akdf__stream_context;

AK_DF_DEF akdf_context* AKDF_Stream()
{
    akdf__arena Arena = AKDF__Create_Arena(1024*1024);
    akdf__stream_context* Context = AKDF__Push_Struct(&Arena, akdf__stream_context);
    return (akdf_context*)Context;
}

AK_DF_DEF akdf_parser_node* AKDF_Create_Integer(akdf_context* Context, akdf_i64 Value);
AK_DF_DEF akdf_parser_node* AKDF_Create_Double(akdf_context* Context, double Value);
AK_DF_DEF akdf_parser_node* AKDF_Create_String(akdf_context* Context, const char* Str);
AK_DF_DEF akdf_parser_node* AKDF_Create_Array(akdf_context* Context);
AK_DF_DEF akdf_parser_node* AKDF_Create_Object(akdf_context* Context);
AK_DF_DEF void AKDF_Array_Insert(akdf_parser_node* ArrayNode,  akdf_parser_node* ValueNode);
AK_DF_DEF void AKDF_Object_Insert(akdf_parser_node* ParentNode, akdf_parser_node* ChildNode, const char* ChildName);
AK_DF_DEF const char* AKDF_Serialize(akdf_context* Context, akdf_u64* StreamLength);

static void AKDF__Release_Stream(akdf__stream_context* Context)
{
}

//~Release implementation
AK_DF_DEF void AKDF_Release(akdf_context* Context)
{
    akdf__parse_header* Header = (akdf__parse_header*)(Context+1);
    if(Header->Type == AKDF__CONTEXT_TYPE_STREAMING) AKDF__Release_Stream((akdf__stream_context*)Context);
    else AKDF__Release_Parser((akdf__parse_context*)Context);
}

#endif