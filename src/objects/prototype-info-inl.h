// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROTOTYPE_INFO_INL_H_
#define V8_OBJECTS_PROTOTYPE_INFO_INL_H_

#include "src/objects/prototype-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(PrototypeInfo)

Map* PrototypeInfo::ObjectCreateMap() {
  return Map::cast(object_create_map()->ToWeakHeapObject());
}

// static
void PrototypeInfo::SetObjectCreateMap(Handle<PrototypeInfo> info,
                                       Handle<Map> map) {
  info->set_object_create_map(HeapObjectReference::Weak(*map));
}

bool PrototypeInfo::HasObjectCreateMap() {
  MaybeObject* cache = object_create_map();
  return cache->IsWeakHeapObject();
}

ACCESSORS(PrototypeInfo, weak_cell, Object, kWeakCellOffset)
ACCESSORS(PrototypeInfo, prototype_users, Object, kPrototypeUsersOffset)
WEAK_ACCESSORS(PrototypeInfo, object_create_map, kObjectCreateMapOffset)
SMI_ACCESSORS(PrototypeInfo, registry_slot, kRegistrySlotOffset)
SMI_ACCESSORS(PrototypeInfo, bit_field, kBitFieldOffset)
BOOL_ACCESSORS(PrototypeInfo, bit_field, should_be_fast_map, kShouldBeFastBit)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROTOTYPE_INFO_INL_H_