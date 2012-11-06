// generate_error_codes.js

/*   Copyright 2012 10gen Inc.
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

// This JavaScript file is run under Windows Script Host from the Visual Studio build.
// It creates error_codes.h and error_codes.cpp from error_codes.err and is intended to
// duplicate the functionality of the generate_error_codes.py Python program.  By using
// only standard Windows components (Windows Script Host, JScript) we avoid the need for
// Visual Studio builders to install Python.

function die(message) {
    WScript.Echo(message);
    WScript.Quit(1);
}

function usage(message) {
    WScript.Echo([
        'Generate error_codes.{h,cpp} from error_codes.err.',
        '',
        'Format of error_codes.err:',
        '',
        'error_code("symbol1", code1)',
        'error_code("symbol2", code2)',
        '...',
        'error_class("class1", ["symbol1", "symbol2, ..."])',
        '',
        'Usage:',
        '    cscript generate_error_codes.js <path to error_codes.err> <header file path> <source file path>'
    ].join('\n'));
    die(message);
}

function checkArguments() {
    if (WScript.Arguments.Unnamed.Count != 3) {
        usage("Wrong number of arguments.");
    }
}

function exitIfOutputFilesNewer(fso, errFile, headerFile, sourceFile) {
    if (!fso.FileExists(errFile)) {
        die("Input file " + errFile + " does not exist");
    }
    if (!fso.FileExists(headerFile) || !fso.FileExists(sourceFile)) {
        return;
    }
    var errFileDate = fso.GetFile(errFile).DateLastModified;
    if (fso.GetFile(headerFile).DateLastModified > errFileDate &&
        fso.GetFile(sourceFile).DateLastModified > errFileDate) {
        WScript.Quit(1);
    }
}

function readErrFile(fso, errFileName) {
    var errFileText = [];
    var errFile = fso.GetFile(errFileName);
    var inputStream = errFile.OpenAsTextStream(1 /* ForReading */, 0 /* TristateFalse == ASCII */);
    while (!inputStream.AtEndOfStream) {
        errFileText.push(inputStream.ReadLine());
    }
    inputStream.Close();
    return errFileText;
}

function parseErrorCodes(errFileText) {
    var errorCodes = [];
    var errorCodeRegex = "\\s*error_code\\(\\s*\"([\\w]+)\"\\s*,\\s*(\\d+)\\s*\\)";
    for (var i = 0, len = errFileText.length; i < len; ++i) {
        var match = errFileText[i].match(errorCodeRegex);
        if (match) {
            var codePair = [];
            codePair.push(match[1]);
            codePair.push(match[2]);
            errorCodes.push(codePair);
        }
    }
    return errorCodes;
}

function parseErrorClasses(errFileText) {
    var errorClasses = [];
    var errorClassRegex = '\\s*error_class\\(\\s*"([\\w]+)"\\s*,\\s*\\[\\s*(.*)\\s*\\]\\s*\\)\\s*';
    for (var i = 0, len = errFileText.length; i < len; ++i ) {
        var match = errFileText[i].match(errorClassRegex);
        if (match) {
            var codePair = [];
            codePair.push(match[1]);
            codePair.push(match[2]);
            errorClasses.push(codePair);
        }
    }
    return errorClasses;
}

function generateHeader(headerFileName, errorCodes, errorClasses) {
    var headerStart = ['// AUTO-GENERATED FILE DO NOT EDIT',
'// See src/mongo/base/generate_error_codes.py',
'',
'#pragma once',
'',
'#include "mongo/base/string_data.h"',
'',
'namespace mongo {',
'',
'    /**',
'     * This is a generated class containing a table of error codes and their corresponding error',
'     * strings. The class is derived from the definitions in src/mongo/base/error_codes.err file.',
'     *',
'     * Do not update this file directly. Update src/mongo/base/error_codes.err instead.',
'     */',
'',
'    class ErrorCodes {',
'    public:',
'        enum Error {'
    ];
    var headerText = headerStart;
    for (var i = 0, len = errorCodes.length; i < len; ++i) {
        var line = '            ' + errorCodes[i][0] + ' = ' + errorCodes[i][1] + ',';
        headerText.push(line);
    }
    var headerMiddle = ['            MaxError',
'        };',
'',
'        static const char* errorString(Error err);',
'',
'        /**',
'         * Parse an Error from its "name".  Returns UnknownError if "name" is unrecognized.',
'         *',
'         * NOTE: Also returns UnknownError for the string "UnknownError".',
'         */',
'        static Error fromString(const StringData& name);',
'',
'        /**',
'         * Parse an Error from its "code".  Returns UnknownError if "code" is unrecognized.',
'         *',
'         * NOTE: Also returns UnknownError for the integer code for UnknownError.',
'         */',
'        static Error fromInt(int code);',
''
    ];
    for (var i = 0, len = headerMiddle.length; i < len; ++i) {
        headerText.push(headerMiddle[i]);
    }
    for (var i = 0, len = errorClasses.length; i < len; ++i) {
        var line = '        static bool is' + errorClasses[i][0] + '(Error err);';
        headerText.push(line);
    }
    var headerEnd = ['    };',
'',
'}  // namespace mongo',
''
    ];
    for (var i = 0, len = headerEnd.length; i < len; ++i) {
        headerText.push(headerEnd[i]);
    }

    var headerFile = fso.CreateTextFile(headerFileName, true /* overwrite */);
    headerFile.Write(headerText.join('\n'));
    headerFile.Close();
}

checkArguments();

var fso = new ActiveXObject("Scripting.FileSystemObject");

var errFileName    = WScript.Arguments.Unnamed.Item(0);
var headerFileName = WScript.Arguments.Unnamed.Item(1);
var sourceFileName = WScript.Arguments.Unnamed.Item(2);
exitIfOutputFilesNewer(fso, errFileName, headerFileName, sourceFileName);

var errFileText = readErrFile(fso, errFileName);

var errorCodes   = parseErrorCodes(errFileText);
var errorClasses = parseErrorClasses(errFileText);

//checkForConflicts(errorCodes, errorClasses);

generateHeader(headerFileName, errorCodes, errorClasses);
//generateSource(sourceFileName, errorCodes, errorClasses);
