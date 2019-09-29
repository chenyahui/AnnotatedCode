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


AUTOMAKE_OPTIONS = subdir-objects nostdinc
lib_LTLIBRARIES = libflinter_core.la libflinter.la

include_flinterdir = $(includedir)/flinter
include_typesdir = $(includedir)/flinter/types
include_threaddir = $(includedir)/flinter/thread
include_fastcgidir = $(includedir)/flinter/fastcgi
include_linkagedir = $(includedir)/flinter/linkage
include_zookeeperdir = $(includedir)/flinter/zookeeper

SUBDIRS = .

libflinter_core_la_LIBADD =
libflinter_la_LIBADD =

include_flinter_HEADERS = babysitter.h \
                          charset.h \
                          cmdline.h \
                          common.h \
                          convert.h \
                          crawler.h \
                          daemon.h \
                          encode.h \
                          envvars.h \
                          explode.h \
                          factory.h \
                          hash.h \
                          logger.h \
                          mkdirs.h \
                          msleep.h \
                          object_map.h \
                          object_pool.h \
                          openssl.h \
                          pidfile.h \
                          pkcs7.h \
                          revision.h \
                          rmdirs.h \
                          runnable.h \
                          safeio.h \
                          signals.h \
                          singleton.h \
                          system.h \
                          timeout_pool.h \
                          trim.h \
                          utility.h \
                          xml.h

include_types_HEADERS = types/atomic.h \
                        types/auto_buffer.h \
                        types/decimal.h \
                        types/read_only_map.h \
                        types/scoped_ptr.h \
                        types/shared_ptr.h \
                        types/tree.h \
                        types/unordered_hash.h \
                        types/unordered_map.h \
                        types/unordered_set.h \
                        types/uint128_t.h \
                        types/uuid.h

include_thread_HEADERS = thread/abstract_thread_pool.h \
                         thread/condition.h \
                         thread/fixed_thread_pool.h \
                         thread/growable_thread_pool.h \
                         thread/keyed_thread_pool.h \
                         thread/mutex.h \
                         thread/mutex_locker.h \
                         thread/read_locker.h \
                         thread/read_write_lock.h \
                         thread/semaphore.h \
                         thread/scheduler.h \
                         thread/spinlock.h \
                         thread/tls.h \
                         thread/write_locker.h

include_linkage_HEADERS = linkage/abstract_io.h \
                          linkage/async_router.h \
                          linkage/easy_context.h \
                          linkage/easy_handler.h \
                          linkage/easy_tuner.h \
                          linkage/easy_server.h \
                          linkage/file_descriptor_io.h \
                          linkage/interface.h \
                          linkage/linkage.h \
                          linkage/linkage_base.h \
                          linkage/linkage_handler.h \
                          linkage/linkage_peer.h \
                          linkage/linkage_worker.h \
                          linkage/listener.h \
                          linkage/resolver.h \
                          linkage/route_handler.h \
                          linkage/ssl_context.h \
                          linkage/ssl_io.h \
                          linkage/ssl_peer.h

libflinter_core_la_SOURCES = MurmurHash3.c \
                             MurmurHash3.h \
                             babysitter.c \
                             cmdline.c \
                             daemon.c \
                             envvars.c \
                             hash.c \
                             mkdirs.c \
                             msleep.c \
                             pidfile.c \
                             revision_default.c \
                             revision_impl.c \
                             rmdirs.c \
                             safeio.c \
                             signals.c \
                             system.c \
                             utility.c

