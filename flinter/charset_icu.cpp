/* Copyright 2015 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#include "flinter/charset.h"

#include <assert.h>

#include <unicode/ucnv.h>

#include "flinter/types/auto_buffer.h"

namespace flinter {
namespace {

template <class F>
int charset_icu_load(const F &input,
                     AutoBuffer<UChar> *output,
                     const char *encoding,
                     size_t *length)
{
    UConverter *conv;
    UErrorCode error;

    error = U_ZERO_ERROR;
    conv = ucnv_open(encoding, &error);
    if (!conv) {
        return -1;
    }

    error = U_ZERO_ERROR;
    ucnv_setToUCallBack(conv, UCNV_TO_U_CALLBACK_STOP, NULL, NULL, NULL, &error);
    if (error) {
        return -1;
    }

    long outpos = 0;
    size_t outlen = 1024;
    const char *source = reinterpret_cast<const char *>(&input[0]);
    const char *const source_limit = source + input.size() * sizeof(typename F::value_type);

    while (true) {
        output->resize(outlen);
        UChar *target = output->get() + outpos;
        UChar *const target_limit = output->get() + outlen;
        outlen *= 2;

        error = U_ZERO_ERROR;
        ucnv_toUnicode(conv,
                       &target, target_limit,
                       &source, source_limit,
                       NULL, TRUE, &error);

        outpos = target - output->get();
        if (error == U_ZERO_ERROR) {
            *length = static_cast<size_t>(outpos);
            break;
        } else if (error != U_BUFFER_OVERFLOW_ERROR) {
            ucnv_close(conv);
            return -1;
        }
    }

    ucnv_close(conv);
    return 0;
}

template <class T>
int charset_icu_save(const AutoBuffer<UChar> &input,
                     T *output,
                     const char *encoding,
                     size_t length)
{
    UConverter *conv;
    UErrorCode error;

    error = U_ZERO_ERROR;
    conv = ucnv_open(encoding, &error);
    if (!conv) {
        return -1;
    }

    error = U_ZERO_ERROR;
    ucnv_setFromUCallBack(conv, UCNV_FROM_U_CALLBACK_STOP, NULL, NULL, NULL, &error);
    if (error) {
        return -1;
    }

    long outpos = 0;
    size_t outlen = 1024;
    const UChar *source = input.get();
    const UChar *const source_limit = source + length;

    while (true) {
        output->resize(outlen / sizeof(typename T::value_type));
        char *const out = reinterpret_cast<char *>(&(*output)[0]);
        char *target = out + outpos;
        char *const target_limit = out + outlen;
        outlen *= 2;

        error = U_ZERO_ERROR;
        ucnv_fromUnicode(conv,
                         &target, target_limit,
                         &source, source_limit,
                         NULL, TRUE, &error);

        outpos = target - out;
        if (error == U_ZERO_ERROR) {
            output->resize(static_cast<size_t>(outpos) / sizeof(typename T::value_type));
            break;
        } else if (error != U_BUFFER_OVERFLOW_ERROR) {
            ucnv_close(conv);
            return -1;
        }
    }

    ucnv_close(conv);
    return 0;
}

template <class F, class T>
int charset_icu(const F &input,
                T *output,
                const char *from,
                const char *to)
{
    assert(from && *from);
    assert(to && *to);

    if (!output) {
        return -1;
    } else if (input.empty()) {
        output->clear();
        return 0;
    }

    AutoBuffer<UChar> i;
    size_t length;
    int ret;

    ret = charset_icu_load(input, &i, from, &length);
    if (ret) {
        return ret;
    }

    ret = charset_icu_save(i, output, to, length);
    if (ret) {
        return ret;
    }

    return 0;
}

} // anonymous namespace

#define CHARSET(f,t,ef,et) \
int charset_##f##_to_##t(const std::string &from, std::string *to) \
{ \
    return charset_icu(from, to, ef, et); \
}

#define CHARSET_I(tp,f,t,ef,et) \
int charset_##f##_to_##t(const std::basic_string<tp> &from, std::string *to) \
{ \
    return charset_icu(from, to, ef, et); \
}

#define CHARSET_O(tp,f,t,ef,et) \
int charset_##f##_to_##t(const std::string &from, std::basic_string<tp> *to) \
{ \
    return charset_icu(from, to, ef, et); \
}

#define CHARSET_IO(tp1,tp2,f,t,ef,et) \
int charset_##f##_to_##t(const std::basic_string<tp1> &from, std::basic_string<tp2> *to) \
{ \
    return charset_icu(from, to, ef, et); \
}

CHARSET(utf8,    gbk,     "UTF-8",   "GBK"    );
CHARSET(gbk,     utf8,    "GBK",     "UTF-8"  );
CHARSET(utf8,    gb18030, "UTF-8",   "GB18030");
CHARSET(gb18030, utf8,    "GB18030", "UTF-8"  );

CHARSET_O(uint16_t, utf8,  utf16, "UTF-8", "UTF16_PlatformEndian");
CHARSET_O(uint32_t, utf8,  utf32, "UTF-8", "UTF32_PlatformEndian");
CHARSET_I(uint16_t, utf16, utf8,  "UTF16_PlatformEndian", "UTF-8");
CHARSET_I(uint32_t, utf32, utf8,  "UTF32_PlatformEndian", "UTF-8");

CHARSET_IO(uint16_t, uint32_t, utf16, utf32, "UTF16_PlatformEndian", "UTF32_PlatformEndian");
CHARSET_IO(uint32_t, uint16_t, utf32, utf16, "UTF32_PlatformEndian", "UTF16_PlatformEndian");

} // namespace flinter
