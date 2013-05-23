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

#include "mongo/db/ops/modifier_push.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "mongo/base/status.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/mutable/algorithm.h"
#include "mongo/bson/mutable/document.h"
#include "mongo/bson/mutable/mutable_bson_test_utils.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/json.h"
#include "mongo/platform/cstdint.h"
#include "mongo/unittest/unittest.h"

namespace {

    using mongo::BSONObj;
    using mongo::BSONObjBuilder;
    using mongo::BSONArrayBuilder;
    using mongo::fromjson;
    using mongo::ModifierInterface;
    using mongo::ModifierPush;
    using mongo::NumberInt;
    using mongo::Status;
    using mongo::StringData;
    using mongo::mutablebson::checkDoc;
    using mongo::mutablebson::ConstElement;
    using mongo::mutablebson::countChildren;
    using mongo::mutablebson::Document;
    using mongo::mutablebson::Element;
    using std::sort;
    using std::vector;

    void combineVec(const vector<int>& origVec,
                    const vector<int>& modVec,
                    int32_t slice,
                    vector<int>* combined) {

        // TODO: support positive slices, when $push does so.
        ASSERT_LESS_THAN_OR_EQUALS(slice, 0);

        combined->clear();
        int32_t origSize = origVec.size();
        int32_t modSize = modVec.size();
        for (int32_t i = std::max(0, origSize + modSize + slice);
             i < origSize;
             i++) {
            combined->push_back(origVec[i]);
        }
        for (int32_t i = std::max(0, modSize + slice);
             i < modSize;
             i++) {
            combined->push_back(modVec[i]);
        }
    }

    /**
     * Comparator between two BSONObjects that takes in consideration only the keys and
     * direction described in the sort pattern.
     */
    struct ProjectKeyCmp {
        BSONObj sortPattern;

        ProjectKeyCmp(BSONObj pattern) : sortPattern(pattern) {}

        int operator()(const BSONObj& left, const BSONObj& right) const {
            BSONObj keyLeft = left.extractFields(sortPattern, true);
            BSONObj keyRight = right.extractFields(sortPattern, true);
            return keyLeft.woCompare(keyRight, sortPattern) < 0;
        }
    };

    void combineAndSortVec(const vector<BSONObj>& origVec,
                           const vector<BSONObj>& modVec,
                           int32_t slice,
                           BSONObj sortOrder,
                           vector<BSONObj>* combined) {

        // TODO: support positive slices, when $push does so.
        ASSERT_LESS_THAN_OR_EQUALS(slice, 0);

        combined->clear();
        *combined = origVec;
        combined->insert(combined->end(), modVec.begin(), modVec.end());

        sort(combined->begin(), combined->end(), ProjectKeyCmp(sortOrder));

        int size = combined->size();
        for (int i = 0; i < size + slice; i++) {
            combined->erase(combined->begin());
        }
    }

    //
    // Init testing (module field checking, which is done in 'fieldchecker'
    //