libflinter_la_SOURCES = $(libflinter_core_la_SOURCES) \
                        thread/abstract_thread_pool.cpp \
                        thread/condition.cpp \
                        thread/fixed_thread_pool.cpp \
                        thread/growable_thread_pool.cpp \
                        thread/keyed_thread_pool.cpp \
                        thread/mutex.cpp \
                        thread/read_write_lock.cpp \
                        thread/scheduler.cpp \
                        thread/semaphore.cpp \
                        thread/thread.h \
                        thread/thread.cpp \
                        thread/thread_job.h \
                        thread/tls.cpp \
                        types/tree.cpp \
                        types/tree_cs.cpp \
                        types/tree_json.cpp \
                        types/tree_xml.cpp \
                        types/uint128_t.cpp \
                        linkage/easy_context.cpp \
                        linkage/easy_handler.cpp \
                        linkage/easy_server.cpp \
                        linkage/file_descriptor_io.cpp \
                        linkage/interface.cpp \
                        linkage/linkage.cpp \
                        linkage/linkage_base.cpp \
                        linkage/linkage_handler.cpp \
                        linkage/linkage_peer.cpp \
                        linkage/linkage_worker.cpp \
                        linkage/listener.cpp \
                        linkage/resolver.cpp \
                        linkage/ssl_context.cpp \
                        linkage/ssl_io.cpp \
                        charset.cpp \
                        convert.cpp \
                        encode.cpp \
                        encode_crypto.cpp \
                        encode_cs.cpp \
                        explode.cpp \
                        logger.cpp \
                        openssl.cpp \
                        trim.cpp

libflinter_la_LIBADD += @JSONCPP_LIBS@
libflinter_la_LIBADD += @EV_LIBS@

if WITH_MULTIPRECISION
libflinter_la_SOURCES += types/decimal.cpp
libflinter_la_LIBADD += -lgmp
endif

if WITH_OPENSSL
libflinter_core_la_SOURCES += hash_crypto.c pkcs7.c
libflinter_core_la_LIBADD += ${openssl_LIBS}
libflinter_la_LIBADD += ${openssl_LIBS}
endif

if WITH_LIBCURL
libflinter_la_SOURCES += crawler.cpp
libflinter_la_LIBADD += ${libcurl_LIBS}
endif

if WITH_LIBXML2
libflinter_la_SOURCES += xml.cpp
libflinter_la_LIBADD += ${libxml2_LIBS}
endif

if WITH_LIBUUID
libflinter_la_SOURCES += types/uuid.cpp
libflinter_la_LIBADD += ${libuuid_LIBS}
endif

if WITH_ICU
libflinter_la_SOURCES += charset_icu.cpp
libflinter_la_LIBADD += ${icu_LIBS}
else
libflinter_la_SOURCES += charset_iconv.cpp
libflinter_la_LIBADD += -liconv
endif

if WITH_ZOOKEEPER
include_zookeeper_HEADERS = zookeeper/council.h \
                            zookeeper/naming.h \
                            zookeeper/zookeeper.h \
                            zookeeper/zookeeper_callback.h \
                            zookeeper/zookeeper_tracker.h \
                            zookeeper/zookeeper_watcher.h

libflinter_la_SOURCES += zookeeper/council.cpp \
                         zookeeper/naming.cpp \
                         zookeeper/zookeeper.cpp \
                         zookeeper/zookeeper_callback.cpp \
                         zookeeper/zookeeper_tracker.cpp \
                         zookeeper/zookeeper_watcher.cpp

libflinter_la_LIBADD += -lzookeeper_mt
endif

if WITH_FASTCGI
include_fastcgi_HEADERS = fastcgi/cgi.h \
                          fastcgi/common_filters.h \
                          fastcgi/custom_dispatcher.h \
                          fastcgi/default_handlers.h \
                          fastcgi/dispatcher.h \
                          fastcgi/filter.h \
                          fastcgi/http_exception.h \
                          fastcgi/http_status_codes.h

libflinter_la_SOURCES += fastcgi/cgi.cpp \
                         fastcgi/common_filters.cpp \
                         fastcgi/default_handlers.cpp \
                         fastcgi/dispatcher.cpp \
                         fastcgi/http_exception.cpp \
                         fastcgi/http_status_codes.cpp

libflinter_la_LIBADD += -lneo_cgi -lneo_cs -lneo_utl -lfcgi
SUBDIRS += fastcgi/main
endif
