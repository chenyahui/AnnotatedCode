/* Copyright 2016 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_OPENSSL_H
#define FLINTER_OPENSSL_H

namespace flinter {

/**
 * Create an object in main() before going multi-threaded once only.
 * Then forget about it.
 */
class OpenSSLInitializer {
public:
    OpenSSLInitializer();
    ~OpenSSLInitializer();

    void Shutdown();

private:
    bool _initialized;

}; // class OpenSSLInitializer

} // namespace flinter

#endif // FLINTER_OPENSSL_H
