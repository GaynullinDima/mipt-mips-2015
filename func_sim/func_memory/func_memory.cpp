/**
 * func_memory.cpp - the module implementing the concept of
 * programer-visible memory space accesing via memory address.
 * @author Alexander Titov <alexander.igorevich.titov@gmail.com>
 * Copyright 2012 uArchSim iLab project
 */

// Generic C
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <cassert>
// Generic C++
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
// uArchSim modules
#include <func_memory.h>


enum States
{
    RDWR,    // read or write byte
    CNG_PG,  // change page
    CNG_SET, // change set
  	EXIT     // exit failure
};

FuncMemory::FuncMemory( const char* executable_file_name,
                        uint64 addr_size,
                        uint64 page_bits,
                        uint64 offset_bits)
{
    assert( executable_file_name);
    assert( page_bits + offset_bits <= addr_size);

    
    addr_bits = addr_size;
    page_num_size = page_bits;
    offset_size = offset_bits;
    sets_array = NULL;

    vector<ElfSection> sections_array;
    ElfSection::getAllElfSections( executable_file_name, sections_array);
    vector<ElfSection>::iterator section_itr = sections_array.begin();

    sets_array_size  = 1 << ( addr_size - page_bits - offset_bits);
    pages_array_size = 1 << page_bits;
    page_size        = 1 << offset_bits;

    //create array of pointers to arraies of page
    sets_array = ( uint8***)calloc( sets_array_size, sizeof(uint8**)); 

    while ( section_itr != sections_array.end())
    {
        uint64 start_addr = section_itr->start_addr;
      
        if ( strcmp( section_itr->name, ".text") == 0)
            text_start_addr = start_addr;
        
        for ( int i = 0; i < section_itr->size; i++)
        {
            uint64 set_idx = getSetIdx( addr_size - page_bits - offset_bits, 
                                        section_itr->start_addr + i,        
                                        addr_size);							 
          
            if ( sets_array[ set_idx] == NULL)
            {
            //create array of pointers to pages
                sets_array[ set_idx] = ( uint8**)calloc( pages_array_size, sizeof( uint8*));
            }
          
            uint64 page_idx = getPageIdx( page_bits,
                                          offset_bits,
                                          section_itr->start_addr + i);		
                          
            if ( sets_array[ set_idx][ page_idx] == NULL)
            {
            //create array of bytes
                sets_array[ set_idx][ page_idx] = ( uint8*)calloc( page_size, sizeof( uint8));
            }
            
            uint64 offset_idx = getOffsetIdx( offset_bits, section_itr->start_addr + i);
            sets_array[ set_idx][ page_idx][ offset_idx] = ( section_itr->content)[ i];
          
        }
        section_itr++;
    }
}


FuncMemory::~FuncMemory()
{
    delete [] this->sets_array;
}


uint64 FuncMemory::startPC() const
{
    return this->text_start_addr;
}

