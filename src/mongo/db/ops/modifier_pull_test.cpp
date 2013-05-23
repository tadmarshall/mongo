/**
 *    Copyright (C) 2013 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "mongo/db/ops/modifier_pull.h"

#include "mongo/base/string_data.h"
#include "mongo/bson/mutable/document.h"
#include "mongo/bson/mutable/mutable_bson_test_utils.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/json.h"
#include "mongo/platform/cstdint.h"
#include "mongo/unittest/unittest.h"

namespace {

    using mongo::BSONObj;
    using mongo::ModifierPull;
    using mongo::ModifierInterface;
    using mongo::Status;
    using mongo::StringData;
    using mongo::fromjson;
    using mongo::mutablebson::checkDoc;
    using mongo::mutablebson::Document;
    using mongo::mutablebson::Element;

    /** Helper to build and manipulate a $pull mod. */
    class Mod {
    public:
        Mod() : _mod() {}

        explicit Mod(BSONObj modObj)
            : _modObj(modObj)
            , _mod() {
            ASSERT_OK(_mod.init(_modObj["$pull"].embeddedObject().firstElement()));
        }

        Status prepare(Element root,
                       const StringData& matchedField,
                       ModifierInterface::ExecInfo* execInfo) {
            return _mod.prepare(root, matchedField, execInfo);
        }

        Status apply() const {
            return _mod.apply();
        }

        Status log(Element logRoot) const {
            return _mod.log(logRoot);
        }

        ModifierPull& mod() {
            return _mod;
        }

    private:
        BSONObj _modObj;
        ModifierPull _mod;
    };

    TEST(SimpleMod, PrepareOKTargetNotFound) {
        Document doc(fromjson("{}"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_TRUE(execInfo.inPlace);
        ASSERT_TRUE(execInfo.noOp);
    }

    TEST(SimpleMod, PrepareOKTargetFound) {
        Document doc(fromjson("{ a : [ 0, 1, 2, 3 ] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);
    }

    TEST(SimpleMod, PrepareInvalidTargetString) {
        Document doc(fromjson("{ a : 'foo' }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_NOT_OK(mod.prepare(doc.root(), "", &execInfo));
    }

    TEST(SimpleMod, PrepareInvalidTargetObject) {
        Document doc(fromjson("{ a : { 'foo' : 'bar' } }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_NOT_OK(mod.prepare(doc.root(), "", &execInfo));
    }

    TEST(SimpleMod, PrepareAndLogEmptyDocument) {
        Document doc(fromjson("{}"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_TRUE(execInfo.inPlace);
        ASSERT_TRUE(execInfo.noOp);

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $unset : { a : 1 } }")));
    }

    TEST(SimpleMod, PrepareAndLogMissingElementAfterFoundPath) {
        Document doc(fromjson("{ a : { b : { c : {} } } }"));
        Mod mod(fromjson("{ $pull : { 'a.b.c.d' : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a.b.c.d");
        ASSERT_TRUE(execInfo.inPlace);
        ASSERT_TRUE(execInfo.noOp);

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $unset : { 'a.b.c.d' : 1 } }")));
    }

    TEST(SimpleMod, PrepareAndLogEmptyArray) {
        Document doc(fromjson("{ a : [] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_TRUE(execInfo.inPlace);
        ASSERT_TRUE(execInfo.noOp);

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $set : { a : [] } }")));
    }

    TEST(SimpleMod, PullMatchesNone) {
        Document doc(fromjson("{ a : [2, 3, 4, 5] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_TRUE(execInfo.inPlace);
        ASSERT_TRUE(execInfo.noOp);

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $set : { a : [2, 3, 4, 5] } }")));
    }

    TEST(SimpleMod, ApplyAndLogPullMatchesOne) {
        Document doc(fromjson("{ a : [0, 1, 2, 3, 4, 5] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{ a : [ 1, 2, 3, 4, 5 ] }")));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $set : { a : [1, 2, 3, 4, 5] } }")));
    }

    TEST(SimpleMod, ApplyAndLogPullMatchesSeveral) {
        Document doc(fromjson("{ a : [0, 1, 0, 2, 0, 3, 0, 4, 0, 5] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{ a : [ 1, 2, 3, 4, 5 ] }")));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $set : { a : [1, 2, 3, 4, 5] } }")));
    }

    TEST(SimpleMod, ApplyAndLogPullMatchesAll) {
        Document doc(fromjson("{ a : [0, -1, -2, -3, -4, -5] }"));
        Mod mod(fromjson("{ $pull : { a : { $lt : 1 } } }"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{ a : [] }")));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{ $set : { a : [] } }")));
    }


// The following two tests are currently not passing.
#if 0
    TEST(ComplexMod, ApplyAndLogComplexDocAndMatching1) {

        // Fails with 'unknown operator $or'
        const char* const strings[] = {
            // Document:
            "{ a : { b : [ { x : 1 }, { y : 'y' }, { x : 2 }, { z : 'z' } ] } }",

            // Modifier:
            "{ $pull : { 'a.b' : { $or : [ "
            "  { 'y' : { $exists : true } }, "
            "  { 'z' : { $exists : true } } "
            "] } } }",

            // Document result:
            "{ a : { b : [ { x : 1 }, { x : 2 } ] } }",

            // Log result:
            "{ $set : { 'a.b' : [ { x : 1 }, { x : 2 } ] } }"
        };

        Document doc(fromjson(strings[0]));
        Mod mod(fromjson(strings[1]));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a.b");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson(strings[2])));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson(strings[3])));
    }

    TEST(ComplexMod, ApplyAndLogComplexDocAndMatching2) {

        // Fails, doesn't see value as matching
        const char* const strings[] = {
            // Document:
            "{ a : { b : [ { x : 1 }, { y : 'y' }, { x : 2 }, { z : 'z' } ] } }",

            // Modifier:
            "{ $pull : { 'a.b' : { 'y' : { $exists : true } } } }",

            // Document result:
            "{ a : { b : [ { x : 1 }, { x : 2 }, { z : 'z' } ] } }",

            // Log result:
            "{ $set : { 'a.b' : [ { x : 1 }, { x : 2 }, { z : 'z' } ] } }"
        };

        Document doc(fromjson(strings[0]));
        Mod mod(fromjson(strings[1]));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a.b");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson(strings[2])));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson(strings[3])));
    }
#endif

    TEST(ComplexMod, ApplyAndLogComplexDocAndMatching3) {

        const char* const strings[] = {
            // Document:
            "{ a : { b : [ { x : 1 }, { y : 'y' }, { x : 2 }, { z : 'z' } ] } }",

            // Modifier:
            "{ $pull : { 'a.b' : { $in : [ { x : 1 }, { y : 'y' } ] } } }",

            // Document result:
            "{ a : { b : [ { x : 2 }, { z : 'z' } ] } }",

            // Log result:
            "{ $set : { 'a.b' : [ { x : 2 }, { z : 'z' } ] } }"
        };

        Document doc(fromjson(strings[0]));
        Mod mod(fromjson(strings[1]));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a.b");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson(strings[2])));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson(strings[3])));
    }

    TEST(ValueMod, ApplyAndLogScalarValueMod) {

        const char* const strings[] = {
            // Document:
            "{ a : [1, 2, 1, 2, 1, 2] }",

            // Modifier:
            "{ $pull : { a : 1 } }",

            // Document result:
            "{ a : [ 2, 2, 2] }",

            // Log result:
            "{ $set : { a : [ 2, 2, 2 ] } }"
        };

        Document doc(fromjson(strings[0]));
        Mod mod(fromjson(strings[1]));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson(strings[2])));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson(strings[3])));
    }

    TEST(ValueMod, ApplyAndLogObjectValueMod) {

        const char* const strings[] = {
            // Document:
            "{ a : [ { x : 1 }, { y : 2 }, { x : 1 }, { y : 2 } ] }",

            // Modifier:
            "{ $pull : { a : { y : 2 } } }",

            // Document result:
            "{ a : [ { x : 1 }, { x : 1 }] }",

            // Log result:
            "{ $set : { a : [ { x : 1 }, { x : 1 } ] } }"
        };

        Document doc(fromjson(strings[0]));
        Mod mod(fromjson(strings[1]));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(mod.prepare(doc.root(), "", &execInfo));
        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(mod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson(strings[2])));

        Document logDoc;
        ASSERT_OK(mod.log(logDoc.root()));
        ASSERT_TRUE(checkDoc(logDoc, fromjson(strings[3])));
    }


} // namespace
