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

#include "flinter/types/tree.h"

#include <string.h>

#include "config.h"
#if HAVE_CLEARSILVER_CLEARSILVER_H
#include <ClearSilver/ClearSilver.h>
#endif

namespace flinter {
namespace {

#if HAVE_CLEARSILVER_CLEARSILVER_H
static NEOERR *RenderCallback(void *ctx, char *buffer)
{
    std::ostream &out = *reinterpret_cast<std::ostream *>(ctx);
    out << buffer;
    return STATUS_OK;
}
#endif // HAVE_CLEARSILVER_CLEARSILVER_H

} // anonymous namespace

#if HAVE_CLEARSILVER_CLEARSILVER_H
bool Tree::RenderTemplateInternal(const std::string &tmpl,
                                  bool file_or_string,
                                  std::ostream *out,
                                  std::string *error) const
{
    if (!out) {
        return false;
    }

    CSPARSE *csparse = NULL;
    HDF *hdf = NULL;
    NEOERR *err;

    do {
        err = hdf_init(&hdf);
        if (err != STATUS_OK) {
            break;
        }

        if (!SerializeToHdfInternal(hdf, true)) {
            hdf_destroy(&hdf);
            if (error) {
                *error = "SerializationError: serializing to HDF failed";
            }
            return false;
        }

        err = cs_init(&csparse, hdf);
        if (err != STATUS_OK) {
            break;
        }

        err = cgi_register_strfuncs(csparse);
        if (err != STATUS_OK) {
            break;
        }

        if (file_or_string) {
            err = cs_parse_file(csparse, tmpl.c_str());

        } else {
            char *ctmpl = strdup(tmpl.c_str());
            if (!ctmpl) {
                cs_destroy(&csparse);
                hdf_destroy(&hdf);
                if (error) {
                    *error = "MemoryError: allocating buffer for template";
                }
                break;
            }

            err = cs_parse_string(csparse, ctmpl, tmpl.length());
        }

        if (err != STATUS_OK) {
            break;
        }

        err = cs_render(csparse, out, RenderCallback);
        if (err != STATUS_OK) {
            break;
        }

    } while (false);

    cs_destroy(&csparse);
    hdf_destroy(&hdf);

    if (err != STATUS_OK && error) {
        STRING str;
        string_init(&str);
        nerr_error_string(err, &str);
        *error = str.buf;
        string_clear(&str);
        nerr_ignore(&err);
        return false;
    }

    return true;
}

bool Tree::SerializeToHdfFile(const std::string &filename,
                              bool only_nodes_with_value) const
{
    NEOERR *err;
    HDF *hdf;

    err = hdf_init(&hdf);
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        return false;
    }

    if (!SerializeToHdfInternal(hdf, only_nodes_with_value)) {
        hdf_destroy(&hdf);
        return false;
    }

    err = hdf_write_file(hdf, filename.c_str());
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        hdf_destroy(&hdf);
        return false;
    }

    hdf_destroy(&hdf);
    return true;
}

bool Tree::SerializeToHdfString(std::string *serialized,
                                bool only_nodes_with_value) const
{
    if (!serialized) {
        return false;
    }

    NEOERR *err;
    HDF *hdf;

    err = hdf_init(&hdf);
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        return false;
    }

    if (!SerializeToHdfInternal(hdf, only_nodes_with_value)) {
        hdf_destroy(&hdf);
        return false;
    }

    char *str;
    err = hdf_write_string(hdf, &str);
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        hdf_destroy(&hdf);
        return false;
    }

    serialized->assign(str);
    hdf_destroy(&hdf);
    free(str);

    return true;
}

bool Tree::SerializeToHdfInternal(struct _hdf *hdf,
                                  bool only_nodes_with_value) const
{
    for (std::map<std::string, Tree *>::const_iterator p = _children.begin();
         p != _children.end(); ++p) {

        if (!p->second->SerializeToHdfInternal(hdf, only_nodes_with_value)) {
            return false;
        }
    }

    // Any intermediate nodes with empty value are filtered out.
    // Leaf nodes are controlled by only_nodes_with_value.
    if (_value.empty()) {
        if (!_children.empty() || only_nodes_with_value) {
            return true;
        }
    }

    NEOERR *err = hdf_set_value(hdf, _full_path.c_str(), _value.c_str());
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        return false;
    }

    return true;
}

bool Tree::ParseFromHdfFile(const std::string &filename)
{
    Clear();

    HDF *hdf;
    NEOERR *err = hdf_init(&hdf);
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        return false;
    }

    err = hdf_read_file(hdf, filename.c_str());
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        hdf_destroy(&hdf);
        return false;
    }

    HDF *child = hdf_obj_child(hdf);
    if (!child) {
        hdf_destroy(&hdf);
        return false;
    }

    if (!ParseFromHdfInternal(child)) {
        hdf_destroy(&hdf);
        return false;
    }

    hdf_destroy(&hdf);
    return true;
}

bool Tree::ParseFromHdfString(const std::string &serialized)
{
    Clear();
    if (serialized.empty()) {
        return true;
    }

    HDF *hdf;
    NEOERR *err = hdf_init(&hdf);
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        return false;
    }

    err = hdf_read_string(hdf, serialized.c_str());
    if (err != STATUS_OK) {
        nerr_ignore(&err);
        hdf_destroy(&hdf);
        return false;
    }

    HDF *child = hdf_obj_child(hdf);
    if (!child) {
        hdf_destroy(&hdf);
        return false;
    }

    if (!ParseFromHdfInternal(child)) {
        hdf_destroy(&hdf);
        return false;
    }

    hdf_destroy(&hdf);
    return true;
}

bool Tree::ParseFromHdfInternal(struct _hdf *hdf)
{
    while (hdf) {
        const char *key = hdf_obj_name(hdf);
        const char *value = hdf_obj_value(hdf);
        if (!value) {
            value = "";
        }

        std::string full_path = GetFullPath(key);
        Tree *child = new Tree(full_path, key, value);
        _children.insert(std::make_pair(key, child));

        HDF *hc = hdf_obj_child(hdf);
        if (!child->ParseFromHdfInternal(hc)) {
            return false;
        }

        hdf = hdf_obj_next(hdf);
    }

    return true;
}
#else
bool Tree::ParseFromHdfInternal(struct _hdf *)
{
    return false;
}
#endif // HAVE_CLEARSILVER_CLEARSILVER_H

} // namespace flinter
