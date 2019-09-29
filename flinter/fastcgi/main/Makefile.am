# Copyright 2014 yiyuanzhong@gmail.com (Yiyuan Zhong)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


AUTOMAKE_OPTIONS = nostdinc
lib_LTLIBRARIES = libflinter_fastcgi_main.la

include_maindir = $(includedir)/flinter/fastcgi/main
include_main_HEADERS = fastcgi_main.h

libflinter_fastcgi_main_la_SOURCES = fastcgi_main.cpp main.cpp
libflinter_fastcgi_main_la_LIBADD = ../../libflinter.la -lprotobuf
