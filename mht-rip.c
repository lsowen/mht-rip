/* Copyright (C) 2010 Logan Owen;
All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

mht-rip converts a mhtml (or mht) file to a series of binary and
plain text files.  Allows reading of the images that were in the 
mhtml file.

usage: mht-rip INPUTFILE */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define ALLOC_SIZE 10000

#define F_PREPARE 15
#define F_READ 20

#define T_TEXT 1
#define T_BASE64 2

struct page
{
  struct page *next;
  struct page *previous;
  char *page_buffer;
  int size;
  int location;
};

static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

void decodeblock( unsigned char in[4], unsigned char out[3] )
{   
    out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

size_t decode_buffer( char *line )
{
  char *writer = line;
  unsigned char in_block[4], out_block[3], v;
  int i;
  int j;
  int len;

  int out_length = 0;
  int in_length = strlen( line );

  i = 0;
  while( i < in_length )
    {
      v = (unsigned char) line[i];
      v = (unsigned char) ((v < 43 || v > 122 ) ? 0 : cd64[ v - 43 ]);
      if( v )
      {
        v = (unsigned char) ((v == '$') ? 0 : v - 61);
      }
      line[i] = v;
      i++;
    }

  j = 0;
  i = 0;
  len = 0;
  
  while( j < in_length )
    {
      v = line[j];
      
      if( v != 0 )
	  {
        in_block[ i ] = (unsigned char) (v - 1);
        i++;
        len++;
	  }

      if( j == in_length - 1)
	  {
        while( i < 4 )
	    {
	      in_block[ i ] = 0;
          i++;
	    }

	  }

      if( i == 4 )
      {
          decodeblock( in_block, out_block );
          for( i = 0; i < len - 1; i++ )
          {
            *writer = out_block[ i ];
            writer++;
            out_length++;
          }   
          i = 0;
          len = 0;
      }
      j++;
    }
  
  return out_length;
}

struct page *new_page()
{
  struct page *new_page;

  new_page = malloc( sizeof( struct page ) );

  new_page->previous = NULL;
  new_page->page_buffer = NULL;
  new_page->size = 0;
  new_page->location = 0;
  new_page->next = NULL;


  return new_page;
} 

int line_to_page( char *line, struct page *current, int line_length )
{
  if( (current->location + line_length) > current->size )
    {
      current->size += ((current->location + line_length - current->size )/ALLOC_SIZE +1)*ALLOC_SIZE;
      current->page_buffer = (char *) realloc( current->page_buffer, sizeof( char ) * current->size );
    }

  memcpy( current->page_buffer + current->location, line, line_length * sizeof( char ) );
  current->location += line_length;
}

int page_to_file( struct page *current, int o_count )
{
  FILE *o_file;
  char *filename = malloc( 15 * sizeof( char ) );

  if( current->location > 0 )
    {
      sprintf( filename, "%d.file", o_count );  

      o_file = fopen( filename, "wb" );

      fwrite( current->page_buffer, sizeof( char ), current->location , o_file );

      fclose( o_file );
    }
  free( filename );
}


char *get_end_of_line( char *in_buffer, size_t buffer_size, char *line )
{
	char *seeker = line;

	while( seeker - in_buffer < buffer_size )
	{
		if( *seeker == EOF )
		{
			return seeker;
		}
        
		if( (*seeker == '\r' && *(seeker + 1) == '\n') || (*seeker == '\n' && *(seeker + 1) == '\r') )
		{
			return seeker + 1;
		}

		if( *seeker == '\n' || *seeker == '\r' || *seeker == EOF )
		{
			return seeker;
		}
		seeker++;
	}

	return NULL;
}

char *get_start_of_line( char *in_buffer, size_t buffer_size, char *previous_line )
{
	char *line = NULL;
	if( previous_line )
	{
		line = get_end_of_line( in_buffer, buffer_size, previous_line );
		if( line && line + 1 < in_buffer + buffer_size )
		{
			return line + 1;
		}
		return NULL;
	}
	else
	{
		return in_buffer;
	}
}


char *get_boundary( char *in_buffer, size_t buffer_size )
{
	char *seeker = in_buffer;
	char *end_seeker = NULL;
	char *boundary = NULL;
	const char *target = "boundary=";
	char *line_end = NULL;
	size_t boundary_len;

	while( seeker - in_buffer < buffer_size )
	{
		if( !strncmp( seeker, target, strlen( target ) ) )
		{
			seeker += strlen( target );
			if(seeker[0] == '\"')
				seeker++;
			end_seeker = seeker;

			while( end_seeker - in_buffer < buffer_size )
			{
				if( *end_seeker == '"' || *end_seeker == '\n' || *end_seeker == '\r' )
				{
					break;
				}
				end_seeker++;
			}

			boundary_len = end_seeker - seeker;
			boundary = malloc( boundary_len + 3 );
			boundary[0] = '-';
			boundary[1] = '-';
			strncpy( boundary + 2, seeker, boundary_len );
			boundary[ boundary_len + 2 ] = '\0';
			return boundary;
		}
		seeker++;
	}

	return NULL;
}

void str_replace( char *string, char target, char replacement )
{
	size_t len = strlen( string );
	int i;

	for( i = 0; i < len; i++ )
	{
		if( string[i] == target )
		{
			string[i] = replacement;
		}
	}
}

void debug_line( char *line, int ACTION_FLAG, int TYPE_FLAG )
{
	printf( "Line\t" );

	switch( ACTION_FLAG )
	{
	case F_PREPARE:
		printf( "F_PREPARE\t" );
		break;
	case F_READ:
		printf( "F_READ\t" );
		break;
	}

	switch( TYPE_FLAG )
	{
	case T_TEXT:
		printf( "T_TEXT\t" );
		break;
	case T_BASE64:
		printf( "T_BASE64\t" );
		break;
	}

	printf( "%s\n", line );
}

struct page *buffer_section( char *section_start, char *section_end )
{
	char *line = NULL;
	char *line_start = NULL;
	char *line_end = NULL;
	size_t line_size;
	size_t buffer_size = section_end - section_start;
	struct page *section_page = new_page();
	int ACTION_FLAG = F_PREPARE;
	int TYPE_FLAG = T_TEXT;


	line_start = section_start;
	while( line_start && line_start < section_end )
	{
		line_end = get_end_of_line( section_start, buffer_size, line_start );

		line_size = line_end - line_start + 1;
		line = realloc( line, line_size + 1 );
		memcpy( line, line_start, line_size );
		line[line_size] = '\0';

		//debug_line( line, ACTION_FLAG, TYPE_FLAG );
		if( ACTION_FLAG == F_PREPARE )
		{
			if( strstr( line, "Content-Transfer-Encoding:" ) )
			{
				if( strstr( line, "base64" ) )
				{
					TYPE_FLAG = T_BASE64;
				}
			}
			if( line[0] == '\r' || line[0] == '\n' || strlen( line ) == 0 )
			{
				ACTION_FLAG = F_READ;
			}
		}
		else if( ACTION_FLAG == F_READ )
		{
			if( TYPE_FLAG == T_BASE64 )
			{
				str_replace(line, '\r', '\0' );
				str_replace(line, '\n', '\0' );
				line_size = decode_buffer( line );
			}
			else if( TYPE_FLAG == T_TEXT )
			{
				line_size = strlen( line );
			}

			line_to_page( line, section_page, line_size );
		}
		line_start = get_start_of_line( section_start, buffer_size, line_start );
	}
	
	if( line )
	{
		free( line );
	}
	
	return section_page;
}

struct page* buffer_to_pages( char *in_buffer, size_t buffer_size )
{
  char *boundary = NULL;
  char *section_start = NULL, *section_end = NULL;
  char *line_start;
  struct page *page_head = NULL;
  struct page *current_page = NULL;
  struct page *new_page = NULL;

  boundary = get_boundary( in_buffer, buffer_size );
  if( !boundary )
  {
	  return NULL;
  }

  line_start = in_buffer;
  while( line_start )
  {

	  line_start = get_start_of_line( in_buffer, buffer_size, line_start );
	  if( line_start && !strncmp( boundary, line_start, strlen( boundary ) ) )
	  {
		  if( !section_start )
		  {
			  section_start = line_start;
		  }
		  else
		  {
			  section_end = line_start;
			  
			  new_page = buffer_section( section_start, section_end );
			  if( !page_head )
			  {
				  page_head = new_page;
				  current_page = new_page;
			  }
			  else
			  {
				  current_page->next = new_page;
				  new_page->previous = current_page;
				  current_page = new_page;
			  }
			  section_start = section_end;
		  }
	  }
  }

  free( boundary );
  return page_head;
}


int main( int argc, char *argv[] )
{
  FILE *input;
  char *in_buffer;

  struct page *current = NULL;

  size_t size;
  int o_count = 0;
  
  if( argc != 2 )
    {
      printf( "Incorrect options\nmht-rip INPUTFILE\n" );
      return EXIT_FAILURE;
    }

  input = fopen( argv[1], "rb" );
  if( input == NULL )
    {
      printf( "Cannot read %s\n", argv[1] );
      return EXIT_FAILURE;
    }


  fseek( input, 0, SEEK_END );
  size = ftell( input );
  rewind( input );

  in_buffer = malloc( size );

  fread( in_buffer, sizeof( char ), size, input );
  fclose( input );

  current = buffer_to_pages( in_buffer, size );
  
  free( in_buffer );

  while( current )
    {
      page_to_file( current, o_count );
      current = current->next;
      o_count++;
    }

  return EXIT_SUCCESS;
}
