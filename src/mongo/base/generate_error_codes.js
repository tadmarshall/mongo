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

function main() {
    checkArguments();

    var fso = new ActiveXObject("Scripting.FileSystemObject");
}

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

main();
