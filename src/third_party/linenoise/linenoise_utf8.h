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

typedef unsigned char   UChar8;     // UTF-8 octet
//typedef unsigned short  UChar16;    // UTF-16 code unit
typedef unsigned int    UChar32;    // Unicode code point

// Error codes returned from utf8toUChar32string
//
enum BadUTF8 {
    BadUTF8_no_error = 0,
    BadUTF8_invalid_byte,
    BadUTF8_surrogate
};

size_t strlen32( const UChar32* str32 );
UChar32* strcpy32( UChar32* dest32, const UChar32* source32 );
//UChar32* strncpy32( UChar32* dest32, const UChar32* source32, size_t destLength );
//UChar32* strcat32( UChar32* dest32, const UChar32* source32 );
int write32( int fileHandle, const UChar32* string32, unsigned int len );

size_t uChar32toUTF8byCount( UChar8* dest8, const UChar32* string32, size_t charCount, size_t outputBufferSizeInBytes );

size_t uChar32toUTF8string( UChar8* dest8, const UChar32* string32, size_t outputBufferSizeInBytes );

bool utf8toUChar32string(
        UChar32* uchar32output,
        const UChar8* utf8input,
        size_t outputBufferSizeInCharacters,
        size_t & outputUnicodeCharacterCount,
        int & conversionErrorCode );