    TEST(Init, SimplePush) {
        BSONObj modObj = fromjson("{$push: {x: 0}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, SimplePushNotOkForStorage) {
        BSONObj modObj = fromjson("{$push: {$inc: {x: 1}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    //
    // If present, is the $each clause valid?
    //

    TEST(Init, PushEachNormal) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2]}}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachMixed) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, {a: 2}]}}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachObject) {
        // $each must be an array
        BSONObj modObj = fromjson("{$push: {x: {$each: {'0': 1}}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachSimpleType) {
        // $each must be an array.
        BSONObj modObj = fromjson("{$push: {x: {$each: 1}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    // TODO Should we check this? We currently don't.
    // TEST(Init, PushEachDotted) {
    //     // Push does not create structure; so dotted fields are invalid
    //     BSONObj modObj = fromjson("{$push: {x: {$each: [{'a.b': 1}]}}}}");
    //     ModifierPush mod;
    //     ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    // }

    TEST(Init, PushEachEmpty) {
        BSONObj modObj = fromjson("{$push: {x: {$each: []}}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithNotOkForStorage) {
        // $each items shouldn't contain '$' in field names.
        BSONObj modObj = fromjson("{$push: {x: {$each: [{$inc: {b: 1}}]}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachInvalidType) {
        // $each must be an array.
        BSONObj modObj = fromjson("{$push: {x: {$each: {b: 1}}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    //
    // If present, is the $slice clause valid?
    //

    TEST(Init, PushEachWithSlice) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: -3}}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithInvalidSliceObject) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: {a: 1}}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithInvalidSliceDouble) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: -2.1}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithValidSliceDouble) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: -2.0}}}");
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithUnsupportedPositiveSlice) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: 3}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithUnsupportedFullSlice) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: [1,2]}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithWrongTypeSlice) {
        BSONObj modObj = fromjson("{$push: {x: {$each: [1, 2], $slice: '-1'}}}");
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    //
    // If present, is the sort $sort clause valid?
    //

    TEST(Init, PushEachWithSort) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithInvalidSortType) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: [{a:1}]}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachDuplicateSortPattern) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: [{a:1,a:1}]}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithInvalidSortValue) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {a:100}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithEmptySortField) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {'':1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithEmptyDottedSortField) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {'.':1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithMissingSortFieldSuffix) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {'a.':1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithMissingSortFieldPreffix) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {'.b':1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithMissingSortFieldMiddle) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {'a..b':1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithUnsupportedSort) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort:1 }}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithEmptySort) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort:{} }}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    //
    // Are all clauses present? Is anything extroneous? Is anything duplicated?
    //

    TEST(Init, PushEachWithSortMissingSlice) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $sort:{a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachInvalidClause) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $xxx: -1, $sort:{a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachExtraField) {
        const char* c = "{$push: {x: {$each: [{a:1},{a:2}], $slice: -2.0, $sort: {a:1}, b: 1}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachDuplicateSortClause) {
        const char* c = "{$push: {x:{$each:[{a:1},{a:2}], $slice:-2.0, $sort:{a:1}, $sort:{a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachDuplicateSliceClause) {
        const char* c = "{$push: {x: {$each:[{a:1},{a:2}], $slice:-2.0, $slice:-2, $sort:{a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachDuplicateEachClause) {
        const char* c = "{$push: {x: {$each:[{a:1}], $each:[{a:2}], $slice:-3, $sort:{a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_NOT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithSliceFirst) {
        const char* c = "{$push: {x: {$slice: -2.0, $each: [{a:1},{a:2}], $sort: {a:1}}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    TEST(Init, PushEachWithSortFirst) {
        const char* c = "{$push: {x: {$sort: {a:1}, $slice: -2.0, $each: [{a:1},{a:2}]}}}";
        BSONObj modObj = fromjson(c);
        ModifierPush mod;
        ASSERT_OK(mod.init(modObj["$push"].embeddedObject().firstElement()));
    }

    //
    // Simple mod
    //

    /** Helper to build and manipulate a $set mod. */
    class Mod {
    public:
        Mod() : _mod() {}

        explicit Mod(BSONObj modObj) {
            _modObj = modObj;
            ASSERT_OK(_mod.init(_modObj["$push"].embeddedObject().firstElement()));
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

        ModifierPush& mod() { return _mod; }

    private:
        ModifierPush _mod;
        BSONObj _modObj;
    };

    TEST(SimpleMod, PrepareNonArray) {
        Document doc(fromjson("{a: 1}"));
        Mod pushMod(fromjson("{$push: {a: 1}}"));

        ModifierInterface::ExecInfo dummy;
        ASSERT_NOT_OK(pushMod.prepare(doc.root(), "", &dummy));
    }

    TEST(SimpleMod, PrepareApplyEmpty) {
        Document doc(fromjson("{a: []}"));
        Mod pushMod(fromjson("{$push: {a: 1}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [1]}}")));
    }

    TEST(SimpleMod, PrepareApplyInexistent) {
        Document doc(fromjson("{}"));
        Mod pushMod(fromjson("{$push: {a: 1}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [1]}}")));
    }

    TEST(SimpleMod, PrepareApplyNormal) {
        Document doc(fromjson("{a: [0]}"));
        Mod pushMod(fromjson("{$push: {a: 1}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [0,1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [0,1]}}")));
    }

    //
    // Simple object mod
    //

    TEST(SimpleObjMod, PrepareNonArray) {
        Document doc(fromjson("{a: 1}"));
        Mod pushMod(fromjson("{$push: {a: {b: 1}}}"));

        ModifierInterface::ExecInfo dummy;
        ASSERT_NOT_OK(pushMod.prepare(doc.root(), "", &dummy));
    }

    TEST(SimpleObjMod, PrepareApplyEmpty) {
        Document doc(fromjson("{a: []}"));
        Mod pushMod(fromjson("{$push: {a: {b: 1}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [{b:1}]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [{b:1}]}}")));
    }

    TEST(SimpleObjMod, PrepareApplyInexistent) {
        Document doc(fromjson("{}"));
        Mod pushMod(fromjson("{$push: {a: {b: 1}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [{b:1}]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [{b:1}]}}")));
    }

    TEST(SimpleObjMod, PrepareApplyNormal) {
        Document doc(fromjson("{a: [{b:0}]}"));
        Mod pushMod(fromjson("{$push: {a: {b: 1}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [{b:0},{b:1}]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [{b:0},{b:1}]}}")));
    }

    //
    // Simple $each mod
    //

    TEST(SimpleEachMod, PrepareNonArray) {
        Document doc(fromjson("{a: 1}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1]}}}"));

        ModifierInterface::ExecInfo dummy;
        ASSERT_NOT_OK(pushMod.prepare(doc.root(), "", &dummy));
    }

    TEST(SimpleEachMod, PrepareApplyEmpty) {
        Document doc(fromjson("{a: []}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1]}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [1]}}")));
    }

     TEST(SimpleEachMod, PrepareApplyInexistent) {
        Document doc(fromjson("{}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1]}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [1]}}")));
    }

     TEST(SimpleEachMod, PrepareApplyInexistentMultiple) {
        Document doc(fromjson("{}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1, 2]}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [1, 2]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [1, 2]}}")));
    }

    TEST(SimpleEachMod, PrepareApplyNormal) {
        Document doc(fromjson("{a: [0]}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1]}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [0,1]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [0,1]}}")));
    }

    TEST(SimpleEachMod, PrepareApplyNormalMultiple) {
        Document doc(fromjson("{a: [0]}"));
        Mod pushMod(fromjson("{$push: {a: {$each: [1,2]}}}"));

        ModifierInterface::ExecInfo execInfo;
        ASSERT_OK(pushMod.prepare(doc.root(), "", &execInfo));

        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
        ASSERT_FALSE(execInfo.inPlace);
        ASSERT_FALSE(execInfo.noOp);

        ASSERT_OK(pushMod.apply());
        ASSERT_TRUE(checkDoc(doc, fromjson("{a: [0,1,2]}")));

        Document logDoc;
        ASSERT_OK(pushMod.log(logDoc.root()));
        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
        ASSERT_TRUE(checkDoc(logDoc, fromjson("{$set: {a: [0,1,2]}}")));
    }

    /**
     * This fixture supports building $push mods with parameterized $each arrays and $slices.
     * It always assume that the array being operated on is called 'a'. To build a mod, one
     * issues a set*Mod() call.
     *
     * The setSimpleMod() call will build a $each array of numbers. The setObjectMod() call
     * will build a $each array with object. Both these calls take the slice as a parameter as
     * well.
     *
     * Here's a typical test case flow:
     *  + Determine what the original document's 'a' array would contain
     *  + Ditto for the $push's $each arrray
     *  + Loop over slice value
     *    + Apply the $push with current slice value to the doc
     *    + Use the fixture/helpers to combine and slice the mod's and original's 'a'
     *      array
     *    + Build a document with the above and check against the one generated by the mod apply
     */
    class SlicedMod : public mongo::unittest::Test {
    public:
        SlicedMod() : _mod() {}

        virtual void setUp() {
            // no op; set all state using the setMod() call
        }

        /** Sets up the mod to be {$push: {a: {$each: [<eachArray>], $slice: <slice>}}} */
        void setSimpleMod(int32_t slice, const vector<int>& eachArray) {

            BSONArrayBuilder arrBuilder;
            for (vector<int>::const_iterator it = eachArray.begin(); it != eachArray.end(); ++it) {
                arrBuilder.append(*it);
            }

            _modObj = BSON("$push" <<
                           BSON("a"
                                << BSON("$each" << arrBuilder.arr() <<
                                        "$slice" << slice)));

            ASSERT_OK(_mod.init(_modObj["$push"].embeddedObject().firstElement()));
        }

        /** Sets up the mod to be {$push: {a: {$each:[<Obj>,...], $slice:<slice>, $sort:<Obj>}}} */
        void setSortMod(int32_t slice, const vector<BSONObj>& eachArray, BSONObj sort) {

            BSONArrayBuilder arrBuilder;
            for (vector<BSONObj>::const_iterator it = eachArray.begin();
                 it != eachArray.end();
                 ++it) {
                arrBuilder.append(*it);
            }

            _modObj = BSON("$push" <<
                           BSON("a"
                                << BSON("$each" << arrBuilder.arr() <<
                                        "$slice" << slice <<
                                        "$sort" << sort)));

            ASSERT_OK(_mod.init(_modObj["$push"].embeddedObject().firstElement()));
        }

        /** Returns an object {a: [<'vec's content>]} */
        BSONObj getObjectUsing(const vector<int>& vec) {

            BSONArrayBuilder arrBuilder;
            for (vector<int>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
                arrBuilder.append(*it);
            }

            BSONObjBuilder builder;
            builder.appendArray("a", arrBuilder.obj());

            return builder.obj();
        }

        /** Returns an object {a: [<'vec's content>]} */
        BSONObj getObjectUsing(const vector<BSONObj>& vec) {

            BSONArrayBuilder arrBuilder;
            for (vector<BSONObj>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
                arrBuilder.append(*it);
            }

            BSONObjBuilder builder;
            builder.appendArray("a", arrBuilder.obj());

            return builder.obj();
        }

        ModifierPush& mod() { return _mod; }

        BSONObj modObj() { return _modObj; }

    private:
        ModifierPush _mod;
        BSONObj _modObj;
        vector<int> _eachArray;
    };

    TEST_F(SlicedMod, SimpleArrayFromEmpty) {
        // We'll simulate the original document having {a: []} and the mod being
        // {$push: {a: {$each: [1], $slice: <-2..0>}}}
        vector<int> docArray;
        vector<int> eachArray;
        eachArray.push_back(1);

        for (int32_t slice = -2; slice <= 0; slice++) {
            setSimpleMod(slice, eachArray);
            Document doc(getObjectUsing(docArray /* {a: []} */));

            ModifierInterface::ExecInfo execInfo;
            ASSERT_OK(mod().prepare(doc.root(), "", &execInfo));

            ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
            ASSERT_FALSE(execInfo.inPlace);
            ASSERT_FALSE(execInfo.noOp);

            ASSERT_OK(mod().apply());

            vector<int> combinedVec;
            combineVec(docArray,   /* a: []  */
                       eachArray,  /* a: [1] */
                       slice,
                       &combinedVec);
            ASSERT_TRUE(checkDoc(doc, getObjectUsing(combinedVec)));
        }
    }

    TEST_F(SlicedMod, SimpleArrayFromExisting) {
        // We'll simulate the original document having {a: [2,3]} and the mod being
        // {$push: {a: {$each: [1], $slice: <-4..0>}}}
        vector<int> docArray;
        docArray.push_back(2);
        docArray.push_back(3);
        vector<int> eachArray;
        eachArray.push_back(1);

        for (int32_t slice = -4; slice <= 0; slice++) {
            setSimpleMod(slice, eachArray);
            Document doc(getObjectUsing(docArray /* {a: [2, 3]} */));

            ModifierInterface::ExecInfo execInfo;
            ASSERT_OK(mod().prepare(doc.root(), "", &execInfo));

            ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
            ASSERT_FALSE(execInfo.inPlace);
            ASSERT_FALSE(execInfo.noOp);

            ASSERT_OK(mod().apply());

            vector<int> combinedVec;
            combineVec(docArray,  /* a: [2, 3] */
                       eachArray, /* a: [1] */
                       slice,
                       &combinedVec);
            ASSERT_TRUE(checkDoc(doc, getObjectUsing(combinedVec)));
        }
    }

    TEST_F(SlicedMod, ObjectArrayFromEmpty) {
        // We'll simulate the original document having {a: []} and the mod being
        // {$push: {a: {$each: [{a:2,b:1}], $slice: <-4..0>}, $sort: {a:-1/1,b:-1/1}}
        vector<BSONObj> docArray;
        vector<BSONObj> eachArray;
        eachArray.push_back(fromjson("{a:2,b:1}"));
        eachArray.push_back(fromjson("{a:1,b:2}"));

        for (int32_t aOrB = 0; aOrB < 2 ; aOrB++) {
            for (int32_t sortA = 0; sortA < 2; sortA++) {
                for (int32_t sortB = 0; sortB < 2; sortB++) {
                    for (int32_t slice = -3; slice <= 0; slice++) {

                        BSONObj sortOrder;
                        if (aOrB == 0) {
                            sortOrder = BSON("a" << (sortA ? 1 : -1) << "b" << (sortB ? 1 : -1));
                        }
                        else {
                            sortOrder = BSON("b" << (sortB ? 1 : -1) << "a" << (sortA ? 1 : -1));
                        }

                        setSortMod(slice, eachArray, sortOrder);
                        Document doc(getObjectUsing(docArray /* {a: []} */));

                        ModifierInterface::ExecInfo execInfo;
                        ASSERT_OK(mod().prepare(doc.root(), "", &execInfo));

                        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
                        ASSERT_FALSE(execInfo.inPlace);
                        ASSERT_FALSE(execInfo.noOp);

                        ASSERT_OK(mod().apply());

                        vector<BSONObj> combinedVec;
                        combineAndSortVec(docArray,  /* a: [] */
                                          eachArray, /* a: [{a:2,b:1},{a:1,b:2}] */
                                          slice,
                                          sortOrder,
                                          &combinedVec);
                        ASSERT_TRUE(checkDoc(doc, getObjectUsing(combinedVec)));

                        Document logDoc;
                        ASSERT_OK(mod().log(logDoc.root()));
                        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
                        ASSERT_TRUE(checkDoc(logDoc, BSON("$set" << getObjectUsing(combinedVec))));

                    }
                }
            }
        }
    }

    TEST_F(SlicedMod, ObjectArrayFromExisting) {
        // We'll simulate the original document having {a: [{a:2,b:3},{a:3,:b1}]} and the mod being
        // {$push: {a: {$each: [{a:2,b:1}], $slice: <-4..0>}, $sort: {a:-1/1,b:-1/1}}
        vector<BSONObj> docArray;
        docArray.push_back(fromjson("{a:2,b:3}"));
        docArray.push_back(fromjson("{a:3,b:1}"));
        vector<BSONObj> eachArray;
        eachArray.push_back(fromjson("{a:2,b:1}"));

        for (int32_t aOrB = 0; aOrB < 2 ; aOrB++) {
            for (int32_t sortA = 0; sortA < 2; sortA++) {
                for (int32_t sortB = 0; sortB < 2; sortB++) {
                    for (int32_t slice = -4; slice <= 0; slice++) {

                        BSONObj sortOrder;
                        if (aOrB == 0) {
                            sortOrder = BSON("a" << (sortA ? 1 : -1) << "b" << (sortB ? 1 : -1));
                        }
                        else {
                            sortOrder = BSON("b" << (sortB ? 1 : -1) << "a" << (sortA ? 1 : -1));
                        }

                        setSortMod(slice, eachArray, sortOrder);
                        Document doc(getObjectUsing(docArray /* {a: [{a:2,b:b},{a:3,:b1}]} */));

                        ModifierInterface::ExecInfo execInfo;
                        ASSERT_OK(mod().prepare(doc.root(), "", &execInfo));

                        ASSERT_EQUALS(execInfo.fieldRef[0]->dottedField(), "a");
                        ASSERT_FALSE(execInfo.inPlace);
                        ASSERT_FALSE(execInfo.noOp);

                        ASSERT_OK(mod().apply());

                        vector<BSONObj> combinedVec;
                        combineAndSortVec(docArray,  /* a: [{a:2,b:3},{a:3,:b1}] */
                                          eachArray, /* a: [{a:2,b:1}] */
                                          slice,
                                          sortOrder,
                                          &combinedVec);
                        ASSERT_TRUE(checkDoc(doc, getObjectUsing(combinedVec)));

                        Document logDoc;
                        ASSERT_OK(mod().log(logDoc.root()));
                        ASSERT_EQUALS(countChildren(logDoc.root()), 1u);
                        ASSERT_TRUE(checkDoc(logDoc, BSON("$set" << getObjectUsing(combinedVec))));

                    }
                }
            }
        }
    }

} // unnamed namespace
