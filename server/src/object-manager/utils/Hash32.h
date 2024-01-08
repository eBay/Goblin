/************************************************************************
Copyright 2021-2022 eBay Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/
//
#ifndef SERVER_SRC_OBJECT_MANAGER_UTILS_HASH32_H_
#define SERVER_SRC_OBJECT_MANAGER_UTILS_HASH32_H_
//
// namespace goblin::objectmanager::utils {
//
//  static uint32_t hash32(const char *key, uint32_t len) {
//    static const uint32_t c1 = 0xcc9e2d51;
//    static const uint32_t c2 = 0x1b873593;
//    static const uint32_t r1 = 15;
//    static const uint32_t r2 = 13;
//    static const uint32_t m = 5;
//    static const uint32_t n = 0xe6546b64;
//
//    uint32_t hash = 17; /// better to be a primer number
//
//    const int nblocks = len / 4;
//    const auto *blocks = (const uint32_t *) key;
//    for (int i = 0; i < nblocks; i++) {
//      uint32_t k = blocks[i];
//      k *= c1;
//      k = (k << r1) | (k >> (32 - r1));
//      k *= c2;
//
//      hash ^= k;
//      hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
//    }
//
//    const auto *tail = (const uint8_t *) (key + nblocks * 4);
//    uint32_t k1 = 0;
//
//    switch (len & 3) {
//      case 3:
//        k1 ^= tail[2] << 16;
//      case 2:
//        k1 ^= tail[1] << 8;
//      case 1:
//        k1 ^= tail[0];
//
//        k1 *= c1;
//        k1 = (k1 << r1) | (k1 >> (32 - r1));
//        k1 *= c2;
//        hash ^= k1;
//    }
//
//    hash ^= len;
//    hash ^= (hash >> 16);
//    hash *= 0x85ebca6b;
//    hash ^= (hash >> 13);
//    hash *= 0xc2b2ae35;
//    hash ^= (hash >> 16);
//
//    return hash;
//  }
//}
//
#endif  //  SERVER_SRC_OBJECT_MANAGER_UTILS_HASH32_H_
