// linenoise_utf8.cpp
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

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include "linenoise_utf8.h"

#ifdef _WIN32
#define write _write  // Microsoft headers use underscores in some names
#endif

int write32( int fileHandle, const UChar32 * string32, unsigned int len ) {
    size_t tempBufferBytes = 4 * len + 1;
    UChar8 * tempCharString = new UChar8[ tempBufferBytes ];
    size_t count = uChar32toUTF8string( tempCharString, string32, tempBufferBytes );
    int returnValue = write( fileHandle, tempCharString, count );
    delete [] tempCharString;
    return returnValue;
}

UChar32 * strcpy32( UChar32 * dest32, const UChar32 * source32 ) {
    UChar32 * pOut = dest32;
    while ( *source32 ) {
        *pOut = *source32;
        ++pOut;
        ++source32;
    }
    *pOut = 0;
    return dest32;
}

UChar32 * strchr32( const char * string, UChar32 ch32 ) {
    return 0;
}

size_t uChar32toUTF8string( UChar8 * dest8, const UChar32 * string32, const size_t outputBufferSizeInBytes ) {
    size_t outputUTF8ByteCount = 0;
    size_t reducedBufferSize = outputBufferSizeInBytes - 4;
    while ( *string32 && outputUTF8ByteCount < reducedBufferSize ) {
        UChar32 c = *string32++;
        if ( c <= 0x7F ) {
            *dest8++ = c;
            outputUTF8ByteCount += 1;
        }
        else if ( c <= 0x7FF ) {
            *dest8++ = 0xC0 | ( c >> 6 );
            *dest8++ = 0x80 | ( 0x3F & c );
            outputUTF8ByteCount += 2;
        }
        else if ( c <= 0xFFFF ) {
            *dest8++ = 0xE0 | ( c >> 12 );
            *dest8++ = 0x80 | ( 0x3F & ( c >> 6) );
            *dest8++ = 0x80 | ( 0x3F & c );
            outputUTF8ByteCount += 3;
        }
        else if ( c <= 0x1FFFFF ) {
            *dest8++ = 0xF0 | ( c >> 18 );
            *dest8++ = 0x80 | ( 0x3F & ( c >> 12) );
            *dest8++ = 0x80 | ( 0x3F & ( c >> 6) );
            *dest8++ = 0x80 | ( 0x3F & c );
            outputUTF8ByteCount += 4;
        }
    }
    *dest8 = 0;
    return outputUTF8ByteCount;
}

bool utf8toUChar32string(
        UChar32 * uchar32output,
        const UChar8 * utf8input,
        const size_t outputBufferSizeInCharacters,
        size_t & outputUnicodeCharacterCount,
        int & conversionErrorCode ) {
    conversionErrorCode = BadUTF8_no_error;
    unsigned char * pIn = const_cast< UChar8 * >( utf8input );
    UChar32 * pOut = uchar32output;
    int reducedBufferSize = outputBufferSizeInCharacters - 1;
    while ( *pIn && ( pOut - uchar32output ) < reducedBufferSize ) {
        if ( pIn[0] <= 0x7F ) {         // 0x00000000 to 0x0000007F
            *pOut++ = pIn[0];
            pIn += 1;
        }
        else if ( pIn[0] <= 0xDF ) {    // 0x00000080 to 0x000007FF
            if ( ( pIn[0] >= 0xC2 ) && ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[0] & 0x1F ) << 6 ) | ( pIn[1] & 0x3F );
                pIn += 2;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] == 0xE0 ) {    // 0x00000800 to 0x00000FFF
            if ( ( pIn[1] >= 0xA0 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[1] & 0x3F ) << 6 ) | ( pIn[2] & 0x3F );
                pIn += 3;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] <= 0xEC ) {    // 0x00001000 to 0x0000CFFF
            if ( ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[0] & 0x0F ) << 12 ) | ( ( pIn[1] & 0x3F ) << 6 ) | ( pIn[2] & 0x3F );
                pIn += 3;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] == 0xED ) {    // 0x0000D000 to 0x0000D7FF
            if ( ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0x9F ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF ) ) {
                *pOut++ = ( 0x0D << 12 ) | ( ( pIn[1] & 0x3F ) << 6 ) | ( pIn[2] & 0x3F );
                pIn += 3;
            }
            //                          // 0x0000D800 to 0x0000DFFF -- illegal surrogate value
            else if ( ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF ) ) {
                conversionErrorCode = BadUTF8_surrogate;
                break;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] <= 0xEF ) {    // 0x0000E000 to 0x0000FFFF
            if ( ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[0] & 0x0F ) << 12 ) | ( ( pIn[1] & 0x3F ) << 6 ) | ( pIn[2] & 0x3F );
                pIn += 3;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] == 0xF0 ) {    // 0x00010000 to 0x0003FFFF
            if ( ( pIn[1] >= 0x90 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF )  && ( pIn[3] >= 0x80 ) && ( pIn[3] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[1] & 0x3F ) << 12 ) | ( ( pIn[2] & 0x3F ) << 6 ) | ( pIn[3] & 0x3F );
                pIn += 4;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else if ( pIn[0] <= 0xF4 ) {    // 0x00040000 to 0x0010FFFF
            if ( ( pIn[1] >= 0x80 ) && ( pIn[1] <= 0xBF ) && ( pIn[2] >= 0x80 ) && ( pIn[2] <= 0xBF )  && ( pIn[3] >= 0x80 ) && ( pIn[3] <= 0xBF ) ) {
                *pOut++ = ( ( pIn[0] & 0x07 ) << 18 ) | ( ( pIn[1] & 0x3F ) << 12 ) | ( ( pIn[2] & 0x3F ) << 6 ) | ( pIn[3] & 0x3F );
                pIn += 4;
            }
            else {
                conversionErrorCode = BadUTF8_invalid_byte;
                break;
            }
        }
        else {
            conversionErrorCode = BadUTF8_invalid_byte;
            break;
        }
    }
    *pOut = 0;
    outputUnicodeCharacterCount = pOut - uchar32output;
    return ( conversionErrorCode == BadUTF8_no_error );
}
