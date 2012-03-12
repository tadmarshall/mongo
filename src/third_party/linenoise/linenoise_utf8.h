// linenoise_utf8.h
/*
 *    Copyright 2012 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <boost/smart_ptr/scoped_array.hpp>

typedef unsigned char   UChar8;     // UTF-8 octet
typedef unsigned int    UChar32;    // Unicode code point

// Error codes returned from utf8toUChar32string
//
enum BadUTF8 {
    BadUTF8_no_error = 0,
    BadUTF8_invalid_byte,
    BadUTF8_surrogate
};

size_t strlen32( const UChar32* str32 );

/**
 * Copy a null terminated UChar32 string
 * Always null terminates the destination string if at least one character position is available
 * 
 * @param dest32                    Destination UChar32 string
 * @param source32                  Source UChar32 string
 * @param destLengthInCharacters    Destination length in characters
 */
void copyString32( UChar32* dest32, const UChar32* source32, size_t destLengthInCharacters );

UChar32* strcpy8to32( UChar32* dest32, const char* source8 );

int strncmp32( UChar32* first32, UChar32* second32, size_t length );

/**
 * Internally convert an array of UChar32 characters of specified length to UTF-8 and write it to fileHandle
 *
 * @param fileHandle                File handle to write to
 * @param string32                  Source UChar32 character array, may not be null terminated
 * @param sourceLengthInCharacters  Number of source characters to convert and write
 */
int write32( int fileHandle, const UChar32* string32, unsigned int sourceLengthInCharacters );

size_t uChar32toUTF8byCount( UChar8* dest8, const UChar32* string32, size_t charCount, size_t outputBufferSizeInBytes );

size_t uChar32toUTF8string( UChar8* dest8, const UChar32* string32, size_t outputBufferSizeInBytes );

void utf8toUChar32string(
        UChar32* uchar32output,
        const UChar8* utf8input,
        size_t outputBufferSizeInCharacters,
        size_t & outputUnicodeCharacterCount,
        int & conversionErrorCode );
