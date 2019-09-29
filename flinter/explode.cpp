/* Copyright 2014 yiyuanzhong@gmail.com (Yiyuan Zhong)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "flinter/explode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

namespace flinter {
namespace {

template <class T>
void explode_internal(const std::string &methods,
                      const char *delim,
                      bool preserve_null,
                      T *result)
{
    result->clear();
    size_t pos = 0;
    while (pos < methods.length()) {
        size_t hit = methods.find_first_of(delim, pos);
        if (hit == std::string::npos) {
            result->push_back(methods.substr(pos));
            return;
        }

        if (pos != hit || preserve_null) {
            result->push_back(methods.substr(pos, hit - pos));
        }

        pos = hit + 1;
    }

    if (preserve_null) {
        result->push_back(std::string());
    }
}

} // anonymous namespace

void explode(const std::string &methods,
             const char *delim,
             std::vector<std::string> *result,
             bool preserve_null)
{
    explode_internal(methods, delim, preserve_null, result);
}

void explode(const std::string &methods,
             const char *delim,
             std::list<std::string> *result,
             bool preserve_null)
{
    explode_internal(methods, delim, preserve_null, result);
}

void explode(const std::string &methods,
             const char *delim,
             std::set<std::string> *result)
{
    result->clear();
    size_t pos = 0;
    while (pos < methods.length()) {
        size_t hit = methods.find_first_of(delim, pos);
        if (hit == std::string::npos) {
            result->insert(methods.substr(pos));
            break;
        }

        if (pos != hit) {
            result->insert(methods.substr(pos, hit - pos));
        }

        pos = hit + 1;
    }
}

static const char *explode_list_internal(const char *s,
                                         std::vector<int> *o,
                                         int max)
{
    enum {
        NONE,
        DASH,
        COMMA,
        DIGIT,
        RANGE,
    } last = NONE;
    int digit = -1;

    for (/* Nothing */; /* Nothing */; /* Nothing */) {
        if (*s >= '0' && *s <= '9') {
            char *end;
            unsigned long n;
            if (last != COMMA && last != DASH && last != NONE) {
                return NULL;
            }

            n = strtoul(s, &end, 10);
            if (s == end || n > static_cast<unsigned long>(max)) {
                return NULL;
            }

            int now = static_cast<int>(n);
            if (last == DASH) {
                if (digit > now) {
                    return NULL;
                }

                last = RANGE;
                for (int i = digit; i <= now; ++i) {
                    o->push_back(i);
                }

            } else {
                last = DIGIT;
                digit = now;
            }

            s = end;

        } else if (*s == ',') {
            if (last != DIGIT && last != RANGE) {
                return NULL;
            }

            if (last == DIGIT) {
                o->push_back(digit);
            }

            last = COMMA;
            ++s;

        } else if (*s == '-') {
            if (last != DIGIT) {
                return NULL;
            }

            last = DASH;
            ++s;

        } else if (*s == ' ' || *s == '\t') {
            ++s;

        } else if (*s == ';' || *s == '\0') {
            if (last == DIGIT) {
                o->push_back(digit);
            } else if (last != RANGE && last != NONE) {
                return NULL;
            }

            ++s;
            break;

        } else {
            return NULL;
        }
    }

    std::sort(o->begin(), o->end());
    std::vector<int>::iterator ne = std::unique(o->begin(), o->end());
    o->erase(ne, o->end());
    return s;
}

int explode_list(const std::string &input,
                 std::vector<int> *output,
                 int max)
{
    if (!output || max < 0) {
        return -1;
    }

    output->clear();
    return explode_list_internal(input.c_str(), output, max) ? 0 : -1;
}

int explode_lists(const std::string &input,
                  std::vector<std::vector<int> > *output,
                  int max)
{
    if (!output || max < 0) {
        return -1;
    }

    output->clear();
    const char *s = input.c_str();
    const char *e = s + input.length();
    while (s <= e) {
        std::vector<int> one;
        s = explode_list_internal(s, &one, max);
        if (!s) {
            return -1;
        }

        output->push_back(one);
    }

    return 0;
}

} // namespace flinter
