/** @file bsondemo.cpp

    Example of use of BSON from C++.

    Requires boost (headers only).
    Works headers only (the parts actually exercised herein that is - some functions require .cpp files).

    To build and run:
      g++ -o bsondemo bsondemo.cpp
      ./bsondemo

    Windows: project files are available in this directory for bsondemo.cpp for use with Visual Studio.
*/

/*
 *    Copyright 2010 10gen Inc.
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

#include "../bson.h"
#include <iostream>
#include <vector>

using namespace std;
using namespace bson;

void iter(bo o) {
    /* iterator example */
    cout << "\niter()\n";
    for( bo::iterator i(o); i.more(); ) {
        cout << ' ' << i.next().toString() << '\n';
    }
}

int main() {
    cout << "build bits: " << 8 * sizeof(char *) << '\n' <<  endl;

    /* a bson object defaults on construction to { } */
    bo empty;
    cout << "empty: " << empty << endl;

    /* make a simple { name : 'joe', age : 33.7 } object */
    {
        bob b;
        b.append("name", "joe");
        b.append("age", 33.7);
        b.obj();
    }

    /* make { name : 'joe', age : 33.7 } with a more compact notation. */
    bo x = bob().append("name", "joe").append("age", 33.7).obj();

    /* convert from bson to json */
    string json = x.toString();
    cout << "json for x:" << json << endl;

    /* access some fields of bson object x */
    cout << "Some x things: " << x["name"] << ' ' << x["age"].Number() << ' ' << x.isEmpty() << endl;

    /* make a bit more complex object with some nesting
       { x : 'asdf', y : true, subobj : { z : 3, q : 4 } }
    */
    bo y = BSON( "x" << "asdf" << "y" << true << "subobj" << BSON( "z" << 3 << "q" << 4 ) );

    /* print it */
    cout << "y: " << y << endl;

    /* reach in and get subobj.z */
    cout << "subobj.z: " << y.getFieldDotted("subobj.z").Number() << endl;

    /* alternate syntax: */
    cout << "subobj.z: " << y["subobj"]["z"].Number() << endl;

    /* fetch all *top level* elements from object y into a vector */
    vector<be> v;
    y.elems(v);
    cout << v[0] << endl;

    /* into an array */
    list<be> L;
    y.elems(L);

    bo sub = y["subobj"].Obj();

    /* grab all the int's that were in subobj.  if it had elements that were not ints, we throw an exception
       (capital V on Vals() means exception if wrong type found
    */
    vector<int> myints;
    sub.Vals(myints);
    cout << "my ints: " << myints[0] << ' ' << myints[1] << endl;

    /* grab all the string values from x.  if the field isn't of string type, just skip it --
       lowercase v on vals() indicates skip don't throw.
    */
    vector<string> strs;
    x.vals(strs);
    cout << strs.size() << " strings, first one: " << strs[0] << endl;

    mongo::BSONObjBuilder topLevel;
    {
        topLevel.append("\"ip\"","10.0.22.163");
        topLevel.append("\"~collection\"", "cloud_monitoring");
        topLevel.append("\"fsedition\"", "SMONGO");

        mongo::BSONArrayBuilder items (topLevel.subarrayStart("cloud_attribute_results"));
        {
            mongo::BSONObjBuilder item (items.subobjStart());
            item.append("dly", 6);
            item.append("er",  "error string in the event of an error collecting the attribute");
            item.append("ere", 5);
            item.append("lc", 4);
            item.append("lv", "string representation of the collected value");
            item.append("pt", 3);
            item.append("op", "operation");
            item.append("adesc", "attribute description");
            item.append("atyp", 2);
            item.append("vtyp", 1);
            item.done();

            mongo::BSONObjBuilder item2 (items.subobjStart());
            item2.append("dly", 10);
            item2.append("er",  "error string in the event of an error collecting the attribute");
            item2.append("ere",  20);
            item2.append("lc", 30);
            item2.append("lv", "string representation of the collected value");
            item2.append("pt", 40);
            item2.append("op", "operation");
            item2.append("adesc", "attribute description");
            item2.append("atyp", 50);
            item2.append("vtyp", 60);
            item2.done();

            mongo::BSONObjBuilder item20 (items.subobjStart());
            item20.append("dly", 100);
            item20.append("er",  "error string in the event of an error collecting the attribute");
            item20.append("ere",  200);
            item20.append("lc", 300);
            item20.append("lv", "string representation of the collected value");
            item20.append("pt", 400);
            item20.append("op", "operation");
            item20.append("adesc", "attribute description");
            item20.append("atyp", 500);
            item20.append("vtyp", 600);
            item20.done();
        }
        items.done();
    }
    mongo::BSONObj finished (topLevel.obj());

    iter(y);
    return 0;
}