uint64 FuncMemory::read( uint64 addr, unsigned short num_of_bytes) const
{
    assert( num_of_bytes > 0);
    
    unsigned short bytes_read = 0;
    uint64 number = 0;

    uint64 set_idx = getSetIdx( addr_bits - page_num_size - offset_size, 
                                addr,					                 
                                addr_bits);						         

    uint64 page_idx = getPageIdx( page_num_size,
                                  offset_size,
                                  addr);							 	 

    uint64 offset_idx = getOffsetIdx( offset_size, addr);

    assert( sets_array != NULL)
    if ( sets_array[ set_idx] == NULL)
    {
        cerr << "Sigmentation fault";
        assert( sets_array[ set_idx == NULL]);
    } else if ( sets_array[ set_idx][ page_idx] == NULL)
    {
        cerr << "Sigmentation fault";
        assert( sets_array[ set_idx][ page_idx] == NULL);
    }
    
    States state = RDWR;

    //State machine: 
    //State RDWR    - reading bytes
    //State CNG_PG  - changing page
    //State CNG_SET - changing set
    //State EXIT    - exit in case of an error
    while ( bytes_read < num_of_bytes)
    {
        switch( state)
        {
            case RDWR:
                number += ( sets_array[ set_idx][ page_idx][ offset_idx] << ( bytes_read * 8));
                bytes_read++;
                offset_idx++;
      
                if ( offset_idx >= page_size)
                {
                    state = CNG_PG;
                    page_idx++;
                }
            break;
    
            case CNG_PG:
                offset_idx = 0;
                state = RDWR;
      
                if ( page_idx >= pages_array_size)
                {
                    state = CNG_SET;
                    set_idx++;
                }
                else if ( sets_array[ set_idx][ page_idx] == NULL)
                    state = EXIT;
            break;
    
            case CNG_SET:
                page_idx = 0;
                state = CNG_PG;
      
      
                if ( set_idx >= sets_array_size)
                    state = EXIT;
                else if ( sets_array[ set_idx] == NULL)
                    state = EXIT;
            break;
    
            case EXIT:
                cerr << "Sigmentation fault1" << endl << endl;
                assert( 0);
            break;
      }
}

    return number;

    return 0;
}

void FuncMemory::write( uint64 value, uint64 addr, unsigned short num_of_bytes)
{
    assert(num_of_bytes > 0);
    
	
    union
    {
	uint64 number;
	uint8 bytes[8];
    };

    unsigned short bytes_write = 0;
    number = value;
	
    uint64 set_idx = getSetIdx( addr_bits - page_num_size - offset_size, 
			        addr,				
			        addr_bits);						       	
    uint64 page_idx = getPageIdx( page_num_size,
			          offset_size,
                	          addr);							 	 	
    uint64 offset_idx = getOffsetIdx( offset_size, addr);
	

    if ( sets_array[ set_idx] == NULL)
    {
	sets_array[ set_idx] = ( uint8**)calloc( pages_array_size, sizeof( uint8*));
	sets_array[ set_idx][ page_idx] = ( uint8*)calloc( page_size, sizeof( uint8));
    } else if ( sets_array[ set_idx][ page_idx] == NULL)
	sets_array[ set_idx][ page_idx] = ( uint8*)calloc( page_size, sizeof( uint8));		
	
    //State machine: 
    //State RDWR    - writing bytes
    //State CNG_PG  - changing page
    //State CNG_SET - changing set
    //State EXIT    - exit in case of an error
    States state = RDWR;
	
    while ( bytes_write < num_of_bytes)
    {
	switch( state)
	{
	    case RDWR:
		sets_array[ set_idx][ page_idx][ offset_idx] = bytes[ bytes_write];
		bytes_write++;
		offset_idx++;			
                if ( offset_idx >= page_size)
		{
		    state = CNG_PG;
       		    page_idx++;
		}
	     break;
			
             case CNG_PG:
		offset_idx = 0;
		state = RDWR;
				
		if ( page_idx >= pages_array_size)
		{
		    state = CNG_SET;
		    set_idx++;
		}
		else if ( sets_array[ set_idx][ page_idx] == NULL)
	            sets_array[ set_idx][ page_idx] = ( uint8*)calloc( page_size, sizeof( uint8));
	     break;
			
	     case CNG_SET:
		page_idx = 0;
		state = CNG_PG;
				
		if ( set_idx >= sets_array_size)
		    state = EXIT;
		else if ( sets_array[ set_idx] == NULL)
	            sets_array[ set_idx] = ( uint8**)calloc( pages_array_size, sizeof( uint8*));
	     break;
			
	     case EXIT:
		cerr << "Sigmentation fault" << endl;
		assert( 0);
	     break;
	}
    }
}

string FuncMemory::dump( string indent) const
{
    // put your code here
    return string("ERROR: You need to implement FuncMemory!");
}

